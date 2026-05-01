#define GLM_ENABLE_EXPERIMENTAL
#include "viewport.h"
#include "core/application.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/intersect.hpp>
#include <algorithm>
#include "commands/scene_commands.h"
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace {
std::unordered_map<std::string, std::shared_ptr<Model>> g_modelCache;

bool isRenderDisabledByEditor(const Haruka::SceneObject& obj) {
    if (!obj.properties.is_object()) return false;
    if (!obj.properties.contains("terrainEditor")) return false;
    const auto& te = obj.properties["terrainEditor"];
    return te.value("disableRender", false);
}

void buildPrimitiveMeshFromProperties(Haruka::SceneObject& obj) {
    if (!obj.meshRenderer) {
        obj.meshRenderer = std::make_shared<MeshRendererComponent>();
    }
    if (!obj.meshRenderer || obj.meshRenderer->isResident()) return;
    if (!obj.properties.contains("meshRenderer")) return;

    const auto& mr = obj.properties["meshRenderer"];
    std::string meshType = mr.value("meshType", "");
    std::vector<glm::vec3> verts, norms;
    std::vector<unsigned int> indices;

    if (meshType == "cube") {
        PrimitiveShapes::createCube(mr.value("size", 1.0f), verts, norms, indices);
    } else if (meshType == "sphere") {
        float radius = mr.value("radius", 1.0f);
        int segments = mr.value("segments", 32);
        PrimitiveShapes::createSphere(radius, segments, segments, verts, norms, indices);
    } else if (meshType == "capsule") {
        PrimitiveShapes::createCapsule(
            mr.value("radius", 0.5f),
            mr.value("height", 2.0f),
            mr.value("segments", 24),
            mr.value("stacks", 16),
            verts, norms, indices);
    } else if (meshType == "plane") {
        PrimitiveShapes::createPlane(
            mr.value("width", 2.0f),
            mr.value("height", 2.0f),
            mr.value("subdivisions", 10),
            verts, norms, indices);
    }

    if (!verts.empty()) {
        obj.meshRenderer->setMesh(verts, norms, indices);
    }
}

void maybeReleasePrimitiveMesh(Haruka::SceneObject& obj) {
    if (obj.meshRenderer && obj.meshRenderer->isResident()) {
        obj.meshRenderer->releaseMesh();
    }
}

std::shared_ptr<Model> getOrLoadModelCached(const std::string& path) {
    auto it = g_modelCache.find(path);
    if (it != g_modelCache.end()) {
        return it->second;
    }

    try {
        auto model = std::make_shared<Model>(path);
        g_modelCache[path] = model;
        return model;
    } catch (...) {
        return nullptr;
    }
}

void releaseModelFromCache(const std::string& path) {
    g_modelCache.erase(path);
}
}

ViewportPanel::ViewportPanel()
    : editorWorldSystem(std::make_unique<Haruka::WorldSystem>())
    , editorTerrainStreaming(std::make_unique<Haruka::TerrainStreamingSystem>())
{}

ViewportPanel::~ViewportPanel() = default;

void ViewportPanel::setScene(Haruka::Scene* scene) {
    currentScene = scene;
    
    // Inicializar Application si no existe
    if (scene) {
        auto* motorApp = MotorInstance::getInstance().getApplication();
        if (!motorApp) {
            if (!ownedApplication) {
                ownedApplication = std::make_unique<Application>();
            }
            MotorInstance::getInstance().setApplication(ownedApplication.get());
            ownedApplication->init(*scene);
            if (!camera) {
                camera = ownedApplication->getCamera();
                MotorInstance::getInstance().setCamera(camera);
            }
        } else {
            motorApp->init(*scene);
            if (!camera) {
                camera = motorApp->getCamera();
                MotorInstance::getInstance().setCamera(camera);
            }
        }
    }
    
    // Registrar en MotorInstance cuando cambia la escena
    MotorInstance::getInstance().setScene(scene);
}

void ViewportPanel::setCamera(Camera* cam) {
    camera = cam;
    if (camera) {
        MotorInstance::getInstance().setCamera(camera);
    } else {
        std::cout << "[ViewportPanel] setCamera: cámara nula, no se registra en MotorInstance" << std::endl;
    }
}

void ViewportPanel::recreateRenderTarget() {
    renderTarget = std::make_unique<RenderTarget>(width, height);
    MotorInstance::getInstance().setRenderTarget(renderTarget.get());

    Application* app = ownedApplication
        ? ownedApplication.get()
        : MotorInstance::getInstance().getApplication();
    if (app) app->recreateFBOs(width, height);
}

