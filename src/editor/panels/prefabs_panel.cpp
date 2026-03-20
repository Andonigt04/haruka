#include "prefabs_panel.h"
#include "core/prefab_utils.h"
#include <imgui.h>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace Haruka {

PrefabsPanel::PrefabsPanel() {
    prefabsDir = "Assets/Prefabs";
    std::filesystem::create_directories(prefabsDir);
    refreshPrefabsList();
}

PrefabsPanel::~PrefabsPanel() {}

void PrefabsPanel::setProject(Haruka::Project* proj) {
    if (project == proj) return; // evita refresh si no cambió
    project = proj;
    
    if (project && !project->getPath().empty()) {
        prefabsDir = project->getPath() + "/prefabs";
    } else {
        prefabsDir = "Assets/Prefabs";
    }
    
    std::filesystem::create_directories(prefabsDir);
    refreshPrefabsList();
    std::cout << "Prefabs panel: synced to " << prefabsDir << std::endl;
}

void PrefabsPanel::refreshPrefabsList() {
    prefabsList.clear();
    if (std::filesystem::exists(prefabsDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(prefabsDir)) {
            if (entry.path().extension() == ".prefab") {
                prefabsList.push_back(entry.path().filename().string());
            }
        }
    }
}

void PrefabsPanel::savePrefab(const Haruka::SceneObject& obj, const std::string& name) {
    std::string path = prefabsDir + "/" + name + ".prefab";
    Haruka::savePrefab(obj, path);
    refreshPrefabsList();
    if (onPrefabSave) onPrefabSave(name);
}

void PrefabsPanel::onImGuiRender() {
    
    if (!project) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "No project loaded");
        return;
    }

    ImGui::Text("Available Prefabs: %zu", prefabsList.size());
    ImGui::Separator();

    ImGui::BeginChild("PrefabsList", ImVec2(0, -50), true);
    
    ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));
    ImGui::Separator();

    for (const auto& prefab : prefabsList) {
        std::string lower_prefab = prefab;
        std::transform(lower_prefab.begin(), lower_prefab.end(), lower_prefab.begin(), ::tolower);
        std::string lower_search = searchBuffer;
        std::transform(lower_search.begin(), lower_search.end(), lower_search.begin(), ::tolower);
        
        if (lower_prefab.find(lower_search) != std::string::npos) {
            if (ImGui::Selectable(prefab.c_str(), selectedPrefab == prefab)) {
                selectedPrefab = prefab;
            }
            
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Instantiate")) {
                    if (currentScene) {
                        std::string path = prefabsDir + "/" + prefab;
                        auto loadedObj = Haruka::loadPrefab(path);
                        currentScene->addObject(loadedObj);
                        if (onPrefabLoad) onPrefabLoad(prefab);
                    }
                }
                if (ImGui::MenuItem("Delete")) {
                    std::string path = prefabsDir + "/" + prefab;
                    std::filesystem::remove(path);
                    if (onPrefabDelete) onPrefabDelete(prefab);
                    selectedPrefab.clear();
                    refreshPrefabsList();
                }
                ImGui::EndPopup();
            }
        }
    }
    
    ImGui::EndChild();

    // Modal de renombrar
    if (ImGui::BeginPopupModal("RenamePrefab")) {
        static char newName[128] = "";
        ImGui::InputText("New Name", newName, sizeof(newName));
        
        if (ImGui::Button("Rename")) {
            std::string oldPath = prefabsDir + "/" + selectedPrefab;
            std::string newPath = prefabsDir + "/" + std::string(newName) + ".prefab";
            std::filesystem::rename(oldPath, newPath);
            selectedPrefab = std::string(newName) + ".prefab";
            refreshPrefabsList();
            memset(newName, 0, sizeof(newName));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

}