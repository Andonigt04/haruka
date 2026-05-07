#define GLM_ENABLE_EXPERIMENTAL
#include "inspector.h"

#include "commands/scene_commands.h"
#include <algorithm>
#include <iostream>
#include <cstring>
#include "core/components/transform_component.h"
#include "core/components/mesh_renderer_component.h"
#include "core/components/script_component.h"
#include <glm/gtx/quaternion.hpp>

namespace {
bool drawVec3Control(const char* label, glm::dvec3& value, float speed, const glm::dvec3& resetValue) {
    bool changed = false;
    ImGui::PushID(label);
    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 100.0f);
    ImGui::TextUnformatted(label);
    ImGui::NextColumn();

    float v[3] = { (float)value.x, (float)value.y, (float)value.z };
    if (ImGui::DragFloat3("##v", v, speed)) {
        value = glm::dvec3(v[0], v[1], v[2]);
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset")) {
        value = resetValue;
        changed = true;
    }

    ImGui::Columns(1);
    ImGui::PopID();
    return changed;
}
}


void InspectorPanel::setScene(Haruka::SceneManager* scene) {
    currentScene = scene;
}

void InspectorPanel::setSelectedObjectIndex(int index) {
    selectedObjectIndex = index;
}

void InspectorPanel::setCommandHistory(CommandHistory* history) {
    commandHistory = history;
}

void InspectorPanel::drawNoSelectionState() const {
    ImGui::TextUnformatted("No object selected");
    ImGui::Spacing();
    ImGui::TextDisabled("Select an object from Scene Hierarchy to inspect/edit.");
}

bool InspectorPanel::drawObjectHeader(Haruka::SceneObject& obj) {
    bool changed = false;
    char nameBuf[256] = {0};
    strncpy(nameBuf, obj.name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        obj.name = nameBuf;
        changed = true;
    }

    ImGui::BeginDisabled();
    ImGui::InputText("Type", const_cast<char*>(obj.type.c_str()), obj.type.size() + 1, ImGuiInputTextFlags_ReadOnly);
    ImGui::EndDisabled();
    return changed;
}

bool InspectorPanel::drawTransformSection(Haruka::SceneObject& obj) {
    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return false;
    bool changed = false;
    changed |= drawVec3Control("Position", obj.position, 0.05f, glm::dvec3(0.0));
    glm::dvec3 rotationEuler = glm::degrees(glm::eulerAngles(obj.rotation));
    if (drawVec3Control("Rotation", rotationEuler, 0.25f, glm::dvec3(0.0))) {
        obj.rotation = glm::quat(glm::radians(rotationEuler));
        changed = true;
    }
    changed |= drawVec3Control("Scale", obj.scale, 0.05f, glm::dvec3(1.0));
    return changed;
}

bool InspectorPanel::drawVisualSection(Haruka::SceneObject& obj) {
    if (!ImGui::CollapsingHeader("Visual", ImGuiTreeNodeFlags_DefaultOpen)) return false;
    bool changed = false;

    if (obj.type == "Model" || !obj.modelPath.empty()) {
        char pathBuf[512] = {0};
        strncpy(pathBuf, obj.modelPath.c_str(), sizeof(pathBuf) - 1);
        if (ImGui::InputText("Model Path", pathBuf, sizeof(pathBuf))) {
            obj.modelPath = pathBuf;
            changed = true;
        }
    }

    bool castLight = obj.flags.castLight;
    if (ImGui::Checkbox("Cast Light", &castLight)) {
        obj.flags.castLight = castLight;
        changed = true;
    }

    if (castLight) {
        glm::vec3 colorFloat = glm::vec3(obj.color);
        if (ImGui::ColorEdit3("Light Color", &colorFloat.x)) {
            obj.color = glm::dvec3(colorFloat);
            changed = true;
        }

        float intensityFloat = (float)obj.intensity;
        if (ImGui::DragFloat("Intensity", &intensityFloat, 0.1f, 0.0f, 1000.0f)) {
            obj.intensity = (double)intensityFloat;
            changed = true;
        }
    }

    return changed;
}

bool InspectorPanel::drawMetadataSection(Haruka::SceneObject& obj) {
    if (!ImGui::CollapsingHeader("Metadata", ImGuiTreeNodeFlags_DefaultOpen)) return false;
    bool changed = false;

    int renderLayer = obj.renderLayer;
    if (ImGui::DragInt("Render Layer", &renderLayer, 1.0f, 0, 64)) {
        obj.renderLayer = renderLayer;
        changed = true;
    }

    ImGui::SeparatorText("Data Blocks");
    ImGui::Text("properties: %s", obj.properties.is_null() ? "empty" : "set");
    ImGui::Text("terrainSettings: %s", obj.terrainSettings.has_value() ? "set" : "empty");
    ImGui::Text("streamingSettings: %s", obj.streamingSettings.has_value() ? "set" : "empty");
    ImGui::Text("lodSettings: %s", obj.lodSettings.has_value() ? "set" : "empty");

    return changed;
}

void InspectorPanel::onImGuiRender() {
    ImGui::Begin("Inspector");

    if (selectedObjectIndex < 0 || !currentScene) {
        drawNoSelectionState();
        ImGui::End();
        return;
    }

    const auto& objects = currentScene->getAllObjects();
    if (selectedObjectIndex >= (int)objects.size() || !objects[selectedObjectIndex]) {
        ImGui::TextUnformatted("Invalid object selection");
        ImGui::End();
        return;
    }

    auto objPtr = currentScene->getObjectByName(objects[selectedObjectIndex]->name);
    if (!objPtr) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Object not found in scene");
        ImGui::End();
        return;
    }

    Haruka::SceneObject& obj = *objPtr;
    bool sceneChanged = false;

    sceneChanged |= drawObjectHeader(obj);
    ImGui::Separator();

    if (playMode) ImGui::BeginDisabled();
    sceneChanged |= drawTransformSection(obj);
    sceneChanged |= drawVisualSection(obj);
    sceneChanged |= drawMetadataSection(obj);
    if (playMode) ImGui::EndDisabled();

    if (sceneChanged && onSceneChanged) {
        onSceneChanged();
    }

    ImGui::End();
}