#pragma once

#include "backend/engine_backend.h"
#include "backend/backend_loader.h"
#include "commands/command_history.h"
#include "panels/panel_registry.h"
#include "panels/project_browser.h"
#include "panels/console.h"
#include "panels/asset_importer.h"
#include "panels/search_panel.h"
#include "panels/ui_builder.h"
#include "panels/generic_scene_panels.h"
#include "plugins/plugin_manager.h"
#include "menu_bar.h"

#include <memory>
#include <string>

struct SDL_Window;

/**
 * @brief El editor GENÉRICO: ventana, ImGui, menús, paneles, plugins y el ciclo de documentos.
 *
 * ⚠️ AQUÍ NO ENTRA NINGUNA CABECERA DEL MOTOR (lo comprueba el build: ver
 * cmake/check_core_engine_free.cmake). Todo lo que sabe de un motor pasa por `IEngineBackend`:
 * la escena, el viewport, los paneles que entienden de objetos, crear objetos, el play mode. Sin
 * backend el editor arranca igual, con sus paneles genéricos (HARUKA_EDITOR_NO_BACKEND=1). El
 * backend es un `.so` que se carga al arrancar (BackendLoader): el núcleo no enlaza ningún motor.
 *
 * Lo que sí es del núcleo: qué documento está abierto, si está sucio, el autoguardado y sus copias
 * de seguridad, el historial de deshacer, y crear/compilar el proyecto con lo que declara el motor
 * en `haruka-engine.json` (EngineSdk).
 */
class EditorApplication {
public:
    /// Sin argumento, el backend se busca y se carga al arrancar (BackendLoader). Pasar uno sirve
    /// para pruebas; si no hay ninguno, el editor arranca sin motor.
    explicit EditorApplication(std::unique_ptr<IEngineBackend> backend = nullptr);
    ~EditorApplication();

    /** @brief Runs the editor main loop. */
    void run();

    /// Las ventanas del editor. Público para que quien añada paneles no tenga que tocar esta clase.
    PanelRegistry& panelRegistry() { return panels; }

    friend class MenuBar;

private:
    void init();
    void shutdown();
    void update();
    void render();
    void renderUI();
    /// Paneles del núcleo, en orden de dibujo; el backend mete los suyos en medio (ver init()).
    void registerCorePanelsBefore();
    void registerCorePanelsAfter();
    /// Jerarquía e inspector genéricos si el backend describe su escena (ver scene_model.h).
    void registerGenericScenePanels();

    /// Da a TODOS los paneles la ruta del proyecto abierto (uno solo: antes eran tres listas a mano).
    void syncPanelsToProject();

    // Proyecto
    void openProject(const std::string& root);
    void createNewProject(const std::string& name, const std::string& basePath);
    void compileProject();
    std::string projectRoot() const { return backend ? backend->projectRoot() : std::string{}; }

    // Documentos: el núcleo lleva la ruta, lo sucio y las copias; leer y escribir es del backend.
    void openDocument(const std::string& path);   // con el aviso de cambios sin guardar
    void saveFile(const std::string& path);
    void loadFile(const std::string& path);
    void createFileBackup(const std::string& path);
    void cleanOldBackups(const std::string& path);
    void deleteAllBackups(const std::string& path);

    bool isPlaying() const { return backend && backend->isPlaying(); }

    BackendLoader backendLoader;
    std::unique_ptr<IEngineBackend> backend;
    SDL_Window* window = nullptr;
    std::unique_ptr<MenuBar> menuBar;

    PanelRegistry panels;
    // DETRÁS de `panels`: se destruye antes y descarga sus paneles de un registro aún vivo.
    PluginManager plugins{panels};
    CommandHistory commandHistory;

    ProjectBrowserPanel projectBrowserPanel;
    ConsolePanel consolePanel;
    AssetImporter assetImporter;
    SearchPanel searchPanel;
    UIBuilder uiBuilder;
    // Para backends que describen su escena de forma genérica (IEngineBackend::genericScene).
    GenericHierarchyPanel genericHierarchy;
    GenericInspectorPanel genericInspector;
    std::unique_ptr<StreamCapture> coutCapture;
    std::unique_ptr<StreamCapture> cerrCapture;

    // Diálogos
    bool showNewProjectDialog = false;
    char newProjectNameBuffer[256] = {0};
    char newProjectPathBuffer[512] = {0};
    bool showSaveAsPopup = false;
    char saveAsBuffer[512] = {0};
    bool showUnsavedChangesPopup = false;
    std::string pendingSceneToLoad;

    // Documento abierto
    struct DocumentFile {
        std::string path;
        std::string name;
        float lastSaveTime = 0.0f;
    };
    DocumentFile currentFile;
    bool sceneDirty = false;
    float autoSaveInterval = 30.0f;
    float timeSinceLastSave = 0.0f;
    bool autoSaveEnabled = true;
    int maxBackups = 5;

    bool isProjectCompiling = false;

    int width = 1600;
    int height = 900;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    double autoQuitAt = 0.0;   // HARUKA_EDITOR_QUIT_AFTER (segundos desde el arranque)
};
