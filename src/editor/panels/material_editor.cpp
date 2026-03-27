#include "material_editor.h"
#include "core/error_reporter.h"
#include <iostream>
#include <stdexcept>

void MaterialEditorPanel::setSelectedObject(Haruka::SceneObject* obj) {
    try {
        if (!obj) {
            selectedObject = nullptr;
            editingMaterial = nullptr;
            return;
        }
        
        selectedObject = obj;
        
        // Crear material si no existe
        if (!obj->material) {
            obj->material = std::make_shared<Haruka::MaterialComponent>();
            obj->material->name = obj->name + "_Material";
        }
        
        editingMaterial = obj->material.get();
    } catch (const std::exception& e) {
        HARUKA_EDITOR_ERROR(ErrorCode::FAILED_TO_LOAD_MATERIAL, "Failed to load material: " + std::string(e.what()));
        selectedObject = nullptr;
        editingMaterial = nullptr;
    }
}

void MaterialEditorPanel::onImGuiRender() {
    try {
        ImGui::SetNextWindowSize(ImVec2(450, 700), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Material Editor")) {
            ImGui::End();
            return;
        }
        
        // Si no hay objeto seleccionado, mostrar mensaje
        if (!selectedObject) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Select an object to edit material");
            ImGui::End();
            return;
        }
        
        // Validar que el material existe
        if (!editingMaterial || !selectedObject->material) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Material initialization failed");
            ImGui::End();
            return;
        }
        
        ImGui::Text("Object: %s", selectedObject->name.c_str());
        ImGui::Separator();
        
        if (ImGui::CollapsingHeader("Material Properties", ImGuiTreeNodeFlags_DefaultOpen)) {
            char nameBuf[256] = {};
            strncpy(nameBuf, editingMaterial->name.c_str(), sizeof(nameBuf));
            if (ImGui::InputText("Material Name", nameBuf, sizeof(nameBuf))) {
                editingMaterial->name = nameBuf;
            }
            
            ImGui::Separator();
            
            // Color Picker para Albedo
            ImGui::ColorEdit3("Albedo Color", &editingMaterial->albedo.x);
            
            // Sliders para propiedades PBR
            ImGui::SliderFloat("Metallic", &editingMaterial->metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &editingMaterial->roughness, 0.0f, 1.0f);
            ImGui::SliderFloat("Ambient Occlusion", &editingMaterial->ao, 0.0f, 1.0f);
        }
        
        if (ImGui::CollapsingHeader("Emission", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Color Picker para Emisión
            ImGui::ColorEdit3("Emission Color", &editingMaterial->emission.x);
            ImGui::Text("Tip: Set color to white/yellow for glowing objects");
        }
        
        ImGui::End();
    } catch (const std::exception& e) {
        std::cerr << "MaterialEditorPanel error: " << e.what() << std::endl;
        ImGui::End();
    } catch (...) {
        std::cerr << "MaterialEditorPanel unknown error" << std::endl;
        ImGui::End();
    }
}