#pragma once

#include "core/application.h"
#include "core/project.h"
#include "core/scene.h"
#include "core/camera.h"
#include "panels/scene_hierarchy.h"
#include "panels/inspector.h"
#include "panels/project_browser.h"
#include "panels/material_editor.h"
#include "panels/viewport.h"
#include "editor/panels/console.h"
#include "editor/panels/stats.h"
#include "editor/commands/command_history.h"
#include "editor/panels/scene_manager.h"
#include "game/ingame_chat.h"
#include "editor/panels/prefabs_panel.h"
#include "game/planetary_system.h"
#include <imgui.h>
#include <memory>

class EditorApplication {
public:
    EditorApplication();
    ~EditorApplication();
    
    void run();

private:
    void init();
    void shutdown();
    void update();
    void render();
    void renderUI();
    void showMenuBar();

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
    Haruka::SceneManagerPanel sceneManagerPanel;
    MaterialEditorPanel materialEditorPanel;
    
    // Gizmos
    int gizmoMode = 0;
    bool gizmoActive = false;
    
    // UI State
    bool showDemoWindow = false;
    bool showNewProjectDialog = false;
    char newProjectNameBuffer[256] = {0};
    char newProjectPathBuffer[512] = {0};
    
    int width = 1600;
    int height = 900;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    void saveScene(const std::string& path);
    void loadScene(const std::string& path);

    // Play Mode
    void enterPlayMode();
    void exitPlayMode();
    void updatePlayMode(float deltaTime);

    bool isPlayMode = false;
    std::unique_ptr<Haruka::Scene> playModeScene;
    Haruka::Scene* editorScene = nullptr;
    float playModeTime = 0.0f;

    std::string playModeBackupPath = "/tmp/haruka_playmode_backup.scene";

    CommandHistory commandHistory;
    
    Haruka::WorldPos editorCamPos{};
    Haruka::Rotation editorCamRot{};
    std::unique_ptr<StreamCapture> coutCapture;
    std::unique_ptr<StreamCapture> cerrCapture;
    std::unique_ptr<Haruka::InGameChat> inGameChat;
    std::string currentScenePath;
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
};