#include "material_editor.h"
#include <iostream>

void MaterialEditorPanel::setSelectedObject(const std::string& objectName) {
    selectedObjectName = objectName;
    if (!currentScene) return;
    
    auto obj = currentScene->getObject(objectName);
    if (obj) {
        if (!obj->material) {
            obj->material = std::make_shared<Haruka::MaterialComponent>();
            obj->material->name = objectName + "_Material";
        }
        editingMaterial = obj->material.get();
    }
}

void MaterialEditorPanel::onImGuiRender() {
    ImGui::SetNextWindowSize(ImVec2(450, 700), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Material Editor")) {
        ImGui::End();
        return;
    }
    
    if (!currentScene || selectedObjectName.empty() || !editingMaterial) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Select an object with Material component");
        ImGui::End();
        return;
    }
    
    ImGui::Text("Material: %s", editingMaterial->name.c_str());
    ImGui::Separator();
    
    strncpy(shaderBuffer, editingMaterial->shaderPath.c_str(), sizeof(shaderBuffer));
    if (ImGui::InputText("Shader Path", shaderBuffer, sizeof(shaderBuffer))) {
        editingMaterial->shaderPath = shaderBuffer;
    }
    
    ImGui::Separator();
    
    if (ImGui::CollapsingHeader("Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (auto& [texType, texPath] : editingMaterial->textures) {
            char texBuf[256] = {};
            strncpy(texBuf, texPath.c_str(), sizeof(texBuf));
            
            if (ImGui::InputText(texType.c_str(), texBuf, sizeof(texBuf))) {
                editingMaterial->textures[texType] = texBuf;
            }
        }
    }
    
    ImGui::Separator();
    
    if (ImGui::CollapsingHeader("PBR Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Albedo", &editingMaterial->albedo.x);
        ImGui::SliderFloat("Metallic", &editingMaterial->metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &editingMaterial->roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("AO", &editingMaterial->ao, 0.0f, 1.0f);
    }
    
    ImGui::Separator();
    
    if (ImGui::CollapsingHeader("Emission")) {
        ImGui::ColorEdit3("Emission Color", &editingMaterial->emission.x);
    }
    
    ImGui::End();
}