glm::vec3 ViewportPanel::getRayFromMouse(const glm::mat4& proj, const glm::mat4& view) {
    ImVec2 mousePos = ImGui::GetMousePos();

    float x = (mousePos.x - viewportMin.x) / (viewportMax.x - viewportMin.x);
    float y = (mousePos.y - viewportMin.y) / (viewportMax.y - viewportMin.y);

    glm::vec4 ndc = glm::vec4(x * 2.0f - 1.0f, -(y * 2.0f - 1.0f), -1.0f, 1.0f);
    glm::vec4 eye = glm::inverse(proj) * ndc;
    eye = glm::vec4(eye.x, eye.y, -1.0f, 0.0f);
    return glm::normalize(glm::vec3(glm::inverse(view) * eye));
}

int ViewportPanel::getHoveredObjectIndex(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::mat4& proj, const glm::mat4& view) {
    if (!renderTarget) {
        std::cout << "[ViewportPanel] renderScene: renderTarget nullptr" << std::endl;
        return -1;
    }

    float closestDist = FLT_MAX;
    int closestIdx = -1;

    const auto& objects = currentScene->getObjects();
    for (size_t i = 0; i < objects.size(); i++) {
        const auto& obj = objects[i];
        if (isRenderDisabledByEditor(obj)) continue;
        
        // Bounding sphere (radio 0.5 * escala)
        glm::vec3 center = glm::vec3(obj.position);
        float radius = 0.5f * glm::length(glm::vec3(obj.scale));
        
        float distance;
        if (glm::intersectRaySphere(rayOrigin, rayDir, center, radius, distance)) {
            if (distance < closestDist) {
                closestDist = distance;
                closestIdx = static_cast<int>(i);
            }
        }
    }

    return closestIdx;
}

void ViewportPanel::handleGizmoInput() {
    if (playMode) return;

    ImGuiIO& io = ImGui::GetIO();

    if (isViewportHovered && !io.WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000000000000.0f);
        glm::mat4 view = camera->getViewMatrix();

        glm::vec3 rayDir = getRayFromMouse(proj, view);
        glm::vec3 rayOrigin = camera->position;

        selectedObjectIndex = getHoveredObjectIndex(rayOrigin, rayDir, proj, view);

        if (selectedObjectIndex >= 0 && currentScene) {
            auto& obj = currentScene->getObjectsMutable()[selectedObjectIndex];

            // Convierte dvec3 a vec3 para ImGuizmo
            glm::vec3 pos   = glm::vec3(obj.position);
            glm::vec3 rot   = glm::vec3(obj.rotation);
            glm::vec3 scale = glm::vec3(obj.scale);

            // Construye la matriz de transformación en float
            glm::mat4 objTransform = glm::translate(glm::mat4(1.0f), pos)
                * glm::rotate(glm::mat4(1.0f), glm::radians(rot.x), glm::vec3(1,0,0))
                * glm::rotate(glm::mat4(1.0f), glm::radians(rot.y), glm::vec3(0,1,0))
                * glm::rotate(glm::mat4(1.0f), glm::radians(rot.z), glm::vec3(0,0,1))
                * glm::scale(glm::mat4(1.0f), scale);

            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y);

            glm::mat4 view = camera->getViewMatrix();
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000000000000.0f);

            ImGuizmo::Manipulate(
                glm::value_ptr(view), glm::value_ptr(proj),
                (ImGuizmo::OPERATION)currentGizmoOperation, ImGuizmo::LOCAL,
                glm::value_ptr(objTransform)
            );

            if (ImGuizmo::IsUsing()) {
                glm::vec3 newPos, newRot, newScale;
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(objTransform), &newPos.x, &newRot.x, &newScale.x);
                obj.position = glm::dvec3(newPos);
                obj.rotation = glm::dvec3(newRot);
                obj.scale    = glm::dvec3(newScale);
            }
        }
    }

    if (isDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left) && activeAxis != GizmoAxis::None) {
        ImGuiIO& io = ImGui::GetIO();
        auto* obj = currentScene->getObject(currentScene->getObjects()[selectedObjectIndex].name);
        if (!obj) return;

        glm::vec3 axisDir =
            (activeAxis == GizmoAxis::X) ? glm::vec3(1,0,0) :
            (activeAxis == GizmoAxis::Y) ? glm::vec3(0,1,0) :
                                           glm::vec3(0,0,1);

        if (gizmoMode == 0) { // MOVE
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000000000000.0f);
            glm::mat4 view = camera->getViewMatrix();

            glm::vec3 rayDir = getRayFromMouse(proj, view);
            glm::vec3 rayOrigin = camera->position;

            float tAxis;
            if (rayIntersectsAxis(rayOrigin, rayDir, dragStartPos, axisDir, tAxis)) {
                obj->position = dragStartPos + axisDir * tAxis;
            }
        }
        else if (gizmoMode == 1) { // ROTATE
            float rotDelta = io.MouseDelta.x * 0.5f;
            obj->rotation = dragStartRot;
            if (activeAxis == GizmoAxis::X) obj->rotation.x += rotDelta;
            if (activeAxis == GizmoAxis::Y) obj->rotation.y += rotDelta;
            if (activeAxis == GizmoAxis::Z) obj->rotation.z += rotDelta;
        }
        else if (gizmoMode == 2) { // SCALE
            float scaleDelta = io.MouseDelta.x * 0.01f;
            obj->scale = dragStartScale;
            if (activeAxis == GizmoAxis::X) obj->scale.x = std::max(0.1f, dragStartScale.x + scaleDelta);
            if (activeAxis == GizmoAxis::Y) obj->scale.y = std::max(0.1f, dragStartScale.y + scaleDelta);
            if (activeAxis == GizmoAxis::Z) obj->scale.z = std::max(0.1f, dragStartScale.z + scaleDelta);
        }
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && isDragging) {
        isDragging = false;

        auto* obj = currentScene->getObject(currentScene->getObjects()[selectedObjectIndex].name);
        if (obj && commandHistory) {
            if (obj->position != glm::dvec3(dragStartPos))
            {
                commandHistory->execute(std::make_unique<TransformObjectCommand>(
                    currentScene, obj->name, obj->position, obj->rotation, obj->scale
                ));
            }
        }
        activeAxis = GizmoAxis::None;
    }
}

