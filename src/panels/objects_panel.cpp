#define GLM_ENABLE_EXPERIMENTAL
#include "objects_panel.h"

#include "tools/error_reporter.h"
#include "tools/math_types.h"
#include "editor_util.h"

#include "renderer/primitive_shapes.h"
#include "core/components/material_component.h"
#include "core/components/mesh_renderer_component.h"
#include "commands/scene_commands.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iostream>
#include <map>
#include <tuple>

// ---------------------------------------------------------------------------
// Sistema de capas
// ---------------------------------------------------------------------------
const std::vector<std::string>& ObjectsPanel::defaultLayers() {
    static const std::vector<std::string> s = {
        "Default", "Props", "Trees", "Monsters", "Spawns", "Characters", "Planets", "Lights"
    };
    return s;
}

std::string ObjectsPanel::objectLayer(const Haruka::SceneObject& obj) {
    if (obj.properties.is_object() && obj.properties.contains("layer") &&
        obj.properties["layer"].is_string()) {
        return obj.properties["layer"].get<std::string>();
    }
    return "Default";
}

void ObjectsPanel::setObjectLayer(Haruka::SceneObject& obj, const std::string& layer) {
    if (!obj.properties.is_object()) obj.properties = nlohmann::json::object();
    obj.properties["layer"] = layer;
}

// ---------------------------------------------------------------------------
// Configuración
// ---------------------------------------------------------------------------
void ObjectsPanel::setScene(Haruka::SceneManager* scene) {
    currentScene = scene;
    selectedObjectIndex = -1;
}

void ObjectsPanel::setCommandHistory(CommandHistory* history) {
    commandHistory = history;
}

void ObjectsPanel::setSelectedObjectIndex(int index) {
    selectedObjectIndex = index;
}

void ObjectsPanel::setOnObjectSelectedByIndex(std::function<void(int)> cb) {
    onObjectSelectedByIndex = std::move(cb);
}

void ObjectsPanel::setOnObjectSelectedByName(std::function<void(const std::string&)> cb) {
    onObjectSelectedByName = std::move(cb);
}

// ---------------------------------------------------------------------------
// Helpers de creación
// ---------------------------------------------------------------------------
std::string ObjectsPanel::nextName(const std::string& prefix) {
    int n = currentScene ? (int)currentScene->getObjects().size() : 0;
    std::string base = prefix + "_" + std::to_string(n);
    int i = n;
    while (currentScene && currentScene->getObject(base)) {
        base = prefix + "_" + std::to_string(++i);
    }
    return base;
}

