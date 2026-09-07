#pragma once

#include "core/application.h"
#include "core/project.h"
#include "core/scene/scene_manager.h"
#include "core/camera.h"
#include "core/game_interface.h"
#include "panels/scene_hierarchy.h"
#include "panels/inspector.h"
#include "panels/project_browser.h"
#include "panels/material_editor.h"
#include "panels/node_graph_editor.h"
#include "panels/viewport.h"
#include "panels/console.h"
#include "panels/stats.h"
#include "commands/command_history.h"
#include "panels/settings.h"
#include "panels/asset_importer.h"
#include "panels/search_panel.h"
#include "panels/ui_builder.h"
#include "panels/export_panel.h"
#include "menu_bar.h"
#include <imgui.h>
#include <memory>
#include <SDL3/SDL.h>

class MenuBar;

/**
 * @brief Main editor application shell.
 *
 * Owns project/scene state, viewport camera, dockable panels, play-mode state,
 * and project/file workflow actions.
 */
class EditorApplication {
public:
    /** @brief Constructs the editor app with default UI state. */
    EditorApplication();
    /** @brief Releases editor resources and UI-owned state. */
    ~EditorApplication();
    
    /** @brief Runs the editor main loop. */
    void run();
    /** @brief Returns the currently loaded project, if any. */
    Haruka::Project* getProject() { return currentProject.get(); }
    
    friend class MenuBar;

    // ⚠️ CREAR UN OBJETO NO ERA COSA DE UN PANEL, y estaba dentro de uno. Al quitar `ObjectsPanel`
    // se habría ido con él la única forma de crear un Prop, un Monster, un Spawn Point o un
    // Character — que se piden desde el MENÚ, no desde el panel. Viven aquí, que es quien tiene la
    // escena y el historial de deshacer.
    void createProp();
    void createMonster();
    void createSpawnPoint();
    void createCharacter();
    void createMesh();
    void createLight();

private:
    /// Nombre libre con un prefijo ("Prop_3"): no repite ninguno de la escena.
    std::string nextObjectName(const std::string& prefix);
    /// Primitiva con su malla en CPU y su material. `meshType`: "cube" | "sphere" | "capsule".
    std::shared_ptr<Haruka::SceneObject> makeMeshObject(const std::string& name,
                                                        const std::string& meshType,
                                                        const glm::vec3& albedo);
    /// La añade a la escena (por el historial si lo hay) y la deja seleccionada.
    void addSceneObject(std::shared_ptr<Haruka::SceneObject> obj);

    /** @brief Initializes subsystems, panels, and runtime state. */
    void init();
    /** @brief Shuts down the editor and releases owned resources. */
    void shutdown();
    /** @brief Updates editor-side logic for the current frame. */
    void update();
    /** @brief Renders the active viewport/frame. */
    void render();
    /** @brief Renders all dockable UI panels and menus. */
    void renderUI();
    
    std::unique_ptr<MenuBar> menuBar;

    // UI state
    SDL_Window* window;
    std::unique_ptr<Haruka::Project> currentProject;
    std::unique_ptr<Haruka::SceneManager> currentScene;
    std::unique_ptr<Haruka::Core::Camera> viewportCamera;
    
    // Panels
    SceneHierarchyPanel sceneHierarchyPanel;
    InspectorPanel inspectorPanel;
    ProjectBrowserPanel projectBrowserPanel;
    ViewportPanel viewportPanel;
    ConsolePanel consolePanel;
    StatsPanel statsPanel;
    MaterialEditorPanel materialEditorPanel;
    NodeGraphEditorPanel nodeGraphEditorPanel;
    SettingsPanel settingsPanel;
    AssetImporter assetImporter;
    SearchPanel searchPanel;
    UIBuilder uiBuilder;
    ExportPanel exportPanel;
    
    // Gizmos
    int gizmoMode = 0;
    bool gizmoActive = false;
    
