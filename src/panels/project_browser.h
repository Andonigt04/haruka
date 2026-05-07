#pragma once

#include "core/project.h"
#include "core/scene/scene_manager.h"
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>


struct FileItem {
    std::string name;
    std::string path;
    std::string extension;
    bool isDirectory;
};

/**
 * @brief Project file browser and asset tree panel.
 */
class ProjectBrowserPanel {
public:
    /** @brief Creates an empty project browser panel. */
    ProjectBrowserPanel();
    ~ProjectBrowserPanel() = default;

    /** @brief Binds the project root used by the browser. */
    void setProject(Haruka::Project* project);
    /** @brief Binds the scene for scene-aware file actions. */
    void setScene(Haruka::SceneManager* scene);
    /** @brief Draws the browser UI. */
    void onImGuiRender();

    /** @brief Sets callback invoked when a file is chosen for load. */
    void setOnFileLoad(std::function<void(const std::string&)> cb) { onFileLoad = std::move(cb); }

private:
    Haruka::Project* currentProject = nullptr;
    Haruka::SceneManager* currentScene = nullptr;
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

    /** @brief Scans a directory and returns visible items. */
    std::vector<FileItem> scanDirectory(const std::string& path);
    /** @brief Returns the file extension for a filename. */
    std::string getFileExtension(const std::string& filename);
    /** @brief Returns an icon label for a file extension. */
    std::string getFileIcon(const std::string& extension, bool isDirectory);
    /** @brief Renders the hierarchical file tree. */
    void renderFileTree(const std::string& path, const std::string& displayName, int depth = 0);
    /** @brief Renders the context menu for a file entry. */
    void renderFileContextMenu(const FileItem& item);
    /** @brief Handles a file click selection event. */
    void handleFileClick(const FileItem& item);
    /** @brief Shows a quick preview for selected file. */
    void showFilePreview(const FileItem& item);
    /** @brief Checks whether filesystem state changed. */
    void checkForChanges();
    /** @brief Refreshes cached directory state. */
    void refreshFileSystem();
    
    // File operations
    /** @brief Creates a new file under the parent path. */
    void createNewFile(const std::string& parentPath, const std::string& fileName);
    /** @brief Creates a new folder under the parent path. */
    void createNewFolder(const std::string& parentPath, const std::string& folderName);
    /** @brief Deletes a file or folder path. */
    void deleteFile(const std::string& path);
    /** @brief Moves a file or folder to a new destination. */
    void moveFile(const std::string& source, const std::string& destination);
    /** @brief Renders the new file/folder dialog. */
    void renderNewItemDialog();
};