#pragma once
#include "core/components/material_component.h"
#include "core/scene.h"
#include <imgui.h>

/**
 * @brief Material inspector/editor panel for scene objects.
 */
class MaterialEditorPanel {
public:
    /** @brief Constructs an empty material editor panel. */
    MaterialEditorPanel() = default;
    
    /** @brief Sets the object whose material is being edited. */
    void setSelectedObject(Haruka::SceneObject* obj);
    /** @brief Draws the material editor UI. */
    void onImGuiRender();
    
private:
    /** @brief Non-owning pointer to selected object. */
    Haruka::SceneObject* selectedObject = nullptr;
    /** @brief Non-owning pointer to the currently edited material. */
    Haruka::MaterialComponent* editingMaterial = nullptr;
    
    char shaderBuffer[256] = {};
    char texturePathBuffer[256] = {};
    std::string selectedTextureType;
    
    /** @brief Renders editable material property controls. */
    void renderMaterialProperties();
};