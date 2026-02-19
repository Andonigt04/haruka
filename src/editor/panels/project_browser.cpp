#include "project_browser.h"
#include <iostream>
#include <cstring>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

void ProjectBrowserPanel::setProject(Haruka::Project* project) {
    currentProject = project;
    refreshAssets();
}

std::vector<AssetItem> ProjectBrowserPanel::scanDirectory(const std::string& path) {
    std::vector<AssetItem> items;
    
    try {
        if (fs::exists(path) && fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                AssetItem item;
                item.name = entry.path().filename().string();
                item.path = entry.path().string();
                item.isDirectory = entry.is_directory();
                items.push_back(item);
            }
            
            // Ordenar: directorios primero, luego archivos alfabéticamente
            std::sort(items.begin(), items.end(), [](const AssetItem& a, const AssetItem& b) {
                if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
                return a.name < b.name;
            });
        }
    } catch (const std::exception& e) {
        std::cerr << "Error scanning directory: " << e.what() << std::endl;
    }
    
    return items;
}

void ProjectBrowserPanel::refreshAssets() {
    if (currentProject && !currentProject->getPath().empty()) {
        std::string fullPath = currentProject->getPath() + "/" + currentAssetPath;
        currentItems = scanDirectory(fullPath);
        std::cout << "Refreshed: " << fullPath << " (" << currentItems.size() << " items)" << std::endl;
    } else {
        currentItems.clear();
    }
}

void ProjectBrowserPanel::renderDirectoryTree(const std::string& path, const std::string& displayName) {
    auto items = scanDirectory(path);
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    
    // Comparar correctamente la ruta actual
    std::string relativePath = path.substr(currentProject->getPath().length() + 1) + "/";
    if (currentAssetPath == relativePath) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    
    bool opened = ImGui::TreeNodeEx(displayName.c_str(), flags);
    
    if (ImGui::IsItemClicked()) {
        currentAssetPath = relativePath;
        refreshAssets();
    }
    
    if (opened) {
        for (const auto& item : items) {
            if (item.isDirectory) {
                renderDirectoryTree(item.path, item.name);
            }
        }
        ImGui::TreePop();
    }
}

void ProjectBrowserPanel::renderFileList() {
    ImGui::BeginChild("FileList", ImVec2(0, 300), true);
    
    ImGui::Text("Location: %s", currentAssetPath.c_str());
    ImGui::Text("Items: %zu", currentItems.size());
    ImGui::Separator();
    
    // Botón Up
    if (currentAssetPath != "assets/") {
        if (ImGui::Button("🔙 Up")) {
            size_t lastSlash = currentAssetPath.find_last_of('/', currentAssetPath.length() - 2);
            if (lastSlash != std::string::npos) {
                currentAssetPath = currentAssetPath.substr(0, lastSlash + 1);
            } else {
                currentAssetPath = "assets/";
            }
            refreshAssets();
        }
        ImGui::Separator();
    }
    
    // Lista de items
    for (size_t i = 0; i < currentItems.size(); i++) {
        const auto& item = currentItems[i];
        
        // Determinar ícono según extensión
        std::string icon = "📄";
        if (item.isDirectory) {
            icon = "📁";
        } else if (item.name.find(".obj") != std::string::npos || 
                   item.name.find(".gltf") != std::string::npos || 
                   item.name.find(".glb") != std::string::npos ||
                   item.name.find(".fbx") != std::string::npos) {
            icon = "🗿"; // model 3D
        } else if (item.name.find(".png") != std::string::npos || 
                   item.name.find(".jpg") != std::string::npos) {
            icon = "🖼️"; // images
        }
        
        std::string label = icon + " " + item.name;
        
        if (ImGui::Selectable(label.c_str(), (int)i == selectedIndex, ImGuiSelectableFlags_AllowDoubleClick)) {
            selectedIndex = i;
            selectedAsset = item.name;
            
            if (item.isDirectory && ImGui::IsMouseDoubleClicked(0)) {
                currentAssetPath += item.name + "/";
                refreshAssets();
                selectedIndex = -1;
            }
        }
        
        // Drag & drop para modelos
        if (!item.isDirectory && 
            (item.name.find(".obj") != std::string::npos || 
             item.name.find(".gltf") != std::string::npos || 
             item.name.find(".glb") != std::string::npos ||
             item.name.find(".fbx") != std::string::npos)) {
            
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ASSET_PATH", item.path.c_str(), item.path.size() + 1);
                ImGui::Text("📦 %s", item.name.c_str());
                ImGui::EndDragDropSource();
            }
        }
    }
    
    ImGui::EndChild();
}

void ProjectBrowserPanel::showAssetBrowser() {
    if (!currentProject || currentProject->getPath().empty()) {
        ImGui::Text("No project loaded");
        return;
    }
    
    ImGui::Separator();
    ImGui::Text("Asset Browser");
    
    if (ImGui::Button("🔄 Refresh")) {
        refreshAssets();
    }
    
    ImGui::Columns(2, "AssetColumns");
    ImGui::SetColumnWidth(0, 200);
    
    // Columna izquierda: árbol de directorios
    ImGui::BeginChild("DirectoryTree", ImVec2(0, 300), true);
    std::string assetsPath = currentProject->getPath() + "/assets";
    renderDirectoryTree(assetsPath, "assets");
    ImGui::EndChild();
    
    ImGui::NextColumn();
    
    // Columna derecha: lista de archivos
    renderFileList();
    
    ImGui::Columns(1);
    
    // Info del asset seleccionado
    if (selectedIndex >= 0 && selectedIndex < (int)currentItems.size()) {
        const auto& item = currentItems[selectedIndex];
        ImGui::Separator();
        ImGui::Text("Selected: %s", item.name.c_str());
        ImGui::Text("Type: %s", item.isDirectory ? "Directory" : "File");
        ImGui::Text("Path: %s", item.path.c_str());
    }
}

void ProjectBrowserPanel::onImGuiRender() {
    ImGui::Begin("Project Browser");
    
    if (ImGui::BeginTabBar("BrowserTabs")) {
        if (ImGui::BeginTabItem("Assets")) {
            showAssetBrowser();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Scenes")) {
            renderSceneSelector();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void ProjectBrowserPanel::renderSceneSelector() {
    if (!currentProject) {
        ImGui::Text("No project loaded");
        return;
    }
    
    std::string scenePath = currentProject->getPath() + "/scenes";
    if (!std::filesystem::exists(scenePath)) {
        ImGui::Text("No scenes folder found");
        return;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(scenePath)) {
        if (entry.path().extension() == ".scene") {
            std::string filename = entry.path().filename().string();
            std::string fullPath = entry.path().string();
            
            if (ImGui::Selectable(filename.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::IsMouseDoubleClicked(0)) {
                    selectedScenePath = fullPath;
                    sceneSelected = true;
                }
            }
        }
    }
}