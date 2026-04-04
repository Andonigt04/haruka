#pragma once

#include "core/scene.h"
#include "editor/commands/command_history.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <functional>

class InspectorPanel {
public:
    InspectorPanel() = default;
    
    void setScene(Haruka::Scene* scene);
    void setSelectedObjectIndex(int index);
    void setCommandHistory(CommandHistory* history);
    void setPlayMode(bool mode) { playMode = mode; }
    void onImGuiRender();
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