#include "scene_hierarchy.h"
#include "core/error_reporter.h"

#include "renderer/primitive_shapes.h"
#include "core/components/material_component.h"
#include "core/components/mesh_renderer_component.h"
#include "editor/commands/scene_commands.h"
#include <glm/glm.hpp>
#include <iostream>
#include <cstdint>

void SceneHierarchyPanel::setScene(Haruka::Scene* scene) {
    currentScene = scene;
}

void SceneHierarchyPanel::setSelectedObjectIndex(int index) {
    selectedObjectIndex = index;
}

void SceneHierarchyPanel::setCommandHistory(CommandHistory* history) {
    commandHistory = history;
}

void SceneHierarchyPanel::onImGuiRender() {
    ImGui::Begin("Scene Hierarchy");
    
    if (ImGui::Button("Add Cube", ImVec2(-1, 0))) {
        createPrimitive("Cube", "cube");
    }
    if (ImGui::Button("Add Sphere", ImVec2(-1, 0))) {
        createPrimitive("Sphere", "sphere");
    }
    if (ImGui::Button("Add Plane", ImVec2(-1, 0))) {
        createPrimitive("Plane", "plane");
    }
    if (ImGui::Button("Add Light", ImVec2(-1, 0))) {
        createPrimitive("Light", "light");
    }
    
    ImGui::Separator();
    
    if (currentScene) {
        const auto& objects = currentScene->getObjects();
        for (size_t i = 0; i < objects.size(); ++i) {
            try {
                if (objects[i].parentIndex == -1) {
                    renderObjectNode((int)i);
                }
            } catch (...) {
                std::cerr << "Error rendering object at index " << i << std::endl;
            }
        }
    }
    
    ImGui::End();
}

void SceneHierarchyPanel::createPrimitive(const std::string& name, const std::string& type) {
    if (!currentScene) return;
    
    Haruka::SceneObject obj;
    obj.name = name + "_" + std::to_string(currentScene->getObjects().size());
    obj.type = type == "light" ? "Light" : 
               type == "cube" ? "Cube" :
               type == "sphere" ? "Sphere" : "Plane";
    obj.position = glm::dvec3(0, 0, 0);
    obj.rotation = glm::dvec3(0, 0, 0);
    obj.scale = glm::dvec3(1, 1, 1);
    
    // Crear mesh
    obj.meshRenderer = std::make_shared<MeshRendererComponent>();
    std::vector<glm::vec3> verts, norms;
    std::vector<unsigned int> indices;
    
    if (type == "cube") {
        PrimitiveShapes::createCube(1.0f, verts, norms, indices);
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(0.8f, 0.8f, 0.8f);
    } else if (type == "sphere") {
        PrimitiveShapes::createSphere(1.0f, 32, 32, verts, norms, indices);
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(0.5f, 0.7f, 0.5f);
    } else if (type == "plane") {
        PrimitiveShapes::createPlane(2.0f, 2.0f, 10, verts, norms, indices);
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(0.7f, 0.7f, 0.7f);
    } else if (type == "light") {
        PrimitiveShapes::createSphere(0.5f, 16, 16, verts, norms, indices);
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(1.0f, 1.0f, 0.0f);
    }
    
    obj.meshRenderer->setMesh(verts, norms, indices);
    currentScene->addObject(obj);

	std::cout << "✓ Created " << obj.name << std::endl;
}

void SceneHierarchyPanel::renderObjectNode(int index) {
    if (!currentScene || index < 0 || index >= (int)currentScene->getObjects().size()) {
        return;
    }
    
    // Acceder al objeto sin mantener referencia (puede invalidarse)
    const auto& objects = currentScene->getObjects();
    if (index >= (int)objects.size()) return;  // Double check
    
    std::string objName = objects[index].name;
    bool hasChildren = !objects[index].childrenIndices.empty() || !objects[index].children.empty();
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (index == selectedObjectIndex) flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
    
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)index, flags, "%s", objName.c_str());
    
    if (ImGui::IsItemClicked()) {
        selectedObjectIndex = index;
        if (onObjectSelectedByIndex) onObjectSelectedByIndex(index);
        if (onObjectSelectedByName) onObjectSelectedByName(objName);
    }
    
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("SCENE_OBJECT", &index, sizeof(int));
        ImGui::Text("Move: %s", objName.c_str());
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
        // Re-validar índice antes de acceder
        if (index >= 0 && index < (int)currentScene->getObjects().size()) {
            const auto& obj = currentScene->getObjects()[index];
            
            for (int childIndex : obj.childrenIndices) {
                if (childIndex >= 0 && childIndex < (int)currentScene->getObjects().size()) {
                    renderObjectNode(childIndex);
                }
            }
            
            // Renderizar children vector (hijos del prefab, etc)
            for (size_t i = 0; i < obj.children.size(); ++i) {
                renderChildObject(obj.children[i], i);
            }
        }
        
        ImGui::TreePop();
    }
}

// Nueva función para renderizar objetos hijos del vector children
void SceneHierarchyPanel::renderChildObject(const Haruka::SceneObject& child, size_t index) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;
    
    // Usar un ID único basado en el nombre, no en el puntero (que puede ser inválido si el vector se realoca)
    std::string childId = child.name + "##child_" + std::to_string(index);
    bool nodeOpen = ImGui::TreeNodeEx(childId.c_str(), flags, "%s (%s)", child.name.c_str(), child.type.c_str());
    
    showContextMenuChild(child);
    
    if (nodeOpen) {
        ImGui::TreePop();
    }
}

void SceneHierarchyPanel::showContextMenuChild(const Haruka::SceneObject& child) {
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
