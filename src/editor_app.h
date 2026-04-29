#pragma once

#include <imgui.h>
#include <SDL3/SDL.h>
#include <memory>

#include "engine/IEngine.h"
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

class MenuBar;

/**
 * @brief Main editor application shell.
 *
 * El IDE no sabe nada de Vulkan, OpenGL ni ninguna API gráfica.
 * Toda la parte gráfica va a través de IEngine*.
 * Para cambiar de motor: cambiar qué IEngine* se crea en main.cpp.
 */
class EditorApplication {
public:
    explicit EditorApplication(IEngine* engine);
    ~EditorApplication();

    void run();
    Haruka::Project* getProject() { return currentProject.get(); }

    friend class MenuBar;

private:
    IEngine*    engine = nullptr;
    SDL_Window* window = nullptr;

    void init();
    void shutdown();
    void update();
    void render();
    void renderUI();

    std::unique_ptr<MenuBar>         menuBar;
    std::unique_ptr<Haruka::Project> currentProject;
    std::unique_ptr<Haruka::Scene>   currentScene;
    std::unique_ptr<Camera>          viewportCamera;

    SceneHierarchyPanel      sceneHierarchyPanel;
    InspectorPanel           inspectorPanel;
    ProjectBrowserPanel      projectBrowserPanel;
    ViewportPanel            viewportPanel;
    ConsolePanel             consolePanel;
    StatsPanel               statsPanel;
    MaterialEditorPanel      materialEditorPanel;
    SettingsPanel            settingsPanel;
    AssetImporter            assetImporter;
    SearchPanel              searchPanel;
    UIBuilder                uiBuilder;
    ExportPanel              exportPanel;
    PlanetTerrainEditorPanel planetTerrainEditorPanel;

    bool imguiInitialized = false;
    int  gizmoMode   = 0;
    bool gizmoActive = false;

    bool showDemoWindow       = false;
    bool showNewProjectDialog = false;
    char newProjectNameBuffer[256] = {0};
    char newProjectPathBuffer[512] = {0};

    bool showSceneHierarchy      = true;
    bool showInspector           = true;
    bool showProjectBrowser      = true;
    bool showViewport            = true;
    bool showConsole             = true;
    bool showStats               = true;
    bool showMaterialEditor      = true;
    bool showSettings            = false;
    bool showAssetImporter       = false;
    bool showSearchPanel         = false;
    bool showUIBuilder           = false;
    bool showPlanetTerrainEditor = true;

    int   width     = 1600;
    int   height    = 900;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    void enterPlayMode();
    void exitPlayMode();
    void updatePlayMode(float dt);

    bool isPlayMode    = false;
    std::unique_ptr<Haruka::Scene> playModeScene;
    Haruka::Scene* editorScene  = nullptr;
    float playModeTime          = 0.0f;
    std::string playModeBackupPath = "/tmp/haruka_playmode_backup.scene";

    void*                  gameLibHandle = nullptr;
    Haruka::GameInterface* gameInterface = nullptr;

    CommandHistory commandHistory;

    Haruka::WorldPos editorCamPos{};
    Haruka::Rotation editorCamRot{};
    std::unique_ptr<StreamCapture>      coutCapture;
    std::unique_ptr<StreamCapture>      cerrCapture;
    std::unique_ptr<Haruka::InGameChat> inGameChat;

    bool showSaveAsPopup = false;
    char saveAsBuffer[512] = {0};

    bool        sceneDirty             = false;
    bool        showUnsavedChangesPopup = false;
    std::string pendingSceneToLoad;

    void createNewProject(const std::string& name, const std::string& basePath);

    std::unique_ptr<Haruka::PlanetarySystem> planetarySystem;
    bool runningPlanetarySystem = false;

    void createSceneObject(const std::string& type);
    void compileProject();
    bool isProjectCompiling = false;
    void exportGame();
    void showExportDialog();

    struct SceneFile {
        std::string path;
        std::string name;
        bool        isPrefab     = false;
        float       lastSaveTime = 0.0f;
    };

    SceneFile currentFile;
    float autoSaveInterval  = 30.0f;
    float timeSinceLastSave = 0.0f;
    bool  autoSaveEnabled   = true;
    int   maxBackups        = 5;

    void saveFile(const std::string& path, bool asPrefab = false);
    void loadFile(const std::string& path);
    void createFileBackup(const std::string& path);
    void cleanOldBackups(const std::string& path);
    void deleteAllBackups(const std::string& path);
    std::string getFileType(const std::string& path);
};