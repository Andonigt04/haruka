// GLM experimental extensions must be enabled before including any GTX headers.
#define GLM_ENABLE_EXPERIMENTAL

#include "viewport.h"

// ImGui & ImGuizmo
#include <imgui.h>
#include <ImGuizmo.h>

// GLM Extensions para matrices y transformaciones
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

// Motor / Core
#include "core/camera.h"
#include "core/scene/scene_manager.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/model.h"

#include <iostream>

// --- Caché de Modelos (Salvado de tu implementación original) ---
namespace {
    std::unordered_map<std::string, std::shared_ptr<Model>> g_modelCache;

    Model* getOrLoadModelCached(const std::string& path) {
        if (g_modelCache.find(path) == g_modelCache.end()) {
            auto model = std::make_shared<Model>(path);
            g_modelCache[path] = model;
        }
        return g_modelCache[path].get();
    }
}

// --- Helper: De SceneObject (double) a Matriz (float) ---
// Usamos Euler XYZ en grados como define tu struct SceneObject
glm::mat4 GetTransformMatrix(const Haruka::SceneObject& obj) {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(obj.position));
    
    glm::mat4 rotation = glm::eulerAngleXYZ(
        glm::radians((float)obj.rotation.x),
        glm::radians((float)obj.rotation.y),
        glm::radians((float)obj.rotation.z)
    );
    
    transform *= rotation;
    transform = glm::scale(transform, glm::vec3(obj.scale));
    return transform;
}

ViewportPanel::ViewportPanel() {
    // NOTE: defer creation of GL resources (RenderTarget / Shader)
    // until a valid GL context exists. Creating them in the constructor
    // runs before EditorApplication::init() (and before glad), which
    // causes NULL GL function pointers and crashes.
}

ViewportPanel::~ViewportPanel() {
    // La limpieza de std::unique_ptr es automática
}

void ViewportPanel::onImGuiRender() {
    // Estilo sin bordes para que la imagen ocupe todo el panel
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    
    ImGui::Begin("Viewport");

    m_isFocused = ImGui::IsWindowFocused();
    m_isHovered = ImGui::IsWindowHovered();

    // 1. Sincronizar tamaño del RenderTarget con el panel de ImGui
    manageResize();

    // 2. Renderizar la escena 3D en el Framebuffer
    renderScene();

    // 3. Mostrar la textura resultante
    // Invertimos las V (0,1 a 1,0) porque OpenGL y ImGui tienen el origen Y opuesto
    uint32_t textureID = m_renderTarget->getColorTexture();
    ImGui::Image((void*)(intptr_t)textureID, 
                 ImVec2{ m_viewportSize.x, m_viewportSize.y }, 
                 ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

    // 4. Dibujar Gizmos encima de la imagen
    handleGizmos();

    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportPanel::manageResize() {
    ImVec2 contentSize = ImGui::GetContentRegionAvail();
    
    if (contentSize.x != m_viewportSize.x || contentSize.y != m_viewportSize.y) {
        if (contentSize.x > 0 && contentSize.y > 0) {
            m_viewportSize = contentSize;
            m_renderTarget = std::make_unique<RenderTarget>(
                (uint32_t)m_viewportSize.x,
                (uint32_t)m_viewportSize.y);
            
            // Si tu cámara tiene un aspecto fijo, actualízalo aquí
            // m_camera->setAspectRatio(m_viewportSize.x / m_viewportSize.y);
        }
    }
}

void ViewportPanel::renderScene() {
    if (!m_currentScene || !m_camera) return;

    // Ensure GL resources exist (lazy init after GL context available)
    if (!m_sceneShader) {
        m_sceneShader = std::make_unique<Shader>("shaders/simple.vert", "shaders/simple.frag");
    }

    if (!m_renderTarget) {
        // fallback to stored viewport size or a sensible default
        uint32_t w = (m_viewportSize.x > 0) ? (uint32_t)m_viewportSize.x : 1280u;
        uint32_t h = (m_viewportSize.y > 0) ? (uint32_t)m_viewportSize.y : 720u;
        m_renderTarget = std::make_unique<RenderTarget>(w, h);
    }

    m_renderTarget->bindForWriting();
    
    // Limpieza de buffer
    glViewport(0, 0, (GLsizei)m_viewportSize.x, (GLsizei)m_viewportSize.y);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_sceneShader->use();
    
    // Matrices Globales
    float aspect = m_viewportSize.x / m_viewportSize.y;
    m_sceneShader->setMat4(1, m_camera->getViewMatrix());
    m_sceneShader->setMat4(2, m_camera->getProjectionMatrix(aspect));

    // Dibujado de objetos
    for (auto& obj : m_currentScene->getAllObjects()) {
        glm::mat4 modelMat = GetTransformMatrix(*obj);
        m_sceneShader->setMat4(0, modelMat);
        
        // Lógica de carga/dibujado (Salvada de tu original)
        // if (obj->type == "Model") {
        //    auto* model = getOrLoadModelCached(obj->meshPath);
        //    if(model) model->draw(*m_sceneShader);
        // }
    }

    m_renderTarget->unbind();
}

void ViewportPanel::handleGizmos() {
    // m_selectedObjectIndex debe ser gestionado por tu HierarchyPanel
    if (m_selectedObjectIndex < 0 || !m_currentScene) return;

    auto& objects = m_currentScene->getAllObjects();
    if (m_selectedObjectIndex >= (int)objects.size()) return;

    auto& selectedObj = *objects[m_selectedObjectIndex];

    // Configuración de ImGuizmo
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    
    // El Gizmo debe saber dónde está el panel en la pantalla global
    ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, 
                      m_viewportSize.x, m_viewportSize.y);

    // Matrices de la cámara
    glm::mat4 view = m_camera->getViewMatrix();
    glm::mat4 projection = m_camera->getProjectionMatrix(m_viewportSize.x / m_viewportSize.y);

    // Matriz actual del objeto
    glm::mat4 modelMatrix = GetTransformMatrix(selectedObj);

    // Manipular matriz
    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(projection),
        m_gizmoOperation,
        m_gizmoMode,
        glm::value_ptr(modelMatrix)
    );

    // Si hubo interacción, descomponemos la matriz y guardamos en el SceneObject
    if (ImGuizmo::IsUsing()) {
        glm::vec3 translation, scale, skew;
        glm::vec4 perspective;
        glm::quat rotation;

        glm::decompose(modelMatrix, scale, rotation, translation, skew, perspective);

        selectedObj.position = glm::dvec3(translation);
        selectedObj.scale    = glm::dvec3(scale);

        // Convertimos el Cuaternión de vuelta a Euler en GRADOS para tu motor
        glm::vec3 euler = glm::degrees(glm::eulerAngles(rotation));
        selectedObj.rotation = glm::dvec3(euler);
    }
}

void ViewportPanel::update(float deltaTime) {
    // Cambio de herramientas (W, E, R)
    if (m_isFocused) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoOperation = ImGuizmo::SCALE;
    }

    // Aquí puedes re-introducir tu lógica de movimiento de cámara SDL
    // pero asegurándote de que solo ocurra si m_isHovered es true.
}

Model* ViewportPanel::getOrLoadModel(const std::string& path) {
    return getOrLoadModelCached(path);
}