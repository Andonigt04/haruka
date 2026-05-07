#define GLM_ENABLE_EXPERIMENTAL
#include "scene_hierarchy.h"
#include "tools/events.h"

#include "renderer/primitive_shapes.h"
#include "core/object_types.h"
#include "core/components/material_component.h"
#include "core/components/mesh_renderer_component.h"
#include "commands/scene_commands.h"
#include <glm/glm.hpp>
#include <iostream>
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <cstdlib>

// ---------------------------------------------------------------------------
// createPrimitive — posts a minimal Created event; engine applies defaults
// ---------------------------------------------------------------------------

void SceneHierarchyPanel::createPrimitive(const std::string& name,
                                          const Haruka::PrimitiveType& type,
                                          int parentIndex)
{
    if (!eventManager) {
        std::cerr << "[SceneHierarchy] No EventManager — cannot create " << name << "\n";
        return;
    }

    nlohmann::json data;
    data["parentIndex"] = parentIndex; // -1 = root

    std::string objName = name + "_" + std::to_string(rand() & 0xFFFF);

    eventManager->post(std::make_shared<Haruka::ObjectEvent>(
        objName, Haruka::primitiveTypeToString(type), Haruka::ObjectEvent::ActionType::Created, data));
    eventManager->post(std::make_shared<Haruka::LogEvent>(
        Haruka::LogEvent::Level::Info, "Create requested: " + objName + " [" + Haruka::primitiveTypeToString(type) + "]"));
}

// ---------------------------------------------------------------------------
// onImGuiRender
// ---------------------------------------------------------------------------

