// Backend para un motor en OTRO proceso (remote_protocol.h): el modelo de cómo se conectaría el
// editor a Unreal u O3DE. No enlaza ningún motor: arranca uno (o se conecta a uno que ya corre),
// pinta en el viewport la imagen que el motor manda, le reenvía el ratón, y deja que el NÚCLEO
// dibuje la jerarquía y el inspector sobre su escena (genericScene()).
//
// Qué motor:
//   $HARUKA_REMOTE_SOCKET=<ruta>   conectarse a un motor que ya escucha (no se arranca nada)
//   $HARUKA_REMOTE_ENGINE=<exe>    arrancar ese ejecutable con --socket <ruta temporal>
//   por defecto                    <dir del editor>/tools/haruka_mock_engine (el motor simulado)
//
// La imagen viaja por el socket en RGBA8 y se sube a una textura GL cada frame nuevo. Es la vía
// lenta pero universal; la rápida (memoria de GPU compartida, Vulkan external memory ->
// GL_EXT_memory_object_fd) es una optimización de este mismo punto, no un cambio de diseño.
#include <imgui.h>   // ANTES que engine_backend.h: activa harukaBindImGui
#include <glad/glad.h>

#include "backend/engine_backend.h"
#include "panels/panel_registry.h"
#include "remote_client.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {

std::string exeDir() {
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    return ec ? std::string{} : exe.parent_path().string();
}

/// Tiempo límite de lo que el editor espera en el acto (guardar, cargar, abrir proyecto). El
/// viewport y el inspector nunca esperan: van por mensajes sin respuesta.
constexpr int kCallTimeoutMs = 5000;

class RemoteBackend final : public IEngineBackend {
public:
    const char* name() const override { return displayName.c_str(); }

    bool init(const EditorServices& s) override {
        services = s;
        std::string err;
        bool ok = false;
        if (const char* sock = std::getenv("HARUKA_REMOTE_SOCKET"); sock && *sock) {
            ok = client.connectTo(sock, err);
        } else {
            const char* env = std::getenv("HARUKA_REMOTE_ENGINE");
            const std::string exe = (env && *env) ? env : (fs::path(exeDir()) / "tools" / "haruka_mock_engine").string();
            ok = client.launch(exe, err);
        }
        if (!ok) {
            std::cerr << "[remote] sin motor: " << err << std::endl;
            return false;
        }
        displayName = "remote:" + client.engineName();
        std::cout << "[remote] conectado a " << client.engineName() << std::endl;

        PanelEntry e{"viewport", "Viewport", 0, true, false};
        e.draw = [this] { drawViewport(); };
        services.panels->add(std::move(e));
        return true;
    }

    void shutdown() override {
        if (texture) { glDeleteTextures(1, &texture); texture = 0; }
        client.disconnect();
        std::cout << "[remote] frames recibidos: " << framesShown << std::endl;
    }

    void update(float) override {
        if (!client.connected() && !reportedDown) {
            reportedDown = true;
            std::cerr << "[remote] el motor se ha desconectado: " << client.lastError() << std::endl;
        }
    }

    // ── Proyecto y documentos: peticiones con respuesta, con tiempo límite ──────────────────
    bool openProject(const std::string& root) override {
        auto r = client.call({{"type", "open_project"}, {"root", root}}, kCallTimeoutMs);
        const bool ok = r && (*r).value("ok", false);
        if (ok) projectPath = root;
        else std::cerr << "[remote] open_project: " << (r ? (*r).value("error", std::string("?")) : client.lastError()) << std::endl;
        return ok;
    }
    std::string initializeNewProject(const std::string& root, const std::string& projectNameArg) override {
        auto r = client.call({{"type", "new_project"}, {"root", root}, {"name", projectNameArg}}, kCallTimeoutMs);
        if (!r || !(*r).value("ok", false)) return {};
        projectPath = root;
        return (*r).value("document", std::string{});
    }
    std::string projectRoot() const override { return projectPath; }
    std::string projectName() const override { return projectPath.empty() ? std::string{} : fs::path(projectPath).filename().string(); }
    std::string defaultDocument(const std::string& root) const override {
        const fs::path doc = fs::path(root) / "scenes" / "main.mock.json";
        return fs::exists(doc) ? doc.string() : std::string{};
    }
    std::string documentFilter() const override { return "json"; }
    std::string untitledDocument() const override { return "scenes/untitled.mock.json"; }
    bool saveDocument(const std::string& path) override { return docCall("save", path); }
    bool loadDocument(const std::string& path) override { return docCall("load", path); }
    std::string documentKind(const std::string&) const override { return "Escena remota"; }

