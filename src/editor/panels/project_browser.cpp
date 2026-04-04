#include "project_browser.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <chrono>

namespace fs = std::filesystem;
using namespace std::chrono;

ProjectBrowserPanel::ProjectBrowserPanel() {
    lastRefreshTime = steady_clock::now();
}

void ProjectBrowserPanel::setProject(Haruka::Project* project) {
    currentProject = project;
    lastProjectPath = project ? project->getPath() : "";
    needsRefresh = true;
}

void ProjectBrowserPanel::setScene(Haruka::Scene* scene) {
    currentScene = scene;
}

std::string ProjectBrowserPanel::getFileExtension(const std::string& filename) {
    size_t dotPos = filename.find_last_of('.');
    if (dotPos != std::string::npos && dotPos < filename.length() - 1) {
        std::string ext = filename.substr(dotPos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext;
    }
    return "";
}

std::string ProjectBrowserPanel::getFileIcon(const std::string& extension, bool isDirectory) {
    if (isDirectory) return "📁";
    
    if (extension == "cpp" || extension == "h" || extension == "hpp") return "📝";
    if (extension == "scene") return "🎬";
    if (extension == "prefab") return "🔲";
    if (extension == "hrk") return "⚙️";
    if (extension == "png" || extension == "jpg" || extension == "jpeg") return "🖼️";
    if (extension == "obj" || extension == "gltf" || extension == "glb" || extension == "fbx") return "🗿";
    if (extension == "txt" || extension == "md") return "📄";
    if (extension == "json") return "🔧";
    if (extension == "cmake" || extension == "txt") return "🔨";
    
    return "📦";
}

std::vector<FileItem> ProjectBrowserPanel::scanDirectory(const std::string& path) {
    std::vector<FileItem> items;
    
    try {
        if (fs::exists(path) && fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                FileItem item;
                item.name = entry.path().filename().string();
                item.path = entry.path().string();
                item.isDirectory = entry.is_directory();
                item.extension = getFileExtension(item.name);
                items.push_back(item);
            }
            
            std::sort(items.begin(), items.end(), [](const FileItem& a, const FileItem& b) {
                if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
                return a.name < b.name;
            });
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
    }
    
    return items;
}

void ProjectBrowserPanel::checkForChanges() {
    auto now = steady_clock::now();
    
    if (now - lastRefreshTime >= refreshInterval) {
        // Verificar cambios en el sistema de archivos
        if (!currentProject || currentProject->getPath().empty()) return;
        
        try {
            static std::map<std::string, fs::file_time_type> fileTimestamps;
            bool hasChanges = false;
            
            for (const auto& entry : fs::recursive_directory_iterator(currentProject->getPath())) {
                if (fs::is_regular_file(entry)) {
                    auto lastWriteTime = fs::last_write_time(entry);
                    
                    if (fileTimestamps.find(entry.path().string()) == fileTimestamps.end()) {
                        hasChanges = true;
                    } else if (fileTimestamps[entry.path().string()] != lastWriteTime) {
                        hasChanges = true;
                    }
                    
                    fileTimestamps[entry.path().string()] = lastWriteTime;
                }
            }
            
            if (hasChanges) {
                needsRefresh = true;
                std::cout << "File system changes detected, refreshing..." << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error checking for changes: " << e.what() << std::endl;
        }
        
        lastRefreshTime = now;
    }
}

void ProjectBrowserPanel::refreshFileSystem() {
    needsRefresh = false;
    // El refresh automático ocurre al re-renderizar el árbol
}

void ProjectBrowserPanel::createNewFile(const std::string& parentPath, const std::string& fileName) {
    try {
        std::string filePath = parentPath + "/" + fileName;
        std::ofstream file(filePath);
        if (file.is_open()) {
            file.close();
            std::cout << "File created: " << filePath << std::endl;
            needsRefresh = true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error creating file: " << e.what() << std::endl;
    }
}

void ProjectBrowserPanel::createNewFolder(const std::string& parentPath, const std::string& folderName) {
    try {
        std::string folderPath = parentPath + "/" + folderName;
        fs::create_directory(folderPath);
        std::cout << "Folder created: " << folderPath << std::endl;
        needsRefresh = true;
    } catch (const std::exception& e) {
        std::cerr << "Error creating folder: " << e.what() << std::endl;
    }
}

void ProjectBrowserPanel::deleteFile(const std::string& path) {
    try {
        if (fs::exists(path)) {
            fs::remove_all(path);
            std::cout << "Deleted: " << path << std::endl;
            if (selectedPath == path) selectedPath = "";
            needsRefresh = true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error deleting: " << e.what() << std::endl;
    }
}

void ProjectBrowserPanel::moveFile(const std::string& source, const std::string& destination) {
    try {
        if (fs::exists(source) && fs::is_directory(destination)) {
            std::string newPath = destination + "/" + fs::path(source).filename().string();
            fs::rename(source, newPath);
            std::cout << "Moved: " << source << " -> " << newPath << std::endl;
            needsRefresh = true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error moving file: " << e.what() << std::endl;
    }
}

void ProjectBrowserPanel::renderFileContextMenu(const FileItem& item) {
    if (ImGui::BeginPopupContextItem()) {
        if (item.isDirectory) {
            if (ImGui::MenuItem("New File")) {
                showNewFileDialog = true;
                newItemParentPath = item.path;
            }
            if (ImGui::MenuItem("New Folder")) {
                showNewFolderDialog = true;
                newItemParentPath = item.path;
            }
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("Delete")) {
            deleteFile(item.path);
        }
        if (ImGui::MenuItem("Rename")) {
            // TODO: Implementar rename
        }
        
        ImGui::EndPopup();
    }
}

void ProjectBrowserPanel::renderNewItemDialog() {
    if (showNewFileDialog) {
        ImGui::OpenPopup("New File");
        showNewFileDialog = false;
    }
    if (showNewFolderDialog) {
        ImGui::OpenPopup("New Folder");
        showNewFolderDialog = false;
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("New File", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("File Name##newfile", newItemName, sizeof(newItemName));
        
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (strlen(newItemName) > 0) {
                createNewFile(newItemParentPath, newItemName);
                strcpy(newItemName, "");
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            strcpy(newItemName, "");
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Folder Name##newfolder", newItemName, sizeof(newItemName));
        
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (strlen(newItemName) > 0) {
                createNewFolder(newItemParentPath, newItemName);
                strcpy(newItemName, "");
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            strcpy(newItemName, "");
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ProjectBrowserPanel::renderFileTree(const std::string& path, const std::string& displayName, int depth) {
    auto items = scanDirectory(path);
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (depth == 0) flags |= ImGuiTreeNodeFlags_DefaultOpen;
    
    // Si la carpeta está vacía, no mostrar flecha expandible
    if (items.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    
    std::string icon = getFileIcon("", true);
    std::string label = icon + " " + displayName;
    
    bool opened = ImGui::TreeNodeEx(label.c_str(), flags);
    
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_MOVE")) {
            const char* sourcePath = (const char*)payload->Data;
            moveFile(sourcePath, path);
        }
        ImGui::EndDragDropTarget();
    }
    
    if (opened) {
        for (const auto& item : items) {
            std::string itemLabel = getFileIcon(item.extension, item.isDirectory) + " " + item.name;
            
            if (item.isDirectory) {
                renderFileTree(item.path, item.name, depth + 1);
            } else {
                ImGuiTreeNodeFlags leafFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (item.path == selectedPath) leafFlags |= ImGuiTreeNodeFlags_Selected;
                
                ImGui::TreeNodeEx(itemLabel.c_str(), leafFlags);
                
                if (ImGui::IsItemClicked()) {
                    selectedPath = item.path;
                    selectedExtension = item.extension;
                    handleFileClick(item);
                }
                
                renderFileContextMenu(item);
                
                if (ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("FILE_MOVE", item.path.c_str(), item.path.size() + 1);
                    ImGui::Text("%s %s", getFileIcon(item.extension, false).c_str(), item.name.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        }
        ImGui::TreePop();
    }
}

void ProjectBrowserPanel::handleFileClick(const FileItem& item) {
    std::cout << "Selected: " << item.path << " (" << item.extension << ")" << std::endl;
    
    // Doble click en escenas
    if (item.extension == "scene" && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (onFileLoad) {
            onFileLoad(item.path);
        }
    }
    
    // Doble click en prefabs
    if (item.extension == "prefab" && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (onFileLoad) {
            onFileLoad(item.path);
        }
    }
}

void ProjectBrowserPanel::showFilePreview(const FileItem& item) {
    ImGui::Separator();
    ImGui::Text("Selected File");
    ImGui::Text("Name: %s", item.name.c_str());
    ImGui::Text("Type: %s", item.extension.empty() ? "Directory" : item.extension.c_str());
    ImGui::Text("Path: %s", item.path.c_str());
}

void ProjectBrowserPanel::onImGuiRender() {
    // Verificar cambios en tiempo real
    checkForChanges();

    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Project Browser")) {
        ImGui::End();
        return;
    }

    if (!currentProject || currentProject->getPath().empty()) {
        ImGui::Text("No project loaded");
        ImGui::End();
        return;
    }

    ImGui::Text("Project: %s", currentProject->getPath().c_str());
        
    if (ImGui::Button("New File")) {
        showNewFileDialog = true;
        newItemParentPath = currentProject->getPath();
    }
    ImGui::SameLine();
    if (ImGui::Button("New Folder")) {
        showNewFolderDialog = true;
        newItemParentPath = currentProject->getPath();
    }
    
    ImGui::Separator();

    ImGui::BeginChild("FileTree", ImVec2(0, -100), true);
    std::string projectPath = currentProject->getPath();
    renderFileTree(projectPath, "Project Root");
    ImGui::EndChild();

    if (!selectedPath.empty()) {
        FileItem item;
        item.name = fs::path(selectedPath).filename().string();
        item.path = selectedPath;
        item.extension = selectedExtension;
        showFilePreview(item);
    }

    renderNewItemDialog();

    ImGui::End();
}
