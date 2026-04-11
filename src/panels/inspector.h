#pragma once

#include "core/scene.h"
#include "editor/commands/command_history.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <functional>

class InspectorPanel {
public:
    /** @brief Constructs an empty inspector panel. */
    InspectorPanel() = default;
    
    /** @brief Sets the scene whose selected object is inspected. */
    void setScene(Haruka::Scene* scene);
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
    Haruka::Scene* currentScene = nullptr;
    int selectedObjectIndex = -1;
    CommandHistory* commandHistory = nullptr;
    bool playMode = false;
    
    bool editingPosition = false, editingRotation = false, editingScale = false;
    glm::dvec3 editStartPosition, editStartRotation, editStartScale;
    std::function<void()> onSceneChanged;
};