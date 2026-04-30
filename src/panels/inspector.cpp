#include "inspector.h"

#include "commands/scene_commands.h"
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
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Object not found in scene");
        ImGui::End();
        return;
    }

    // Mostrar nombre
    char nameBuf[256] = {0};
    strncpy(nameBuf, obj->name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        obj->name = nameBuf;
    }

    ImGui::Text("Type: %s", obj->type.c_str());
    int renderLayer = obj->renderLayer;
    if (ImGui::SliderInt("Render Layer", &renderLayer, 1, 5)) {
        obj->renderLayer = renderLayer;
    }
    ImGui::Separator();

    // Transform
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        {
            float pos[3] = { (float)obj->position.x, (float)obj->position.y, (float)obj->position.z };
            if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                obj->position = glm::dvec3(pos[0], pos[1], pos[2]);
            }
        }

        {
            float rot[3] = { (float)obj->rotation.x, (float)obj->rotation.y, (float)obj->rotation.z };
            if (ImGui::DragFloat3("Rotation", rot, 1.0f)) {
                obj->rotation = glm::dvec3(rot[0], rot[1], rot[2]);
            }
        }

        {
            float scale[3] = { (float)obj->scale.x, (float)obj->scale.y, (float)obj->scale.z };
            if (ImGui::DragFloat3("Scale", scale, 0.1f)) {
                obj->scale = glm::dvec3(scale[0], scale[1], scale[2]);
            }
        }
    }

    // Propiedades específicas por tipo
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
    } 
    else if (obj->type == "Model") {
        if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
            char pathBuf[512] = {0};
            strncpy(pathBuf, obj->modelPath.c_str(), sizeof(pathBuf) - 1);
            if (ImGui::InputText("Model Path", pathBuf, sizeof(pathBuf))) {
                obj->modelPath = pathBuf;
            }
        }
    }

    if (playMode) ImGui::EndDisabled();

    ImGui::End();
}