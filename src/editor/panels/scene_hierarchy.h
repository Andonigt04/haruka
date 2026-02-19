#pragma once

#include "core/scene.h"
#include "editor/commands/command_history.h"
#include <imgui.h>
#include <memory>

class SceneHierarchyPanel {
public:
    SceneHierarchyPanel() = default;
    
    void setScene(Haruka::Scene* scene);
    void setCommandHistory(CommandHistory* history);
    void onImGuiRender();
    
    int getSelectedObjectIndex() const { return selectedObjectIndex; }

private:
    void showContextMenu(int index);
    void duplicateObject(int index);

    Haruka::Scene* currentScene = nullptr;
    CommandHistory* commandHistory = nullptr;
    int selectedObjectIndex = -1;
};