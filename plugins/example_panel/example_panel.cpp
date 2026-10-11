// Plugin de ejemplo del editor: un panel que muestra lo que el editor le cuenta.
//
// Es la plantilla de un plugin: no enlaza el motor NI ImGui. Se compila solo con `imgui.h` y
// `haruka_editor_plugin.h`; las llamadas a ImGui se resuelven al cargarlo contra el ImGui del
// editor (ver la cabecera del contrato). Para hacer otro: copiar esta carpeta, cambiar el id.
#include <imgui.h>
#include "haruka_editor_plugin.h"

#include <string>

namespace {

struct State {
    const HarukaEditorHost* host = nullptr;
    std::string projectRoot;
    void* scene = nullptr;
    int sceneChanges = 0;
    int frames = 0;
};

State g;

void draw(void* user) {
    auto* s = static_cast<State*>(user);
    ++s->frames;
    if (!ImGui::Begin("Example Plugin")) { ImGui::End(); return; }
    ImGui::Text("Motor: %s", s->host->engineName && *s->host->engineName ? s->host->engineName : "(ninguno)");
    ImGui::Text("Proyecto: %s", s->projectRoot.empty() ? "(ninguno)" : s->projectRoot.c_str());
    // La escena es OPACA para un plugin genérico: solo se sabe si hay y cuántas veces cambió. Un
    // plugin de un motor concreto miraría `engineName` antes de convertir el puntero.
    ImGui::Text("Escena: %s (cambios: %d)", s->scene ? "si" : "no", s->sceneChanges);
    ImGui::Text("Frames dibujados: %d", s->frames);
    ImGui::End();
}

void onProject(void* user, const char* root) { static_cast<State*>(user)->projectRoot = root ? root : ""; }
void onScene(void* user, void* scene) {
    auto* s = static_cast<State*>(user);
    s->scene = scene;
    ++s->sceneChanges;
}

} // namespace

HARUKA_EDITOR_PLUGIN_DECLARE_API();

HARUKA_EDITOR_PLUGIN_EXPORT int haruka_editor_plugin_init(const HarukaEditorHost* host) {
    if (!harukaPluginBindImGui(host)) {
        host->log(host->self, 2, "ImGui distinto del del editor: no se carga");
        return 0;
    }
    g = State{};
    g.host = host;

    HarukaPanelDesc d{};
    d.id = "example";
    d.label = "Example Plugin";
    d.menuGroup = 2;
    d.openByDefault = 0;
    d.lockedInPlay = 0;
    d.user = &g;
    d.draw = &draw;
    d.onProjectChanged = &onProject;
    d.onSceneChanged = &onScene;
    return host->registerPanel(host->self, &d);
}

HARUKA_EDITOR_PLUGIN_EXPORT void haruka_editor_plugin_shutdown(void) {
    g = State{};
}
