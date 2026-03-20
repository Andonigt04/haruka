#define GLM_ENABLE_EXPERIMENTAL
#include "viewport.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/intersect.hpp>
#include <algorithm>
#include "editor/commands/scene_commands.h"
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>

ViewportPanel::ViewportPanel() {}

ViewportPanel::~ViewportPanel() = default;

void ViewportPanel::setScene(Haruka::Scene* scene) {
    currentScene = scene;
}

void ViewportPanel::setCamera(Camera* cam) {
    camera = cam;
}

void ViewportPanel::recreateRenderTarget() {
    renderTarget = std::make_unique<RenderTarget>(width, height);
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
    if (!currentScene) return -1;

    float closestDist = FLT_MAX;
    int closestIdx = -1;

    const auto& objects = currentScene->getObjects();
    for (size_t i = 0; i < objects.size(); i++) {
        const auto& obj = objects[i];
        
        // Bounding sphere (radio 0.5 * escala)
        glm::vec3 center = glm::vec3(obj.position);
        float radius = 0.5f * glm::length(glm::vec3(obj.scale));
        
        float distance;
        if (glm::intersectRaySphere(rayOrigin, rayDir, center, radius, distance)) {
            if (distance < closestDist) {
                closestDist = distance;
                closestIdx = (int)i;
            }
        }
    }

    return closestIdx;
}

