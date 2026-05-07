#pragma once

#include "core/scene/scene_manager.h"
#include "commands/command_history.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <functional>

class InspectorPanel {
public:
    /** @brief Constructs an empty inspector panel. */
    InspectorPanel() = default;
    
    /** @brief Sets the scene whose selected object is inspected. */
    void setScene(Haruka::SceneManager* scene);
    /** @brief Sets the object index currently inspected. */
    void setSelectedObjectIndex(int index);
    /** @brief Sets the command history used for undoable edits. */
    void setCommandHistory(CommandHistory* history);
    /** @brief Enables or disables play mode behavior. */
    void setPlayMode(bool mode) { playMode = mode; }
    /** @brief Draws the inspector UI. */
    void onImGuiRender();
    /** @brief Callback invoked after scene-editing changes. */
    void setOnSceneChanged(std::function<void()> cb) { onSceneChanged = std::move(cb); }

private:
    void drawNoSelectionState() const;
    bool drawObjectHeader(Haruka::SceneObject& obj);
    bool drawTransformSection(Haruka::SceneObject& obj);
    bool drawVisualSection(Haruka::SceneObject& obj);
    bool drawMetadataSection(Haruka::SceneObject& obj);

    Haruka::SceneManager* currentScene = nullptr;
    int selectedObjectIndex = -1;
    CommandHistory* commandHistory = nullptr;
    bool playMode = false;
    
    bool editingPosition = false, editingRotation = false, editingScale = false;
    glm::dvec3 editStartPosition, editStartRotation, editStartScale;
    std::function<void()> onSceneChanged;
};