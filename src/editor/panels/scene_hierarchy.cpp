#include "scene_hierarchy.h"
#include "editor/commands/scene_commands.h"
#include <glm/glm.hpp>
#include <iostream>
#include <cstdint>

void SceneHierarchyPanel::setScene(Haruka::Scene* scene) {
    currentScene = scene;
    selectedObjectIndex = -1;
}

void SceneHierarchyPanel::setCommandHistory(CommandHistory* history) {
    commandHistory = history;
}

void SceneHierarchyPanel::duplicateObject(int index) {
    if (!currentScene || index < 0 || index >= (int)currentScene->getObjects().size()) return;
    
    const auto& original = currentScene->getObjects()[index];
    Haruka::SceneObject duplicate = original;
    duplicate.name = original.name + "_copy";
    duplicate.position += glm::vec3(1.0f, 0.0f, 0.0f);
    
    if (commandHistory) {
        commandHistory->execute(std::make_unique<AddObjectCommand>(currentScene, duplicate));
    } else {
        currentScene->addObject(duplicate);
    }
}

void SceneHierarchyPanel::showContextMenu(int index) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Duplicate")) {
            duplicateObject(index);
        }
        
        if (ImGui::MenuItem("Delete")) {
            const auto& obj = currentScene->getObjects()[index];
            if (commandHistory) {
                commandHistory->execute(std::make_unique<DeleteObjectCommand>(currentScene, obj.name));
            } else {
                currentScene->removeObject(obj.name);
            }
            selectedObjectIndex = -1;
        }
        
        ImGui::EndPopup();
    }
}

void SceneHierarchyPanel::onImGuiRender() {
    ImGui::Begin("Scene Hierarchy");
    
    if (!currentScene) {
        ImGui::Text("No scene loaded");
        ImGui::End();
        return;
    }

    ImGui::Text("Scene: %s", currentScene->getName().c_str());
    ImGui::Separator();

    if (ImGui::Button("Add Object")) {
        ImGui::OpenPopup("AddObjectPopup");
    }

    if (ImGui::BeginPopup("AddObjectPopup")) {
        if (ImGui::MenuItem("Empty Object")) {
            Haruka::SceneObject obj;
            obj.name = "GameObject";
            obj.type = "Empty";
            obj.position = glm::vec3(0.0f);
            obj.rotation = glm::vec3(0.0f);
            obj.scale = glm::vec3(1.0f);
            
            if (commandHistory) {
                commandHistory->execute(std::make_unique<AddObjectCommand>(currentScene, obj));
            } else {
                currentScene->addObject(obj);
            }
        }
        if (ImGui::MenuItem("Light")) {
            Haruka::SceneObject light;
            light.name = "Light";
            light.type = "Light";
            light.position = glm::vec3(0.0f, 5.0f, 0.0f);
            light.color = glm::vec3(1.0f);
            light.intensity = 1.0f;
            light.scale = glm::vec3(1.0f);
            
            if (commandHistory) {
                commandHistory->execute(std::make_unique<AddObjectCommand>(currentScene, light));
            } else {
                currentScene->addObject(light);
            }
        }
        if (ImGui::MenuItem("Model")) {
            Haruka::SceneObject model;
            model.name = "Model";
            model.type = "Model";
            model.position = glm::vec3(0.0f);
            model.scale = glm::vec3(1.0f);
            
            if (commandHistory) {
                commandHistory->execute(std::make_unique<AddObjectCommand>(currentScene, model));
            } else {
                currentScene->addObject(model);
            }
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    const auto& objects = currentScene->getObjects();
    for (size_t i = 0; i < objects.size(); i++) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (selectedObjectIndex == (int)i) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::TreeNodeEx((void*)(intptr_t)i, flags, "%s", objects[i].name.c_str());
        
        if (ImGui::IsItemClicked()) {
            selectedObjectIndex = i;
        }
        
        showContextMenu(i);
    }

    ImGui::End();
}