    ISceneModel* genericScene() override { return &client; }

    bool canPlay() const override { return client.connected(); }
    bool isPlaying() const override { return client.playing(); }
    void enterPlay() override { client.setPlaying(true); }
    void exitPlay() override { client.setPlaying(false); }

private:
    bool docCall(const char* type, const std::string& path) {
        auto r = client.call({{"type", type}, {"path", path}}, kCallTimeoutMs);
        if (r && (*r).value("ok", false)) return true;
        std::cerr << "[remote] " << type << " " << path << ": "
                  << (r ? (*r).value("error", std::string("?")) : client.lastError()) << std::endl;
        return false;
    }

    void drawViewport() {
        if (!ImGui::Begin("Viewport")) { ImGui::End(); return; }
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const int w = std::max(1, (int)avail.x), h = std::max(1, (int)avail.y - (client.connected() ? 0 : 20));

        if (!client.connected()) {
            ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.4f, 1), "Motor desconectado: %s", client.lastError().c_str());
        }
        client.requestFrame(w, h);   // no hace nada si ya hay uno en camino

        RemoteClient::Frame f;
        if (client.takeFrame(f, lastSeq)) {
            lastSeq = f.seq;
            ++framesShown;
            if (!texture) {
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
            glBindTexture(GL_TEXTURE_2D, texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            if (f.w != texW || f.h != texH) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, f.w, f.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, f.rgba.data());
                texW = f.w; texH = f.h;
            } else {
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, f.w, f.h, GL_RGBA, GL_UNSIGNED_BYTE, f.rgba.data());
            }
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        if (texture) {
            // La imagen se estira al tamaño del panel mientras llega la del tamaño nuevo: un frame
            // de retraso al redimensionar, nunca un hueco.
            ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2((float)w, (float)h));
            const ImVec2 o = ImGui::GetItemRectMin();
            if (ImGui::IsItemHovered()) {
                ImGuiIO& io = ImGui::GetIO();
                if (io.MouseWheel != 0.0f) client.wheel(io.MouseWheel);
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) client.drag(io.MouseDelta.x, io.MouseDelta.y);
                    dragged = true;
                }
                // Clic SIN arrastre = seleccionar (lo decide el motor, que es quien sabe qué hay ahí).
                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    if (!dragged) client.click(io.MousePos.x - o.x, io.MousePos.y - o.y, w, h);
                    dragged = false;
                }
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) dragged = false;
        } else {
            ImGui::TextDisabled("Esperando la primera imagen del motor...");
        }
        ImGui::End();
    }

    EditorServices services;
    RemoteClient client;
    std::string displayName = "remote";
    std::string projectPath;

    GLuint texture = 0;
    int texW = 0, texH = 0;
    uint64_t lastSeq = 0;
    uint64_t framesShown = 0;
    bool dragged = false;
    bool reportedDown = false;
};

} // namespace

HARUKA_EDITOR_BACKEND_DECLARE_API();

HARUKA_EDITOR_PLUGIN_EXPORT IEngineBackend* haruka_editor_backend_create(const HarukaImGuiAbi* imgui,
                                                                       char* error, size_t errorLen) {
    if (!imgui || !harukaBindImGui(*imgui)) {
        std::snprintf(error, errorLen, "ImGui distinto: el editor trae %s, este backend se compilo con %s",
                      imgui && imgui->version ? imgui->version : "?", IMGUI_VERSION);
        return nullptr;
    }
    return new RemoteBackend();
}
