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
#include "editor/panels/console.h"
#include "editor/panels/stats.h"
#include "editor/commands/command_history.h"
#include "game/ingame_chat.h"
#include "editor/panels/prefabs_panel.h"
#include "game/planetary_system.h"
#include "editor/panels/settings.h"
#include "editor/panels/asset_importer.h"
#include "editor/panels/search_panel.h"
#include "editor/panels/multi_scene_manager.h"
#include "editor/panels/scripting_editor.h"
#include "editor/panels/ui_builder.h"
#include "editor/panels/export_panel.h"
#include "menu_bar.h"
#include <imgui.h>
#include <memory>

class MenuBar;

class EditorApplication {
public:
    EditorApplication();
    ~EditorApplication();
    
    void run();
    Haruka::Project* getProject() { return currentProject.get(); }
    
    friend class MenuBar;

private:
    void init();
    void shutdown();
    void update();
    void render();
    void renderUI();
    
    std::unique_ptr<MenuBar> menuBar;

    // UI
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
    ScriptingEditor scriptingEditor;
    UIBuilder uiBuilder;
    ExportPanel exportPanel;
    
    // Gizmos
    int gizmoMode = 0;
    bool gizmoActive = false;
    
    // UI State
    bool showDemoWindow = false;
    bool showNewProjectDialog = false;
    char newProjectNameBuffer[256] = {0};
    char newProjectPathBuffer[512] = {0};
    
    // Panel Visibility
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
    bool showScriptingEditor = false;
    bool showUIBuilder = false;
    
    int width = 1600;
    int height = 900;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    // Play Mode
    void enterPlayMode();
    void exitPlayMode();
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

    void createNewProject(const std::string& name, const std::string& basePath);

    std::unique_ptr<Haruka::PlanetarySystem> planetarySystem;
    bool runningPlanetarySystem = false;

    void compileProject();
    bool isProjectCompiling = false;

    void exportGame();
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
    
    void saveFile(const std::string& path, bool asPrefab = false);
    void loadFile(const std::string& path);
    void createFileBackup(const std::string& path);
    void cleanOldBackups(const std::string& path);
    void deleteAllBackups(const std::string& path);
    std::string getFileType(const std::string& path);
};