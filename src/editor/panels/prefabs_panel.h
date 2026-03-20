#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include "core/scene.h"
#include "core/project.h"

namespace Haruka {

class PrefabsPanel {
public:
    PrefabsPanel();
    ~PrefabsPanel();

    void setProject(Haruka::Project* proj);
    void setScene(Haruka::Scene* scene) { currentScene = scene; }
    void onImGuiRender();
    
    void savePrefab(const Haruka::SceneObject& obj, const std::string& name);
    void loadPrefabsList();

    // Callbacks
    void setOnPrefabLoad(std::function<void(const std::string&)> cb) { onPrefabLoad = std::move(cb); }
    void setOnPrefabSave(std::function<void(const std::string&)> cb) { onPrefabSave = std::move(cb); }
    void setOnPrefabDelete(std::function<void(const std::string&)> cb) { onPrefabDelete = std::move(cb); }

private:
    Haruka::Project* project = nullptr;
    Haruka::Scene* currentScene = nullptr;
    std::vector<std::string> prefabsList;
    std::string prefabsDir;
    std::string selectedPrefab;

    char searchBuffer[128] = "";

    std::function<void(const std::string&)> onPrefabLoad;
    std::function<void(const std::string&)> onPrefabSave;
    std::function<void(const std::string&)> onPrefabDelete;

    void refreshPrefabsList();
};

}