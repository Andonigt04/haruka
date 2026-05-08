// GLM experimental extensions must be enabled before including any GTX headers.
#define GLM_ENABLE_EXPERIMENTAL

#include "viewport.h"

// ImGui & ImGuizmo
#include <imgui.h>
#include <ImGuizmo.h>

// GLM Extensions para matrices y transformaciones
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

// Motor / Core
#include "core/camera.h"
#include "core/application.h"
#include "core/scene/scene_manager.h"
#include "core/scene/scene_render_policy.h"
#include "core/components/mesh_renderer_component.h"
#include "core/terrain/terrain_generator.h"
#include "renderer/render_target.h"
#include "renderer/shader.h"
#include "renderer/model.h"
#include "renderer/primitive_shapes.h"
#include "panels/stats.h"

#include <iostream>
#include <cstdio>

// --- Caché de Modelos (Salvado de tu implementación original) ---
namespace {
    std::unordered_map<std::string, std::shared_ptr<Model>> g_modelCache;
    std::unique_ptr<SimpleMesh> g_cubeMesh;
    std::unique_ptr<SimpleMesh> g_sphereMesh;
    std::unique_ptr<SimpleMesh> g_capsuleMesh;
    std::unique_ptr<SimpleMesh> g_planeMesh;

    Model* getOrLoadModelCached(const std::string& path) {
        if (g_modelCache.find(path) == g_modelCache.end()) {
            auto model = std::make_shared<Model>(path);
            g_modelCache[path] = model;
        }
        return g_modelCache[path].get();
    }

    SimpleMesh* getPrimitiveMesh(Haruka::PrimitiveType primitive) {
        switch (primitive) {
            case Haruka::PrimitiveType::CUBE:
                if (!g_cubeMesh) {
                    std::vector<glm::vec3> vertices;
                    std::vector<glm::vec3> normals;
                    std::vector<unsigned int> indices;
                    PrimitiveShapes::createCube(1.0f, vertices, normals, indices);
                    g_cubeMesh = std::make_unique<SimpleMesh>(vertices, normals, indices);
                }
                return g_cubeMesh.get();
            case Haruka::PrimitiveType::SPHERE:
                if (!g_sphereMesh) {
                    std::vector<glm::vec3> vertices;
                    std::vector<glm::vec3> normals;
                    std::vector<unsigned int> indices;
                    PrimitiveShapes::createSphereLOD(1.0f, 64, 32, vertices, normals, indices);
                    g_sphereMesh = std::make_unique<SimpleMesh>(vertices, normals, indices);
                }
                return g_sphereMesh.get();
            case Haruka::PrimitiveType::CAPSULE:
                if (!g_capsuleMesh) {
                    std::vector<glm::vec3> vertices;
                    std::vector<glm::vec3> normals;
                    std::vector<unsigned int> indices;
                    PrimitiveShapes::createCapsule(0.5f, 1.5f, 24, 12, vertices, normals, indices);
                    g_capsuleMesh = std::make_unique<SimpleMesh>(vertices, normals, indices);
                }
                return g_capsuleMesh.get();
            case Haruka::PrimitiveType::PLANE:
                if (!g_planeMesh) {
                    std::vector<glm::vec3> vertices;
                    std::vector<glm::vec3> normals;
                    std::vector<unsigned int> indices;
                    PrimitiveShapes::createPlane(1.0f, 1.0f, 1, vertices, normals, indices);
                    g_planeMesh = std::make_unique<SimpleMesh>(vertices, normals, indices);
                }
                return g_planeMesh.get();
            default:
                return nullptr;
        }
    }

    bool isPlanetLike(const Haruka::SceneObject& obj) {
        if (obj.terrainSettings || obj.lodSettings || obj.flags.hasChunks) return true;
        std::string lo = obj.type;
        std::transform(lo.begin(), lo.end(), lo.begin(), ::tolower);
        return lo.find("planet")       != std::string::npos
            || lo.find("celestialbody")!= std::string::npos
            || lo.find("star")         != std::string::npos
            || lo.find("satellite")    != std::string::npos;
    }