    // UI state
    bool showDemoWindow = false;
    bool showNewProjectDialog = false;
    char newProjectNameBuffer[256] = {0};
    char newProjectPathBuffer[512] = {0};
    
    // Panel visibility
    bool showSceneHierarchy = true;
    bool showInspector = true;
    bool showProjectBrowser = true;
    bool showViewport = true;
    bool showConsole = true;
    bool showStats = true;
    bool showMaterialEditor = true;
    bool showNodeGraphEditor = false;
    bool showSettings = false;
    bool showAssetImporter = false;
    bool showSearchPanel = false;
    bool showUIBuilder = false;
    
    int width = 1600;
    int height = 900;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    // Play mode
    /** @brief Enters play mode using a temporary runtime scene. */
    void enterPlayMode();
    /** @brief Exits play mode and restores editor state. */
    void exitPlayMode();
    /** @brief Updates runtime simulation while in play mode. */
    void updatePlayMode(float deltaTime);

    bool isPlayMode = false;
    std::unique_ptr<Haruka::SceneManager> playModeScene;
    Haruka::SceneManager* editorScene = nullptr;
    float playModeTime = 0.0f;

    std::string playModeBackupPath = "/tmp/haruka_playmode_backup.scene";
    
    // Dynamic game interface (single unified callback system)
    void* gameLibHandle = nullptr;
    Haruka::GameInterface* gameInterface = nullptr;

    CommandHistory commandHistory;
    
    Haruka::WorldPos editorCamPos{};
    Haruka::Rotation editorCamRot{};
    std::unique_ptr<StreamCapture> coutCapture;
    std::unique_ptr<StreamCapture> cerrCapture;
    bool showSaveAsPopup = false;
    char saveAsBuffer[512] = {0};
    
    bool sceneDirty = false;
    bool showUnsavedChangesPopup = false;
    std::string pendingSceneToLoad;

    /** @brief Creates a new project at the given base path. */
    void createNewProject(const std::string& name, const std::string& basePath);

    /** @brief Creates a scene object of a given type. */
    void createSceneObject(const std::string& type);

    /** @brief Triggers project compilation/export pipeline. */
    void compileProject();
    bool isProjectCompiling = false;

    /** @brief Exports the game project with current settings. */
    void exportGame();
    /** @brief Opens the export configuration dialog. */
    void showExportDialog();

    // Auto-save system
    struct SceneFile {
        std::string path;
        std::string name;
        bool isPrefab;
        float lastSaveTime = 0.0f;
    };
    
    SceneFile currentFile;
    float autoSaveInterval = 30.0f;
    float timeSinceLastSave = 0.0f;
    bool autoSaveEnabled = true;
    int maxBackups = 5;
    
    /** @brief Saves scene or prefab data to disk. */
    void saveFile(const std::string& path, bool asPrefab = false);
    /** @brief Loads scene or prefab data from disk. */
    void loadFile(const std::string& path);

    /**
     * @brief Apunta TODOS los paneles a `currentScene` y borra las selecciones.
     *
     * Obligatorio en cada sitio que sustituya la escena. Antes cada uno repuntaba su propia lista
     * —init() los seis, loadFile() tres, createNewProject() cinco— y los que se quedaban fuera
     * conservaban punteros a la `SceneManager` DESTRUIDA: cargar una escena y abrir el Node Graph
     * Editor era un SIGSEGV dentro de `getObjectByName` (use-after-free sobre el unordered_map).
     * Los paneles guardan además `SceneObject*` crudos, así que las selecciones se limpian aquí.
     */
    void bindPanelsToScene();
    /** @brief Creates a backup copy for the given file. */
    void createFileBackup(const std::string& path);
    /** @brief Removes old backups beyond retention limit. */
    void cleanOldBackups(const std::string& path);
    /** @brief Deletes all backups associated with one file. */
    void deleteAllBackups(const std::string& path);
    /** @brief Returns the file classification used by the editor. */
    std::string getFileType(const std::string& path);
};