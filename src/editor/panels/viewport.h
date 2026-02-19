#pragma once

#include "core/camera.h"
#include "core/scene.h"
#include "renderer/shader.h"
#include "renderer/render_target.h"
#include "renderer/simple_mesh.h"
#include "renderer/primitive_shapes.h"
#include "editor/commands/command_history.h"
#include <imgui.h>
#include <memory>
#include <GLFW/glfw3.h>
#include "renderer/model.h"
#include "editor/panels/stats.h"
#include <map>

class ViewportPanel {
public:
    ViewportPanel();
    ~ViewportPanel();

    void setScene(Haruka::Scene* scene);
    void setCamera(Camera* cam);
    void setGLFWWindow(GLFWwindow* window) { glfwWindow = window; }
    void setCommandHistory(CommandHistory* history) { commandHistory = history; }
    void onImGuiRender();
    void onUpdate(float deltaTime);
    void setPlayMode(bool enabled) { playMode = enabled; }
    void setGizmoMode(int mode) { gizmoMode = mode; } // 0=Move,1=Rotate,2=Scale
    void setStatsPanel(StatsPanel* stats) { statsPanel = stats; }

private:
    bool playMode = false;

    enum class GizmoAxis { None, X, Y, Z };
    GizmoAxis activeAxis = GizmoAxis::None;

    float axisPickRadius = 0.15f;

    void recreateRenderTarget();
    void renderScene();
    void updateCameraFromInput(float deltaTime);
    void handleGizmoInput();
    glm::vec3 getRayFromMouse(const glm::mat4& proj, const glm::mat4& view);
    int getHoveredObjectIndex(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::mat4& proj, const glm::mat4& view);
    void renderGizmoAxes(const glm::mat4& view, const glm::mat4& proj);
    void renderGrid(const glm::mat4& view, const glm::mat4& proj);

    unsigned int gizmoVAO = 0;
    unsigned int gizmoVBO = 0;
    unsigned int gridVAO = 0;
    unsigned int gridVBO = 0;
    bool showGrid = true;

    Haruka::Scene* currentScene = nullptr;
    Camera* camera = nullptr;
    GLFWwindow* glfwWindow = nullptr;
    CommandHistory* commandHistory = nullptr;

    int width = 1280;
    int height = 720;

    float camYaw = 0.0f;
    float camPitch = -20.0f;
    float moveSpeed = 5.0f;
    float mouseSensitivity = 0.1f;
    bool isViewportHovered = false;
    bool isViewportFocused = false;

    int selectedObjectIndex = -1;
    bool isDragging = false;
    glm::vec3 dragStartPos = glm::vec3(0);
    glm::vec3 dragStartRot = glm::vec3(0);
    glm::vec3 dragStartScale = glm::vec3(1);

    std::unique_ptr<RenderTarget> renderTarget;
    std::unique_ptr<Shader> sceneShader;
    std::unique_ptr<SimpleMesh> cubeMesh;

    bool rayIntersectsAxis(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& axisOrigin, const glm::vec3& axisDir, float& tOut);
    int gizmoMode = 0;

    ImVec2 viewportMin{0,0};
    ImVec2 viewportMax{0,0};

    void handleAssetDrop();

    std::map<std::string, std::unique_ptr<Model>> loadedModels;
    Model* getOrLoadModel(const std::string& path);

    StatsPanel* statsPanel = nullptr;
    int renderVertexCount = 0;
    int renderDrawCalls = 0;
};