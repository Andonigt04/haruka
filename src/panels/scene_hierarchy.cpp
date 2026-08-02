#define GLM_ENABLE_EXPERIMENTAL
#include "scene_hierarchy.h"
#include "tools/error_reporter.h"
#include "tools/math_types.h"
#include "editor_util.h"

#include "renderer/primitive_shapes.h"
#include "core/components/material_component.h"
#include "core/components/mesh_renderer_component.h"
#include "commands/scene_commands.h"
#include <glm/glm.hpp>
#include <iostream>
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

namespace {

void decomposeTransform(const glm::mat4& transform, glm::dvec3& position, glm::dvec3& rotation, glm::dvec3& scale) {
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::vec3 translation;
    glm::quat orientation;
    glm::vec3 localScale;

    glm::decompose(transform, localScale, orientation, translation, skew, perspective);
    position = glm::dvec3(translation);
    scale = glm::dvec3(localScale);
    rotation = glm::dvec3(glm::degrees(glm::eulerAngles(orientation)));
}

glm::mat4 computeWorldTransform(const Haruka::SceneObject& obj, const std::vector<std::shared_ptr<Haruka::SceneObject>>& allObjs) {
    glm::mat4 wt = EditorUtil::composeLocalTransform(obj.position, EditorUtil::rotationToEuler(obj.rotation), obj.scale);
    int p = obj.parentIndex;
    while (p >= 0 && p < (int)allObjs.size()) {
        auto& parent = allObjs[p];
        if (!parent) break;
        wt = EditorUtil::composeLocalTransform(parent->position, EditorUtil::rotationToEuler(parent->rotation), parent->scale) * wt;
        p = parent->parentIndex;
    }
    return wt;
}

}

