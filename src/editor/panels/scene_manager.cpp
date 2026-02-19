#include "scene_manager.h"
#include "core/project.h"
#include "core/scene.h"
#include <imgui.h>
#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace Haruka {

SceneManagerPanel::SceneManagerPanel() {
    std::cout << "[SceneManager] Initialized" << std::endl;
}

SceneManagerPanel::~SceneManagerPanel() {}

void SceneManagerPanel::render() {
    if (!ImGui::Begin("Scene Manager")) {
        ImGui::End();
        return;
    }
    
    // Refresh button
    if (ImGui::Button("Refresh")) {
        refreshSceneList();
    }
    
    ImGui::SameLine();
    
    // New scene button
    if (ImGui::Button("New Scene")) {
        showNewSceneDialog = true;
        memset(newSceneNameBuffer, 0, sizeof(newSceneNameBuffer));
        strcpy(newSceneNameBuffer, "new_scene");
    }
    
    ImGui::Separator();
    
    // Scene list
    ImGui::BeginChild("SceneList", ImVec2(0, -30));
    
    for (size_t i = 0; i < sceneList.size(); i++) {
        const auto& sceneName = sceneList[i];
        bool isSelected = (selectedSceneIndex == (int)i);
        
        if (ImGui::Selectable(sceneName.c_str(), isSelected)) {
            selectedSceneIndex = i;
        }
        
        // Context menu
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Load")) {
                if (onSceneLoad) {
                    onSceneLoad(sceneName);
                }
            }
            
            if (ImGui::MenuItem("Duplicate")) {
                showDuplicateDialog = true;
                memset(duplicateNameBuffer, 0, sizeof(duplicateNameBuffer));
                strcpy(duplicateNameBuffer, (sceneName + "_copy").c_str());
            }
            
            if (ImGui::MenuItem("Delete")) {
                showDeleteConfirm = true;
                sceneToDelete = sceneName;
            }
            
            ImGui::EndPopup();
        }
    }
    
    ImGui::EndChild();
    
    // Bottom buttons
    ImGui::Separator();
    if (ImGui::Button("Load Selected") && selectedSceneIndex >= 0) {
        if (onSceneLoad) {
            onSceneLoad(sceneList[selectedSceneIndex]);
        }
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Save Current") && currentScene) {
        if (onSceneSave) {
            onSceneSave(currentScene->getName());
        }
    }
    
    // New Scene Dialog
    if (showNewSceneDialog) {
        ImGui::OpenPopup("New Scene");
        if (ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Scene Name:");
            ImGui::InputText("##NewSceneName", newSceneNameBuffer, sizeof(newSceneNameBuffer));
            
            if (ImGui::Button("Create")) {
                createNewScene(newSceneNameBuffer);
                showNewSceneDialog = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                showNewSceneDialog = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
    }
    
    // Duplicate Dialog
    if (showDuplicateDialog && selectedSceneIndex >= 0) {
        ImGui::OpenPopup("Duplicate Scene");
        if (ImGui::BeginPopupModal("Duplicate Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("New Scene Name:");
            ImGui::InputText("##DuplicateName", duplicateNameBuffer, sizeof(duplicateNameBuffer));
            
            if (ImGui::Button("Duplicate")) {
                duplicateScene(sceneList[selectedSceneIndex], duplicateNameBuffer);
                showDuplicateDialog = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                showDuplicateDialog = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
    }
    
    // Delete Confirmation
    if (showDeleteConfirm) {
        ImGui::OpenPopup("Delete Scene");
        if (ImGui::BeginPopupModal("Delete Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete:");
            ImGui::Text("%s", sceneToDelete.c_str());
            
            if (ImGui::Button("Delete")) {
                deleteScene(sceneToDelete);
                showDeleteConfirm = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                showDeleteConfirm = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }
    }
    
    ImGui::End();
}

void SceneManagerPanel::refreshSceneList() {
    sceneList.clear();
    
    if (!project) return;
    
    std::string scenesPath = project->getPath() + "/scenes";
    
    if (!fs::exists(scenesPath)) {
        std::cout << "[SceneManager] Scenes folder does not exist" << std::endl;
        return;
    }
    
    for (const auto& entry : fs::directory_iterator(scenesPath)) {
        if (entry.path().extension() == ".scene") {
            sceneList.push_back(entry.path().filename().string());
        }
    }
    
    std::cout << "[SceneManager] Found " << sceneList.size() << " scenes" << std::endl;
}

void SceneManagerPanel::createNewScene(const std::string& name) {
    if (!project || name.empty()) return;
    
    std::string scenePath = project->getPath() + "/scenes/" + name + ".scene";
    
    if (fs::exists(scenePath)) {
        std::cout << "[SceneManager] Scene already exists: " << name << std::endl;
        return;
    }
    
    // Create empty scene file
    std::ofstream file(scenePath);
    if (file.is_open()) {
        file << "{\n";
        file << "  \"name\": \"" << name << "\",\n";
        file << "  \"objects\": []\n";
        file << "}\n";
        file.close();
        
        std::cout << "[SceneManager] Created new scene: " << name << std::endl;
        refreshSceneList();
        
        if (onSceneNew) {
            onSceneNew(name + ".scene");
        }
    }
}

void SceneManagerPanel::duplicateScene(const std::string& originalName, const std::string& newName) {
    if (!project || newName.empty()) return;
    
    std::string srcPath = project->getPath() + "/scenes/" + originalName;
    std::string dstPath = project->getPath() + "/scenes/" + newName + ".scene";
    
    if (fs::exists(dstPath)) {
        std::cout << "[SceneManager] Scene already exists: " << newName << std::endl;
        return;
    }
    
    try {
        fs::copy_file(srcPath, dstPath);
        std::cout << "[SceneManager] Duplicated scene: " << originalName << " -> " << newName << std::endl;
        refreshSceneList();
    } catch (const std::exception& e) {
        std::cerr << "[SceneManager] Error duplicating scene: " << e.what() << std::endl;
    }
}

void SceneManagerPanel::deleteScene(const std::string& name) {
    if (!project) return;
    
    std::string scenePath = project->getPath() + "/scenes/" + name;
    
    try {
        fs::remove(scenePath);
        std::cout << "[SceneManager] Deleted scene: " << name << std::endl;
        refreshSceneList();
        
        if (selectedSceneIndex >= (int)sceneList.size()) {
            selectedSceneIndex = sceneList.empty() ? -1 : sceneList.size() - 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[SceneManager] Error deleting scene: " << e.what() << std::endl;
    }
}

void SceneManagerPanel::renameScene(const std::string& oldName, const std::string& newName) {
    if (!project) return;
    
    std::string oldPath = project->getPath() + "/scenes/" + oldName;
    std::string newPath = project->getPath() + "/scenes/" + newName;
    
    try {
        fs::rename(oldPath, newPath);
        std::cout << "[SceneManager] Renamed scene: " << oldName << " -> " << newName << std::endl;
        refreshSceneList();
    } catch (const std::exception& e) {
        std::cerr << "[SceneManager] Error renaming scene: " << e.what() << std::endl;
    }
}

}