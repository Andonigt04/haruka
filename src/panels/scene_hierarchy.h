#pragma once

#include "core/scene.h"
#include "core/event_manager.h"
#include "core/events.h"
#include "core/primitive_types.h"
#include "commands/command_history.h"
#include <imgui.h>
#include <functional>
#include <string>
#include <vector>
#include <utility>

// Display-name / primitive-type pairs listed in the object browser.
inline const std::vector<std::pair<std::string, std::string>> objectTypes = {
    {"Cube",              PrimitiveType::Cube},
    {"Sphere",            PrimitiveType::Sphere},
    {"Capsule",           PrimitiveType::Capsule},
    {"Plane",             PrimitiveType::Plane},
    {"Point Light",       PrimitiveType::PointLight},
    {"Directional Light", PrimitiveType::DirectionalLight},
    {"Sun",               PrimitiveType::Sun},
    {"Planet",            PrimitiveType::Planet},
    {"Empty",             PrimitiveType::Empty},
    {"Camera",            PrimitiveType::Camera},
};

class SceneHierarchyPanel {
public:
    SceneHierarchyPanel() = default;

    void setScene(Haruka::Scene* scene)           { currentScene   = scene; }
    void setSelectedObjectIndex(int index)        { selectedObjectIndex = index; }
    void setCommandHistory(CommandHistory* h)     { commandHistory = h; }
    void setEventManager(Haruka::EventManager* m) { eventManager   = m; }

    void setOnObjectSelectedByIndex(std::function<void(int)> cb) {
        onObjectSelectedByIndex = std::move(cb);
    }
    void setOnObjectSelectedByName(std::function<void(const std::string&)> cb) {
        onObjectSelectedByName = std::move(cb);
    }
    int getSelectedObjectIndex() const { return selectedObjectIndex; }

    std::string currentProjectPath;

    void onImGuiRender();

    /**
     * @brief Posts ObjectEvent::Created for a primitive.
     * parentIndex = -1 means root.  No direct scene mutation.
     */
    void createPrimitive(const std::string& name, const std::string& type, int parentIndex = -1);

private:
    Haruka::Scene*        currentScene   = nullptr;
    CommandHistory*       commandHistory = nullptr;
    Haruka::EventManager* eventManager   = nullptr;
    int                   selectedObjectIndex = -1;
    bool                  showObjectBrowser   = false;
    char                  objectSearchBuffer[256] = {0};

    // Built once per frame: index → child indices.
    std::vector<std::vector<int>> m_childrenMap;

    void renderObjectNode(int index);
    void showContextMenu(int index);

    std::function<void(int)> onObjectSelectedByIndex;
    std::function<void(const std::string&)> onObjectSelectedByName;
};
