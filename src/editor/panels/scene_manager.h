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

    void setProject(Project* proj);
    void setScene(Scene* scene);

    void render();
    void renderEmbedded();
    void onImGuiRender() { renderEmbedded(); }

    void setOnSceneLoad(std::function<void(const std::string&)> cb) { onSceneLoad = std::move(cb); }
    void setOnSceneSave(std::function<void(const std::string&)> cb) { onSceneSave = std::move(cb); }
    void setOnSceneNew(std::function<void(const std::string&)> cb)  { onSceneNew  = std::move(cb); }

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