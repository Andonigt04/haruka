#pragma once
#include "core/components/material_component.h"
#include "core/scene.h"
#include <imgui.h>

class MaterialEditorPanel {
public:
    MaterialEditorPanel() = default;
    
    void setSelectedObject(Haruka::SceneObject* obj);
    void onImGuiRender();
    
private:
    Haruka::SceneObject* selectedObject = nullptr;
    Haruka::MaterialComponent* editingMaterial = nullptr;
    
    char shaderBuffer[256] = {};
    char texturePathBuffer[256] = {};
    std::string selectedTextureType;
    
    void renderMaterialProperties();
};