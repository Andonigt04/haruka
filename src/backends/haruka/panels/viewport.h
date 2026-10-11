#pragma once

#include "Haruka/Core/Camera.h"
#include "Haruka/Core/Scene/SceneManager.h"
#include "Haruka/Renderer/Shader.h"
#include "Haruka/Renderer/RenderTarget.h"
#include "Haruka/Renderer/MotorInstance.h"
#include "Haruka/Renderer/SimpleMesh.h"
#include "Haruka/Renderer/PrimitiveShapes.h"
#include "commands/command_history.h"
#include <imgui.h>
#include <functional>
#include <memory>
#include <SDL3/SDL.h>
#include "Haruka/Renderer/Model.h"
#include "panels/stats.h"
#include <map>
#include <unordered_map>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ImGuizmo.h>
#include "Haruka/Game/Ports/Port.h"
#include "Haruka/Game/Prefab/Prefab.h"

/**
 * @brief Editor viewport panel responsible for scene rendering and gizmo interaction.
 */
namespace Haruka::Tools { class WorldEditPanel; }

class ViewportPanel {
public:
    /** @brief Constructs viewport resources with default state. */
    ViewportPanel();
    /** @brief Releases viewport resources. */
    ~ViewportPanel();

    /** @brief Sets the scene rendered in the viewport. */
    void setScene(Haruka::Core::Scene::SceneManager* scene);
    /** @brief Sets the active camera used by the viewport. */
    void setCamera(Haruka::Core::Camera* cam);
    /** @brief Injects the SDL window used for input handling. */
    void setSDLWindow(SDL_Window* window) { sdlWindow = window; }
    /** @brief Sets the command history for viewport-driven edits. */
    void setCommandHistory(CommandHistory* history) { commandHistory = history; }
    /** @brief Draws the viewport UI. */
    void onImGuiRender();
    /** @brief Updates viewport-side logic and camera controls. */
    void onUpdate(float deltaTime);
    /** @brief Renders the scene into the viewport framebuffer. */
    void renderScene();
    /** @brief Copies motor render stats into the stats panel. */
    void setStatsPanelFromApp(class Haruka::Core::Application* app);
    /** @brief Registers the viewport target with the motor application. */
    void registerEditorTargetWithApp();
    /** @brief Releases motor/GL resources before the GL context is destroyed. */
    void shutdownGLResources();
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
    Haruka::Renderer::Model* getOrLoadModel(const std::string& path);

    void setStatsPanel(StatsPanel* panel) { statsPanel = panel; }
    void setPlayMode(bool play) { playMode = play; }
    void setGizmoMode(int mode) { gizmoMode = mode; }

    /**
     * @brief PUERTO en edición: el gizmo manipula su transform LOCAL, no el del objeto.
     *
     * Se pasa el puntero que posee el PortsPanel (paso 2 de PLAN_PUERTOS.md). Con un puerto activo,
     * ImGuizmo mueve/gira el PUERTO dentro del espacio del objeto seleccionado, y se dibujan encima
     * su eje y —si es un raíl— el BARRIDO entre sus límites: la puerta se ve abrirse en el editor,
     * que es el único sitio donde comprobar que el eje y el cero están bien puestos ANTES de jugar.
     * `nullptr` = volver a manipular el objeto.
     */
    void setPortEdit(Haruka::Game::Ports::Port* port) { editPort = port; }

    /** @brief PIEZA de prefabricado en edición: el gizmo mueve la pieza dentro del conjunto.
     *  Se pasa también el ORIGEN del prefabricado en el mundo, porque sus poses son LOCALES y sin
     *  ese ancla el gizmo estaría manipulando coordenadas que no significan nada en pantalla. */
    void setPrefabEdit(Haruka::Game::Prefab::PrefabPiece* piece, const glm::dvec3& origin) {
        editPiece = piece; prefabOrigin = origin;
    }
    void setSelectedObjectIndex(int index) { selectedObjectIndex = index; }

    /**
     * @brief Aviso de que se ha PICADO un objeto de la escena en el viewport.
     *
     * ⚠️ El clic del viewport no pasa por los callbacks de la jerarquía: elige el objeto él mismo
     * (`getHoveredObjectIndex`) y nadie más se entera. Sin este aviso, el panel de prefabricados
     * seguía cediéndole su pieza al gizmo cada frame y el objeto recién picado se quedaba SIN gizmo
     * — que es como se ve desde fuera: "el gizmo ha desaparecido".
     */
    void setOnObjectPicked(std::function<void()> fn) { onObjectPicked = std::move(fn); }
    int getSelectedObjectIndex() const { return selectedObjectIndex; }

    /**
     * @brief Lleva la cámara al objeto y lo deja CENTRADO y encuadrado, mida lo que mida.
     *
     * Mantiene la dirección de vista actual y solo mueve la posición: el objeto acaba en el centro
     * del viewport ocupando la MISMA fracción de pantalla tanto si es un prop de 40 cm como si es un
     * planeta de 6371 km. El tamaño se lo pregunta al motor
     * (`Application::getObjectBoundingRadius`), que es quien sabe qué hay detrás de un `SceneObject`:
     * el IDE no distingue un planeta de una caja.
     */
    void focusOnObject(int objectIndex);