void SceneHierarchyPanel::setScene(Haruka::SceneManager* scene) {
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
    
    if (ImGui::Button("+##AddObject", ImVec2(40, 0))) {
        showObjectBrowser = true;
        objectSearchBuffer[0] = '\0';
    }
    
    ImGui::SameLine();
    if (ImGui::Button("Cube", ImVec2(-1, 0))) {
        createPrimitive("Cube", "cube");
    }
    
    if (showObjectBrowser) {
        ImGui::OpenPopup("Object Browser##Modal");
    }
    
    if (ImGui::BeginPopupModal("Object Browser##Modal", &showObjectBrowser, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Buscar objeto:");
        ImGui::InputText("##ObjectSearch", objectSearchBuffer, sizeof(objectSearchBuffer));
        ImGui::Separator();
        
        std::string searchStr = objectSearchBuffer;
        std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
        
        std::vector<std::pair<std::string, std::string>> objectTypes = {
            {"Cube", "cube"},
            {"Sphere", "sphere"},
            {"Plane", "plane"},
            {"Capsule", "capsule"},
            {"Light", "light"},
            {"Point Light", "pointlight"},
            {"Directional Light", "directionallight"},
            {"Sun", "sun"},
            {"Cylinder", "cylinder"},
            {"Torus", "torus"},
            {"Model Loader", "model"},
        };
        
        ImGui::BeginChild("ObjectList", ImVec2(0, 200));
        for (const auto& [displayName, type] : objectTypes) {
            std::string lowerName = displayName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            
            if (searchStr.empty() || lowerName.find(searchStr) != std::string::npos) {
                if (ImGui::Selectable(displayName.c_str())) {
                    createPrimitive(displayName, type);
                    showObjectBrowser = false;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndChild();
        
        ImGui::Separator();
        if (ImGui::Button("Cerrar##ObjectBrowser", ImVec2(120, 0))) {
            showObjectBrowser = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
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
    
    auto obj = std::make_shared<Haruka::SceneObject>();
    obj->name = name + "_" + std::to_string(currentScene->getObjects().size());
    obj->type = type == "light" ? "Light" :
               type == "pointlight" ? "PointLight" :
               type == "directionallight" ? "DirectionalLight" :
               type == "cube" ? "Cube" :
               type == "sphere" ? "Sphere" :
               type == "capsule" ? "Capsule" :
               type == "plane" ? "Plane" :
               type == "sun" ? "Light" : "Mesh";
    obj->position = glm::dvec3(0, 0, 0);
    obj->rotation = EditorUtil::eulerToRotation(glm::dvec3(0, 0, 0));
    obj->scale = glm::dvec3(1, 1, 1);
    
    obj->meshRenderer = std::make_shared<MeshRendererComponent>();
    std::vector<glm::vec3> verts, norms;
    std::vector<unsigned int> indices;
    
    if (type == "cube") {
        PrimitiveShapes::createCube(1.0f, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "cube";
        obj->material = std::make_shared<Haruka::MaterialComponent>();
        obj->material->albedo = glm::vec3(0.8f, 0.8f, 0.8f);
    } else if (type == "sphere") {
        PrimitiveShapes::createSphere(1.0f, 32, 32, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "sphere";
        obj->properties["meshRenderer"]["radius"] = 1.0f;
        obj->properties["meshRenderer"]["segments"] = 32;
        obj->material = std::make_shared<Haruka::MaterialComponent>();
        obj->material->albedo = glm::vec3(0.5f, 0.7f, 0.5f);
    } else if (type == "capsule") {
        PrimitiveShapes::createCapsule(0.5f, 2.0f, 24, 16, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "capsule";
        obj->properties["meshRenderer"]["radius"] = 0.5f;
        obj->properties["meshRenderer"]["height"] = 2.0f;
        obj->properties["meshRenderer"]["segments"] = 24;
        obj->properties["meshRenderer"]["stacks"] = 16;
        obj->material = std::make_shared<Haruka::MaterialComponent>();
        obj->material->albedo = glm::vec3(0.65f, 0.65f, 0.68f);
        obj->color = glm::dvec3(0.65f, 0.65f, 0.68f);
        obj->scale = glm::dvec3(0.00095f);
    } else if (type == "plane") {
        PrimitiveShapes::createPlane(2.0f, 2.0f, 10, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "plane";
        obj->properties["meshRenderer"]["width"] = 2.0f;
        obj->properties["meshRenderer"]["height"] = 2.0f;
        obj->properties["meshRenderer"]["subdivisions"] = 10;
        obj->material = std::make_shared<Haruka::MaterialComponent>();
        obj->material->albedo = glm::vec3(0.7f, 0.7f, 0.7f);
    } else if (type == "light" || type == "pointlight") {
        PrimitiveShapes::createSphere(0.5f, 16, 16, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "sphere";
        obj->properties["meshRenderer"]["radius"] = 0.5f;
        obj->properties["meshRenderer"]["segments"] = 16;
        obj->material = std::make_shared<Haruka::MaterialComponent>();
        obj->material->albedo = glm::vec3(1.0f, 1.0f, 0.0f);
        obj->color = glm::dvec3(1.0f, 1.0f, 0.8f);
        obj->intensity = 2.0f;
    } else if (type == "directionallight") {
        PrimitiveShapes::createCube(0.2f, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "cube";
        obj->properties["meshRenderer"]["size"] = 0.2f;
        obj->material = std::make_shared<Haruka::MaterialComponent>();
        obj->material->albedo = glm::vec3(1.0f, 0.95f, 0.8f);
        obj->color = glm::dvec3(1.0f, 0.95f, 0.8f);
        obj->intensity = 1.0f;
    } else if (type == "sun") {
        PrimitiveShapes::createSphere(1.0f, 32, 32, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "sphere";
        obj->properties["meshRenderer"]["radius"] = 1.0f;
        obj->properties["meshRenderer"]["segments"] = 32;
        obj->material = std::make_shared<Haruka::MaterialComponent>();
        obj->material->albedo = glm::vec3(1.0f, 0.95f, 0.75f);
        obj->color = glm::dvec3(1.0f, 0.95f, 0.75f);
        obj->intensity = 20.0f;
        obj->scale = glm::dvec3(50.0);
        obj->type = "Light";
    }
    
    obj->meshRenderer->setMesh(verts, norms, std::vector<glm::vec3>(), indices);

    if (commandHistory) {
        commandHistory->execute(std::make_unique<AddObjectCommand>(currentScene, *obj));
    } else {
        currentScene->addLoadedObject(obj);
    }

    std::cout << "✓ Created " << obj->name << std::endl;
}

void SceneHierarchyPanel::renderObjectNode(int index) {
    if (!currentScene || index < 0 || index >= (int)currentScene->getObjects().size()) {
        return;
    }
    
    const auto& objects = currentScene->getObjects();
    if (index >= (int)objects.size()) return;
    
    std::string objName = objects[index].name;
    bool hasChildren = !objects[index].childrenIndices.empty();
    
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (index == selectedObjectIndex) flags |= ImGuiTreeNodeFlags_Selected;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
    
    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)index, flags, "%s", objName.c_str());
    
    if (ImGui::IsItemClicked()) {
        selectedObjectIndex = index;
        if (onObjectSelectedByIndex) onObjectSelectedByIndex(index);
        if (onObjectSelectedByName) onObjectSelectedByName(objName);
    }
    // DOBLE clic = encuadrar. Separado de la selección a propósito: seleccionar un objeto para
    // editarle una propiedad no debe mover la cámara.
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (onObjectFocused) onObjectFocused(index);
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
        if (index >= 0 && index < (int)currentScene->getObjects().size()) {
            const auto& obj = currentScene->getObjects()[index];
            
            for (int childIndex : obj.childrenIndices) {
                if (childIndex >= 0 && childIndex < (int)currentScene->getObjects().size()) {
                    renderObjectNode(childIndex);
                }
            }
        }
        
        ImGui::TreePop();
    }
}

void SceneHierarchyPanel::renderChildObject(const Haruka::SceneObject& child, size_t index) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_Leaf;
    
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
        }
        ImGui::EndPopup();
    }
}

void SceneHierarchyPanel::reparentObject(int childIndex, int newParentIndex) {
    if (!currentScene || childIndex == newParentIndex) return;
    
    auto& objects = currentScene->getObjectsMutable();
    if (childIndex < 0 || childIndex >= (int)objects.size()) return;

    auto& child = objects[childIndex];
    if (!child) return;

    glm::mat4 childWorld = EditorUtil::composeLocalTransform(child->position, EditorUtil::rotationToEuler(child->rotation), child->scale);
    if (child->parentIndex >= 0 && child->parentIndex < (int)objects.size()) {
        childWorld = computeWorldTransform(*objects[child->parentIndex], objects) * childWorld;
    }

    glm::mat4 parentWorld(1.0f);
    if (newParentIndex >= 0 && newParentIndex < (int)objects.size()) {
        parentWorld = computeWorldTransform(*objects[newParentIndex], objects);
    }
    
    if (child->parentIndex >= 0) {
        auto& oldParent = objects[child->parentIndex];
        if (oldParent) {
            oldParent->childrenIndices.erase(
                std::remove(oldParent->childrenIndices.begin(), oldParent->childrenIndices.end(), childIndex),
                oldParent->childrenIndices.end()
            );
        }
    }
    
    child->parentIndex = newParentIndex;
    if (newParentIndex >= 0 && newParentIndex < (int)objects.size()) {
        objects[newParentIndex]->childrenIndices.push_back(childIndex);
    }

    glm::mat4 local = glm::inverse(parentWorld) * childWorld;
    glm::dvec3 eulerRot;
    decomposeTransform(local, child->position, eulerRot, child->scale);
    child->rotation = EditorUtil::eulerToRotation(eulerRot);
}

void SceneHierarchyPanel::createChildObject(int parentIndex, const std::string& primitiveType) {
    if (!currentScene) return;
    auto& objects = currentScene->getObjectsMutable();
    if (parentIndex < 0 || parentIndex >= (int)objects.size()) return;

    auto child = std::make_shared<Haruka::SceneObject>();
    child->name = primitiveType + "_child_" + std::to_string(objects.size());
    child->type = primitiveType == "Light" ? "Light" : "Mesh";
    child->position = glm::dvec3(0.0, 2.0, 0.0);
    child->rotation = EditorUtil::eulerToRotation(glm::dvec3(0.0));
    child->scale = glm::dvec3(1.0);
    child->color = primitiveType == "Light" ? glm::dvec3(1.0, 0.95, 0.8) : glm::dvec3(0.7);
    child->intensity = primitiveType == "Light" ? 2.0 : 1.0;

    child->meshRenderer = std::make_shared<MeshRendererComponent>();
    std::vector<glm::vec3> verts, norms;
    std::vector<unsigned int> indices;
    if (primitiveType == "Sphere") {
        PrimitiveShapes::createSphere(1.0f, 24, 24, verts, norms, indices);
        child->properties["meshRenderer"]["meshType"] = "sphere";
        child->properties["meshRenderer"]["radius"] = 1.0f;
        child->properties["meshRenderer"]["segments"] = 24;
    } else if (primitiveType == "Light") {
        PrimitiveShapes::createSphere(0.4f, 16, 16, verts, norms, indices);
        child->properties["meshRenderer"]["meshType"] = "sphere";
        child->properties["meshRenderer"]["radius"] = 0.4f;
        child->properties["meshRenderer"]["segments"] = 16;
    } else {
        PrimitiveShapes::createCube(1.0f, verts, norms, indices);
        child->properties["meshRenderer"]["meshType"] = "cube";
        child->properties["meshRenderer"]["size"] = 1.0f;
    }
    child->meshRenderer->setMesh(verts, norms, std::vector<glm::vec3>(), indices);
    child->material = std::make_shared<Haruka::MaterialComponent>();
    child->material->albedo = glm::vec3(child->color);

    child->parentIndex = parentIndex;
    currentScene->addLoadedObject(child);
    int childIndex = (int)objects.size() - 1;
    objects[parentIndex]->childrenIndices.push_back(childIndex);
}

void SceneHierarchyPanel::duplicateObject(int index) {
    if (!currentScene || index < 0 || index >= (int)currentScene->getObjects().size()) return;
    
    const auto& original = currentScene->getObjects()[index];
    auto duplicate = std::make_shared<Haruka::SceneObject>(original);
    duplicate->name = original.name + "_copy";
    duplicate->position += glm::dvec3(1.0f, 0.0f, 0.0f);
    
    if (commandHistory) {
        commandHistory->execute(std::make_unique<AddObjectCommand>(currentScene, *duplicate));
    } else {
        currentScene->addLoadedObject(duplicate);
    }
}

void SceneHierarchyPanel::showContextMenu(int index) {
    if (ImGui::BeginPopupContextItem()) {
        const auto& objects = currentScene->getObjects();
        if (index < 0 || index >= (int)objects.size()) {
            ImGui::EndPopup();
            return;
        }
        const auto& obj = objects[index];
        
        if (obj.type == "Scene") {
            if (ImGui::MenuItem("Enter Scene")) {
                std::string scenePath = currentProjectPath + "/scenes/" + obj.name + ".scene";
                currentScene->load(scenePath);
            }
        }
        
        if (obj.type == "Prefab") {
            if (ImGui::MenuItem("Enter Prefab")) {
                std::string prefabPath = currentProjectPath + "/assets/prefabs/" + obj.name + ".prefab";
                currentScene->load(prefabPath);
            }
        }
        
        if (ImGui::MenuItem("Duplicate")) {
            duplicateObject(index);
        }

        if (ImGui::BeginMenu("Create Child")) {
            if (ImGui::MenuItem("Cube")) createChildObject(index, "Cube");
            if (ImGui::MenuItem("Sphere")) createChildObject(index, "Sphere");
            if (ImGui::MenuItem("Light")) createChildObject(index, "Light");
            ImGui::EndMenu();
        }
        
        if (ImGui::MenuItem("Delete")) {
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
