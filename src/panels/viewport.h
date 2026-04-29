#pragma once

#include "core/camera.h"
#include "core/scene.h"
#include "renderer/shader.h"
#include "renderer/render_target.h"
#include "renderer/motor_instance.h"
#include "IEngine.h"
#include "renderer/simple_mesh.h"
#include "renderer/primitive_shapes.h"
#include "commands/command_history.h"
#include <imgui.h>
#include <memory>
#include "renderer/model.h"
#include "panels/stats.h"
#include <map>
#include <unordered_map>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ImGuizmo.h>

class Application;

/**
 * @brief Editor viewport panel responsible for scene rendering and gizmo interaction.
 */
class ViewportPanel {
public:
    /** @brief Constructs viewport resources with default state. */
    ViewportPanel();
    /** @brief Releases viewport resources. */
    ~ViewportPanel();

    /** @brief Sets the scene rendered in the viewport. */
    void setScene(Haruka::Scene* scene);
    /** @brief Sets the active camera used by the viewport. */
    void setCamera(Camera* cam);
    /** @brief Sets the command history for viewport-driven edits. */
    void setCommandHistory(CommandHistory* history) { commandHistory = history; }
    /** @brief Draws the viewport UI. */
    void onImGuiRender();
    /** @brief Updates viewport-side logic and camera controls. */
    void onUpdate(float deltaTime);
    /** @brief Renders the scene into the viewport framebuffer. */
    void renderScene();
    /** @brief Renders the active ImGuizmo manipulator. */
    void renderGizmoImGuizmo();
    /** @brief Recreates the offscreen render target. */
    void recreateRenderTarget();
    /** @brief Applies keyboard/mouse input to the viewport camera. */
    void updateCameraFromInput(float deltaTime);

    /** @brief Draws axis guides for selection gizmo. */
    void renderGizmoAxes(const glm::mat4& view, const glm::mat4& proj);
    /** @brief Draws editor grid overlay. */
    void renderGrid(const glm::mat4& view, const glm::mat4& proj);
    /** @brief Processes gizmo input state. */
    void handleGizmoInput();
    /** @brief Handles asset drop events in the viewport. */
    void handleAssetDrop();
    /** @brief Builds a world-space ray from the current mouse position. */
    glm::vec3 getRayFromMouse(const glm::mat4& proj, const glm::mat4& view);
    /** @brief Returns the index of the hovered object, if any. */
    int getHoveredObjectIndex(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::mat4& proj, const glm::mat4& view);
    /** @brief Tests ray/axis intersection for gizmo picking. */
    bool rayIntersectsAxis(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& axisOrigin, const glm::vec3& axisDir, float& tOut);
    /** @brief Loads a model from cache or disk. */
    Model* getOrLoadModel(const std::string& path);

    void setStatsPanel(StatsPanel* panel) { statsPanel = panel; }
    void setPlayMode(bool play) { playMode = play; }
    void setGizmoMode(int mode) { gizmoMode = mode; }
    void setSelectedObjectIndex(int index) { selectedObjectIndex = index; }
    int getSelectedObjectIndex() const { return selectedObjectIndex; }

private:
    // Motor app instance owned by the viewport when running in editor mode
    std::unique_ptr<Application> ownedApplication;

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

    // Camera controls
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

    // Shader para render local/editor
    std::unique_ptr<Shader> sceneShader;

    // Stats panel
    StatsPanel* statsPanel = nullptr;
    int renderVertex_count = 0, renderDraw_calls = 0;

    // Command history
    CommandHistory* commandHistory = nullptr;

    // SDL3 window (para integración futura)
    void setSDLWindow(struct SDL_Window* window) { sdlWindow = window; }
    struct SDL_Window* sdlWindow = nullptr; // Ahora es SDL_Window*

};