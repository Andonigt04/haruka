#pragma once

#include <string>
#include <vector>
#include <functional>
#include "core/project.h"
#include "core/scene.h"

namespace Haruka {

class SceneManagerPanel {
public:
    SceneManagerPanel();
    ~SceneManagerPanel();
    
    void render();
    
    void setProject(Project* proj) { project = proj; }
    void setCurrentScene(Scene* scene) { currentScene = scene; }
    void setScene(Scene* scene) { currentScene = scene; }
    
    void setOnSceneLoadCallback(std::function<void(const std::string&)> callback) {
        onSceneLoad = callback;
    }
    void setOnSceneSaveCallback(std::function<void(const std::string&)> callback) {
        onSceneSave = callback;
    }
    void setOnSceneNewCallback(std::function<void(const std::string&)> callback) {
        onSceneNew = callback;
    }

private:
    Project* project = nullptr;
    Scene* currentScene = nullptr;
    
    std::vector<std::string> sceneList;
    int selectedSceneIndex = -1;
    
    char newSceneNameBuffer[256] = {0};
    char duplicateNameBuffer[256] = {0};
    bool showNewSceneDialog = false;
    bool showDuplicateDialog = false;
    bool showDeleteConfirm = false;
    std::string sceneToDelete;
    
    std::function<void(const std::string&)> onSceneLoad;
    std::function<void(const std::string&)> onSceneSave;
    std::function<void(const std::string&)> onSceneNew;
    
    void refreshSceneList();
    void createNewScene(const std::string& name);
    void duplicateScene(const std::string& originalName, const std::string& newName);
    void deleteScene(const std::string& name);
    void renameScene(const std::string& oldName, const std::string& newName);
};

}