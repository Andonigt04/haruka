#include "inspector.h"

#include "editor/commands/scene_commands.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include "core/components/transform_component.h"
#include "core/components/mesh_renderer_component.h"
#include "core/components/script_component.h"


void InspectorPanel::setScene(Haruka::Scene* scene) {
    currentScene = scene;
}

void InspectorPanel::setSelectedObjectIndex(int index) {
    selectedObjectIndex = index;
}

void InspectorPanel::setCommandHistory(CommandHistory* history) {
    commandHistory = history;
}

void InspectorPanel::onImGuiRender() {
    ImGui::Begin("Inspector");

    if (selectedObjectIndex < 0 || !currentScene) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }

    const auto& objects = currentScene->getObjects();
    if (selectedObjectIndex >= (int)objects.size()) {
        ImGui::Text("Invalid object");
        ImGui::End();
        return;
    }

    auto obj = currentScene->getObject(objects[selectedObjectIndex].name);
    if (!obj) {
        ImGui::Text("Invalid object");
        ImGui::End();
        return;
    }

    char nameBuf[256];
    strncpy(nameBuf, obj->name.c_str(), sizeof(nameBuf));
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        obj->name = nameBuf;
    }

    ImGui::Text("Type: %s", obj->type.c_str());
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        bool changed = false;

        {
            float pos[3] = { (float)obj->position.x, (float)obj->position.y, (float)obj->position.z };
            if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                obj->position = glm::dvec3(pos[0], pos[1], pos[2]);
                changed = true;
            }
        }
        if (ImGui::IsItemActivated()) {
            editingPosition = true;
            editStartPosition = obj->position;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && editingPosition) {
            editingPosition = false;
            if (commandHistory) {
                glm::vec3 newPos = obj->position;
                glm::vec3 newRot = obj->rotation;
                glm::vec3 newScale = obj->scale;

                // restore old state before creating command
                obj->position = editStartPosition;

                commandHistory->execute(std::make_unique<TransformObjectCommand>(
                    currentScene, obj->name, newPos, newRot, newScale
                ));
            }
        }

        {
            float rot[3] = { (float)obj->rotation.x, (float)obj->rotation.y, (float)obj->rotation.z };
            if (ImGui::DragFloat3("Rotation", rot, 1.0f)) {
                obj->rotation = glm::dvec3(rot[0], rot[1], rot[2]);
                changed = true;
            }
        }
        if (ImGui::IsItemActivated()) {
            editingRotation = true;
            editStartRotation = obj->rotation;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && editingRotation) {
            editingRotation = false;
            if (commandHistory) {
                glm::vec3 newPos = obj->position;
                glm::vec3 newRot = obj->rotation;
                glm::vec3 newScale = obj->scale;

                obj->rotation = editStartRotation;

                commandHistory->execute(std::make_unique<TransformObjectCommand>(
                    currentScene, obj->name, newPos, newRot, newScale
                ));
            }
        }

        {
            float scale[3] = { (float)obj->scale.x, (float)obj->scale.y, (float)obj->scale.z };
            if (ImGui::DragFloat3("Scale", scale, 0.1f)) {
                obj->scale = glm::dvec3(scale[0], scale[1], scale[2]);
                changed = true;
            }
        }
        if (ImGui::IsItemActivated()) {
            editingScale = true;
            editStartScale = obj->scale;
        }
        if (ImGui::IsItemDeactivatedAfterEdit() && editingScale) {
            editingScale = false;
            if (commandHistory) {
                glm::vec3 newPos = obj->position;
                glm::vec3 newRot = obj->rotation;
                glm::vec3 newScale = obj->scale;

                obj->scale = editStartScale;

                commandHistory->execute(std::make_unique<TransformObjectCommand>(
                    currentScene, obj->name, newPos, newRot, newScale
                ));
            }
        }
    }

    if (playMode) ImGui::BeginDisabled();

    if (obj->type == "Light") {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            glm::vec3 colorFloat = glm::vec3(obj->color);
            if (ImGui::ColorEdit3("Color", &colorFloat.x)) {
                obj->color = glm::dvec3(colorFloat);
            }
            
            float intensityFloat = (float)obj->intensity;
            if (ImGui::DragFloat("Intensity", &intensityFloat, 0.1f, 0.0f, 100.0f)) {
                obj->intensity = (double)intensityFloat;
            }
        }
    } else if (obj->type == "Model") {
        if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
            char pathBuf[512];
            strncpy(pathBuf, obj->modelPath.c_str(), sizeof(pathBuf));
            if (ImGui::InputText("Model Path", pathBuf, sizeof(pathBuf))) {
                obj->modelPath = pathBuf;
            }
        }
    }

    if (playMode) ImGui::EndDisabled();
    
    /**
    if (ImGui::Button("Add Component")) {
        // Mostrar popup con tipos disponibles
        ImGui::OpenPopup("AddComponentPopup");
    }
    if (ImGui::BeginPopup("AddComponentPopup")) {
        if (ImGui::MenuItem("Transform")) {
            obj->components.push_back(std::make_shared<Haruka::TransformComponent>());
        }
        if (ImGui::MenuItem("MeshRenderer")) {
            obj->components.push_back(std::make_shared<Haruka::MeshRendererComponent>());
        }
        if (ImGui::MenuItem("Script")) {
            obj->components.push_back(std::make_shared<Haruka::ScriptComponent>());
        }
        ImGui::EndPopup();
    }*/

    if (obj) {
        /**
        for (auto& comp : obj->components) {
            ImGui::Separator();
            ImGui::Text("%s", comp->getType().c_str());
            comp->renderInspector();
            ImGui::SameLine();
            if (ImGui::Button(("Remove##" + comp->getType()).c_str())) {
                auto it = std::find(obj->components.begin(), obj->components.end(), comp);
                if (it != obj->components.end()) {
                    obj->components.erase(it);
                    break;
                }
            }
        }

        // Botón para añadir componentes
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponentPopup");
        }
        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (ImGui::MenuItem("Transform")) {
                obj->components.push_back(std::make_shared<Haruka::TransformComponent>());
            }
            if (ImGui::MenuItem("MeshRenderer")) {
                obj->components.push_back(std::make_shared<Haruka::MeshRendererComponent>());
            }
            if (ImGui::MenuItem("Script")) {
                obj->components.push_back(std::make_shared<Haruka::ScriptComponent>());
            }
            // Aquí puedes añadir más tipos
            ImGui::EndPopup();
        }*/

        // Botón para guardar como prefab
        ImGui::Separator();
        if (ImGui::Button("Save as Prefab", ImVec2(-1, 0))) {
            ImGui::OpenPopup("SavePrefabModal");
        }

        if (ImGui::BeginPopupModal("SavePrefabModal")) {
            static char prefabName[128] = "";
            ImGui::InputText("Prefab Name", prefabName, sizeof(prefabName));

            if (ImGui::Button("Save", ImVec2(100, 0))) {
                // TODO: Implementar guardado de prefab desde ProjectBrowser
                // prefabsPanel->savePrefab(*obj, prefabName);
                std::cout << "Prefab save not implemented yet: " << prefabName << std::endl;
                memset(prefabName, 0, sizeof(prefabName));
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::End();
}