    void applyVisualizationPreset(ViewportPanel::VisualizationMode mode) {
        switch (mode) {
            case ViewportPanel::VisualizationMode::Simple:
                Application::setRenderFeatureHDR(false);
                Application::setRenderFeatureBloom(false);
                Application::setRenderFeatureSSAO(false);
                Application::setRenderFeatureIBL(false);
                Application::setRenderFeatureShadows(false);
                break;
            case ViewportPanel::VisualizationMode::Complete:
                Application::setRenderFeatureHDR(true);
                Application::setRenderFeatureBloom(false);
                Application::setRenderFeatureSSAO(false);
                Application::setRenderFeatureIBL(false);
                Application::setRenderFeatureShadows(true);
                break;
            case ViewportPanel::VisualizationMode::Final:
                Application::setRenderFeatureHDR(true);
                Application::setRenderFeatureBloom(true);
                Application::setRenderFeatureSSAO(true);
                Application::setRenderFeatureIBL(true);
                Application::setRenderFeatureShadows(true);
                break;
        }
    }
}

// --- Helper: De SceneObject a Matriz (float) ---
glm::mat4 GetTransformMatrix(const Haruka::SceneObject& obj) {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(obj.position));
    transform *= glm::mat4_cast(static_cast<glm::quat>(obj.rotation));
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
    if (m_uboPerFrame  != 0) glDeleteBuffers(1, &m_uboPerFrame);
    if (m_uboPerObject != 0) glDeleteBuffers(1, &m_uboPerObject);
    for (auto& [name, faces] : m_planetPreviewCache) {
        for (auto& face : faces) {
            if (face.vao) glDeleteVertexArrays(1, &face.vao);
            if (face.vbo) glDeleteBuffers(1, &face.vbo);
            if (face.nbo) glDeleteBuffers(1, &face.nbo);
            if (face.ebo) glDeleteBuffers(1, &face.ebo);
        }
    }
}

