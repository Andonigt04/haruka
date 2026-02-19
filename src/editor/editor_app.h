#pragma once

#include "core/application.h"
#include "core/project.h"
#include "core/scene.h"
#include "core/camera.h"
#include "panels/scene_hierarchy.h"
#include "panels/inspector.h"
#include "panels/project_browser.h"
#include "panels/viewport.h"
#include "editor/panels/console.h"
#include "editor/panels/stats.h"
#include "editor/commands/command_history.h"
#include "editor/panels/scene_manager.h"
#include "game/ingame_chat.h"
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
    
    // UI
    void showMenuBar();
    
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
    
    // Gizmos (se pueden mover después)
    int gizmoMode = 0;
    bool gizmoActive = false;
    
    // UI State
    bool showDemoWindow = false;
    
    int width = 1600;
    int height = 900;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    void saveScene(const std::string& path);
    void loadScene(const std::string& path);
    CommandHistory commandHistory;
    
    bool isPlayMode = false;
    Haruka::WorldPos editorCamPos{};
    Haruka::Rotation editorCamRot{};
    std::unique_ptr<StreamCapture> coutCapture;
    std::unique_ptr<StreamCapture> cerrCapture;
    std::unique_ptr<Haruka::InGameChat> inGameChat;
};