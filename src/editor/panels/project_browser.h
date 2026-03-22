#pragma once

#include "core/project.h"
#include "core/scene.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace Haruka {
    class PrefabsPanel;
}

struct FileItem {
    std::string name;
    std::string path;
    std::string extension;
    bool isDirectory;
};

class ProjectBrowserPanel {
public:
    ProjectBrowserPanel();
    ~ProjectBrowserPanel() = default;

    void setProject(Haruka::Project* project);
    void setScene(Haruka::Scene* scene);
    void onImGuiRender();

    void setOnFileLoad(std::function<void(const std::string&)> cb) { onFileLoad = std::move(cb); }
    void setOnPrefabLoad(std::function<void(const std::string&)> cb);
    void setOnPrefabSave(std::function<void(const std::string&)> cb);

private:
    Haruka::Project* currentProject = nullptr;
    Haruka::Scene* currentScene = nullptr;

    std::unique_ptr<Haruka::PrefabsPanel> prefabsPanel;

    std::string selectedPath;
    std::string selectedExtension;
    std::string draggedPath;
    std::string contextMenuPath;
    bool showNewFileDialog = false;
    bool showNewFolderDialog = false;
    char newItemName[256] = "";
    
    std::function<void(const std::string&)> onFileLoad;
    std::string newItemParentPath;

    // File system watching
    std::chrono::steady_clock::time_point lastRefreshTime;
    std::chrono::steady_clock::duration refreshInterval = std::chrono::milliseconds(500);
    bool needsRefresh = false;
    std::string lastProjectPath;

    std::vector<FileItem> scanDirectory(const std::string& path);
    std::string getFileExtension(const std::string& filename);
    std::string getFileIcon(const std::string& extension, bool isDirectory);
    void renderFileTree(const std::string& path, const std::string& displayName, int depth = 0);
    void renderFileContextMenu(const FileItem& item);
    void handleFileClick(const FileItem& item);
    void showFilePreview(const FileItem& item);
    void checkForChanges();
    void refreshFileSystem();
    
    // Operaciones de archivo
    void createNewFile(const std::string& parentPath, const std::string& fileName);
    void createNewFolder(const std::string& parentPath, const std::string& folderName);
    void deleteFile(const std::string& path);
    void moveFile(const std::string& source, const std::string& destination);
    void renderNewItemDialog();
};