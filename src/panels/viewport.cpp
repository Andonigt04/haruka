#define GLM_ENABLE_EXPERIMENTAL
#include "viewport.h"
#include "core/application.h"
#include "core/asset_paths.h"
#include "core/components/material_component.h"
#include "game/planetary_system.h"
#include "tools/procgraph/tree_textures.h"
#include "editor_util.h"
#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/intersect.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <vector>
#include <utility>
#include <string>
#include "commands/scene_commands.h"
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

namespace {
bool isRenderDisabledByEditor(const Haruka::SceneObject& obj) {
    if (!obj.properties.is_object()) return false;
    if (!obj.properties.contains("terrainEditor")) return false;
    const auto& te = obj.properties["terrainEditor"];
    return te.value("disableRender", false);
}
}

ViewportPanel::ViewportPanel() {}

ViewportPanel::~ViewportPanel() = default;

void ViewportPanel::setScene(Haruka::SceneManager* scene) {
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
    registerEditorTargetWithApp();
}

void ViewportPanel::setStatsPanelFromApp(class Application* app) {
    if (!statsPanel || !app) return;
    statsPanel->setVertexCount(app->getRenderedVertices());
    statsPanel->setDrawCalls(app->getRenderedDrawCalls());
    statsPanel->setTriangleCount(app->getRenderedTriangles());
    statsPanel->setTotalVertexCount(app->getTotalVertices());
    statsPanel->setTotalDrawCalls(app->getTotalDrawCalls());
    statsPanel->setTotalTriangleCount(app->getTotalTriangles());
    statsPanel->setVisibleChunkCount(0);
    statsPanel->setResidentChunkCount(0);
    statsPanel->setPendingChunkLoads(0);
    statsPanel->setPendingChunkEvictions(0);
    statsPanel->setResidentMemoryMB(0);
    statsPanel->setTrackedChunkCount(0);
    statsPanel->setMaxMemoryMB(0);
}

void ViewportPanel::registerEditorTargetWithApp() {
    Application* app = MotorInstance::getInstance().getApplication();
    if (app && renderTarget) {
        app->setEditorTarget(renderTarget.get());
        app->setEditorViewportSize(width, height);
    }
}

