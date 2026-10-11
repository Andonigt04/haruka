#pragma once
#include "Haruka/Core/Components/MaterialComponent.h"
#include "Haruka/Core/Scene/SceneManager.h"
#include <imgui.h>
#include <functional>
#include <string>

/**
 * @brief Material inspector/editor panel for scene objects.
 */
class MaterialEditorPanel {
public:
    /** @brief Constructs an empty material editor panel. */
    MaterialEditorPanel() = default;
    
    /** @brief Sets the object whose material is being edited. */
    void setSelectedObject(Haruka::Core::Scene::SceneObject* obj);
    /** @brief Sets the project root (for relative texture paths). */
    void setProjectPath(const std::string& path) { projectPath = path; }
    /** @brief Callback de cambio de escena (marca dirty). */
    void setOnSceneChanged(std::function<void()> cb) { onSceneChanged = std::move(cb); }
    /** @brief Draws the material editor UI. */
    void onImGuiRender();
    
private:
    /** @brief Non-owning pointer to selected object. */
    Haruka::Core::Scene::SceneObject* selectedObject = nullptr;
    /** @brief Non-owning pointer to the currently edited material. */
    Haruka::Core::Components::MaterialComponent* editingMaterial = nullptr;
    
    char shaderBuffer[256] = {};
    char texturePathBuffer[256] = {};
    std::string selectedTextureType;
    std::string projectPath;
    std::function<void()> onSceneChanged;
    
    /** @brief Renders editable material property controls. */
    void renderMaterialProperties();
};