void ViewportPanel::handleAssetDrop() {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
            std::string assetPath = (const char*)payload->Data;
            
            // Soportar múltiples formatos de modelo
            bool isModel = (assetPath.find(".obj") != std::string::npos ||
                           assetPath.find(".gltf") != std::string::npos ||
                           assetPath.find(".glb") != std::string::npos ||
                           assetPath.find(".fbx") != std::string::npos);
            
            if (currentScene && isModel) {
                Haruka::SceneObject obj;
                obj.name = "Model_" + std::to_string(currentScene->getObjects().size());
                obj.type = "Model";
                obj.modelPath = assetPath;
                obj.position = glm::vec3(0, 0, 0);
                obj.rotation = glm::vec3(0, 0, 0);
                obj.scale = glm::vec3(1, 1, 1);
                
                if (commandHistory) {
                    commandHistory->execute(std::make_unique<AddObjectCommand>(currentScene, obj));
                } else {
                    currentScene->addObject(obj);
                }
                
                std::cout << "Model added: " << assetPath << std::endl;
            }
        }
        ImGui::EndDragDropTarget();
    }
}

void ViewportPanel::renderScene() {
    if (!renderTarget) return;

    renderVertex_count = 0;
    renderDraw_calls = 0;

    // --- Render del motor vs render local ---
    // Si quieres forzar render local en el editor, usa esta bandera:
    #ifdef HARUKA_EDITOR
    static bool forceLocalRender = false;
    if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_L)) {
        forceLocalRender = !forceLocalRender;
    }
    #else
    constexpr bool forceLocalRender = false;
    #endif

    RenderTarget* motorTarget = MotorInstance::getInstance().getRenderTarget();
    bool motorActivo = MotorInstance::getInstance().isMotorActive();
    bool motorTieneApp = (MotorInstance::getInstance().getApplication() != nullptr);
    bool motorTieneCam = (MotorInstance::getInstance().getCamera() != nullptr);
    bool motorRenderDirecto = (motorTarget && (motorTarget == renderTarget.get()));

    auto computeSceneStats = [&](int& outVertices, int& outTriangles, int& outDrawCalls) {
        outVertices = 0;
        outTriangles = 0;
        outDrawCalls = 0;
        Haruka::Scene* sceneForStats = MotorInstance::getInstance().getScene();
        if (!sceneForStats) sceneForStats = currentScene;
        if (!sceneForStats) return;

        for (const auto& obj : sceneForStats->getObjects()) {
            if (isRenderDisabledByEditor(obj)) continue;
            if (obj.meshRenderer) {
                outDrawCalls++;
                outVertices += obj.meshRenderer->getVertexCount();
                outTriangles += obj.meshRenderer->getTriangleCount();
                continue;
            }
            if (!obj.modelPath.empty()) {
                try {
                    auto model = getOrLoadModelCached(obj.modelPath);
                    if (!model) continue;
                    outDrawCalls++;
                    outVertices += model->getVertexCount();
                    outTriangles += model->getTriangleCount();
                } catch (...) {}
            }
        }
    };

    bool useMotorOutput = playMode && !forceLocalRender;
    if (useMotorOutput) {
        if (motorTarget && motorRenderDirecto) {
            // El motor ya renderiza directo en este target, no hacer nada más
            if (statsPanel) {
                statsPanel->setVertexCount(Application::getLastRenderedVertices());
                statsPanel->setDrawCalls(Application::getLastRenderedDrawCalls());
                statsPanel->setTriangleCount(Application::getLastRenderedTriangles());
                statsPanel->setTotalVertexCount(Application::getLastTotalVertices());
                statsPanel->setTotalDrawCalls(Application::getLastTotalDrawCalls());
                statsPanel->setTotalTriangleCount(Application::getLastTotalTriangles());
                statsPanel->setVisibleChunkCount(Application::getLastVisibleChunks());
                statsPanel->setResidentChunkCount(Application::getLastResidentChunks());
                statsPanel->setPendingChunkLoads(Application::getLastPendingChunkLoads());
                statsPanel->setPendingChunkEvictions(Application::getLastPendingChunkEvictions());
                statsPanel->setResidentMemoryMB(Application::getLastResidentMemoryMB());
                statsPanel->setTrackedChunkCount(Application::getLastTrackedChunks());
                statsPanel->setMaxMemoryMB(Application::getLastMaxMemoryMB());
            }
            return;
        } else if (motorTarget) {
            // Copiar textura del motor al renderTarget del viewport
            glBindFramebuffer(GL_READ_FRAMEBUFFER, motorTarget->getFBO());
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, renderTarget->getFBO());
            glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            if (statsPanel) {
                statsPanel->setVertexCount(Application::getLastRenderedVertices());
                statsPanel->setDrawCalls(Application::getLastRenderedDrawCalls());
                statsPanel->setTriangleCount(Application::getLastRenderedTriangles());
                statsPanel->setTotalVertexCount(Application::getLastTotalVertices());
                statsPanel->setTotalDrawCalls(Application::getLastTotalDrawCalls());
                statsPanel->setTotalTriangleCount(Application::getLastTotalTriangles());
                statsPanel->setVisibleChunkCount(Application::getLastVisibleChunks());
                statsPanel->setResidentChunkCount(Application::getLastResidentChunks());
                statsPanel->setPendingChunkLoads(Application::getLastPendingChunkLoads());
                statsPanel->setPendingChunkEvictions(Application::getLastPendingChunkEvictions());
                statsPanel->setResidentMemoryMB(Application::getLastResidentMemoryMB());
                statsPanel->setTrackedChunkCount(Application::getLastTrackedChunks());
                statsPanel->setMaxMemoryMB(Application::getLastMaxMemoryMB());
            }
            return;
        } else {
            // Sin render target del motor: mantener el viewport sin renderizar la ruta local inestable.
            if (statsPanel) {
                statsPanel->setVertexCount(0);
                statsPanel->setDrawCalls(0);
                statsPanel->setTriangleCount(0);
                statsPanel->setTotalVertexCount(0);
                statsPanel->setTotalDrawCalls(0);
                statsPanel->setTotalTriangleCount(0);
                statsPanel->setVisibleChunkCount(0);
                statsPanel->setResidentChunkCount(0);
                statsPanel->setPendingChunkLoads(0);
                statsPanel->setPendingChunkEvictions(0);
                statsPanel->setResidentMemoryMB(0);
                statsPanel->setTrackedChunkCount(0);
                statsPanel->setMaxMemoryMB(0);
            }
            return;
        }
    }

    // Render local (editor o fallback)
    renderTarget->bindForWriting();
    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int localDrawCalls = 0;
    int localVertices = 0;
    int localTriangles = 0;
    int totalVertices = 0;
    int totalTriangles = 0;
    int totalDrawCalls = 0;
    computeSceneStats(totalVertices, totalTriangles, totalDrawCalls);
    if (currentScene) {
        glm::dvec3 camPos = camera ? glm::dvec3(camera->position) : glm::dvec3(0.0);

        // Run terrain streaming every frame in the editor so chunks are generated
        // without requiring play mode.
        if (editorTerrainStreaming && editorWorldSystem && camera) {
            Haruka::TerrainStreamingStats tStats;
            editorTerrainStreaming->update(currentScene, editorWorldSystem.get(),
                                           nullptr, nullptr, camera, &tStats);
            if (statsPanel) {
                statsPanel->setVisibleChunkCount(tStats.visibleChunks);
                statsPanel->setResidentChunkCount(tStats.residentChunks);
                statsPanel->setPendingChunkLoads(tStats.pendingChunkLoads);
                statsPanel->setPendingChunkEvictions(tStats.pendingChunkEvictions);
                statsPanel->setResidentMemoryMB(tStats.residentMemoryMB);
                statsPanel->setTrackedChunkCount(tStats.trackedChunks);
                statsPanel->setMaxMemoryMB(tStats.maxMemoryMB);
            }
        }

        bool shaderReady = true;
        if (!sceneShader) {
            try {
                // Shader simple/estable para editor local
                sceneShader = std::make_unique<Shader>("shaders/simple.vert", "shaders/light_cube.frag");
            } catch (const std::exception& e) {
                std::cerr << "[ViewportPanel] Error al crear sceneShader: " << e.what() << std::endl;
                shaderReady = false;
            }
        }
        if (!sceneShader) {
            std::cerr << "[ViewportPanel] sceneShader es nullptr, abortando render local" << std::endl;
            shaderReady = false;
        }
        if (shaderReady) {
            Application* motorApp = MotorInstance::getInstance().getApplication();
            CascadedShadowMap* cascadedShadow = motorApp ? motorApp->getCascadedShadowMap() : nullptr;
            Shader* cascadeShadowShader = motorApp ? motorApp->getCascadedShadowShader() : nullptr;

            const bool enableShadows = true;

            glm::vec3 sunPos(5000.0f, 5000.0f, -5000.0f);
            glm::vec3 sunColor(1.0f, 1.0f, 0.95f);
            float sunIntensity = 20.0f;
            for (const auto& obj : currentScene->getObjects()) {
                if (isRenderDisabledByEditor(obj)) continue;
                if (obj.type == "Light" || obj.type == "PointLight" || obj.type == "DirectionalLight") {
                    sunPos = glm::vec3(obj.getWorldPosition(currentScene));
                    sunColor = glm::vec3(obj.color);
                    sunIntensity = std::max((float)obj.intensity, 0.0f);
                    break;
                }
            }

            glm::vec3 sunDir = glm::normalize(sunPos);
            glm::mat4 cameraView = camera ? camera->getViewMatrix() : glm::lookAt(glm::vec3(0.0f, 2.0f, 8.0f), glm::vec3(0.0f), glm::vec3(0, 1, 0));
            float camDist = camera ? glm::length(glm::vec3(camera->position)) : 1000.0f;
            float nearPlane = std::clamp(camDist * 0.001f, 0.5f, 20.0f);
            float farPlane = std::max(200000.0f, camDist * 400.0f);
            if (currentScene && currentScene->getObject("Sun")) {
                const auto* sunObj = currentScene->getObject("Sun");
                glm::vec3 sunPosObj = glm::vec3(sunObj->getWorldPosition(currentScene));
                float sunDistance = glm::length(sunPosObj - (camera ? glm::vec3(camera->position) : glm::vec3(0.0f)));
                float sunRadius = std::max(std::abs((float)sunObj->scale.x), std::max(std::abs((float)sunObj->scale.y), std::abs((float)sunObj->scale.z)));
                farPlane = std::max(farPlane, sunDistance + sunRadius * 3.0f);
                farPlane = std::min(farPlane, 300000000.0f);
            }

            if (enableShadows && cascadedShadow) {
                glm::vec3 camForward = camera ? camera->getFront() : glm::vec3(0.0f, 0.0f, -1.0f);
                glm::vec3 camUp = camera ? camera->getUp() : glm::vec3(0.0f, 1.0f, 0.0f);
                cascadedShadow->updateCascades(
                    -sunDir,
                    camera ? glm::vec3(camera->position) : glm::vec3(0.0f),
                    camForward,
                    camUp,
                    (float)width / (float)height,
                    nearPlane,
                    farPlane,
                    144.0f);
            }

            // Shadow depth pass
            if (enableShadows && cascadedShadow && cascadeShadowShader) {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(1.5f, 4.0f);

                cascadeShadowShader->use();
                for (int cascade = 0; cascade < cascadedShadow->getNumCascades(); ++cascade) {
                    cascadedShadow->bindForWriting(cascade);
                    glClear(GL_DEPTH_BUFFER_BIT);

                    cascadeShadowShader->setMat4("lightSpaceMatrix", cascadedShadow->getCascadeMatrix(cascade));

                    for (const auto& obj : currentScene->getObjects()) {
                        if (isRenderDisabledByEditor(obj)) continue;
                        glm::mat4 modelMatrix = obj.getWorldTransform(currentScene);
                        cascadeShadowShader->setMat4("model", modelMatrix);

                        if (obj.meshRenderer && obj.meshRenderer->isResident()) {
                            obj.meshRenderer->render(*cascadeShadowShader);
                        } else if (!obj.modelPath.empty()) {
                            try {
                                auto model = getOrLoadModelCached(obj.modelPath);
                                if (model) model->Draw(*cascadeShadowShader);
                            } catch (...) {}
                        }
                    }
                }

                glCullFace(GL_BACK);
                glDisable(GL_POLYGON_OFFSET_FILL);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                renderTarget->bindForWriting();
                glViewport(0, 0, width, height);
            }

            sceneShader->use();
            const glm::mat4 projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, nearPlane, farPlane);
            sceneShader->setMat4("projection", projection);
            sceneShader->setMat4("view", cameraView);
            sceneShader->setMat4("lightSpaceMatrix", glm::mat4(1.0f));
            if (cascadedShadow) {
                sceneShader->setInt("numCascades", cascadedShadow->getNumCascades());
                for (int i = 0; i < cascadedShadow->getNumCascades(); ++i) {
                    sceneShader->setMat4("cascadeLightSpaceMatrices[" + std::to_string(i) + "]", cascadedShadow->getCascadeMatrix(i));
                    sceneShader->setFloat("cascadeSplits[" + std::to_string(i) + "]", cascadedShadow->getCascadeInfo(i).zFar);
                    cascadedShadow->bindForReading(i, 7 + i);
                    sceneShader->setInt("cascadeShadowMaps[" + std::to_string(i) + "]", 7 + i);
                }
            } else {
                sceneShader->setInt("numCascades", 0);
            }
            sceneShader->setVec3("sunDirection", sunDir);
            float sunEnergy = std::clamp(sunIntensity * 0.01f, 0.2f, 2.0f);
            sceneShader->setVec3("sunLightColor", sunColor * sunEnergy);
            sceneShader->setFloat("ambientStrength", 0.12f);
            sceneShader->setBool("useShadowMap", false);

            // Find planet root once for camDir, hemisphere culling, and terrain shader uniforms.
            glm::dvec3 planetCenter(0.0);
            float planetRadius = 1.0f;
            for (const auto& o : currentScene->getObjects()) {
                if (o.properties.is_object() && o.properties.contains("terrainEditor") &&
                    o.properties["terrainEditor"].value("isPlanetRoot", false)) {
                    planetCenter = o.getWorldPosition(currentScene);
                    glm::vec3 sc = glm::vec3(o.scale);
                    planetRadius = glm::length(sc) / std::sqrt(3.0f);
                    break;
                }
            }

            // Compute camDir as direction from planet center to camera (planet-local space).
            glm::vec3 camDir = glm::vec3(0.0f, 0.0f, 1.0f);
            if (camera) {
                glm::dvec3 relPos = camera->position - planetCenter;
                double rpl = glm::length(relPos);
                if (rpl > 1e-6) camDir = glm::vec3(relPos / rpl);
            }

            auto isChunkFacingCamera = [&](const Haruka::SceneObject& obj) -> bool {
                if (!obj.properties.is_object()) return true;
                if (!obj.properties.contains("terrainEditor")) return true;
                const auto& te = obj.properties["terrainEditor"];
                if (!te.is_object() || !te.value("isChunk", false)) return true;

                if (!te.contains("chunkFace") || !te.contains("chunkX") || !te.contains("chunkY") ||
                    !te.contains("chunkTilesX") || !te.contains("chunkTilesY")) return true;

                int face  = te.value("chunkFace", 0);
                int tileX = te.value("chunkX", 0);
                int tileY = te.value("chunkY", 0);
                int tilesX = te.value("chunkTilesX", 1);
                int tilesY = te.value("chunkTilesY", 1);
                if (tilesX <= 0 || tilesY <= 0) return true;

                // Compute the cube-sphere direction for this tile's center (same mapping
                // as generateChunkInternal) so the facing test is geometrically correct.
                float u = (static_cast<float>(tileX) + 0.5f) / static_cast<float>(tilesX) * 2.0f - 1.0f;
                float v = (static_cast<float>(tileY) + 0.5f) / static_cast<float>(tilesY) * 2.0f - 1.0f;
                glm::vec3 cube;
                switch (face) {
                    case 0: cube = glm::vec3( 1.0f,  v, -u); break;
                    case 1: cube = glm::vec3(-1.0f,  v,  u); break;
                    case 2: cube = glm::vec3( u,  1.0f, -v); break;
                    case 3: cube = glm::vec3( u, -1.0f,  v); break;
                    case 4: cube = glm::vec3( u,  v,  1.0f); break;
                    default:cube = glm::vec3(-u,  v, -1.0f); break;
                }
                glm::vec3 chunkDir = glm::normalize(cube);

                // Render front hemisphere plus a small back-face margin.
                return glm::dot(chunkDir, camDir) > -0.15f;
            };

            for (auto& obj : currentScene->getObjectsMutable()) {
                if (isRenderDisabledByEditor(obj)) continue;
                if (!isChunkFacingCamera(obj)) continue;
                int layer = std::clamp(obj.renderLayer, 1, 5);
                double unloadDistance = Application::getLayerMaxDistance(layer);
                if (obj.meshRenderer && layer >= 4) {
                    glm::dvec3 worldPos = obj.getWorldPosition(currentScene);
                    double dist = glm::length(worldPos - camPos);
                    if (dist > unloadDistance * 1.15) {
                        maybeReleasePrimitiveMesh(obj);
                    } else if (!obj.meshRenderer->isResident() && dist < unloadDistance * 0.85) {
                        buildPrimitiveMeshFromProperties(obj);
                    }
                }

                glm::mat4 modelMatrix = obj.getWorldTransform(currentScene);
                sceneShader->setMat4("model", modelMatrix);
                glm::vec3 baseColor = glm::vec3(obj.color);
                if (glm::length(baseColor) < 0.001f) baseColor = glm::vec3(0.8f);

                const bool isLightObj = (obj.type == "Light" || obj.type == "PointLight" || obj.type == "DirectionalLight");
                float emission = isLightObj ? std::max((float)obj.intensity, 0.0f) : 1.0f;
                glm::vec3 c = isLightObj ? (baseColor * emission) : baseColor;
                sceneShader->setVec3("lightColor", c);

                const bool isTerrainChunk = obj.properties.is_object() &&
                    obj.properties.contains("terrainEditor") &&
                    obj.properties["terrainEditor"].is_object() &&
                    obj.properties["terrainEditor"].value("isChunk", false);
                sceneShader->setBool("useProceduralTerrain", isTerrainChunk);
                if (isTerrainChunk) {
                    sceneShader->setVec3("planetCenter", glm::vec3(planetCenter));
                    sceneShader->setFloat("planetRadius", planetRadius);
                }
                if (obj.meshRenderer && obj.meshRenderer->isResident()) {
                    obj.meshRenderer->render(*sceneShader);
                    localDrawCalls++;
                    localVertices += obj.meshRenderer->getResidentVertexCount();
                    localTriangles += obj.meshRenderer->getResidentTriangleCount();
                    continue;
                }
                if (!obj.modelPath.empty()) {
                    try {
                        auto model = getOrLoadModelCached(obj.modelPath);
                        if (model) {
                            model->Draw(*sceneShader);
                            localDrawCalls++;
                            localVertices += model->getVertexCount();
                            localTriangles += model->getTriangleCount();
                        }
                    } catch (...) {}
                }
            }

            // Outline amarillo del objeto seleccionado
            if (selectedObjectIndex >= 0 && selectedObjectIndex < (int)currentScene->getObjects().size()) {
                const auto& selObj = currentScene->getObjects()[selectedObjectIndex];
                if (!isRenderDisabledByEditor(selObj)) {
                    glDisable(GL_CULL_FACE);
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(GL_LEQUAL);
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glLineWidth(3.0f);

                    sceneShader->use();
                    sceneShader->setMat4("projection", glm::perspective(glm::radians(60.0f), (float)width / (float)height, nearPlane, farPlane));
                    sceneShader->setMat4("view", cameraView);
                    sceneShader->setVec3("sunDirection", sunDir);
                    sceneShader->setVec3("sunLightColor", glm::vec3(1.0f));
                    sceneShader->setFloat("ambientStrength", 1.0f);
                    sceneShader->setBool("useShadowMap", false);
                    sceneShader->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 0.0f));

                    glm::mat4 outlineModel = selObj.getWorldTransform(currentScene);
                    outlineModel = outlineModel * glm::scale(glm::mat4(1.0f), glm::vec3(1.003f));
                    sceneShader->setMat4("model", outlineModel);

                    if (selObj.meshRenderer && selObj.meshRenderer->isResident()) {
                        selObj.meshRenderer->render(*sceneShader);
                    } else if (!selObj.modelPath.empty()) {
                        try {
                            auto model = getOrLoadModelCached(selObj.modelPath);
                            if (model) model->Draw(*sceneShader);
                        } catch (...) {}
                    }

                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    glLineWidth(1.0f);
                    glDepthFunc(GL_LESS);
                }
            }
        }
    }
    renderTarget->unbind();
    renderDraw_calls = localDrawCalls;
    renderVertex_count = localVertices;
    if (statsPanel) {
        statsPanel->setVertexCount(renderVertex_count);
        statsPanel->setDrawCalls(renderDraw_calls);
        statsPanel->setTriangleCount(localTriangles);
        statsPanel->setTotalVertexCount(totalVertices);
        statsPanel->setTotalDrawCalls(totalDrawCalls);
        statsPanel->setTotalTriangleCount(totalTriangles);
    }
}

