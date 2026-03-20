#include "scene_manager.h"
#include "core/project.h"
#include "core/scene.h"
#include <imgui.h>
#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace Haruka {

static std::string GetScenesDir(Project* project) {
    if (project) return project->getPath() + "/scenes";
    return "scenes";
}

SceneManagerPanel::SceneManagerPanel() {
    std::cout << "[SceneManager] Initialized" << std::endl;
}

SceneManagerPanel::~SceneManagerPanel() {}

void SceneManagerPanel::render() {
    if (ImGui::Begin("Scene Manager")) {
        renderEmbedded();
    }
    ImGui::End();
}

void SceneManagerPanel::renderEmbedded() {
    if (ImGui::Button("New Scene", ImVec2(-1, 0))) {
        showNewSceneDialog = true;
    }

    ImGui::Separator();
    refreshSceneList();

    if (ImGui::BeginListBox("##SceneList", ImVec2(-1, 180))) {
        for (size_t i = 0; i < sceneList.size(); ++i) {
            bool selected = (selectedSceneIndex == static_cast<int>(i));
            if (ImGui::Selectable(sceneList[i].c_str(), selected)) {
                selectedSceneIndex = static_cast<int>(i);
            }
        }
        ImGui::EndListBox();
    }

    if (selectedSceneIndex >= 0 && selectedSceneIndex < static_cast<int>(sceneList.size())) {
        const std::string sceneName = sceneList[selectedSceneIndex];
        const std::string filepath = GetScenesDir(project) + "/" + sceneName + ".scene";

        if (ImGui::Button("Load", ImVec2(-1, 0))) {
            const std::string base = GetScenesDir(project) + "/" + sceneName;
            const std::string scenePath = base + ".scene";
            const std::string jsonPath  = base + ".json";

            if (!currentScene) {
                std::cout << "✗ No currentScene set in SceneManager" << std::endl;
            } else {
                bool loaded = currentScene->load(scenePath);
                if (!loaded) loaded = currentScene->load(jsonPath);

                if (loaded) {
                    currentScene->setName(sceneName);
                    std::cout << "✓ Scene loaded: " << sceneName << std::endl;
                    if (onSceneLoad) onSceneLoad(sceneName);
                } else {
                    std::cout << "✗ Failed to load: " << scenePath
                              << " (and fallback " << jsonPath << ")" << std::endl;
                }
            }
        }

        if (ImGui::Button("Save", ImVec2(-1, 0))) {
            std::filesystem::create_directories(GetScenesDir(project));
            if (currentScene) {
                currentScene->setName(sceneName);
                if (currentScene->save(filepath)) {
                    std::cout << "✓ Scene saved: " << filepath << std::endl;
                    if (onSceneSave) onSceneSave(sceneName);
                } else {
                    std::cout << "✗ Failed to save: " << filepath << std::endl;
                }
            }
        }
    }

    if (showNewSceneDialog) ImGui::OpenPopup("New Scene");
    if (ImGui::BeginPopupModal("New Scene", &showNewSceneDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", newSceneNameBuffer, sizeof(newSceneNameBuffer));
        if (ImGui::Button("Create")) {
            createNewScene(newSceneNameBuffer);
            newSceneNameBuffer[0] = '\0';
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

void SceneManagerPanel::refreshSceneList() {
    sceneList.clear();
    const std::string scenesDir = GetScenesDir(project);

    if (!fs::exists(scenesDir)) return;

    for (const auto& entry : fs::directory_iterator(scenesDir)) {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();
        if (ext == ".scene") {
            sceneList.push_back(entry.path().stem().string());
        }
    }

    std::sort(sceneList.begin(), sceneList.end());
}

void SceneManagerPanel::createNewScene(const std::string& name) {
    if (name.empty() || !currentScene) return;

    const std::string scenesDir = GetScenesDir(project);
    std::filesystem::create_directories(scenesDir);

    const std::string filepath = scenesDir + "/" + name + ".scene";
    currentScene->setName(name);

    if (currentScene->save(filepath)) {
        std::cout << "✓ Scene created: " << filepath << std::endl;
        refreshSceneList();
        if (onSceneNew) onSceneNew(name);
    } else {
        std::cout << "✗ Failed to create scene: " << filepath << std::endl;
    }
}

void SceneManagerPanel::setProject(Project* proj) {
    project = proj;
    selectedSceneIndex = -1;
    refreshSceneList();
}

void SceneManagerPanel::setScene(Scene* scene) {
    currentScene = scene;
}

} // namespace Haruka