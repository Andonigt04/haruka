#pragma once

#include "core/scene/scene_manager.h"
#include "commands/command_history.h"
#include <imgui.h>
#include <memory>
#include <functional>
#include <string>

class SceneHierarchyPanel {
public:
    SceneHierarchyPanel() = default;
    
    void setScene(Haruka::SceneManager* scene);
    void setSelectedObjectIndex(int index);
    void setCommandHistory(CommandHistory* history);
    void onImGuiRender();
    void createPrimitive(const std::string& name, const std::string& type);

    std::string currentProjectPath;
    
    void setOnObjectSelectedByIndex(std::function<void(int)> cb) {
        onObjectSelectedByIndex = std::move(cb);
    }
    
    void setOnObjectSelectedByName(std::function<void(const std::string&)> cb) {
        onObjectSelectedByName = std::move(cb);
    }

    /** @brief Callback de DOBLE clic: encuadrar el objeto en el viewport (no es selección). */
    void setOnObjectFocused(std::function<void(int)> cb) {
        onObjectFocused = std::move(cb);
    }

    int getSelectedObjectIndex() const { return selectedObjectIndex; }

private:
    Haruka::SceneManager* currentScene = nullptr;
    CommandHistory* commandHistory = nullptr;
    int selectedObjectIndex = -1;
    bool showObjectBrowser = false;
    char objectSearchBuffer[256] = {0};
    
    void setProjectPath(const std::string& path) { currentProjectPath = path; }
    
    void renderObjectNode(int index);
    void reparentObject(int childIndex, int newParentIndex);
    void createChildObject(int parentIndex, const std::string& primitiveType);
    void duplicateObject(int index);
    void showContextMenu(int index);
    
    std::function<void(int)> onObjectSelectedByIndex;
    std::function<void(const std::string&)> onObjectSelectedByName;
    std::function<void(int)> onObjectFocused;

    void renderChildObject(const Haruka::SceneObject& child, size_t index);
    void showContextMenuChild(const Haruka::SceneObject& child);
};