void SceneHierarchyPanel::onImGuiRender() {
    ImGui::Begin("Scene Hierarchy");

    if (ImGui::Button("+##AddObject", ImVec2(40, 0))) {
        showObjectBrowser = true;
        objectSearchBuffer[0] = '\0';
    }
    ImGui::SameLine();
    if (ImGui::Button("Cube", ImVec2(-1, 0)))
        createPrimitive("Cube", Haruka::PrimitiveType::CUBE);

    if (showObjectBrowser) ImGui::OpenPopup("Object Browser##Modal");

    if (ImGui::BeginPopupModal("Object Browser##Modal", &showObjectBrowser,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Search:");
        ImGui::InputText("##ObjSearch", objectSearchBuffer, sizeof(objectSearchBuffer));
        ImGui::Separator();

        std::string query = objectSearchBuffer;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        ImGui::BeginChild("ObjectList", ImVec2(0, 200));
        for (const auto& [displayName, typeStr] : objectTypes) {
            std::string lower = displayName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (query.empty() || lower.find(query) != std::string::npos) {
                if (ImGui::Selectable(displayName.c_str())) {
                    createPrimitive(displayName, Haruka::stringToPrimitiveType(typeStr));
                    showObjectBrowser = false;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button("Close##ObjBrowser", ImVec2(120, 0))) {
            showObjectBrowser = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    if (currentScene) {
        const auto& objects = currentScene->getAllObjects();

        // Rebuild children map each frame from parentIndex.
        m_childrenMap.assign(objects.size(), {});
        for (int i = 0; i < (int)objects.size(); ++i) {
            int p = objects[i]->parentIndex;
            if (p >= 0 && p < (int)objects.size())
                m_childrenMap[p].push_back(i);
        }

        for (int i = 0; i < (int)objects.size(); ++i) {
            if (objects[i]->parentIndex == -1)
                renderObjectNode(i);
        }
    }

    ImGui::End();
}

void SceneHierarchyPanel::createPrimitive(const std::string& name, const std::string& type) {
    if (!currentScene) return;
    
    Haruka::SceneObject obj;
    obj.name = name + "_" + std::to_string(currentScene->getObjects().size());
    obj.type = type == "light" ? "Light" :
               type == "pointlight" ? "PointLight" :
               type == "directionallight" ? "DirectionalLight" :
               type == "cube" ? "Cube" :
               type == "sphere" ? "Sphere" :
               type == "capsule" ? "Capsule" :
               type == "plane" ? "Plane" :
               type == "sun" ? "Light" :
               type == "planet" ? "Mesh" : "Mesh";
    obj.position = glm::dvec3(0, 0, 0);
    obj.rotation = glm::dvec3(0, 0, 0);
    obj.scale = glm::dvec3(1, 1, 1);
    
    // Crear mesh
    obj.meshRenderer = std::make_shared<MeshRendererComponent>();
    std::vector<glm::vec3> verts, norms;
    std::vector<unsigned int> indices;
    
    using PT = Haruka::PrimitiveMeshType;
    auto setMeshType = [&](PT t) {
        obj.properties["meshRenderer"]["meshType"] = Haruka::primitiveMeshTypeToString(t);
    };

    if (type == "cube") {
        PrimitiveShapes::createCube(1.0f, verts, norms, indices);
        setMeshType(PT::CUBE);
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(0.8f, 0.8f, 0.8f);
    } else if (type == "sphere") {
        PrimitiveShapes::createSphere(1.0f, 32, 32, verts, norms, indices);
        setMeshType(PT::SPHERE);
        obj.properties["meshRenderer"]["radius"] = 1.0f;
        obj.properties["meshRenderer"]["segments"] = 32;
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(0.5f, 0.7f, 0.5f);
    } else if (type == "capsule") {
        PrimitiveShapes::createCapsule(0.5f, 2.0f, 24, 16, verts, norms, indices);
        setMeshType(PT::CAPSULE);
        obj.properties["meshRenderer"]["radius"] = 0.5f;
        obj.properties["meshRenderer"]["height"] = 2.0f;
        obj.properties["meshRenderer"]["segments"] = 24;
        obj.properties["meshRenderer"]["stacks"] = 16;
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(0.65f, 0.65f, 0.68f);
        obj.color = glm::vec3(0.65f, 0.65f, 0.68f);
        obj.scale = glm::dvec3(0.00095f);
    } else if (type == "plane") {
        PrimitiveShapes::createPlane(2.0f, 2.0f, 10, verts, norms, indices);
        setMeshType(PT::PLANE);
        obj.properties["meshRenderer"]["width"] = 2.0f;
        obj.properties["meshRenderer"]["height"] = 2.0f;
        obj.properties["meshRenderer"]["subdivisions"] = 10;
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(0.7f, 0.7f, 0.7f);
    } else if (type == "light" || type == "pointlight") {
        PrimitiveShapes::createSphere(0.5f, 16, 16, verts, norms, indices);
        setMeshType(PT::SPHERE);
        obj.properties["meshRenderer"]["radius"] = 0.5f;
        obj.properties["meshRenderer"]["segments"] = 16;
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(1.0f, 1.0f, 0.0f);
        obj.color = glm::vec3(1.0f, 1.0f, 0.8f);
        obj.intensity = 2.0f;
    } else if (type == "directionallight") {
        PrimitiveShapes::createCube(0.2f, verts, norms, indices);
        setMeshType(PT::CUBE);
        obj.properties["meshRenderer"]["size"] = 0.2f;
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(1.0f, 0.95f, 0.8f);
        obj.color = glm::vec3(1.0f, 0.95f, 0.8f);
        obj.intensity = 1.0f;
    } else if (type == "sun") {
        using namespace Haruka::Units;
        const double sunRadiusKm = STAR_RADIUS_MEDIUM / KM;
        const double sunScale = kmToRender(sunRadiusKm);

        PrimitiveShapes::createSphere(1.0f, 32, 32, verts, norms, indices);
        setMeshType(PT::SPHERE);
        obj.properties["meshRenderer"]["radius"] = 1.0f;
        obj.properties["meshRenderer"]["segments"] = 32;
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(1.0f, 0.95f, 0.75f);
        obj.color = glm::vec3(1.0f, 0.95f, 0.75f);
        obj.intensity = 20.0f;
        obj.scale = glm::dvec3(sunScale);
        obj.type = "Light";
    } else if (type == "planet") {
        using namespace Haruka::Units;
        const double planetRadiusKm = PLANETARY_RADIUS_MEDIUM / KM; // km value
        const double planetScale = kmToRender(planetRadiusKm);

        PrimitiveShapes::createSphere(1.0f, 48, 48, verts, norms, indices);
        setMeshType(PT::SPHERE);
        obj.properties["meshRenderer"]["radius"] = 1.0f;
        obj.properties["meshRenderer"]["segments"] = 48;
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(0.25f, 0.45f, 1.0f);
        obj.color = glm::vec3(0.25f, 0.45f, 1.0f);
        obj.scale = glm::dvec3(planetScale);
        obj.type = "Mesh";
        obj.properties["terrainEditor"]["isPlanetRoot"] = true;
        obj.properties["terrainEditor"]["tilesPerFace"] = 8;
        obj.properties["terrainEditor"]["maxLod"] = 3;
        obj.properties["terrainEditor"]["generator"]["seed"] = 42;
        obj.properties["terrainEditor"]["generator"]["baseRadiusKm"] = planetRadiusKm;
        obj.properties["terrainEditor"]["generator"]["enableContinents"] = true;
        obj.properties["terrainEditor"]["generator"]["enableMountains"] = true;
        obj.properties["terrainEditor"]["generator"]["seaLevel"] = 0.52f;
        obj.properties["terrainEditor"]["generator"]["continentFrequency"] = 1.2f;
        obj.properties["terrainEditor"]["generator"]["continentHeightStrength"] = 0.08f;
        obj.properties["terrainEditor"]["generator"]["macroFrequency"] = 3.5f;
        obj.properties["terrainEditor"]["generator"]["macroHeightStrength"] = 0.15f;
        obj.properties["terrainEditor"]["generator"]["detailFrequency"] = 12.0f;
        obj.properties["terrainEditor"]["generator"]["detailHeightStrength"] = 0.03f;
        obj.properties["terrainEditor"]["generator"]["persistence"] = 0.5f;
        obj.properties["terrainEditor"]["generator"]["lacunarity"] = 2.0f;
    }
    
    obj.meshRenderer->setMesh(verts, norms, indices);
    currentScene->addObject(obj);

	std::cout << "✓ Created " << obj.name << std::endl;
}

void SceneHierarchyPanel::renderObjectNode(int index) {
    if (!currentScene || index < 0 ||
        index >= (int)currentScene->getAllObjects().size()) return;

    const Haruka::SceneObject& obj = *currentScene->getAllObjects()[index];

    ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (index == selectedObjectIndex) flags |= ImGuiTreeNodeFlags_Selected;
    if (m_childrenMap[index].empty())  flags |= ImGuiTreeNodeFlags_Leaf;

    bool nodeOpen = ImGui::TreeNodeEx(
        (obj.name + " (" + obj.type + ")##" + std::to_string(index)).c_str(), flags);

    if (ImGui::IsItemClicked()) {
        selectedObjectIndex = index;
        if (onObjectSelectedByIndex) onObjectSelectedByIndex(index);
        if (onObjectSelectedByName)  onObjectSelectedByName(obj.name);
        if (eventManager) {
            eventManager->post(std::make_shared<Haruka::ObjectEvent>(
                obj.name, obj.type, Haruka::ObjectEvent::ActionType::Selected));
        }
    }

    // Drag-and-drop for reparenting.
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("SCENE_OBJ_IDX", &index, sizeof(int));
        ImGui::Text("%s", obj.name.c_str());
        ImGui::EndDragDropSource();
    }
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("SCENE_OBJ_IDX")) {
            int dragged = *(const int*)p->Data;
            if (dragged != index && eventManager) {
                nlohmann::json d;
                d["newParentIndex"] = index;
                const auto& draggedObj = currentScene->getAllObjects()[dragged];
                eventManager->post(std::make_shared<Haruka::ObjectEvent>(
                    draggedObj->name, draggedObj->type,
                    Haruka::ObjectEvent::ActionType::Reparented, d));
            }
        }
        ImGui::EndDragDropTarget();
    }

    showContextMenu(index);

    if (nodeOpen) {
        for (int childIdx : m_childrenMap[index])
            renderObjectNode(childIdx);
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
    
    auto& objects = currentScene->getObjects();
    if (childIndex < 0 || childIndex >= (int)objects.size()) return;

    auto& child = objects[childIndex];
    glm::mat4 childWorld = composeLocalTransform(child.position, child.rotation, child.scale);
    if (child.parentIndex >= 0 && child.parentIndex < (int)objects.size()) {
        childWorld = objects[child.parentIndex].getWorldTransform(currentScene) * childWorld;
    }

    glm::mat4 parentWorld(1.0f);
    if (newParentIndex >= 0 && newParentIndex < (int)objects.size()) {
        parentWorld = objects[newParentIndex].getWorldTransform(currentScene);
    }
    
    if (child.parentIndex >= 0) {
        auto& oldParent = objects[child.parentIndex];
        oldParent.childrenIndices.erase(
            std::remove(oldParent.childrenIndices.begin(), oldParent.childrenIndices.end(), childIndex),
            oldParent.childrenIndices.end()
        );
    }
    
    child.parentIndex = newParentIndex;
    if (newParentIndex >= 0 && newParentIndex < (int)objects.size()) {
        objects[newParentIndex].childrenIndices.push_back(childIndex);
    }

    glm::mat4 local = glm::inverse(parentWorld) * childWorld;
    decomposeTransform(local, child.position, child.rotation, child.scale);
}

void SceneHierarchyPanel::createChildObject(int parentIndex, const std::string& primitiveType) {
    if (!currentScene) return;
    auto& objects = currentScene->getObjects();
    if (parentIndex < 0 || parentIndex >= (int)objects.size()) return;

    Haruka::SceneObject child;
    child.name = primitiveType + "_child_" + std::to_string(objects.size());
    child.type = primitiveType == "Light" ? "Light" : "Mesh";
    child.position = glm::dvec3(0.0, 2.0, 0.0); // local offset respecto al padre
    child.rotation = glm::dvec3(0.0);
    child.scale = glm::dvec3(1.0);
    child.color = primitiveType == "Light" ? glm::dvec3(1.0, 0.95, 0.8) : glm::dvec3(0.7);
    child.intensity = primitiveType == "Light" ? 2.0 : 1.0;

    child.meshRenderer = std::make_shared<MeshRendererComponent>();
    std::vector<glm::vec3> verts, norms;
    std::vector<unsigned int> indices;
    using CPT = Haruka::PrimitiveMeshType;
    auto setChildMeshType = [&](CPT t) {
        child.properties["meshRenderer"]["meshType"] = Haruka::primitiveMeshTypeToString(t);
    };
    if (primitiveType == "Sphere") {
        PrimitiveShapes::createSphere(1.0f, 24, 24, verts, norms, indices);
        setChildMeshType(CPT::SPHERE);
        child.properties["meshRenderer"]["radius"] = 1.0f;
        child.properties["meshRenderer"]["segments"] = 24;
    } else if (primitiveType == "Light") {
        PrimitiveShapes::createSphere(0.4f, 16, 16, verts, norms, indices);
        setChildMeshType(CPT::SPHERE);
        child.properties["meshRenderer"]["radius"] = 0.4f;
        child.properties["meshRenderer"]["segments"] = 16;
    } else {
        PrimitiveShapes::createCube(1.0f, verts, norms, indices);
        setChildMeshType(CPT::CUBE);
        child.properties["meshRenderer"]["size"] = 1.0f;
    }
    child.meshRenderer->setMesh(verts, norms, indices);
    child.material = std::make_shared<Haruka::MaterialComponent>();
    child.material->albedo = glm::vec3(child.color);

    child.parentIndex = parentIndex;
    currentScene->addObject(child);
    int childIndex = (int)currentScene->getObjects().size() - 1;
    currentScene->getObjects()[parentIndex].childrenIndices.push_back(childIndex);
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
    if (!ImGui::BeginPopupContextItem()) return;

    const Haruka::SceneObject& obj = *currentScene->getAllObjects()[index];

    if (ImGui::BeginMenu("Create Child")) {
        for (const auto& [displayName, typeStr] : objectTypes) {
            if (ImGui::MenuItem(displayName.c_str()))
                createPrimitive(displayName, Haruka::stringToPrimitiveType(typeStr), index);
        }
        ImGui::EndMenu();
    }

    if (ImGui::MenuItem("Duplicate")) {
        if (eventManager) {
            nlohmann::json d;
            d["parentIndex"] = obj.parentIndex;
            eventManager->post(std::make_shared<Haruka::ObjectEvent>(
                obj.name, obj.type, Haruka::ObjectEvent::ActionType::Duplicated, d));
        }
    }

    if (ImGui::MenuItem("Delete")) {
        if (eventManager) {
            eventManager->post(std::make_shared<Haruka::ObjectEvent>(
                obj.name, obj.type, Haruka::ObjectEvent::ActionType::Deleted));
            eventManager->post(std::make_shared<Haruka::LogEvent>(
                Haruka::LogEvent::Level::Info, "Delete requested: " + obj.name));
        }
        selectedObjectIndex = -1;
    }

    ImGui::EndPopup();
}
