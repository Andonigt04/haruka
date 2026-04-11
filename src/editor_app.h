#pragma once

#include "core/application.h"
#include "core/project.h"
#include "core/scene.h"
#include "core/camera.h"
#include "core/game_interface.h"
#include "panels/scene_hierarchy.h"
#include "panels/inspector.h"
#include "panels/project_browser.h"
#include "panels/material_editor.h"
#include "panels/viewport.h"
#include "panels/console.h"
#include "panels/stats.h"
#include "commands/command_history.h"
#include "game/ingame_chat.h"
#include "game/planetary_system.h"
#include "panels/settings.h"
#include "panels/asset_importer.h"
#include "panels/search_panel.h"
#include "panels/multi_scene_manager.h"
#include "panels/ui_builder.h"
#include "panels/export_panel.h"
#include "panels/planet_terrain_editor.h"
#include "menu_bar.h"
#include <imgui.h>
#include <memory>

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

private:
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
    GLFWwindow* window;
    std::unique_ptr<Haruka::Project> currentProject;
    std::unique_ptr<Haruka::Scene> currentScene;
    std::unique_ptr<Camera> viewportCamera;
    
    // Panels
    SceneHierarchyPanel sceneHierarchyPanel;
    InspectorPanel inspectorPanel;
    ProjectBrowserPanel projectBrowserPanel;
    ViewportPanel viewportPanel;
    ConsolePanel consolePanel;
    StatsPanel statsPanel;
    MaterialEditorPanel materialEditorPanel;
    SettingsPanel settingsPanel;
    AssetImporter assetImporter;
    SearchPanel searchPanel;
    UIBuilder uiBuilder;
    ExportPanel exportPanel;
    PlanetTerrainEditorPanel planetTerrainEditorPanel;
    
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
    bool showSettings = false;
    bool showAssetImporter = false;
    bool showSearchPanel = false;
    bool showUIBuilder = false;
    bool showPlanetTerrainEditor = true;
    
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
    std::unique_ptr<Haruka::Scene> playModeScene;
    Haruka::Scene* editorScene = nullptr;
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
    std::unique_ptr<Haruka::InGameChat> inGameChat;
    bool showSaveAsPopup = false;
    char saveAsBuffer[512] = {0};
    
    bool sceneDirty = false;
    bool showUnsavedChangesPopup = false;
    std::string pendingSceneToLoad;

    /** @brief Creates a new project at the given base path. */
    void createNewProject(const std::string& name, const std::string& basePath);

    std::unique_ptr<Haruka::PlanetarySystem> planetarySystem;
    bool runningPlanetarySystem = false;

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
    /** @brief Creates a backup copy for the given file. */
    void createFileBackup(const std::string& path);
    /** @brief Removes old backups beyond retention limit. */
    void cleanOldBackups(const std::string& path);
    /** @brief Deletes all backups associated with one file. */
    void deleteAllBackups(const std::string& path);
    /** @brief Returns the file classification used by the editor. */
    std::string getFileType(const std::string& path);
};