void ViewportPanel::onImGuiRender() {
    // Estilo sin bordes para que la imagen ocupe todo el panel
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
    
    ImGui::Begin("Viewport");

    // Barra superior de modos de visualización
    ImGui::SetCursorPos(ImVec2(8.0f, 8.0f));
    if (ImGui::Button("Simple")) {
        m_visualizationMode = VisualizationMode::Simple;
        applyVisualizationPreset(m_visualizationMode);
    }
    ImGui::SameLine();
    if (ImGui::Button("Completo")) {
        m_visualizationMode = VisualizationMode::Complete;
        applyVisualizationPreset(m_visualizationMode);
    }
    ImGui::SameLine();
    if (ImGui::Button("Final")) {
        m_visualizationMode = VisualizationMode::Final;

        applyVisualizationPreset(m_visualizationMode);
    }

    ImGui::SetCursorPosY(40.0f);

    ImGui::SeparatorText("Effects");
    bool hdr = Application::getRenderFeatureHDR();
    bool bloom = Application::getRenderFeatureBloom();
    bool ssao = Application::getRenderFeatureSSAO();
    bool ibl = Application::getRenderFeatureIBL();
    bool shadows = Application::getRenderFeatureShadows();

    if (ImGui::Checkbox("HDR", &hdr)) Application::setRenderFeatureHDR(hdr);
    ImGui::SameLine();
    if (ImGui::Checkbox("Bloom", &bloom)) Application::setRenderFeatureBloom(bloom);
    ImGui::SameLine();
    if (ImGui::Checkbox("SSAO", &ssao)) Application::setRenderFeatureSSAO(ssao);
    ImGui::SameLine();
    if (ImGui::Checkbox("IBL", &ibl)) Application::setRenderFeatureIBL(ibl);
    ImGui::SameLine();
    if (ImGui::Checkbox("Shadows", &shadows)) Application::setRenderFeatureShadows(shadows);

    m_isFocused = ImGui::IsWindowFocused();
    m_isHovered = ImGui::IsWindowHovered();

    // 1. Sincronizar tamaño del RenderTarget con el panel de ImGui
    manageResize();

    // 2. La escena ya fue renderizada por Application::renderFrameContent() en EditorApplication::render().
    //    Solo mostramos la textura resultante.

    // 3. Mostrar la textura resultante
    // Invertimos las V (0,1 a 1,0) porque OpenGL y ImGui tienen el origen Y opuesto
    uint32_t textureID = m_renderTarget->getColorTexture();
    ImGui::Image((void*)(intptr_t)textureID, 
                 ImVec2{ m_viewportSize.x, m_viewportSize.y }, 
                 ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

    // Camera HUD overlay: top-left inside the rendered image
    if (m_camera) {
        ImVec2 imgMin = ImGui::GetItemRectMin();
        ImVec2 padding{ 8.0f, 8.0f };
        ImVec2 pos = ImVec2(imgMin.x + padding.x, imgMin.y + padding.y);

        char line1[128];
        char line2[64];
        char line3[64];
        char line4[64];
        const auto& p = m_camera->position;
        const auto& o = m_camera->orientation;
        std::snprintf(line1, sizeof(line1), "Cam Pos: %.2f, %.2f, %.2f", (double)p.x, (double)p.y, (double)p.z);
        std::snprintf(line2, sizeof(line2), "Cam Rot: %.2f, %.2f, %.2f", (double)o.x, (double)o.y, (double)o.z);
        std::snprintf(line3, sizeof(line3), "Speed: %.2f", m_camera->speed);
        std::snprintf(line4, sizeof(line4), "FOV: %.1f\u00B0", m_camera->zoom);

        ImU32 col = IM_COL32(240, 240, 240, 220);
        float lineHeight = ImGui::GetFontSize() + 2.0f;
        ImGui::GetWindowDrawList()->AddText(pos, col, line1);
        ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, pos.y + lineHeight), col, line2);
        ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, pos.y + 2.0f * lineHeight), col, line3);
        ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, pos.y + 3.0f * lineHeight), col, line4);
    }

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
            
            // Si hay cámara, actualiza su aspect ratio para proyecciones correctas
            if (m_camera) m_camera->setAspectRatio(m_viewportSize.x / m_viewportSize.y);
        }
    }
}

// std140-compatible structs mirroring the UBO declarations in the shaders.
// Any change to the GLSL UBO layout must be reflected here.
namespace {
struct alignas(16) PerFrameUBOData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::vec3 cameraPos;      float _pad0;
    glm::vec3 sunDirection;   float _pad1;
    glm::vec3 sunLightColor;  float ambientStrength;
    int enableHDR;
    int enableBloom;
    int enableSSAO;
    int enableIBL;
    int enableShadows;
    int _pad3[3];
};
static_assert(sizeof(PerFrameUBOData) == 208, "PerFrameUBOData std140 size mismatch");

struct alignas(16) PerObjectUBOData {
    glm::mat4 model;
    glm::vec4 baseColorAndPlanetRadius; // rgb=color, a=planetRadius
    glm::vec4 planetCenterAndFlag;      // xyz=planetCenter, w=useProceduralTerrain
};
static_assert(sizeof(PerObjectUBOData) == 96, "PerObjectUBOData std140 size mismatch");
} // namespace

