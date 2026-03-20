#pragma once
#include "core/components/material_component.h"
#include "core/scene.h"
#include <imgui.h>

class MaterialEditorPanel {
public:
    MaterialEditorPanel() = default;
    
    void setScene(Haruka::Scene* scene) { currentScene = scene; }
    void setSelectedObject(const std::string& objectName);
    void onImGuiRender();
    
private:
    Haruka::Scene* currentScene = nullptr;
    std::string selectedObjectName;
    Haruka::MaterialComponent* editingMaterial = nullptr;
    
    char shaderBuffer[256] = {};
    char texturePathBuffer[256] = {};
    std::string selectedTextureType;
    
    void renderMaterialProperties();
};