void ViewportPanel::handleGizmoInput() {
    if (playMode) return;

    ImGuiIO& io = ImGui::GetIO();

    if (isViewportHovered && !io.WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);
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
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

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
            glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);
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
    if (!renderTarget || !camera) return;

    renderVertex_count = 0;
    renderDraw_calls = 0;

    renderTarget->bindForWriting();
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!sceneShader) {
        sceneShader = std::make_unique<Shader>("shaders/simple.vert", "shaders/light_cube.frag");
    }

    if (!cubeMesh) {
        std::vector<glm::vec3> verts, norms;
        std::vector<unsigned int> indices;
        PrimitiveShapes::createCube(1.0f, verts, norms, indices);
        cubeMesh = std::make_unique<SimpleMesh>(verts, norms, indices);
    }

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);
    glm::mat4 view = camera->getViewMatrix();

    renderGrid(view, proj);
    renderDraw_calls++;

    sceneShader->use();
    sceneShader->setMat4("projection", proj);
    sceneShader->setMat4("view", view);

    if (currentScene) {
        const auto& objects = currentScene->getObjects();
        for (size_t i = 0; i < objects.size(); i++) {
            const auto& obj = objects[i];
            
            glm::dmat4 model = glm::dmat4(1.0f);
            model = glm::translate(model, obj.position);
            model = glm::rotate(model, glm::radians(obj.rotation.x), glm::dvec3(1, 0, 0));
            model = glm::rotate(model, glm::radians(obj.rotation.y), glm::dvec3(0, 1, 0));
            model = glm::rotate(model, glm::radians(obj.rotation.z), glm::dvec3(0, 0, 1));
            model = glm::scale(model, obj.scale);

            sceneShader->setMat4("model", model);
            
            if ((int)i == selectedObjectIndex) {
                sceneShader->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 0.0f));
            } else {
                sceneShader->setVec3("lightColor", glm::vec3(0.8f, 0.8f, 0.8f));
            }

            if (obj.type == "Model" && !obj.modelPath.empty()) {
                Model* modelPtr = getOrLoadModel(obj.modelPath);
                if (modelPtr) {
                    modelPtr->Draw(*sceneShader);
                    renderVertex_count += 1000; 
                } else {
                    cubeMesh->draw();
                    renderVertex_count += 24; // Cubo = 24 vértices
                }
            } else {
                cubeMesh->draw();
                renderVertex_count += 24;
            }
            renderDraw_calls++;
        }
    }

    renderGizmoAxes(view, proj);
    renderDraw_calls += 3; // 3 ejes
    
    renderTarget->unbind();
    
    if (statsPanel) {
        statsPanel->setVertexCount(renderVertex_count);
        statsPanel->setDrawCalls(renderDraw_calls);
        statsPanel->setTriangleCount(renderVertex_count / 3);
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
    if (isViewportFocused && glfwWindow && !ImGui::IsAnyItemActive()) {
        camera->processInput(glfwWindow, deltaTime);
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
    renderScene();
}

void ViewportPanel::renderGizmoAxes(const glm::mat4& view, const glm::mat4& proj) {
    if (!currentScene || selectedObjectIndex < 0) return;

    const auto& objects = currentScene->getObjects();
    if (selectedObjectIndex >= (int)objects.size()) return;

    const auto& obj = objects[selectedObjectIndex];
    if (!sceneShader) return;

    // Desactivar depth test para que gizmos siempre estén visibles
    glDisable(GL_DEPTH_TEST);
    glLineWidth(3.0f);

    if (gizmoVAO == 0) {
        glGenVertexArrays(1, &gizmoVAO);
        glGenBuffers(1, &gizmoVBO);

        glBindVertexArray(gizmoVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gizmoVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 18, nullptr, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glDisableVertexAttribArray(1);
        glVertexAttrib3f(1, 0.0f, 0.0f, 1.0f);
        glDisableVertexAttribArray(2);
        glVertexAttrib2f(2, 0.0f, 0.0f);
        glDisableVertexAttribArray(3);
        glVertexAttrib3f(3, 1.0f, 0.0f, 0.0f);
        glDisableVertexAttribArray(4);
        glVertexAttrib3f(4, 0.0f, 1.0f, 0.0f);

        glBindVertexArray(0);
    }

    glm::dmat4 model = glm::translate(glm::dmat4(1.0f), obj.position);

    sceneShader->use();
    sceneShader->setMat4("projection", proj);
    sceneShader->setMat4("view", view);
    sceneShader->setMat4("model", model);

    auto drawAxis = [&](glm::vec3 a, glm::vec3 b, glm::vec3 color) {
        float verts[6] = { a.x, a.y, a.z, b.x, b.y, b.z };

        glBindVertexArray(gizmoVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gizmoVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        sceneShader->setVec3("lightColor", color);
        glDrawArrays(GL_LINES, 0, 2);
    };

    float axisLen = 2.0f; // Más largos
    drawAxis({0,0,0}, {axisLen,0,0}, {1,0,0}); // X rojo
    drawAxis({0,0,0}, {0,axisLen,0}, {0,1,0}); // Y verde
    drawAxis({0,0,0}, {0,0,axisLen}, {0,0,1}); // Z azul

    glBindVertexArray(0);

    // Restaurar estado
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
}

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
    if (path.empty()) return nullptr;

    auto it = loadedModels.find(path);
    if (it != loadedModels.end()) {
        return it->second.get();
    }

    try {
        auto model = std::make_unique<Model>(path);
        Model* ptr = model.get();
        loadedModels[path] = std::move(model);
        std::cout << "Model loaded: " << path << std::endl;
        return ptr;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load model " << path << ": " << e.what() << std::endl;
        return nullptr;
    }
}

void ViewportPanel::renderGrid(const glm::mat4& view, const glm::mat4& proj) {
    if (!showGrid) return;
    if (!sceneShader) return;

    if (gridVAO == 0) {
        const int gridSize = 1000000;
        const float gridSpacing = 1.0f;
        std::vector<float> gridVertices;

        // Líneas en X
        for (int i = -gridSize; i <= gridSize; ++i) {
            float offset = i * gridSpacing;
            gridVertices.push_back(-gridSize * gridSpacing); gridVertices.push_back(0.0f); gridVertices.push_back(offset);
            gridVertices.push_back( gridSize * gridSpacing); gridVertices.push_back(0.0f); gridVertices.push_back(offset);
        }

        // Líneas en Z
        for (int i = -gridSize; i <= gridSize; ++i) {
            float offset = i * gridSpacing;
            gridVertices.push_back(offset); gridVertices.push_back(0.0f); gridVertices.push_back(-gridSize * gridSpacing);
            gridVertices.push_back(offset); gridVertices.push_back(0.0f); gridVertices.push_back( gridSize * gridSpacing);
        }

        glGenVertexArrays(1, &gridVAO);
        glGenBuffers(1, &gridVBO);

        glBindVertexArray(gridVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glDisableVertexAttribArray(1);
        glVertexAttrib3f(1, 0.0f, 0.0f, 1.0f);
        glDisableVertexAttribArray(2);
        glVertexAttrib2f(2, 0.0f, 0.0f);
        glDisableVertexAttribArray(3);
        glVertexAttrib3f(3, 1.0f, 0.0f, 0.0f);
        glDisableVertexAttribArray(4);
        glVertexAttrib3f(4, 0.0f, 1.0f, 0.0f);

        glBindVertexArray(0);
    }

    sceneShader->use();
    sceneShader->setMat4("projection", proj);
    sceneShader->setMat4("view", view);
    sceneShader->setMat4("model", glm::mat4(1.0f));
    sceneShader->setVec3("lightColor", glm::vec3(0.3f, 0.3f, 0.3f)); // Grid gris oscuro

    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, (1000000 * 2 + 1) * 4); // Cambiar de 20 a 200
    glBindVertexArray(0);
}

void ViewportPanel::renderGizmoImGuizmo()
{
    if (!currentScene || selectedObjectIndex < 0) return;

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
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);

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