void ViewportPanel::updateCameraFromInput(float deltaTime) {
    ImGuiIO& io = ImGui::GetIO();

    // Mouse rotate
    if ((isViewportHovered || isViewportFocused) && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        camYaw   -= delta.x * mouseSensitivity;
        camPitch += delta.y * mouseSensitivity;
    }

    // Aplicar yaw/pitch a la cámara
    if (camera) {
        glm::quat qPitch = glm::angleAxis(glm::radians(camPitch), glm::vec3(1, 0, 0));
        glm::quat qYaw   = glm::angleAxis(glm::radians(camYaw),   glm::vec3(0, 1, 0));
        camera->orientation = qYaw * qPitch;
        camera->orientation = glm::normalize(camera->orientation);

        camera->speed = moveSpeed;
        camera->sensitivity = mouseSensitivity;
    }

    // WASD solo cuando el viewport tiene foco y no hay inputs activos
    if (camera && isViewportFocused && !ImGui::IsAnyItemActive()) {
        camera->processInput(sdlWindow, deltaTime);
    }
}

void ViewportPanel::onImGuiRender() {
    ImGui::Begin("Viewport");

    // Change viewport mode
    if (ImGui::RadioButton("Mover", currentGizmoOperation == ImGuizmo::TRANSLATE)) currentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotar", currentGizmoOperation == ImGuizmo::ROTATE)) currentGizmoOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Escalar", currentGizmoOperation == ImGuizmo::SCALE)) currentGizmoOperation = ImGuizmo::SCALE;

    if (playMode) {
        ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "PLAY MODE (editing locked)");
        ImGui::Separator();
    }

    isViewportHovered = ImGui::IsWindowHovered();

    // Panel superior en una sola fila
    if (camera) {
        glm::vec3 pos = glm::vec3(camera->position);
        ImGui::SetNextItemWidth(100);
        if (ImGui::InputFloat3("Pos##cam", &pos.x, "%.1f")) {
            camera->position = Haruka::WorldPos(pos.x, pos.y, pos.z);
        }
        ImGui::SameLine(140);
        
        ImGui::SetNextItemWidth(80);
        ImGui::DragFloat("Yaw##cam", &camYaw, 1.0f, -180.0f, 180.0f, "%.0f°");
        ImGui::SameLine(240);
        
        ImGui::SetNextItemWidth(80);
        ImGui::DragFloat("Pitch##cam", &camPitch, 1.0f, -90.0f, 90.0f, "%.0f°");
        ImGui::SameLine(340);
        
        ImGui::SetNextItemWidth(70);
        ImGui::SliderFloat("Speed##cam", &moveSpeed, 0.1f, 20.0f, "%.1f");
        ImGui::SameLine(430);
        
        ImGui::SetNextItemWidth(80);
        ImGui::SliderFloat("Sens##cam", &mouseSensitivity, 0.01f, 0.5f, "%.2f");
        ImGui::SameLine(530);
        
        ImGui::Checkbox("Grid##show", &showGrid);
        ImGui::SameLine(600);
        ImGui::TextDisabled("50x50m | MB3:Rotate | WASD:Move");
    }
    
    ImGui::Separator();

    ImVec2 size = ImGui::GetContentRegionAvail();
    int newW = (int)size.x;
    int newH = (int)size.y;
    if (newW < 1) newW = 1;
    if (newH < 1) newH = 1;

    if (!renderTarget || newW != width || newH != height) {
        width = newW;
        height = newH;
        recreateRenderTarget();
    }

    ImGui::Image(
        (void*)(intptr_t)renderTarget->getColorTexture(),
        ImVec2((float)width, (float)height),
        ImVec2(0, 0),
        ImVec2(1, 1)
    );

    viewportMin = ImGui::GetItemRectMin();
    viewportMax = ImGui::GetItemRectMax();

    isViewportHovered = ImGui::IsItemHovered();
    isViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (ImGui::IsItemClicked()) {
        ImGui::SetWindowFocus();
        isViewportFocused = true;
    }

    handleAssetDrop();
    handleGizmoInput();

    ImGui::End();
}

