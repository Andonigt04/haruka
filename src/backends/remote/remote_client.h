#pragma once

#include "backend/scene_model.h"
#include "remote_protocol.h"

#include <sys/types.h>

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

/**
 * @brief Conexión con un motor en otro proceso (remote_protocol.h). Sin GL ni ImGui: se prueba sola.
 *
 * Un hilo lee lo que manda el motor (escenas, frames, respuestas) y el hilo del editor nunca se
 * bloquea salvo en `call()`, que espera una respuesta con un tiempo límite. Si el motor se cae, el
 * cliente pasa a desconectado y TODO sigue respondiendo (vacío o false): el editor no se cuelga
 * porque el motor haya muerto.
 *
 * Es también el ISceneModel del motor: el núcleo dibuja su jerarquía e inspector sobre él.
 */
class RemoteClient final : public ISceneModel {
public:
    struct Frame {
        int w = 0, h = 0;
        std::vector<uint8_t> rgba;
        uint64_t seq = 0;   ///< sube con cada frame recibido
    };

    RemoteClient() = default;
    ~RemoteClient() override { disconnect(); }
    RemoteClient(const RemoteClient&) = delete;
    RemoteClient& operator=(const RemoteClient&) = delete;

    /// Arranca el ejecutable del motor con `--socket <ruta temporal>` y se conecta.
    bool launch(const std::string& engineExe, std::string& error, int timeoutMs = 5000);
    /// Se conecta a un motor que ya está escuchando.
    bool connectTo(const std::string& socketPath, std::string& error, int timeoutMs = 5000);
    /// Manda `quit`, cierra y espera al proceso (lo mata si no sale). Idempotente.
    void disconnect();

    bool connected() const { return alive.load(); }
    std::string lastError() const;
    std::string engineName() const;
    /// PID del motor si lo arrancó este cliente (-1 si no). Para las pruebas.
    pid_t enginePid() const { return child; }

    /// Petición con respuesta, esperando hasta `timeoutMs`. nullopt = sin respuesta o desconectado.
    std::optional<nlohmann::json> call(nlohmann::json request, int timeoutMs);

    // ── Viewport ────────────────────────────────────────────────────────────────────────────
    /// Pide un frame de ese tamaño si no hay otro en camino (uno en vuelo: el motor no se inunda
    /// de peticiones si dibuja más lento de lo que el editor pide).
    void requestFrame(int w, int h);
    /// Copia el último frame si es más nuevo que `lastSeq`.
    bool takeFrame(Frame& out, uint64_t lastSeq) const;
    void drag(double dx, double dy);
    void wheel(double delta);
    void click(double x, double y, int w, int h);

    // ── Play mode ───────────────────────────────────────────────────────────────────────────
    void setPlaying(bool on);
    bool playing() const;

    // ── ISceneModel ─────────────────────────────────────────────────────────────────────────
    uint64_t revision() const override;
    std::vector<SceneNode> nodes() const override;
    std::string selected() const override;
    void select(const std::string& nodeId) override;
    void setProperty(const std::string& nodeId, const SceneProperty& value) override;

private:
    bool handshake(std::string& error, int timeoutMs);
    /// Envía sin esperar respuesta (la respuesta, si llega, se descarta salvo error).
    void post(nlohmann::json request);
    int nextRequestId();
    void readerLoop();
    void applyScene(const nlohmann::json& scene);
    void fail(const std::string& why);

    int fd = -1;
    pid_t child = -1;
    std::thread reader;
    std::atomic<bool> alive{false};
    std::mutex writeMutex;   // un mensaje no puede intercalarse con otro en el socket

    mutable std::mutex m;
    std::condition_variable cv;
    int nextId = 1;
    std::set<int> waiting;                  // ids de `call()` en espera
    std::map<int, nlohmann::json> replies;  // respuestas a esos ids
    std::string error;
    std::string engine;

    Frame latest;
    bool frameInFlight = false;

    std::vector<SceneNode> sceneNodes;
    std::string sceneSelected;
    uint64_t sceneRevision = 0;
    bool isPlaying = false;
};