std::shared_ptr<Haruka::SceneObject> ObjectsPanel::makeMeshObject(const std::string& name,
                                                                  const std::string& meshType,
                                                                  const glm::vec3& albedo) {
    auto obj = std::make_shared<Haruka::SceneObject>();
    obj->name = name;
    obj->type = "Mesh";
    obj->objectType = Haruka::classifyObjectType("Mesh");
    obj->position = glm::dvec3(0.0);
    obj->rotation = EditorUtil::eulerToRotation(glm::dvec3(0.0));
    obj->scale = glm::dvec3(1.0);

    obj->meshRenderer = std::make_shared<MeshRendererComponent>();
    std::vector<glm::vec3> verts, norms;
    std::vector<unsigned int> indices;

    if (meshType == "capsule") {
        PrimitiveShapes::createCapsule(0.5f, 2.0f, 24, 16, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "capsule";
        obj->properties["meshRenderer"]["radius"] = 0.5f;
        obj->properties["meshRenderer"]["height"] = 2.0f;
    } else if (meshType == "sphere") {
        PrimitiveShapes::createSphere(1.0f, 32, 32, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "sphere";
        obj->properties["meshRenderer"]["radius"] = 1.0f;
        obj->properties["meshRenderer"]["segments"] = 32;
    } else {
        PrimitiveShapes::createCube(1.0f, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "cube";
        obj->properties["meshRenderer"]["size"] = 1.0f;
    }

    obj->meshRenderer->setMesh(verts, norms, std::vector<glm::vec3>(), indices);

    obj->material = std::make_shared<Haruka::MaterialComponent>();
    obj->material->name = obj->name + "_Material";
    obj->material->albedo = albedo;
    obj->color = glm::dvec3(albedo);
    return obj;
}

void ObjectsPanel::addObject(std::shared_ptr<Haruka::SceneObject> obj) {
    if (!obj || !currentScene) return;

    if (commandHistory) {
        commandHistory->execute(std::make_unique<AddObjectCommand>(currentScene, *obj));
    } else {
        currentScene->addLoadedObject(obj);
    }
    if (onSceneChanged) onSceneChanged();

    // Seleccionar el objeto recién creado.
    int idx = (int)currentScene->getObjects().size() - 1;
    if (idx >= 0) selectObject(idx, obj->name);
}

void ObjectsPanel::createProp() {
    auto obj = makeMeshObject(nextName("Prop"), "cube", glm::vec3(0.6f, 0.6f, 0.7f));
    obj->type = "Mesh";
    setObjectLayer(*obj, "Props");
    addObject(obj);
}

void ObjectsPanel::createMonster() {
    auto obj = makeMeshObject(nextName("Monster"), "capsule", glm::vec3(0.7f, 0.3f, 0.3f));
    // Entidad dinámica: tipo libre (CUSTOM) con atributos editables por proyecto.
    obj->type = "Monster";
    obj->objectType = Haruka::classifyObjectType("Monster");
    setObjectLayer(*obj, "Monsters");
    obj->properties["monster"] = {
        {"health", 100}, {"speed", 3.0}, {"damage", 10},
        {"attackRange", 2.0}, {"aggroRange", 15.0}, {"radius", 0.5},
        {"spawnTable", ""}
    };
    addObject(obj);
}

void ObjectsPanel::createSpawnPoint() {
    auto obj = makeMeshObject(nextName("SpawnPoint"), "cube", glm::vec3(1.0f, 0.85f, 0.2f));
    obj->type = "SpawnPoint";
    obj->scale = glm::dvec3(0.5);
    setObjectLayer(*obj, "Spawns");
    obj->properties["spawn"] = {
        {"type", "monster"}, {"radius", 5.0}, {"maxEntities", 5},
        {"cooldown", 30.0}, {"active", true}, {"zoneId", ""}
    };
    addObject(obj);
}

void ObjectsPanel::createCharacter() {
    auto obj = makeMeshObject(nextName("Character"), "capsule", glm::vec3(0.3f, 0.7f, 0.9f));
    obj->type = "Character";
    obj->objectType = Haruka::classifyObjectType("Character");
    setObjectLayer(*obj, "Characters");
    addObject(obj);
}

void ObjectsPanel::createMesh() {
    auto obj = makeMeshObject(nextName("Mesh"), "cube", glm::vec3(0.8f, 0.8f, 0.8f));
    setObjectLayer(*obj, "Default");
    addObject(obj);
}

void ObjectsPanel::createLight() {
    auto obj = makeMeshObject(nextName("Light"), "sphere", glm::vec3(1.0f, 1.0f, 0.8f));
    obj->type = "Light";
    obj->objectType = Haruka::classifyObjectType("Light");
    obj->intensity = 2.0;
    obj->color = glm::dvec3(1.0f, 1.0f, 0.8f);
    setObjectLayer(*obj, "Lights");
    addObject(obj);
}

// ---------------------------------------------------------------------------
// Selección
// ---------------------------------------------------------------------------
void ObjectsPanel::selectObject(int index, const std::string& name) {
    selectedObjectIndex = index;
    if (onObjectSelectedByIndex) onObjectSelectedByIndex(index);
    if (onObjectSelectedByName) onObjectSelectedByName(name);
}

void ObjectsPanel::duplicateObject(int index) {
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
    if (onSceneChanged) onSceneChanged();

    int idx = (int)currentScene->getObjects().size() - 1;
    if (idx >= 0) selectObject(idx, duplicate->name);
}

void ObjectsPanel::showContextMenu(int index) {
    if (!ImGui::BeginPopupContextItem()) return;
    try {
        if (!currentScene) { ImGui::EndPopup(); return; }

        const auto& objects = currentScene->getObjects();
        if (index < 0 || index >= (int)objects.size()) { ImGui::EndPopup(); return; }
        const auto& obj = objects[index];

        if (ImGui::MenuItem("Duplicate")) duplicateObject(index);

        if (ImGui::MenuItem("Edit Material Graph")) {
            selectObject(index, obj.name);
            if (onOpenNodeGraph) onOpenNodeGraph(obj.name);
        }

        if (ImGui::MenuItem("Delete")) {
            if (commandHistory) {
                commandHistory->execute(std::make_unique<DeleteObjectCommand>(currentScene, obj.name));
            } else {
                currentScene->removeObject(obj.name);
            }
            if (selectedObjectIndex == index) selectedObjectIndex = -1;
            if (onSceneChanged) onSceneChanged();
        }

        ImGui::EndPopup();
    } catch (...) {
        // Nunca dejar un popup abierto en el stack de ImGui.
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------------------
// Render del panel
// ---------------------------------------------------------------------------
void ObjectsPanel::renderObjectTile(int index) {
    const auto& objects = currentScene->getObjects();
    if (index < 0 || index >= (int)objects.size()) return;
    const auto& obj = objects[index];

    const float thumb = 60.0f;
    const float totalH = thumb + 22.0f;   // thumb + línea de nombre

    ImGui::PushID(("tile_" + std::to_string(index)).c_str());
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    bool selected = index == selectedObjectIndex;
    ImU32 bg = selected ? IM_COL32(70, 96, 130, 255) : IM_COL32(45, 47, 54, 255);

    // Tarjeta (fondo redondeado)
    dl->AddRectFilled(pos, ImVec2(pos.x + thumb, pos.y + totalH), bg, 6.0f);

    // Swatch de color del objeto
    glm::dvec3 c = obj.color;
    ImU32 sw = IM_COL32((int)(c.x * 255.0f), (int)(c.y * 255.0f), (int)(c.z * 255.0f), 255);
    dl->AddRectFilled(ImVec2(pos.x + 5, pos.y + 5), ImVec2(pos.x + thumb - 5, pos.y + thumb - 5), sw, 4.0f);

    // Inicial del tipo centrada en el swatch
    char g[2] = { (char)std::toupper((unsigned char)(obj.type.empty() ? '?' : obj.type[0])), 0 };
    ImVec2 gs = ImGui::CalcTextSize(g);
    dl->AddText(ImVec2(pos.x + (thumb - gs.x) * 0.5f, pos.y + (thumb - gs.y) * 0.5f - 4),
                IM_COL32(0, 0, 0, 170), g);

    // Nombre (truncado al ancho de la tarjeta)
    std::string label = obj.name;
    const float maxW = thumb - 6;
    while (!label.empty() && ImGui::CalcTextSize(label.c_str()).x > maxW) label.pop_back();
    dl->AddText(ImVec2(pos.x + 3, pos.y + thumb + 3), IM_COL32(230, 230, 235, 255), label.c_str());

    // Interacción (selección + menú contextual)
    ImGui::SetCursorScreenPos(pos);
    if (ImGui::InvisibleButton("##tile", ImVec2(thumb, totalH))) {
        selectObject(index, obj.name);
    }
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        selectObject(index, obj.name);
    }
    // DOBLE clic = encuadrar. Separado de la selección a propósito: seleccionar una tarjeta para
    // editarle el material no debe mover la cámara.
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        if (onObjectFocused) onObjectFocused(index);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s\nType: %s\nLayer: %s", obj.name.c_str(), obj.type.c_str(),
                          objectLayer(obj).c_str());
    }
    showContextMenu(index);
    ImGui::PopID();
}

void ObjectsPanel::renderLayerSection(const std::string& layer, const std::vector<int>& indices) {
    std::string header = layer + " (" + std::to_string(indices.size()) + ")" + "##layer_" + layer;
    if (ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        float availW = ImGui::GetContentRegionAvail().x;
        const float tileW = 60.0f;
        const float spacing = 6.0f;
        int perRow = std::max(1, (int)((availW + spacing) / (tileW + spacing)));

        int count = 0;
        for (int i : indices) {
            if (count % perRow != 0) ImGui::SameLine(0, spacing);
            renderObjectTile(i);
            count++;
        }
    }
}

void ObjectsPanel::onImGuiRender() {
    if (!currentScene) return;

    ImGui::Begin("Objects");

    try {
        // ---- Creación rápida ----
        if (ImGui::Button("+ Prop", ImVec2(70, 0))) createProp();
        ImGui::SameLine();
        if (ImGui::Button("+ Monster", ImVec2(80, 0))) createMonster();
        ImGui::SameLine();
        if (ImGui::Button("+ Spawn", ImVec2(70, 0))) createSpawnPoint();
        ImGui::SameLine();
        if (ImGui::Button("+ Char", ImVec2(65, 0))) createCharacter();
        if (ImGui::Button("+ Mesh", ImVec2(65, 0))) createMesh();
        ImGui::SameLine();
        if (ImGui::Button("+ Light", ImVec2(70, 0))) createLight();

        ImGui::Separator();

        // ---- Añadir: Malla del planeta y Props (modo de colocación en el viewport) ----
        // El viewport dibuja un indicador translúcido (círculo/cuadrado) del radio de afectación
        // sobre el planeta y un clic ejecuta: las herramientas de MALLA editan la geometría
        // (levantar/excavar/allanar) y los PROPS colocan un objeto con modelo anclado al suelo.
        if (ImGui::CollapsingHeader("Añadir sobre el planeta", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("Malla del planeta (editan la geometría):");
            if (ImGui::Button("Levantar##mesh", ImVec2(80, 0))) {
                if (onBeginPlacement) onBeginPlacement("", "Levantar", placementRadius, true, 1, "");
            }
            ImGui::SameLine();
            if (ImGui::Button("Excavar##mesh", ImVec2(80, 0))) {
                if (onBeginPlacement) onBeginPlacement("", "Excavar", placementRadius, true, 2, "");
            }
            ImGui::SameLine();
            if (ImGui::Button("Allanar##mesh", ImVec2(80, 0))) {
                if (onBeginPlacement) onBeginPlacement("", "Allanar", placementRadius, true, 3, "");
            }

            ImGui::TextDisabled("Props (objetos con modelo):");
            static const std::vector<std::tuple<std::string, std::string, float, std::string>> kProps = {
                {"assets/models/stone.glb",       "Roca",     10.0f, "Props"},
                {"assets/models/table.glb",       "Mesa",     8.0f,  "Props"},
                {"assets/models/planks.glb",      "Tablas",   6.0f,  "Props"},
                {"assets/models/SimpleDoor.glb",  "Puerta",   5.0f,  "Props"},
                {"assets/models/Brick.glb",       "Ladrillo", 4.0f,  "Props"},
                {"assets/models/Window.glb",      "Ventana",  6.0f,  "Props"},
                {"assets/models/axe.glb",         "Hacha",    3.0f,  "Props"},
                {"assets/models/CylinderBase.glb","Tronco",   4.0f,  "Trees"},
                {"assets/models/leaf.glb",        "Copa",     7.0f,  "Trees"},
            };
            for (const auto& [path, label, radius, layer] : kProps) {
                if (ImGui::Button(label.c_str(), ImVec2(85, 0))) {
                    if (onBeginPlacement) onBeginPlacement(path, label, radius, false, 0, layer);
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
            ImGui::SliderFloat("Radio afectación", &placementRadius, 1.0f, 500.0f, "%.0f m");
            ImGui::TextDisabled("Esc / clic derecho cancela la colocación.");
        }

        ImGui::Separator();

        // ---- Búsqueda ----
        ImGui::InputText("##obj_search", searchBuffer, sizeof(searchBuffer));

        // ---- Filtro por capa (chips) ----
        std::vector<std::string> layers = defaultLayers();
        const auto& objects = currentScene->getObjects();
        for (const auto& o : objects) {
            std::string l = objectLayer(o);
            if (std::find(layers.begin(), layers.end(), l) == layers.end()) layers.push_back(l);
        }

        if (ImGui::Selectable("All", activeLayerFilter == -1)) activeLayerFilter = -1;
        for (size_t i = 0; i < layers.size(); ++i) {
            ImGui::SameLine();
            if (ImGui::Selectable(layers[i].c_str(), activeLayerFilter == (int)i)) {
                activeLayerFilter = (int)i;
            }
        }
        ImGui::Separator();

        // ---- Agrupar por capa ----
        std::string search = searchBuffer;
        std::transform(search.begin(), search.end(), search.begin(), ::tolower);

        std::map<std::string, std::vector<int>> byLayer;
        for (size_t i = 0; i < objects.size(); ++i) {
            const auto& o = objects[i];
            std::string l = objectLayer(o);

            if (activeLayerFilter != -1) {
                if (activeLayerFilter >= (int)layers.size()) break;
                if (l != layers[activeLayerFilter]) continue;
            }
            if (!search.empty()) {
                std::string hay = o.name + " " + o.type;
                std::transform(hay.begin(), hay.end(), hay.begin(), ::tolower);
                if (hay.find(search) == std::string::npos) continue;
            }
            byLayer[l].push_back((int)i);
        }

        bool childOpen = false;
        try {
            ImGui::BeginChild("##objects_scroll");
            childOpen = true;
            for (const auto& [layer, indices] : byLayer) {
                renderLayerSection(layer, indices);
            }
            ImGui::EndChild();
            childOpen = false;
        } catch (...) {
            // Por si una callback (selección/duplicado/borrado) lanza una excepción a
            // mitad de frame: cerrar el child si sigue abierto para no desbalancear ImGui.
            if (childOpen) ImGui::EndChild();
        }
    } catch (...) {
        // Nunca dejar el panel a medias: el End() exterior siempre se ejecuta.
    }

    ImGui::End();
}