void ViewportPanel::onUpdate(float deltaTime) {    
    updateCameraFromInput(deltaTime);

    if (ownedApplication) {
        ownedApplication->renderFrame();
    } else if (auto* app = MotorInstance::getInstance().getApplication()) {
        app->renderFrame();
    }

    renderScene();
}

void ViewportPanel::renderGizmoAxes(const glm::mat4& view, const glm::mat4& proj) {}

bool ViewportPanel::rayIntersectsAxis(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const glm::vec3& axisOrigin, const glm::vec3& axisDir, float& tOut) {
    glm::vec3 w0 = rayOrigin - axisOrigin;
    float a = glm::dot(rayDir, rayDir);
    float b = glm::dot(rayDir, axisDir);
    float c = glm::dot(axisDir, axisDir);
    float d = glm::dot(rayDir, w0);
    float e = glm::dot(axisDir, w0);
    float denom = a * c - b * b;

    if (fabs(denom) < 1e-6f) return false;

    float sc = (b * e - c * d) / denom;
    float tc = (a * e - b * d) / denom;

    glm::vec3 pRay = rayOrigin + sc * rayDir;
    glm::vec3 pAxis = axisOrigin + tc * axisDir;

    float dist = glm::length(pRay - pAxis);
    if (dist < axisPickRadius) {
        tOut = tc;
        return true;
    }
    return false;
}

Model* ViewportPanel::getOrLoadModel(const std::string& path) {
    return nullptr;
}

void ViewportPanel::renderGrid(const glm::mat4& view, const glm::mat4& proj) {}

void ViewportPanel::renderGizmoImGuizmo() {}