void ViewportPanel::renderScene() {
    if (!m_currentScene || !m_camera) return;

    // Lazy-create UBOs once a GL context is available
    if (m_uboPerFrame == 0) {
        glGenBuffers(1, &m_uboPerFrame);
        glBindBuffer(GL_UNIFORM_BUFFER, m_uboPerFrame);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(PerFrameUBOData), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
    if (m_uboPerObject == 0) {
        glGenBuffers(1, &m_uboPerObject);
        glBindBuffer(GL_UNIFORM_BUFFER, m_uboPerObject);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(PerObjectUBOData), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    // Reload shader when visualization mode changes
    if (!m_sceneShader || !m_shaderModeLoaded || m_loadedShaderMode != m_visualizationMode) {
        switch (m_visualizationMode) {
            case VisualizationMode::Simple:
                m_sceneShader = std::make_unique<Shader>("shaders/preview.vert", "shaders/preview.frag");
                break;
            case VisualizationMode::Complete:
                m_sceneShader = std::make_unique<Shader>("shaders/simple.vert", "shaders/light_cube.frag");
                break;
            case VisualizationMode::Final:
                m_sceneShader = std::make_unique<Shader>("shaders/simple.vert", "shaders/final.frag");
                break;
        }
        m_loadedShaderMode = m_visualizationMode;
        m_shaderModeLoaded = true;
    }

    if (!m_renderTarget) {
        uint32_t w = (m_viewportSize.x > 0) ? (uint32_t)m_viewportSize.x : 1280u;
        uint32_t h = (m_viewportSize.y > 0) ? (uint32_t)m_viewportSize.y : 720u;
        m_renderTarget = std::make_unique<RenderTarget>(w, h);
    }

    m_renderTarget->bindForWriting();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glViewport(0, 0, (GLsizei)m_viewportSize.x, (GLsizei)m_viewportSize.y);
    glClearColor(0.01f, 0.01f, 0.015f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_sceneShader->use();

    // Bind UBOs to the fixed binding points used by all shaders
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_uboPerFrame);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_uboPerObject);

    // --- Upload per-frame data ---
    float aspect = 1.0f;
    if (m_viewportSize.y > 0.0f)
        aspect = m_viewportSize.x / m_viewportSize.y;
    else if (m_camera)
        aspect = m_camera->aspectRatio;
    if (aspect <= 0.0f) aspect = 1.0f;

    const glm::vec3 cameraOrigin       = glm::vec3(m_camera->position);
    const glm::vec3 kSunDir            = glm::normalize(glm::vec3(0.35f, 0.75f, 0.25f));
    const bool isComplete = (m_visualizationMode == VisualizationMode::Complete);
    const bool isFinal    = (m_visualizationMode == VisualizationMode::Final);

    PerFrameUBOData frameData{};
    // Floating-origin: objects are pre-shifted by -cameraOrigin in their model
    // matrix, so the view matrix must be rotation-only (no translation).
    frameData.view            = glm::mat4(glm::mat3(m_camera->getViewMatrix()));
    frameData.projection      = m_camera->getProjectionMatrix(aspect);
    frameData.cameraPos       = cameraOrigin;
    frameData.sunDirection    = kSunDir;
    frameData.sunLightColor   = glm::vec3(1.0f, 0.98f, 0.95f);
    frameData.ambientStrength = isComplete ? 0.16f : 0.0f;
    frameData.enableHDR       = isFinal ? (int)Application::getRenderFeatureHDR()    : 0;
    frameData.enableBloom     = isFinal ? (int)Application::getRenderFeatureBloom()  : 0;
    frameData.enableSSAO      = isFinal ? (int)Application::getRenderFeatureSSAO()   : 0;
    frameData.enableIBL       = isFinal ? (int)Application::getRenderFeatureIBL()    : 0;
    frameData.enableShadows   = isFinal ? (int)Application::getRenderFeatureShadows(): 0;

    glBindBuffer(GL_UNIFORM_BUFFER, m_uboPerFrame);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PerFrameUBOData), &frameData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // --- Draw objects ---
    const auto renderCommands = Haruka::buildSceneRenderQueue(*m_currentScene);

    int frameDrawCalls = 0;
    int frameVertices  = 0;
    int frameTriangles = 0;

    for (const auto& command : renderCommands) {
        const auto* obj = command.object;
        if (!obj) continue;

        const glm::vec3 objectColor = (glm::length(glm::vec3(obj->color)) > 0.001f)
            ? glm::vec3(obj->color)
            : glm::vec3(0.76f, 0.78f, 0.82f);

        glm::mat4 modelMat = GetTransformMatrix(*obj);
        modelMat = glm::translate(glm::mat4(1.0f), -cameraOrigin) * modelMat;

        PerObjectUBOData objData{};
        objData.model                    = modelMat;
        objData.baseColorAndPlanetRadius = glm::vec4(objectColor, 1.0f);
        objData.planetCenterAndFlag      = glm::vec4(0.0f);

        // Activate procedural terrain albedo for planet-like objects (light_cube.frag, Complete mode)
        if (isComplete && isPlanetLike(*obj)) {
            float radius = (float)std::max({obj->scale.x, obj->scale.y, obj->scale.z});
            glm::vec3 center = glm::vec3(obj->position) - cameraOrigin;
            objData.baseColorAndPlanetRadius.a = radius;
            objData.planetCenterAndFlag = glm::vec4(center, 1.0f);
        }

        glBindBuffer(GL_UNIFORM_BUFFER, m_uboPerObject);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PerObjectUBOData), &objData);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        switch (command.kind) {
            case Haruka::RenderKind::Model: {
                Model* model = getOrLoadModelCached(obj->modelPath);
                if (!model) break;
                model->Draw(*m_sceneShader);
                ++frameDrawCalls;
                frameVertices  += model->getVertexCount();
                frameTriangles += model->getTriangleCount();
                break;
            }
            case Haruka::RenderKind::MeshComponent: {
                if (!obj->meshRenderer || !obj->meshRenderer->isResident()) break;
                obj->meshRenderer->render(*m_sceneShader);
                ++frameDrawCalls;
                frameVertices  += obj->meshRenderer->getResidentVertexCount();
                frameTriangles += obj->meshRenderer->getResidentTriangleCount();
                break;
            }
            case Haruka::RenderKind::Primitive: {
                SimpleMesh* mesh = getPrimitiveMesh(command.primitive);
                if (!mesh) break;
                mesh->draw();
                ++frameDrawCalls;
                frameVertices  += mesh->getVertexCount();
                frameTriangles += mesh->getTriangleCount();
                break;
            }
            case Haruka::RenderKind::None:
                break;
        }
    }

    glDisable(GL_DEPTH_TEST);
    m_renderTarget->unbind();

    // --- Basic frame statistics (editor-side approximation) ---
    if (m_statsPanel) {
        m_statsPanel->setDrawCalls(frameDrawCalls);
        m_statsPanel->setVertexCount(frameVertices);
        m_statsPanel->setTriangleCount(frameTriangles);

        const auto& objs = m_currentScene->getAllObjects();
        int visibleChunks = 0;
        int residentChunks = 0;
        int tracked = 0;
        for (const auto& o : objs) {
            if (!o) continue;
            if (o->flags.hasChunks) visibleChunks += 1;
            if (o->terrainSettings) residentChunks += 0;
            if (o->flags.originShiftingTarget) tracked += 1;
        }
        m_statsPanel->setVisibleChunkCount(visibleChunks);
        m_statsPanel->setResidentChunkCount(residentChunks);
        m_statsPanel->setTrackedChunkCount(tracked);
    }
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

        selectedObj.rotation = glm::normalize(glm::dquat(rotation));
    }
}

void ViewportPanel::update(float deltaTime) {
    // Cambio de herramientas (W, E, R)
    if (m_isFocused) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) m_gizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) m_gizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) m_gizmoOperation = ImGuizmo::SCALE;
    }

    // Movimiento de cámara en el viewport: WASD, Space y Ctrl
    if (m_isHovered && m_camera && !ImGuizmo::IsUsing()) {
        m_camera->processInput(m_sdlWindow, deltaTime);

        // Right-click drag to rotate camera
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
                m_camera->rotate(io.MouseDelta.x, io.MouseDelta.y);
            }
        }

        // Scroll wheel adjusts camera speed (x1.15 per tick)
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_camera->speed = std::max(0.1f, m_camera->speed * std::pow(1.15f, wheel));
        }
    }
}

Model* ViewportPanel::getOrLoadModel(const std::string& path) {
    return getOrLoadModelCached(path);
}