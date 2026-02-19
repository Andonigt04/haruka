#pragma once

#include "core/scene.h"
#include "editor/commands/command_history.h"
#include <imgui.h>
#include <glm/glm.hpp>

class InspectorPanel {
public:
    InspectorPanel() = default;
    
    void setScene(Haruka::Scene* scene);
    void setSelectedObjectIndex(int index);
    void setCommandHistory(CommandHistory* history);
    void onImGuiRender();

private:
    Haruka::Scene* currentScene = nullptr;
    CommandHistory* commandHistory = nullptr;
    int selectedObjectIndex = -1;

    // Edit tracking
    glm::vec3 editStartPosition{0.0f};
    glm::vec3 editStartRotation{0.0f};
    glm::vec3 editStartScale{1.0f};

    bool editingPosition = false;
    bool editingRotation = false;
    bool editingScale = false;
};