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

void SceneHierarchyPanel::onImGuiRender() {
    if (ImGui::Begin("Scene Hierarchy")) {
        if (currentScene) {
            for (size_t i = 0; i < currentScene->getObjects().size(); ++i) {
                if (currentScene->getObjects()[i].parentIndex == -1) {
                    renderObjectNode(i);
                }
            }
        }
    }
    ImGui::End();
}

void SceneHierarchyPanel::renderObjectNode(int index) {
    auto& obj = currentScene->getObjects()[index];
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (index == selectedObjectIndex) flags |= ImGuiTreeNodeFlags_Selected;
    if (obj.childrenIndices.empty() && obj.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)index, flags, "%s", obj.name.c_str());
    
    if (ImGui::IsItemClicked()) {
        selectedObjectIndex = index;
        if (onObjectSelectedByIndex) onObjectSelectedByIndex(index);
        if (onObjectSelectedByName) onObjectSelectedByName(obj.name);
    }
    
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("SCENE_OBJECT", &index, sizeof(int));
        ImGui::Text("Move: %s", obj.name.c_str());
        ImGui::EndDragDropSource();
    }
    
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_OBJECT")) {
            int draggedIndex = *(int*)payload->Data;
            reparentObject(draggedIndex, index);
        }
        ImGui::EndDragDropTarget();
    }
    
    showContextMenu(index);
    
    if (nodeOpen) {
        for (int childIndex : obj.childrenIndices) {
            renderObjectNode(childIndex);
        }
        
        // Renderizar children vector (hijos del prefab, etc)
        for (size_t i = 0; i < obj.children.size(); ++i) {
            renderChildObject(obj.children[i], i);
        }
        
        ImGui::TreePop();
    }
}

// Nueva función para renderizar objetos hijos del vector children
void SceneHierarchyPanel::renderChildObject(Haruka::SceneObject& child, size_t index) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;
    
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(&child), flags, "%s (%s)", child.name.c_str(), child.type.c_str());
    
    showContextMenuChild(child);
    
    if (nodeOpen) {
        ImGui::TreePop();
    }
}

void SceneHierarchyPanel::showContextMenuChild(Haruka::SceneObject& child) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Delete")) {
            // TODO: Implementar borrar hijo
        }
        ImGui::EndPopup();
    }
}

void SceneHierarchyPanel::reparentObject(int childIndex, int newParentIndex) {
    if (!currentScene || childIndex == newParentIndex) return;
    
    auto& child = currentScene->getObjects()[childIndex];
    
    if (child.parentIndex >= 0) {
        auto& oldParent = currentScene->getObjects()[child.parentIndex];
        oldParent.childrenIndices.erase(
            std::remove(oldParent.childrenIndices.begin(), oldParent.childrenIndices.end(), childIndex),
            oldParent.childrenIndices.end()
        );
    }
    
    child.parentIndex = newParentIndex;
    currentScene->getObjects()[newParentIndex].childrenIndices.push_back(childIndex);
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
        auto& obj = currentScene->getObjects()[index];
        
        // Abrir escena si el tipo es "Scene"
        if (obj.type == "Scene") {
            if (ImGui::MenuItem("Enter Scene")) {
                std::string scenePath = currentProjectPath + "/scenes/" + obj.name + ".scene";
                currentScene->load(scenePath);
            }
        }
        
        // Abrir prefab si el tipo es "Prefab"
        if (obj.type == "Prefab") {
            if (ImGui::MenuItem("Enter Prefab")) {
                std::string prefabPath = currentProjectPath + "/assets/prefabs/" + obj.name + ".prefab";
                currentScene->load(prefabPath);
            }
        }
        
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
