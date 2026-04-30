#pragma once

#include "core/scene.h"
#include "commands/command_history.h"
#include <imgui.h>
#include <memory>
#include <functional>
#include <string>

class SceneHierarchyPanel {
public:
    /** @brief Constructs an empty hierarchy panel. */
    SceneHierarchyPanel() = default;
    
    /** @brief Sets the scene displayed by the panel. */
    void setScene(Haruka::Scene* scene);
    /** @brief Updates the currently selected object index. */
    void setSelectedObjectIndex(int index);
    /** @brief Injects the command history used by UI actions. */
    void setCommandHistory(CommandHistory* history);
    /** @brief Draws the hierarchy UI. */
    void onImGuiRender();
    /** @brief Creates a primitive object in the scene. */
    void createPrimitive(const std::string& name, const std::string& type);

    /** @brief Project path used for relative browsing. */
    std::string currentProjectPath;
    
    /** @brief Callback invoked when selection changes by index. */
    void setOnObjectSelectedByIndex(std::function<void(int)> cb) {
        onObjectSelectedByIndex = std::move(cb);
    }
    
    void setOnObjectSelectedByName(std::function<void(const std::string&)> cb) {
        onObjectSelectedByName = std::move(cb);
    }
    /** @brief Returns the current selected object index. */
    int getSelectedObjectIndex() const { return selectedObjectIndex; }

private:
    Haruka::Scene* currentScene = nullptr;
    CommandHistory* commandHistory = nullptr;
    int selectedObjectIndex = -1;
    bool showObjectBrowser = false;
    char objectSearchBuffer[256] = {0};
    
    /** @brief Updates the project root used for browsing. */
    void setProjectPath(const std::string& path) { currentProjectPath = path; }
    
    /** @brief Renders one object tree node. */
    void renderObjectNode(int index);
    /** @brief Reparents an object under a new parent index. */
    void reparentObject(int childIndex, int newParentIndex);
    /** @brief Creates a child primitive under the given parent. */
    void createChildObject(int parentIndex, const std::string& primitiveType);
    /** @brief Duplicates an object by index. */
    void duplicateObject(int index);
    /** @brief Shows the context menu for one object. */
    void showContextMenu(int index);
    
    std::function<void(int)> onObjectSelectedByIndex;
    std::function<void(const std::string&)> onObjectSelectedByName;

    /** @brief Renders a child object entry. */
    void renderChildObject(const Haruka::SceneObject& child, size_t index);
    /** @brief Shows the context menu for a child object. */
    void showContextMenuChild(const Haruka::SceneObject& child);
};