    // --- Colocación de props y herramientas de malla ----------------------------------------
    /** @brief Activa el modo de colocación: el cursor proyecta sobre el planeta un indicador
     *  translúcido (círculo/cuadrado) del radio de afectación y un click coloca el prop o edita el
     *  terreno según `action`. `layer` = capa del objeto colocado ("Trees", "Props"…). */
    void beginPlacement(const std::string& modelPath, const std::string& label,
                        float radius, bool circle, int action, const std::string& layer);
    /** @brief Sale del modo de colocación (también con Esc o clic derecho en el viewport). */
    void cancelPlacement() { placementEnabled = false; }

    // --- El pincel del mundo (panel del motor) ----------------------------------------------
    /** @brief El panel cuyo gesto atiende el viewport: rayo del ratón contra el CAMPO del planeta
     *  (`Application::worldRaycast`), clic/arrastre → trazo o cueva/isla nueva, y el indicador. */
    void setWorldTool(Haruka::Tools::WorldEditPanel* tool) { worldTool = tool; }
    bool isWorldToolActive() const;
    bool isPlacementActive() const { return placementEnabled; }
    /** @brief Radio del indicador (lo cambian los paneles mientras el modo está activo). */
    void setPlacementRadius(float r) { placementRadius = std::max(0.1f, r); }
    float getPlacementRadius() const { return placementRadius; }

private:
    // Estado de colocación
    bool placementEnabled = false;
    bool placementCircle = true;          // círculo (true) o cuadrado
    int placementAction = 0;              // 0=colocar prop, 1=levantar, 2=excavar, 3=allanar
    float placementRadius = 50.0f;
    std::string placementModel;           // ruta del modelo .glb/.obj (vacía = herramienta de malla)
    std::string placementLabel;
    std::string placementLayer;           // capa del objeto colocado ("Trees", "Props"…)
    glm::dvec3 placementGround{0.0};      // punto anclado al terreno (coords de mundo)
    bool placementHasGround = false;
    void handlePlacement();
    void drawPlacementIndicator();
    void executePlacement();
    Haruka::Tools::WorldEditPanel* worldTool = nullptr;
    bool worldToolPressed = false;
    void handleWorldTool();
    void drawWorldToolIndicator();
    /// Proyecta un punto de MUNDO a píxeles del viewport (doble precisión). false si está detrás.
    bool projectToScreen(const glm::dvec3& world, ImVec2& out) const;

private:
    // Motor app instance owned by the viewport when running in editor mode
    std::unique_ptr<Haruka::Core::Application> ownedApplication;

    Haruka::Core::Scene::SceneManager* currentScene = nullptr;
    Haruka::Core::Camera* camera = nullptr;

    int width = 1280, height = 720;
    ImVec2 viewportMin, viewportMax;
    bool isViewportHovered = false;
    bool isViewportFocused = false;
    bool playMode = false;
    bool showGrid = false;

    int selectedObjectIndex = -1;
    int currentGizmoOperation = ImGuizmo::TRANSLATE;

    /** @brief Vista de depuración del planeta (0=normal, 1=elev, 2=zonas, 3=bioma, 4=temp, 5=hum).
     *  Se empuja al motor con `Application::setPlanetDebugView`. */
    int debugView = 0;

    // Camera controls
    float camYaw = 0.0f, camPitch = 0.0f;
    float moveSpeed = 5.0f, mouseSensitivity = 0.1f;

    // Gizmo
    enum class GizmoAxis { None, X, Y, Z };
    GizmoAxis activeAxis = GizmoAxis::None;
    bool isDragging = false;
    glm::vec3 dragStartPos, dragStartRot, dragStartScale;
    int gizmoMode = 0; // 0=move, 1=rotate, 2=scale
    Haruka::Game::Ports::Port* editPort = nullptr;          // puerto en edición (lo posee PortsPanel)
    Haruka::Game::Prefab::PrefabPiece* editPiece = nullptr;  // pieza de prefabricado (la posee PrefabPanel)
    std::function<void()> onObjectPicked;      // ver setOnObjectPicked
    glm::dvec3 prefabOrigin{0.0};

    /// Dibuja eje + barrido del raíl con la draw list de ImGui (proyectando a pantalla): no hace
    /// falta un renderer de líneas para ver si el mecanismo está bien planteado.
    void drawPortOverlay(const glm::dmat4& objXform, const glm::mat4& view, const glm::mat4& proj);
    float axisPickRadius = 0.15f;

    // OpenGL/ImGui resources
    std::unique_ptr<Haruka::Renderer::RenderTarget> renderTarget;

    // Shader para render local/editor
    std::unique_ptr<Haruka::Renderer::Shader> sceneShader;

    // Stats panel
    StatsPanel* statsPanel = nullptr;
    int renderVertex_count = 0, renderDraw_calls = 0;

    // Command history
    CommandHistory* commandHistory = nullptr;

    // SDL window
    SDL_Window* sdlWindow = nullptr;

};