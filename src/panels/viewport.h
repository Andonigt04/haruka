#pragma once

#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

// Forward declarations para optimizar tiempos de compilación
class Camera;
namespace Haruka { class SceneManager; class EventManager; }
class CommandHistory;
class StatsPanel;
class RenderTarget;
class Shader;
class Model;
struct SDL_Window;

/**
 * @brief Panel del Viewport encargado del renderizado de la escena 3D 
 * e interacción mediante Gizmos (ImGuizmo).
 */
class ViewportPanel {
public:
    ViewportPanel();
    ~ViewportPanel();

    /** @name Ciclo de Vida */
    ///@{
    void onImGuiRender();
    void update(float deltaTime);
    ///@}

    /** @name Configuración de Sistemas (Inyección de dependencias) */
    ///@{
    void setScene(Haruka::SceneManager* scene)       { m_currentScene = scene; }
    void setCamera(Camera* cam)                      { m_camera = cam; }
    void setCommandHistory(CommandHistory* ch)       { m_commandHistory = ch; }
    void setStatsPanel(StatsPanel* sp)               { m_statsPanel = sp; }
    void setEventManager(Haruka::EventManager* em)   { m_eventManager = em; }
    void setSDLWindow(SDL_Window* window)            { m_sdlWindow = window; }
    
    /** @brief Define qué objeto de la escena estamos manipulando */
    void setSelectedObject(int index)                { m_selectedObjectIndex = index; }
    ///@}

    /** @name Getters de Estado */
    ///@{
    bool isFocused() const { return m_isFocused; }
    bool isHovered() const { return m_isHovered; }
    const ImVec2& getSize() const { return m_viewportSize; }
    ///@}

    /** @brief Acceso al sistema de caché de modelos */
    Model* getOrLoadModel(const std::string& path);

private:
    /** @brief Dibuja los Gizmos de ImGuizmo sobre la textura renderizada */
    void handleGizmos();

    /** @brief Gestiona el redimensionamiento del RenderTarget para evitar estiramientos */
    void manageResize();

    /** @brief Lógica interna de renderizado OpenGL */
    void renderScene();

private:
    // --- Punteros a Sistemas Externos ---
    Haruka::SceneManager* m_currentScene      = nullptr;
    Camera* m_camera            = nullptr;
    CommandHistory* m_commandHistory    = nullptr;
    StatsPanel* m_statsPanel        = nullptr;
    Haruka::EventManager* m_eventManager      = nullptr;
    SDL_Window* m_sdlWindow         = nullptr;

    // --- Recursos de GPU (OpenGL) ---
    std::unique_ptr<RenderTarget> m_renderTarget;
    std::unique_ptr<Shader>       m_sceneShader;

    // --- Estado del Panel ---
    ImVec2 m_viewportSize{ 0.f, 0.f };
    bool   m_isFocused = false;
    bool   m_isHovered = false;

    // --- Configuración de Gizmos (ImGuizmo) ---
    ImGuizmo::OPERATION m_gizmoOperation      = ImGuizmo::TRANSLATE;
    ImGuizmo::MODE      m_gizmoMode           = ImGuizmo::LOCAL;
    int                 m_selectedObjectIndex = -1;

    // --- Controles de Cámara (Caché local de ángulos) ---
    float m_camYaw            = 0.0f;
    float m_camPitch          = 0.0f;
    float m_moveSpeed         = 5.0f;
    float m_mouseSensitivity  = 0.1f;
};