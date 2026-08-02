#include "material_editor.h"
#include "tools/error_reporter.h"
#include <nfd.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {

// Selector de archivos → ruta relativa al proyecto (si está dentro de él).
bool pickFilePath(const std::string& projectPath, const std::string& filter,
                  std::string& out) {
    nfdchar_t* outPath = nullptr;
    nfdresult_t result = NFD_OpenDialog(filter.c_str(), nullptr, &outPath);
    if (result == NFD_OKAY && outPath) {
        std::string abs(outPath);
        free(outPath);
        if (!projectPath.empty()) {
            std::error_code ec;
            std::string absNorm = std::filesystem::weakly_canonical(abs, ec).string();
            std::string projNorm = std::filesystem::weakly_canonical(projectPath, ec).string();
            if (!ec && absNorm.rfind(projNorm, 0) == 0) {
                std::string rel = absNorm.substr(projNorm.size());
                while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) rel.erase(rel.begin());
                out = rel;
                return true;
            }
        }
        out = abs;
        return true;
    }
    return false;
}

} // namespace

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

            // Shader
            {
                char shBuf[512] = {};
                strncpy(shBuf, editingMaterial->shaderPath.c_str(), sizeof(shBuf) - 1);
                if (ImGui::InputText("Shader", shBuf, sizeof(shBuf))) {
                    editingMaterial->shaderPath = shBuf;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Browse##shader")) {
                    std::string picked;
                    if (pickFilePath(projectPath, "vert,frag,glsl,shader", picked)) {
                        editingMaterial->shaderPath = picked;
                        if (onSceneChanged) onSceneChanged();
                    }
                }
            }

            ImGui::Separator();
            
            // Color Picker para Albedo
            if (ImGui::ColorEdit3("Albedo Color", &editingMaterial->albedo.x)) {
                if (onSceneChanged) onSceneChanged();
            }
            
            // Sliders para propiedades PBR
            bool pbrChanged = false;
            pbrChanged |= ImGui::SliderFloat("Metallic", &editingMaterial->metallic, 0.0f, 1.0f);
            pbrChanged |= ImGui::SliderFloat("Roughness", &editingMaterial->roughness, 0.0f, 1.0f);
            pbrChanged |= ImGui::SliderFloat("Ambient Occlusion", &editingMaterial->ao, 0.0f, 1.0f);
            if (pbrChanged && onSceneChanged) onSceneChanged();
        }

        if (ImGui::CollapsingHeader("Texture Slots", ImGuiTreeNodeFlags_DefaultOpen)) {
            static const char* slots[] = {"albedo", "normal", "metallic", "roughness", "ao"};
            bool anyChanged = false;
            for (const char* s : slots) {
                char buf[512] = {};
                std::string cur = editingMaterial->textures.count(s) ? editingMaterial->textures[s] : std::string();
                strncpy(buf, cur.c_str(), sizeof(buf) - 1);
                std::string label = std::string(s) + " texture";
                if (ImGui::InputText(label.c_str(), buf, sizeof(buf))) {
                    editingMaterial->textures[s] = buf;
                    anyChanged = true;
                }
                ImGui::SameLine();
                std::string btnId = std::string("Browse##") + s;
                if (ImGui::SmallButton(btnId.c_str())) {
                    std::string picked;
                    if (pickFilePath(projectPath, "png,jpg,jpeg,tga,bmp,exr", picked)) {
                        editingMaterial->textures[s] = picked;
                        anyChanged = true;
                    }
                }
                if (!cur.empty() && ImGui::Button((std::string("Clear##") + s).c_str())) {
                    editingMaterial->textures[s].clear();
                    anyChanged = true;
                }
            }
            if (anyChanged && onSceneChanged) onSceneChanged();
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