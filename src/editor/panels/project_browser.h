#pragma once

#include "core/project.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

struct AssetItem {
    std::string name;
    std::string path;
    bool isDirectory;
};

class ProjectBrowserPanel {
public:
    ProjectBrowserPanel() = default;
    
    void setProject(Haruka::Project* project);
    void onImGuiRender();

    // Scene selection
    bool isSceneSelected() const { return sceneSelected; }
    const std::string& getSelectedScenePath() const { return selectedScenePath; }
    void resetSceneSelection() { sceneSelected = false; }

private:
    void showAssetBrowser();
    void refreshAssets();
    void renderDirectoryTree(const std::string& path, const std::string& displayName);
    void renderFileList();
    void renderSceneSelector();
    std::vector<AssetItem> scanDirectory(const std::string& path);

    Haruka::Project* currentProject = nullptr;

    std::string currentAssetPath = "assets/";
    std::vector<AssetItem> currentItems;
    std::string selectedAsset = "";
    int selectedIndex = -1;

    // Scene selection
    bool sceneSelected = false;
    std::string selectedScenePath;
};