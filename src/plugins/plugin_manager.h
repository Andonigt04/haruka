#pragma once

#include "plugins/haruka_editor_plugin.h"
#include "panels/panel_registry.h"

#include <functional>
#include <string>
#include <vector>

/**
 * @brief Carga los plugins del editor (`.so` con haruka_editor_plugin.h) y les da el registro.
 *
 * Lo que hace al cargar uno, en orden, y por qué:
 *   1. dlopen con RTLD_NOW: un símbolo que falta (otra versión de ImGui) falla AQUÍ con su nombre,
 *      no a mitad de un frame con un "symbol lookup error" que tumba el editor.
 *   2. Lee `haruka_editor_plugin_api` ANTES de llamar a nada: con otra versión del contrato, `init`
 *      leería el HarukaEditorHost con otro layout.
 *   3. `init(host)`: el plugin registra sus paneles, que quedan a su nombre (PanelEntry::owner).
 *      Si devuelve 0 se quitan los que hubiera registrado y se descarga.
 *
 * Al descargar, primero se quitan sus paneles del registro y luego dlclose: las std::function del
 * registro apuntan a código del plugin.
 *
 * ⚠️ Cargar y descargar desde la propia UI (el panel "Plugins") NO se hace en el acto: se pide y
 * se ejecuta en `processPending()`, fuera de `PanelRegistry::drawAll`, porque descargar quita
 * entradas del registro que se está recorriendo.
 */
class PluginManager {
public:
    struct Plugin {
        std::string name;    ///< nombre del fichero sin extensión: es el `owner` de sus paneles
        std::string path;
        std::string error;   ///< vacío = cargado
        void* handle = nullptr;
        int panels = 0;
        bool loaded() const { return handle != nullptr; }
    };

    explicit PluginManager(PanelRegistry& registry) : registry(registry) {}
    ~PluginManager() { unloadAll(); }
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /// Qué motor hay (va a los plugins como `engineName`) y cómo preguntar por proyecto y escena,
    /// para avisar a un panel que se registra cuando ya hay ambos.
    void setEngineName(std::string name) { engineName = std::move(name); }
    void setContextQueries(std::function<std::string()> projectRoot,
                           std::function<void*()> scene) {
        queryProject = std::move(projectRoot);
        queryScene = std::move(scene);
    }

    /// Directorios por defecto, en orden (el primero que trae un nombre de plugin gana):
    ///   $HARUKA_EDITOR_PLUGINS (separados por ':'), <dir del exe>/plugins,
    ///   $XDG_DATA_HOME/haruka-editor/plugins (~/.local/share/...), y el del motor si lo declara.
    static std::vector<std::string> defaultSearchDirs(const std::string& enginePluginsDir);

    /// Carga los `.so` de esos directorios que aún no estén cargados. Los que fallaron antes se
    /// reintentan. Necesita el contexto ImGui ya creado. Devuelve cuántos se cargaron ahora.
    int scan(const std::vector<std::string>& dirs);
    bool unload(const std::string& name);
    void unloadAll();

    void requestRescan() { pendingRescan = true; }
    void requestUnload(const std::string& name) { pendingUnload.push_back(name); }
    /// Ejecuta lo pedido desde la UI. Llamar fuera de `PanelRegistry::drawAll`.
    void processPending();

    const std::vector<Plugin>& plugins() const { return list; }
    const std::vector<std::string>& lastSearchDirs() const { return searchDirs; }

    /// El ImGui del editor tal como se le describe a un plugin o a un backend (versión, tamaños,
    /// contexto y asignador). Necesita el contexto ya creado.
    static HarukaImGuiAbi hostImGuiAbi();

    /// La ventana "Plugins": qué hay cargado, qué falló y por qué, y cargar/descargar.
    void drawPanel();

private:
    bool load(const std::string& path, Plugin& out);
    static int hostRegisterPanel(void* self, const HarukaPanelDesc* desc);
    static void hostLog(void* self, int level, const char* message);

    PanelRegistry& registry;
    std::vector<Plugin> list;
    std::vector<std::string> searchDirs;
    std::string engineName;
    std::function<std::string()> queryProject;
    std::function<void*()> queryScene;

    /// Plugin cuyo `init` está corriendo: dueño de los paneles que registre. Fuera de `init` no
    /// se acepta registrar (no se sabría a quién quitárselos al descargar).
    Plugin* loading = nullptr;
    HarukaEditorHost host{};

    bool pendingRescan = false;
    std::vector<std::string> pendingUnload;
};