void ViewportPanel::shutdownGLResources() {
    // Liberar recursos GL del motor MIENTRAS el contexto sigue vivo; si se destruyeran
    // después (al morir los miembros), los glDelete* correrían sobre un contexto muerto.
    renderTarget.reset();               // FBO/RHI target del viewport
    if (ownedApplication) {
        ownedApplication.reset();       // ~Application → cleanup() libera recursos del motor
    }
    MotorInstance::getInstance().clear();
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
    
    // Registrar en MotorInstance cuando cambia el RenderTarget
    MotorInstance::getInstance().setRenderTarget(renderTarget.get());
    registerEditorTargetWithApp();
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

    auto& objects = currentScene->getObjectsMutable();
    for (size_t i = 0; i < objects.size(); i++) {
        auto& objPtr = objects[i];
        if (isRenderDisabledByEditor(*objPtr)) continue;
        
        // Bounding sphere (radio 0.5 * escala)
        glm::vec3 center = glm::vec3(objPtr->position);
        float radius = 0.5f * glm::length(glm::vec3(objPtr->scale));
        
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
    if (placementEnabled) return;   // en modo colocación el clic coloca, no selecciona

    ImGuiIO& io = ImGui::GetIO();

    if (isViewportHovered && !io.WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000000000000.0f);
        glm::mat4 view = camera->getViewMatrix();

        glm::vec3 rayDir = getRayFromMouse(proj, view);
        glm::vec3 rayOrigin = camera->position;

        selectedObjectIndex = getHoveredObjectIndex(rayOrigin, rayDir, proj, view);

        if (selectedObjectIndex >= 0 && currentScene) {
            auto& objects = currentScene->getObjectsMutable();
            if (selectedObjectIndex >= (int)objects.size()) return;
            auto& objPtr = objects[selectedObjectIndex];
            if (!objPtr) return;

            // Convierte dvec3 a vec3 para ImGuizmo
            glm::vec3 pos   = glm::vec3(objPtr->position);
            glm::vec3 rot   = glm::vec3(EditorUtil::rotationToEuler(objPtr->rotation));
            glm::vec3 scale = glm::vec3(objPtr->scale);

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
                objPtr->position = glm::dvec3(newPos);
                objPtr->rotation = EditorUtil::eulerToRotation(glm::dvec3(newRot));
                objPtr->scale    = glm::dvec3(newScale);
            }
        }
    }

    if (isDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left) && activeAxis != GizmoAxis::None) {
        ImGuiIO& io = ImGui::GetIO();
        auto objects = currentScene->getObjects();
        if (selectedObjectIndex < 0 || selectedObjectIndex >= (int)objects.size()) return;
        auto obj = currentScene->getObject(objects[selectedObjectIndex].name);
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
            glm::dvec3 euler = dragStartRot;
            if (activeAxis == GizmoAxis::X) euler.x += rotDelta;
            if (activeAxis == GizmoAxis::Y) euler.y += rotDelta;
            if (activeAxis == GizmoAxis::Z) euler.z += rotDelta;
            obj->rotation = EditorUtil::eulerToRotation(euler);
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

        auto objects = currentScene->getObjects();
        if (selectedObjectIndex < 0 || selectedObjectIndex >= (int)objects.size()) return;
        auto obj = currentScene->getObject(objects[selectedObjectIndex].name);
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
                auto obj = std::make_shared<Haruka::SceneObject>();
                obj->name = "Model_" + std::to_string(currentScene->getObjects().size());
                obj->type = "Model";
                obj->modelPath = assetPath;
                obj->position = glm::dvec3(0, 0, 0);
                obj->rotation = EditorUtil::eulerToRotation(glm::dvec3(0, 0, 0));
                obj->scale = glm::dvec3(1, 1, 1);
                
                if (commandHistory) {
                    commandHistory->execute(std::make_unique<AddObjectCommand>(currentScene, *obj));
                } else {
                    currentScene->addLoadedObject(obj);
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

    Application* app = MotorInstance::getInstance().getApplication();
    RenderTarget* motorTarget = MotorInstance::getInstance().getRenderTarget();

    // El motor renderiza la escena cada frame en este target (setEditorTarget).
    if (app && motorTarget == renderTarget.get()) {
        setStatsPanelFromApp(app);
        return;
    }

    // Sin motor activo: limpiar el target con un color de fondo estable.
    glBindFramebuffer(GL_FRAMEBUFFER, renderTarget->getFBO());
    glViewport(0, 0, width, height);
    glClearColor(0.05f, 0.05f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (statsPanel) {
        statsPanel->setVertexCount(0);
        statsPanel->setDrawCalls(0);
        statsPanel->setTriangleCount(0);
        statsPanel->setTotalVertexCount(0);
        statsPanel->setTotalDrawCalls(0);
        statsPanel->setTotalTriangleCount(0);
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

    // RUEDA = avanzar/retroceder en la dirección de vista (dolly), no zoom de FOV: cambiar el FOV
    // deforma la perspectiva y no te acerca a nada. El paso es PROPORCIONAL a lo lejos que estás
    // del objeto seleccionado (10 % de la distancia por muesca): con paso fijo, acercarse a un prop
    // desde 20 m tarda una eternidad y acercarse a un planeta desde 16 000 km es imposible. Al
    // aproximarte los pasos se acortan solos, así que nunca lo atraviesas de una muesca.
    if (camera && isViewportHovered && !ImGui::IsAnyItemActive()) {
        const float wheel = io.MouseWheel;
        if (wheel != 0.0f) {
            double reference = 10.0;   // sin selección: paso cómodo de escena pequeña
            if (currentScene && selectedObjectIndex >= 0) {
                const auto& all = currentScene->getAllObjects();
                if (selectedObjectIndex < (int)all.size() && all[selectedObjectIndex]) {
                    const double d = glm::length(all[selectedObjectIndex]->position - camera->position);
                    if (d > 1e-6) reference = d;
                }
            }
            const double step = reference * 0.10 * (double)wheel;
            camera->position += Haruka::WorldPos(glm::dvec3(camera->getFront()) * step);
        }
    }

    // WASD movement via SDL keyboard state
    if (camera && isViewportFocused && sdlWindow && !ImGui::IsAnyItemActive()) {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        glm::vec3 moveDelta(0.0f);
        if (keys[SDL_SCANCODE_W]) moveDelta += camera->getFront();
        if (keys[SDL_SCANCODE_S]) moveDelta -= camera->getFront();
        if (keys[SDL_SCANCODE_A]) moveDelta -= glm::normalize(glm::cross(camera->getFront(), camera->getUp()));
        if (keys[SDL_SCANCODE_D]) moveDelta += glm::normalize(glm::cross(camera->getFront(), camera->getUp()));
        if (keys[SDL_SCANCODE_SPACE]) moveDelta += camera->getUp();
        if (keys[SDL_SCANCODE_LSHIFT]) moveDelta -= camera->getUp();
        if (glm::length(moveDelta) > 0.0f) {
            moveDelta = glm::normalize(moveDelta) * camera->speed * deltaTime;
            camera->position += Haruka::WorldPos(moveDelta);
        }
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

    // --- Vista de depuración del planeta (editor → motor) --------------------------------
    // Colorea el planeta para VER cómo funciona el terreno: elevación, zonas del autor, bioma,
    // temperatura, humedad y CAPAS (cómo se aplica cada textura). El shader (biome.frag) hace el
    // resto con uDebug.x. Las capas se listan DINÁMICAS: las que el planeta declare en escena —
    // el selector tiene en cuenta TODAS las texturas (arena, hierba, roca…) y no una lista fija.
    {
        static const char* kBase[] = {"Normal", "Elevación", "Zonas", "Bioma",
                                      "Temperatura", "Humedad"};
        std::vector<std::pair<std::string, int>> items;
        for (int v = 0; v < 6; ++v) items.push_back({kBase[v], v});
        items.push_back({"Capas (todas)", 6});
        if (Application* app = MotorInstance::getInstance().getApplication()) {
            const auto layers = app->getPlanetTerrainLayerNames();
            for (size_t i = 0; i < layers.size(); ++i)
                items.push_back({"Capa: " + layers[i], 10 + (int)i});
        }
        std::string current;
        for (const auto& it : items) if (it.second == debugView) { current = it.first; break; }
        if (current.empty()) current = kBase[0];
        ImGui::SetNextItemWidth(130);
        if (ImGui::BeginCombo("Debug##planet", current.c_str())) {
            for (const auto& it : items) {
                if (ImGui::Selectable(it.first.c_str(), it.second == debugView)) {
                    debugView = it.second;
                    if (Application* app = MotorInstance::getInstance().getApplication())
                        app->setPlanetDebugView(it.second);
                }
            }
            ImGui::EndCombo();
        }
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
        (void*)(intptr_t)renderTarget->getColorTextureGL(),
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
    handlePlacement();

    ImGui::End();
}

void ViewportPanel::focusOnObject(int objectIndex) {
    if (!camera || !currentScene) return;
    const auto& all = currentScene->getAllObjects();
    if (objectIndex < 0 || objectIndex >= (int)all.size() || !all[objectIndex]) return;
    const Haruka::SceneObject& obj = *all[objectIndex];

    // El TAMAÑO lo decide el motor: aquí un planeta y una caja son el mismo SceneObject, y entre
    // uno y otra hay siete órdenes de magnitud. Sin motor, el encuadre no se intenta a ojo.
    Application* app = MotorInstance::getInstance().getApplication();
    if (!app) return;
    const double radius = app->getObjectBoundingRadius(obj);

    // Distancia que hace que una esfera de `radius` quepa entera: con el semiángulo θ del cono de
    // visión, la esfera es tangente al cono a d = r / sin(θ). Se toma el eje MÁS ESTRECHO (en un
    // viewport apaisado, el vertical) o el objeto se saldría por arriba y abajo. El margen deja
    // aire alrededor en vez de dejarlo tocando el borde.
    const double kMargin = 1.6;
    const double aspect  = (height > 0) ? (double)width / (double)height : 1.0;
    const double halfFovY = glm::radians((double)camera->zoom) * 0.5;
    const double halfFovX = std::atan(std::tan(halfFovY) * aspect);
    const double halfFov  = std::min(halfFovY, halfFovX);
    const double dist = radius / std::max(std::sin(halfFov), 1e-6) * kMargin;

    // Solo se mueve la POSICIÓN: conservar la orientación hace que el encuadre se sienta como
    // acercarse, no como que la escena salte a otra orientación. Colocarse a `dist` justo detrás
    // del objeto sobre el eje de vista lo deja centrado por construcción.
    const glm::dvec3 front = glm::normalize(glm::dvec3(camera->getFront()));
    camera->position = obj.position - front * dist;

    // El motor recalcula el near plane por frame desde la altitud, así que encuadrar un planeta
    // desde 16 000 km no rompe la precisión de profundidad.
}

void ViewportPanel::onUpdate(float deltaTime) {
    updateCameraFromInput(deltaTime);

    Application* app = MotorInstance::getInstance().getApplication();
    if (app && camera) {
        // El motor renderiza con SU cámara interna (_camera); sincronizar la del editor
        // cada frame para que el viewport muestre la navegación del editor.
        if (Camera* appCam = app->getCamera()) {
            appCam->position = camera->position;
            appCam->orientation = camera->orientation;
            appCam->zoom = camera->zoom;
            appCam->aspectRatio = (float)width / (float)height;
        }
        // La vista de depuración se re-empuja cada frame: si el motor reinició su PlanetarySystem
        // (recarga de escena) el modo vuelve a quedar aplicado.
        if (debugView != 0) app->setPlanetDebugView(debugView);
    }

    if (ownedApplication) {
        ownedApplication->renderFrame();
    } else if (app) {
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

// ===========================================================================
// Colocación de props y herramientas de malla
// ===========================================================================
// Modo de colocación del editor: el cursor proyecta sobre el PLANETA un indicador translúcido
// (círculo o cuadrado) del radio de afectación y un clic ejecuta la acción. Todo el cálculo es en
// DOBLE precisión: las posiciones de mundo viven a ~1.5e8 y un float pierde ~10 m ahí, suficiente
// para que un indicador de 50 m baile fuera del punto del ratón.
void ViewportPanel::beginPlacement(const std::string& modelPath, const std::string& label,
                                   float radius, bool circle, int action, const std::string& layer) {
    placementModel  = modelPath;
    placementLabel  = label;
    placementRadius = std::max(0.1f, radius);
    placementCircle = circle;
    placementAction = action;
    placementLayer  = layer;
    placementHasGround = false;
    placementEnabled   = true;
}

void ViewportPanel::handlePlacement() {
    if (!placementEnabled) return;
    if (!currentScene || !camera) { placementEnabled = false; return; }

    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
        ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        placementEnabled = false;
        return;
    }

    Application* app = MotorInstance::getInstance().getApplication();
    Haruka::PlanetarySystem* ps = app ? app->getPlanetarySystem() : nullptr;
    if (!ps) { placementEnabled = false; return; }

    glm::dvec3 center; double radius = 0.0;
    if (!ps->getActivePlanet(center, radius)) { placementHasGround = false; return; }

    // Rayo del ratón (dirección con las matrices de float del viewport, como el picking normal).
    glm::mat4 projF = glm::perspective(glm::radians(45.0f), (float)width / (float)height,
                                       0.1f, 1000000000000.0f);
    glm::mat4 viewF = camera->getViewMatrix();
    glm::vec3 dirF = getRayFromMouse(projF, viewF);
    glm::dvec3 origin(camera->position);
    glm::dvec3 dir = glm::normalize(glm::dvec3(dirF));
    if (glm::dot(dir, dir) < 1e-12) return;

    // Intersección rayo-esfera con el planeta, en doble.
    glm::dvec3 oc = origin - center;
    double b = glm::dot(oc, dir);
    double c = glm::dot(oc, oc) - radius * radius;
    double disc = b * b - c;
    if (disc <= 0.0) { placementHasGround = false; return; }
    double t = -b - std::sqrt(disc);
    if (t < 0.0) t = -b + std::sqrt(disc);
    if (t < 0.0) { placementHasGround = false; return; }
    glm::dvec3 dirN = glm::normalize(origin + dir * t - center);

    // Ancla al terreno REAL (la superficie que pisa el jugador), no a la esfera de referencia.
    float elevKm = 0.0f;
    ps->groundHeightKmAtDir(dirN, elevKm);
    placementGround = center + dirN * (radius + (double)elevKm * 1000.0);
    placementHasGround = true;

    drawPlacementIndicator();

    if (isViewportHovered && !io.WantCaptureMouse &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        executePlacement();
    }
}

void ViewportPanel::drawPlacementIndicator() {
    if (!placementHasGround || !camera) return;
    Application* app = MotorInstance::getInstance().getApplication();
    Haruka::PlanetarySystem* ps = app ? app->getPlanetarySystem() : nullptr;
    if (!ps) return;
    glm::dvec3 center; double radius = 0.0;
    if (!ps->getActivePlanet(center, radius)) return;

    // Base ortonormal tangente al planeta en el punto anclado.
    glm::dvec3 up = glm::normalize(placementGround - center);
    glm::dvec3 ref = std::abs(up.y) < 0.99 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0);
    glm::dvec3 right = glm::normalize(glm::cross(up, ref));
    glm::dvec3 fwd = glm::cross(right, up);

    // Vista y proyección en DOBLE (el near dinámico de la cámara, no el fijo del picking).
    glm::dvec3 campos = camera->position;
    glm::dvec3 cfront(camera->getFront());
    glm::dvec3 cup(camera->getUp());
    glm::dmat4 viewD = glm::lookAt(campos, campos + cfront, cup);
    const double f = 1.0 / std::tan(glm::radians((double)camera->zoom) * 0.5);
    const double aspect = (height > 0) ? (double)width / (double)height : 1.0;
    glm::dmat4 projD(0.0);
    projD[0][0] = f / aspect;
    projD[1][1] = f;
    projD[2][3] = -1.0;
    projD[3][2] = (double)camera->getNearPlane();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    std::vector<ImVec2> pts;
    const int N = 48;
    pts.reserve(N);
    for (int i = 0; i < N; ++i) {
        const double ang = 2.0 * glm::pi<double>() * (double)i / (double)N;
        glm::dvec3 off;
        if (placementCircle) {
            off = right * (std::cos(ang) * placementRadius)
                + fwd * (std::sin(ang) * placementRadius);
        } else {
            // Cuadrado: lado = 2·radio, alineado con la base tangente del planeta.
            const double sx = std::cos(ang) >= 0.0 ? 1.0 : -1.0;
            const double sy = std::sin(ang) >= 0.0 ? 1.0 : -1.0;
            off = right * (sx * placementRadius) + fwd * (sy * placementRadius);
        }
        glm::dvec4 clip = projD * viewD * glm::dvec4(placementGround + off, 1.0);
        if (clip.w <= 0.0) continue;
        glm::dvec3 ndc = glm::dvec3(clip) / clip.w;
        const float sx2 = viewportMin.x + (float)((ndc.x * 0.5 + 0.5) * (viewportMax.x - viewportMin.x));
        const float sy2 = viewportMin.y + (float)((1.0 - (ndc.y * 0.5 + 0.5)) * (viewportMax.y - viewportMin.y));
        pts.push_back(ImVec2(sx2, sy2));
    }
    if (pts.size() >= 3) {
        dl->AddConvexPolyFilled(pts.data(), (int)pts.size(), IM_COL32(0, 200, 255, 36));
        dl->AddPolyline(pts.data(), (int)pts.size(), IM_COL32(0, 200, 255, 200), 0, 2.0f);
    }

    glm::dvec4 clipC = projD * viewD * glm::dvec4(placementGround, 1.0);
    if (clipC.w > 0.0) {
        glm::dvec3 ndc = glm::dvec3(clipC) / clipC.w;
        const float cx = viewportMin.x + (float)((ndc.x * 0.5 + 0.5) * (viewportMax.x - viewportMin.x));
        const float cy = viewportMin.y + (float)((1.0 - (ndc.y * 0.5 + 0.5)) * (viewportMax.y - viewportMin.y));
        const std::string label = placementLabel.empty() ? "Colocar" : placementLabel;
        dl->AddText(ImVec2(cx + 8, cy - 20), IM_COL32(255, 255, 255, 255), label.c_str());
    }
}

void ViewportPanel::executePlacement() {
    if (!placementHasGround) return;
    Application* app = MotorInstance::getInstance().getApplication();
    if (!app) return;

    if (placementAction == 0) {           // Colocar un prop (objeto dinámico con modelo)
        if (placementModel.empty()) return;
        auto obj = std::make_shared<Haruka::SceneObject>();
        int n = currentScene ? (int)currentScene->getObjects().size() : 0;
        obj->name = "Prop_" + std::to_string(n);
        obj->type = "Model";
        obj->objectType = Haruka::classifyObjectType("Model");
        obj->modelPath = placementModel;
        obj->position = placementGround;
        obj->rotation = EditorUtil::eulerToRotation(glm::dvec3(0.0, 0.0, 0.0));
        obj->scale = glm::dvec3(1.0);
        if (!obj->properties.is_object()) obj->properties = nlohmann::json::object();
        obj->properties["layer"] = placementLayer.empty() ? "Props" : placementLayer;
        obj->properties["prop"] = {{"radius", placementRadius}, {"label", placementLabel}};
        static std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> seedDist(-100000, 100000);
        const int treeSeed = seedDist(rng);
        obj->properties["treeSeed"] = treeSeed;

        // Semilla -> texturas al instante: el material se hornea del grafo de árbol
        // (estilo anime: nudos + ramas; copa = follaje) sin abrir el editor de nodos.
        const bool foliage = placementModel.find("leaf") != std::string::npos;
        auto res = Haruka::Tools::ProcGraph::bakeTreeTextures(
            treeSeed, foliage, Haruka::AssetPaths::projectTextures(), obj->name, 256);
        if (res.ok) {
            auto mat = std::make_shared<Haruka::MaterialComponent>();
            mat->name = obj->name + "_Material";
            mat->albedo = glm::vec3(1.0f);
            mat->textures["albedo"] = res.albedo;
            mat->textures["normal"] = res.normal;
            mat->textures["roughness"] = res.roughness;
            mat->textures["ao"] = res.ao;
            obj->material = mat;
        } else {
            std::cerr << "[viewport] bake tree textures failed for " << obj->name << "\n";
        }

        if (commandHistory) {
            commandHistory->execute(std::make_unique<AddObjectCommand>(currentScene, *obj));
        } else if (currentScene) {
            currentScene->addLoadedObject(obj);
        }
        if (currentScene) {
            int idx = (int)currentScene->getObjects().size() - 1;
            selectedObjectIndex = idx;
        }
    } else if (placementAction == 1) {    // Levantar terreno (edita la MALLA del planeta)
        app->editTerrain(placementGround, placementRadius, 20.0, false);
    } else if (placementAction == 2) {    // Excavar terreno
        app->editTerrain(placementGround, placementRadius, 20.0, true);
    } else if (placementAction == 3) {    // Allanar al nivel del mar
        app->levelTerrain(placementGround, placementRadius, 0.0);
    }
}
