#include "inspector.h"
#include "editor/commands/scene_commands.h"
#include <cstring>

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
        ImGui::DragFloat3("Position", &obj->position.x, 0.1f);
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

        ImGui::DragFloat3("Rotation", &obj->rotation.x, 1.0f);
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

        ImGui::DragFloat3("Scale", &obj->scale.x, 0.1f);
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

    if (obj->type == "Light") {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::ColorEdit3("Color", &obj->color.x);
            ImGui::DragFloat("Intensity", &obj->intensity, 0.1f, 0.0f, 100.0f);
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

    ImGui::End();
}