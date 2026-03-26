#pragma once

#include "core/camera.h"
#include "core/scene.h"
#include "renderer/shader.h"
#include "renderer/render_target.h"
#include "renderer/motor_instance.h"
#include "renderer/simple_mesh.h"
#include "renderer/primitive_shapes.h"
#include "editor/commands/command_history.h"
#include <imgui.h>
#include <memory>
#include <GLFW/glfw3.h>
#include "renderer/model.h"
#include "editor/panels/stats.h"
#include <map>
#include <unordered_map>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ImGuizmo.h>

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
    void renderScene();
    void renderGizmoImGuizmo();
    void recreateRenderTarget();
    void updateCameraFromInput(float deltaTime);

    void renderGizmoAxes(const glm::mat4& view, const glm::mat4& proj);
    void renderGrid(const glm::mat4& view, const glm::mat4& proj);
    void handleGizmoInput();
    void handleAssetDrop();
    glm::vec3 getRayFromMouse(const glm::mat4& proj, const glm::mat4& view);
    int getHoveredObjectIndex(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::mat4& proj, const glm::mat4& view);
    bool rayIntersectsAxis(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& axisOrigin, const glm::vec3& axisDir, float& tOut);
    Model* getOrLoadModel(const std::string& path);

    void setStatsPanel(StatsPanel* panel) { statsPanel = panel; }
    void setPlayMode(bool play) { playMode = play; }
    void setGizmoMode(int mode) { gizmoMode = mode; }

private:
    Haruka::Scene* currentScene = nullptr;
    Camera* camera = nullptr;

    int width = 1280, height = 720;
    ImVec2 viewportMin, viewportMax;
    bool isViewportHovered = false;
    bool isViewportFocused = false;
    bool playMode = false;
    bool showGrid = false;

    int selectedObjectIndex = -1;
    int currentGizmoOperation = ImGuizmo::TRANSLATE;

    // Para cámara
    float camYaw = 0.0f, camPitch = 0.0f;
    float moveSpeed = 5.0f, mouseSensitivity = 0.1f;

    // Gizmo
    enum class GizmoAxis { None, X, Y, Z };
    GizmoAxis activeAxis = GizmoAxis::None;
    bool isDragging = false;
    glm::vec3 dragStartPos, dragStartRot, dragStartScale;
    int gizmoMode = 0; // 0=move, 1=rotate, 2=scale
    float axisPickRadius = 0.15f;

    // OpenGL/ImGui resources
    std::unique_ptr<RenderTarget> renderTarget;
    std::unique_ptr<Shader> sceneShader;
    std::unique_ptr<SimpleMesh> cubeMesh;
    std::unordered_map<std::string, std::unique_ptr<Model>> loadedModels;

    // Grid/gizmo VAO/VBO
    GLuint gridVAO = 0, gridVBO = 0;
    GLuint gizmoVAO = 0, gizmoVBO = 0;

    // Stats panel
    StatsPanel* statsPanel = nullptr;
    int renderVertex_count = 0, renderDraw_calls = 0;

    // Command history
    CommandHistory* commandHistory = nullptr;

    // GLFW window
    GLFWwindow* glfwWindow = nullptr;

};