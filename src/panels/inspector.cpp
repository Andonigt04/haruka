#include "inspector.h"

#include "commands/scene_commands.h"
#include "editor_util.h"
#include "core/components/material_component.h"
#include "core/application.h"
#include "renderer/render_target.h"
#include "renderer/motor_instance.h"
#include "core/components/transform_component.h"
#include "core/components/mesh_renderer_component.h"
#include "core/components/script_component.h"
#include "core/planet/prop_cond.h"                  // parsePropCond: valida el `when` en vivo
#include <nfd.h>
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <set>

namespace {

// Claves gestionadas por secciones dedicadas: el editor dinámico las salta.
const std::set<std::string> kReservedProps = {
    "meshRenderer", "layer", "spawn", "monster", "terrainEditor", "materialGraph"
};

bool isReservedProp(const std::string& k) {
    return kReservedProps.count(k) != 0;
}

// Abre un selector de archivos y convierte la ruta elegida a relativa del
// proyecto (si vive dentro de él) o a absoluta en caso contrario.
bool pickTexturePath(const std::string& projectPath, std::string& out) {
    nfdchar_t* outPath = nullptr;
    nfdresult_t result = NFD_OpenDialog("png,jpg,jpeg,tga,bmp,exr", nullptr, &outPath);
    if (result == NFD_OKAY && outPath) {
        std::string abs(outPath);
        free(outPath);
        if (!projectPath.empty()) {
            std::error_code ec;
            std::string absNorm = std::filesystem::weakly_canonical(abs, ec).string();
            std::string projNorm = std::filesystem::weakly_canonical(projectPath, ec).string();
            if (!ec && absNorm.rfind(projNorm, 0) == 0) {
                std::string rel = absNorm.substr(projNorm.size());
                while (!rel.empty() && (rel.front() == '/' || rel.front() == '\\')) rel.erase(rel.begin());
                out = rel;
                return true;
            }
        }
        out = abs;
        return true;
    }
    return false;
}

bool allNumbers(const nlohmann::json& a) {
    for (size_t i = 0; i < a.size(); ++i) {
        if (!a[i].is_number()) return false;
    }
    return true;
}

/**
 * @brief Editor genérico de un valor JSON (recursivo).
 * @return true si el usuario modificó algo.
 */
bool renderJsonValue(const char* id, const char* label, nlohmann::json& value) {
    bool changed = false;

    if (value.is_number_float()) {
        float f = value.get<float>();
        if (ImGui::DragFloat(label, &f, 0.1f)) { value = f; changed = true; }
    } else if (value.is_number_integer()) {
        int i = value.get<int>();
        if (ImGui::DragInt(label, &i, 1)) { value = i; changed = true; }
    } else if (value.is_number_unsigned()) {
        int i = (int)value.get<unsigned int>();
        if (ImGui::DragInt(label, &i, 1)) { value = (unsigned int)i; changed = true; }
    } else if (value.is_boolean()) {
        bool b = value.get<bool>();
        if (ImGui::Checkbox(label, &b)) { value = b; changed = true; }
    } else if (value.is_string()) {
        char buf[512] = {0};
        strncpy(buf, value.get_ref<const std::string&>().c_str(), sizeof(buf) - 1);
        if (ImGui::InputText(label, buf, sizeof(buf))) {
            value = std::string(buf);
            changed = true;
        }
    } else if (value.is_array()) {
        if (value.size() == 2 && allNumbers(value)) {
            float v[2] = {(float)value[0], (float)value[1]};
            if (ImGui::DragFloat2(label, v, 0.1f)) { value = {v[0], v[1]}; changed = true; }
        } else if (value.size() == 3 && allNumbers(value)) {
            float v[3] = {(float)value[0], (float)value[1], (float)value[2]};
            if (ImGui::DragFloat3(label, v, 0.1f)) { value = {v[0], v[1], v[2]}; changed = true; }
        } else if (value.size() == 4 && allNumbers(value)) {
            float v[4] = {(float)value[0], (float)value[1], (float)value[2], (float)value[3]};
            if (ImGui::DragFloat4(label, v, 0.1f)) { value = {v[0], v[1], v[2], v[3]}; changed = true; }
        } else {
            std::string treeId = std::string(id) + "_arr";
            if (ImGui::TreeNodeEx(treeId.c_str(), 0, "%s [%d]", label, (int)value.size())) {
                for (size_t i = 0; i < value.size(); ++i) {
                    std::string eid = std::string(id) + "_" + std::to_string(i);
                    std::string elabel = "[" + std::to_string(i) + "]";
                    if (renderJsonValue(eid.c_str(), elabel.c_str(), value[i])) changed = true;
                }
                ImGui::TreePop();
            }
        }
    } else if (value.is_object()) {
        std::string treeId = std::string(id) + "_obj";
        if (ImGui::TreeNodeEx(treeId.c_str(), 0, "%s", label)) {
            for (auto& el : value.items()) {
                std::string kid = std::string(id) + "_" + el.key();
                if (renderJsonValue(kid.c_str(), el.key().c_str(), el.value())) changed = true;
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::TextDisabled("%s = null", label);
    }
    return changed;
}

} // namespace

InspectorPanel::InspectorPanel() = default;
// El destructor va en el .cpp porque `matPreviewTarget` es un tipo incompleto en la cabecera:
// ~unique_ptr necesita ver RenderTarget, y aquí ya está incluido.
InspectorPanel::~InspectorPanel() = default;

// ---------------------------------------------------------------------------
// Preview de material: la ESFERA (la pinta el motor con su propio shader) + una miniatura por
// slot de textura. Sin esto, un slot de textura es una CADENA: no se sabe si la ruta resuelve,
// si el PNG es el que crees, ni qué aspecto tiene el material montado hasta ir a buscarlo a la
// escena. Las dos cosas salen del motor —misma caché de texturas y mismo `final.frag`— así que
// lo que se ve aquí es lo que se verá sobre el objeto, no una aproximación del editor.
// ---------------------------------------------------------------------------
void InspectorPanel::renderMaterialPreview(Haruka::SceneObject& obj) {
    Application* app = MotorInstance::getInstance().getApplication();
    if (!app || !obj.material) return;

    const int kPreview = 128;
    if (!matPreviewTarget)
        matPreviewTarget = std::make_unique<Haruka::Renderer::RenderTarget>(kPreview, kPreview);

    app->renderMaterialPreview(*obj.material, *matPreviewTarget);

    // UV invertida en V: la textura de un FBO de GL tiene el origen ABAJO; con (0,0)-(1,1) la
    // esfera saldría del revés y su luz vendría de abajo, que es justo lo que no quieres al juzgar
    // un material.
    ImGui::Image((void*)(intptr_t)matPreviewTarget->getColorTextureGL(),
                 ImVec2((float)kPreview, (float)kPreview), ImVec2(0, 1), ImVec2(1, 0));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextDisabled("Preview");
    ImGui::TextDisabled("%s", obj.material->name.c_str());
    int slotsWithTexture = 0;
    for (const auto& [slot, path] : obj.material->textures)
        if (!path.empty()) ++slotsWithTexture;
    ImGui::TextDisabled("%d/5 slots con textura", slotsWithTexture);
    ImGui::EndGroup();
}

void InspectorPanel::setScene(Haruka::SceneManager* scene) {
    currentScene = scene;
}

void InspectorPanel::setSelectedObjectIndex(int index) {
    selectedObjectIndex = index;
}

void InspectorPanel::setCommandHistory(CommandHistory* history) {
    commandHistory = history;
}

void InspectorPanel::onImGuiRender() {
    ImGui::Begin("Inspector");

    if (selectedObjectIndex < 0 || !currentScene) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }

    const auto& objects = currentScene->getObjects();
    if (selectedObjectIndex >= (int)objects.size()) {
        ImGui::Text("Invalid object");
        ImGui::End();
        return;
    }

    auto obj = currentScene->getObject(objects[selectedObjectIndex].name);
    if (!obj) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Object not found in scene");
        ImGui::End();
        return;
    }

    // ================= Identidad =================
    char nameBuf[256] = {0};
    strncpy(nameBuf, obj->name.c_str(), sizeof(nameBuf) - 1);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
        obj->name = nameBuf;
    }

    {
        char typeBuf[64] = {0};
        strncpy(typeBuf, obj->type.c_str(), sizeof(typeBuf) - 1);
        if (ImGui::InputText("Type", typeBuf, sizeof(typeBuf))) {
            obj->type = typeBuf;
            obj->objectType = Haruka::classifyObjectType(typeBuf);
            if (onSceneChanged) onSceneChanged();
        }
    }

    {
        char tplBuf[128] = {0};
        strncpy(tplBuf, obj->templateName.c_str(), sizeof(tplBuf) - 1);
        if (ImGui::InputText("Template", tplBuf, sizeof(tplBuf))) {
            obj->templateName = tplBuf;
            if (onSceneChanged) onSceneChanged();
        }
    }

    // ---- Capa ----
    {
        std::string curLayer = EditorUtil::objectLayer(*obj);
        const auto& layers = EditorUtil::defaultLayers();
        if (ImGui::BeginCombo("Layer", curLayer.c_str())) {
            for (const auto& l : layers) {
                if (ImGui::Selectable(l.c_str(), curLayer == l)) {
                    EditorUtil::setObjectLayer(*obj, l);
                    if (onSceneChanged) onSceneChanged();
                }
            }
            ImGui::EndCombo();
        }
        char layerBuf[64] = {0};
        strncpy(layerBuf, curLayer.c_str(), sizeof(layerBuf) - 1);
        if (ImGui::InputText("Layer (custom)", layerBuf, sizeof(layerBuf))) {
            EditorUtil::setObjectLayer(*obj, layerBuf);
            if (onSceneChanged) onSceneChanged();
        }
    }

    int renderLayer = obj->renderLayer;
    if (ImGui::SliderInt("Render Layer", &renderLayer, 1, 5)) {
        obj->renderLayer = renderLayer;
    }
    ImGui::Separator();

    // ================= Transform =================
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        {
            float pos[3] = { (float)obj->position.x, (float)obj->position.y, (float)obj->position.z };
            if (ImGui::DragFloat3("Position", pos, 0.1f)) {
                obj->position = glm::dvec3(pos[0], pos[1], pos[2]);
            }
        }

        {
            float rot[3] = { (float)obj->rotation.x, (float)obj->rotation.y, (float)obj->rotation.z };
            if (ImGui::DragFloat3("Rotation", rot, 1.0f)) {
                obj->rotation = EditorUtil::eulerToRotation(glm::dvec3(rot[0], rot[1], rot[2]));
            }
        }

        {
            float scale[3] = { (float)obj->scale.x, (float)obj->scale.y, (float)obj->scale.z };
            if (ImGui::DragFloat3("Scale", scale, 0.1f)) {
                obj->scale = glm::dvec3(scale[0], scale[1], scale[2]);
            }
        }
    }

    if (playMode) ImGui::BeginDisabled();

    // ================= Material & Texturas =================
    if (ImGui::CollapsingHeader("Material & Textures", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (!obj->material) {
            if (ImGui::Button("Create Material")) {
                obj->material = std::make_shared<Haruka::MaterialComponent>();
                obj->material->name = obj->name + "_Material";
                if (onSceneChanged) onSceneChanged();
            }
        } else {
            auto& mat = obj->material;
            renderMaterialPreview(*obj);
            ImGui::Separator();
            bool matChanged = false;
            matChanged |= ImGui::ColorEdit3("Albedo", &mat->albedo.x);
            matChanged |= ImGui::SliderFloat("Metallic", &mat->metallic, 0.0f, 1.0f);
            matChanged |= ImGui::SliderFloat("Roughness", &mat->roughness, 0.0f, 1.0f);
            matChanged |= ImGui::SliderFloat("AO", &mat->ao, 0.0f, 1.0f);
            matChanged |= ImGui::ColorEdit3("Emission", &mat->emission.x);
            if (matChanged && onSceneChanged) onSceneChanged();

            ImGui::Separator();
            if (ImGui::CollapsingHeader("Texture Slots", ImGuiTreeNodeFlags_DefaultOpen)) {
                static const char* slots[] = {"albedo", "normal", "metallic", "roughness", "ao"};
                Application* app = MotorInstance::getInstance().getApplication();
                const float kThumb = 48.0f;
                for (const char* s : slots) {
                    ImGui::PushID(s);
                    char buf[512] = {0};
                    std::string cur = mat->textures.count(s) ? mat->textures[s] : std::string();
                    strncpy(buf, cur.c_str(), sizeof(buf) - 1);

                    // MINIATURA del PNG, del propio motor. Es además el diagnóstico de la ruta: si
                    // sale el recuadro rojo, el render tampoco la encuentra — el aviso llega aquí y
                    // no como un objeto de color plano en el viewport.
                    unsigned int tex = (app && !cur.empty()) ? app->getMaterialTextureGL(cur) : 0u;
                    if (tex) {
                        ImGui::Image((void*)(intptr_t)tex, ImVec2(kThumb, kThumb));
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::TextUnformatted(cur.c_str());
                            ImGui::Image((void*)(intptr_t)tex, ImVec2(192, 192));
                            ImGui::EndTooltip();
                        }
                    } else {
                        const ImU32 col = cur.empty() ? IM_COL32(70, 70, 78, 255)    // vacío
                                                      : IM_COL32(150, 45, 45, 255);  // no carga
                        ImVec2 p = ImGui::GetCursorScreenPos();
                        ImGui::GetWindowDrawList()->AddRectFilled(
                            p, ImVec2(p.x + kThumb, p.y + kThumb), col, 3.0f);
                        ImGui::Dummy(ImVec2(kThumb, kThumb));
                        if (!cur.empty() && ImGui::IsItemHovered())
                            ImGui::SetTooltip("No se pudo cargar:\n%s", cur.c_str());
                    }
                    ImGui::SameLine();

                    ImGui::BeginGroup();
                    ImGui::TextUnformatted(s);
                    ImGui::SetNextItemWidth(220);
                    if (ImGui::InputText("##path", buf, sizeof(buf))) {
                        mat->textures[s] = buf;
                        if (onSceneChanged) onSceneChanged();
                    }
                    if (ImGui::SmallButton("Browse")) {
                        std::string picked;
                        if (pickTexturePath(projectPath, picked)) {
                            mat->textures[s] = picked;
                            if (onSceneChanged) onSceneChanged();
                        }
                    }
                    if (!cur.empty()) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Clear")) {
                            mat->textures[s].clear();
                            if (onSceneChanged) onSceneChanged();
                        }
                    }
                    ImGui::EndGroup();
                    ImGui::PopID();
                }
            }
        }
    }

    // ================= Spawn Point =================
    bool isSpawn = (obj->type == "SpawnPoint") || obj->properties.contains("spawn");
    if (isSpawn) {
        if (ImGui::CollapsingHeader("Spawn Point", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& sp = obj->properties["spawn"];
            if (!sp.is_object()) sp = nlohmann::json::object();
            ImGui::PushID("spawn");
            bool changed = false;

            static const char* stypes[] = {"player_start", "player_respawn", "npc",
                                           "monster", "boss", "checkpoint"};
            std::string cur = sp.value("type", "monster");
            int st = 0;
            for (int i = 0; i < 6; ++i) if (cur == stypes[i]) st = i;
            if (ImGui::Combo("Spawn type", &st, stypes, 6)) {
                sp["type"] = stypes[st]; changed = true;
            }

            float radius = sp.value("radius", 5.0f);
            if (ImGui::DragFloat("Radius", &radius, 0.5f, 0.0f, 10000.0f)) {
                sp["radius"] = radius; changed = true;
            }
            int maxEnt = sp.value("maxEntities", 5);
            if (ImGui::DragInt("Max Entities", &maxEnt, 1, 1, 1000)) {
                sp["maxEntities"] = maxEnt; changed = true;
            }
            float cooldown = sp.value("cooldown", 30.0f);
            if (ImGui::DragFloat("Cooldown (s)", &cooldown, 1.0f, 0.0f, 3600.0f)) {
                sp["cooldown"] = cooldown; changed = true;
            }
            bool active = sp.value("active", true);
            if (ImGui::Checkbox("Active", &active)) { sp["active"] = active; changed = true; }

            char zoneBuf[128] = {0};
            strncpy(zoneBuf, sp.value("zoneId", "").c_str(), sizeof(zoneBuf) - 1);
            if (ImGui::InputText("Zone", zoneBuf, sizeof(zoneBuf))) {
                sp["zoneId"] = std::string(zoneBuf); changed = true;
            }

            if (changed && onSceneChanged) onSceneChanged();
            ImGui::PopID();
        }
    }

    // ================= Monster / Entidad =================
    if (obj->properties.contains("monster")) {
        if (ImGui::CollapsingHeader("Monster / Entity", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& m = obj->properties["monster"];
            if (!m.is_object()) m = nlohmann::json::object();
            ImGui::PushID("monster");
            bool changed = false;
            for (auto& el : m.items()) {
                std::string kid = "m_" + el.key();
                changed |= renderJsonValue(kid.c_str(), el.key().c_str(), el.value());
            }
            if (changed && onSceneChanged) onSceneChanged();
            ImGui::PopID();
        }
    }

    // ================= Atributos dinámicos =================
    if (ImGui::CollapsingHeader("Dynamic Attributes", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("dynamic");
        if (!obj->properties.is_object()) obj->properties = nlohmann::json::object();

        // Añadir atributo nuevo
        static char newAttrName[64] = {0};
        static int newAttrType = 0;
        static const char* kTypes[] = {"float", "int", "string", "bool", "vec3", "object"};
        ImGui::InputText("Name##newattr", newAttrName, sizeof(newAttrName));
        ImGui::Combo("Type##newattr", &newAttrType, kTypes, 6);
        if (ImGui::Button("Add Attribute") && newAttrName[0]) {
            std::string key = newAttrName;
            if (!isReservedProp(key) && !obj->properties.contains(key)) {
                nlohmann::json def;
                switch (newAttrType) {
                    case 0: def = 0.0f; break;
                    case 1: def = 0; break;
                    case 2: def = std::string(""); break;
                    case 3: def = false; break;
                    case 4: def = {0.0f, 0.0f, 0.0f}; break;
                    default: def = nlohmann::json::object(); break;
                }
                obj->properties[key] = def;
                newAttrName[0] = '\0';
                if (onSceneChanged) onSceneChanged();
            }
        }
        ImGui::Separator();

        std::vector<std::string> keys;
        for (auto& el : obj->properties.items()) keys.push_back(el.key());
        for (const auto& k : keys) {
            if (isReservedProp(k)) continue;
            ImGui::PushID(("attr_" + k).c_str());
            bool changed = renderJsonValue(("v_" + k).c_str(), k.c_str(), obj->properties[k]);
            if (changed && onSceneChanged) onSceneChanged();
            ImGui::SameLine();
            if (ImGui::SmallButton("x##del")) {
                obj->properties.erase(k);
                if (onSceneChanged) onSceneChanged();
            }
            ImGui::PopID();
        }
        ImGui::PopID();
    }

    // ================= Luces / Modelos =================
    if (obj->type == "Light") {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            glm::vec3 colorFloat = glm::vec3(obj->color);
            if (ImGui::ColorEdit3("Color", &colorFloat.x)) {
                obj->color = glm::dvec3(colorFloat);
            }

            float intensityFloat = (float)obj->intensity;
            if (ImGui::DragFloat("Intensity", &intensityFloat, 0.1f, 0.0f, 100.0f)) {
                obj->intensity = (double)intensityFloat;
            }
        }
    }
    else if (obj->type == "Model") {
        if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
            char pathBuf[512] = {0};
            strncpy(pathBuf, obj->modelPath.c_str(), sizeof(pathBuf) - 1);
            if (ImGui::InputText("Model Path", pathBuf, sizeof(pathBuf))) {
                obj->modelPath = pathBuf;
            }
        }
    }

    // ================= Planeta =================
    // El objeto tiene `surfaceConfig` → el motor lo interpreta como planeta (Ver
    // PlanetarySystem::buildFromScene). Aquí se edita ESA config y se regenera el planeta con un
    // click: el mismo camino que usa la carga de escena, así que lo que ves tras "Regenerar" es
    // exactamente lo que se vería al abrir la escena.
    if (obj->surfaceConfig.is_object() && !obj->surfaceConfig.empty()) {
        if (ImGui::CollapsingHeader("Planet", ImGuiTreeNodeFlags_DefaultOpen)) {
            auto& sc = obj->surfaceConfig;
            bool changed = false;

            int seedI = (int)sc.value("seed", 0u);
            if (ImGui::InputInt("Seed", &seedI)) { sc["seed"] = (uint32_t)seedI; changed = true; }

            float landF = sc.value("landFraction", 0.29f);
            if (ImGui::SliderFloat("Land Fraction", &landF, 0.0f, 1.0f)) {
                sc["landFraction"] = landF; changed = true;
            }

            float tiling = sc.value("tiling", 100.0f);
            if (ImGui::DragFloat("Tiling (m/tile)", &tiling, 1.0f, 1.0f, 10000.0f)) {
                sc["tiling"] = tiling; changed = true;
            }

            int texRes = sc.value("texRes", 512);
            if (ImGui::DragInt("Tex Res", &texRes, 8, 64, 4096)) {
                sc["texRes"] = texRes; changed = true;
            }

            glm::vec2 er(-4000.0f, 4000.0f);
            if (sc.contains("elevationRange") && sc["elevationRange"].is_array() &&
                sc["elevationRange"].size() >= 2) {
                er = glm::vec2(sc["elevationRange"][0], sc["elevationRange"][1]);
            }
            if (ImGui::DragFloat2("Elev. Range (m)", &er.x, 50.0f)) {
                sc["elevationRange"] = {er.x, er.y}; changed = true;
            }

            // Mapa de zonas (paleta) y mapa de elevación (gris): el planeta los carga por ruta.
            auto pickMap = [&](const char* label, const char* key) {
                std::string path = sc.value(key, std::string());
                char buf[512] = {0};
                strncpy(buf, path.c_str(), sizeof(buf) - 1);
                if (ImGui::InputText(label, buf, sizeof(buf))) {
                    sc[key] = std::string(buf); changed = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton((std::string("...##") + key).c_str())) {
                    std::string picked;
                    if (pickTexturePath(projectPath, picked)) {
                        sc[key] = picked; changed = true;
                    }
                }
            };
            pickMap("Zone Map", "zoneMap");
            pickMap("Elev. Map", "elevationMap");

            if (changed && onSceneChanged) onSceneChanged();

            // CAPAS del terreno: selector para revelar cómo se aplica cada material.
            // El índice es la POSICIÓN en `surface.materials` (0=agua, 1=arena…) — el mismo valor
            // que lee el shader con `dbg>=10`. Por eso se listan TODOS los materiales, también los
            // sin textura (agua, hielo): aislarlos muestra dónde manda cada uno.
            if (sc.contains("materials") && sc["materials"].is_array()) {
                if (ImGui::CollapsingHeader("Capas (materiales)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    Application* app = MotorInstance::getInstance().getApplication();
                    int dbg = app ? app->getPlanetarySystem()->debugView() : 0;
                    // Cambiar la vista de debug NO toca la escena: solo se pide al motor.
                    auto setView = [&](int v) { if (app) app->setPlanetDebugView(v); };
                    ImGui::TextDisabled("Dónde manda cada material del terreno:");
                    if (ImGui::Selectable("Normal", dbg == 0)) setView(0);
                    if (ImGui::Selectable("Todas las capas", dbg == 6)) setView(6);
                    int idx = 0;
                    for (const auto& m : sc["materials"]) {
                        if (!m.is_object()) continue;
                        const std::string nm = m.contains("name") && m["name"].is_string()
                                             ? m["name"].get<std::string>() : std::string();
                        const std::string label = "Capa " + std::to_string(idx) + ": " +
                                                  (nm.empty() ? "?" : nm);
                        const int val = 10 + idx;
                        if (ImGui::Selectable(label.c_str(), dbg == val)) setView(val);
                        ++idx;
                    }
                }
            }

            // ÁREAS DE SPAWN de las capas de props: pinta dónde instalaría cada capa (bandas de
            // clima/forma × densityMap). El índice es la POSICIÓN en `surface.propLayers` — el
            // mismo valor que lee el shader con `dbg>=40`. Solo se listan capas que instalan mesh.
            if (sc.contains("propLayers") && sc["propLayers"].is_array()) {
                if (ImGui::CollapsingHeader("Capas (props / spawn)", ImGuiTreeNodeFlags_DefaultOpen)) {
                    Application* app = MotorInstance::getInstance().getApplication();
                    int dbg = app ? app->getPlanetarySystem()->debugView() : 0;
                    auto setView = [&](int v) { if (app) app->setPlanetDebugView(v); };
                    ImGui::TextDisabled("Dónde instalaría cada capa de objetos:");
                    if (ImGui::Selectable("Normal", dbg == 0)) setView(0);
                    int idx = 0;
                    for (const auto& p : sc["propLayers"]) {
                        if (!p.is_object()) continue;
                        const bool hasMesh = p.contains("mesh") && p["mesh"].is_string() &&
                                            !p["mesh"].get<std::string>().empty();
                        if (!hasMesh) continue;   // capa informativa: no instala, no se lista
                        const std::string nm = p.contains("name") && p["name"].is_string()
                                             ? p["name"].get<std::string>() : std::string();
                        const std::string label = "Spawn " + std::to_string(idx) + ": " +
                                                  (nm.empty() ? "?" : nm);
                        const int val = 40 + idx;
                        if (ImGui::Selectable(label.c_str(), dbg == val)) setView(val);
                        ++idx;
                    }
                }
            }

            // ===== MATERIALES: añadir/borrar/editar las capas dinámicamente =====
            // `surface.materials` es TODO lo que el planeta necesita para pintarse: cada entrada es
            // una regla (zona, clima, pendiente) + cómo se ve (albedo, color). Editar aquí y pulsar
            // "Regenerar planeta" es exactamente el mismo camino que la carga de escena, así que lo
            // que se ve tras regenerar es lo que se vería al abrir el proyecto.
            if (ImGui::CollapsingHeader("Materiales (editar)", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (!sc.contains("materials") || !sc["materials"].is_array())
                    sc["materials"] = nlohmann::json::array();
                auto& mats = sc["materials"];
                int delIdx = -1;
                int mi = 0;
                for (auto& m : mats) {
                    if (!m.is_object()) { ++mi; continue; }
                    const std::string hdr = m.value("name", std::string())
                                            .empty() ? "Material " + std::to_string(mi) :
                                            m["name"].get<std::string>();
                    ImGui::PushID(mi);
                    if (ImGui::CollapsingHeader(hdr.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        {
                            std::string nm = m.value("name", std::string());
                            char nb[128] = {0};
                            strncpy(nb, nm.c_str(), sizeof(nb) - 1);
                            if (ImGui::InputText("Nombre", nb, sizeof(nb))) {
                                m["name"] = std::string(nb); changed = true;
                            }
                        }
                        {
                            std::string alb = m.value("albedo", std::string());
                            char ab[512] = {0};
                            strncpy(ab, alb.c_str(), sizeof(ab) - 1);
                            if (ImGui::InputText("Textura (albedo)", ab, sizeof(ab))) {
                                m["albedo"] = std::string(ab); changed = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("...##albedo")) {
                                std::string picked;
                                if (pickTexturePath(projectPath, picked)) {
                                    m["albedo"] = picked; changed = true;
                                }
                            }
                        }
                        {
                            int z[3] = {128, 128, 128};
                            if (m.contains("zone") && m["zone"].is_array() && m["zone"].size() >= 3) {
                                z[0] = m["zone"][0].get<int>();
                                z[1] = m["zone"][1].get<int>();
                                z[2] = m["zone"][2].get<int>();
                            }
                            if (ImGui::DragInt3("Zona (RGB)", z, 1, 0, 255)) {
                                m["zone"] = {z[0], z[1], z[2]}; changed = true;
                            }
                        }
                        {
                            float sl[2] = {0.0f, 1.0f};
                            if (m.contains("slope") && m["slope"].is_array() && m["slope"].size() >= 2) {
                                sl[0] = m["slope"][0].get<float>();
                                sl[1] = m["slope"][1].get<float>();
                            }
                            if (ImGui::DragFloat2("Pendiente", sl, 0.01f, 0.0f, 1.0f)) {
                                m["slope"] = {sl[0], sl[1]}; changed = true;
                            }
                        }
                        {
                            bool sub = m.value("submerged", false);
                            if (ImGui::Checkbox("Bajo el agua", &sub)) {
                                m["submerged"] = sub; changed = true;
                            }
                        }
                        {
                            float col[3] = {0.5f, 0.5f, 0.5f};
                            float cw = m.value("colorWeight", 1.0f);
                            if (m.contains("color") && m["color"].is_array() && m["color"].size() >= 3) {
                                col[0] = m["color"][0].get<float>();
                                col[1] = m["color"][1].get<float>();
                                col[2] = m["color"][2].get<float>();
                            }
                            if (ImGui::ColorEdit3("Color", col)) {
                                m["color"] = {col[0], col[1], col[2]}; changed = true;
                            }
                            if (ImGui::SliderFloat("Peso color", &cw, 0.0f, 1.0f)) {
                                m["colorWeight"] = cw; changed = true;
                            }
                        }
                        if (ImGui::Button("Borrar capa##m")) delIdx = mi;
                    }
                    ImGui::PopID();
                    ++mi;
                }
                if (delIdx >= 0 && delIdx < (int)mats.size()) {
                    mats.erase(mats.begin() + delIdx);
                    changed = true;
                }
                if (ImGui::Button("+ Añadir capa")) {
                    mats.push_back(nlohmann::json::object({
                        {"name", "Capa " + std::to_string(mats.size())},
                        {"albedo", ""},
                        {"zone", {128, 128, 128}},
                        {"slope", {0.0, 1.0}},
                        {"submerged", false},
                        {"color", {0.5, 0.5, 0.5}},
                        {"colorWeight", 1.0}
                    }));
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("Presets:");
                auto preset = [&](const nlohmann::json& jm) {
                    mats.push_back(jm); changed = true;
                };
                if (ImGui::SmallButton("Arena")) preset(nlohmann::json::object({
                    {"name", "arena"}, {"albedo", "sand_albedo.png"},
                    {"normal", "sand_normal.png"}, {"zone", {214, 198, 142}},
                    {"slope", {0.0, 0.2}}, {"color", {0.72, 0.66, 0.47}},
                    {"colorWeight", 1.0}, {"grain", 1.0}, {"detail", 0.9}}));
                if (ImGui::SmallButton("Hierba")) preset(nlohmann::json::object({
                    {"name", "hierba"}, {"albedo", "grass_albedo.png"},
                    {"normal", "grass_normal.png"}, {"zone", {118, 158, 96}},
                    {"slope", {0.0, 0.6}}, {"color", {0.3, 0.42, 0.22}},
                    {"colorWeight", 1.0}, {"grain", 1.0}, {"detail", 1.0}}));
                if (ImGui::SmallButton("Tierra")) preset(nlohmann::json::object({
                    {"name", "tierra"}, {"albedo", "land_albedo.png"},
                    {"normal", "land_normal.png"}, {"zone", {150, 120, 90}},
                    {"slope", {0.1, 0.8}}, {"color", {0.45, 0.34, 0.22}},
                    {"colorWeight", 1.0}, {"grain", 1.0}, {"detail", 1.0}}));
                if (ImGui::SmallButton("Roca")) preset(nlohmann::json::object({
                    {"name", "roca"}, {"albedo", "rock_albedo.png"},
                    {"normal", "rock_normal.png"}, {"zone", {130, 130, 135}},
                    {"slope", {0.5, 1.0}}, {"color", {0.33, 0.3, 0.27}},
                    {"colorWeight", 0.8}, {"priority", 3.0}, {"grain", 1.3},
                    {"detail", 1.4}, {"feather", 0.01}}));
                if (ImGui::SmallButton("Agua")) preset(nlohmann::json::object({
                    {"name", "agua"}, {"zone", {42, 106, 186}},
                    {"submerged", true}, {"color", {0.09, 0.22, 0.45}},
                    {"colorWeight", 1.0}, {"grain", 0.2}, {"detail", 0.3},
                    {"tint", {0.9, 0.95, 1.0}}}));
                if (ImGui::SmallButton("Hielo")) preset(nlohmann::json::object({
                    {"name", "hielo"}, {"zone", {240, 240, 250}},
                    {"color", {0.86, 0.89, 0.93}}, {"colorWeight", 1.0},
                    {"grain", 0.15}, {"detail", 0.35}}));
                if (changed && onSceneChanged) onSceneChanged();
            }

            // ===== CAPAS DE OBJETOS (props): añadir/borrar/REORDENAR =============
            // SEPARADAS de las del terreno a propósito. Cada capa instala un OBJETO (árbol,
            // roca, casa, camino…) y el ORDEN de la lista ES la prioridad de construcción:
            // la capa [0] instala y reclama su radio; las siguientes respetan lo reclamado.
            // Vive en `surfaceConfig["propLayers"]` — el motor lo recibe completo dentro del
            // raw del planeta (ver PropLayerTable en el motor) y lo parsea al regenerar.
            if (ImGui::CollapsingHeader("Capas de objetos (props)", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (!sc.contains("propLayers") || !sc["propLayers"].is_array())
                    sc["propLayers"] = nlohmann::json::array();
                auto& pl = sc["propLayers"];
                ImGui::TextDisabled("El ORDEN de arriba a abajo es la prioridad: la 1ª capa instala y reclama primero.");
                int delIdx = -1, moveFrom = -1, moveTo = -1;
                int pi = 0;
                for (auto& layer : pl) {
                    if (!layer.is_object()) { ++pi; continue; }
                    const std::string hdr = layer.value("name", std::string()).empty()
                        ? "Capa " + std::to_string(pi) : layer["name"].get<std::string>();
                    ImGui::PushID(pi);
                    // Flechas de REORDENACIÓN (prioridad). Solo tienen sentido hacia el
                    // vecino: mover hacia arriba/abajo es deslizar en la lista.
                    if (pi > 0 && ImGui::ArrowButton("##up", ImGuiDir_Up)) { moveFrom = pi; moveTo = pi - 1; }
                    ImGui::SameLine();
                    if (pi + 1 < (int)pl.size() && ImGui::ArrowButton("##down", ImGuiDir_Down)) { moveFrom = pi; moveTo = pi + 1; }
                    ImGui::SameLine();
                    if (ImGui::CollapsingHeader(hdr.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        {
                            std::string nm = layer.value("name", std::string());
                            char nb[128] = {0};
                            strncpy(nb, nm.c_str(), sizeof(nb) - 1);
                            if (ImGui::InputText("Nombre", nb, sizeof(nb))) {
                                layer["name"] = std::string(nb); changed = true;
                            }
                        }
                        {
                            std::string ms = layer.value("mesh", std::string());
                            char mb[128] = {0};
                            strncpy(mb, ms.c_str(), sizeof(mb) - 1);
                            if (ImGui::InputText("Instala (mesh)", mb, sizeof(mb))) {
                                layer["mesh"] = std::string(mb); changed = true;
                            }
                            ImGui::SameLine();
                            ImGui::TextDisabled("árbol=tree, roca=rock, casa=house…");
                        }
                        {
                            float den = layer.value("density", 1.0f);
                            if (ImGui::SliderFloat("Densidad", &den, 0.0f, 1.0f)) {
                                layer["density"] = den; changed = true;
                            }
                        }
                        {
                            // Mapa de distribución (densityMap): cuando existe MANDA sobre las
                            // bandas, igual que el zoneMap sobre los materiales del terreno —
                            // pintas dónde crecen los árboles. Vacío = la capa se rige por clima.
                            std::string dm = layer.value("densityMap", std::string());
                            char db[512] = {0};
                            strncpy(db, dm.c_str(), sizeof(db) - 1);
                            if (ImGui::InputText("Mapa de distribución", db, sizeof(db))) {
                                layer["densityMap"] = std::string(db); changed = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton("...##densityMap")) {
                                std::string picked;
                                if (pickTexturePath(projectPath, picked)) {
                                    layer["densityMap"] = picked; changed = true;
                                }
                            }
                        }
                        {
                            // Condición booleana `when`: expresión sobre `layer` (material del
                            // terreno) y `zone` (zona nombrada). P. ej. "layer != sand || zone ==
                            // oasis". Se valida EN VIVO con el parser del motor; la sintaxis la
                            // exige también el SceneValidator al abrir la escena.
                            std::string wh = layer.value("when", std::string());
                            char wb[512] = {0};
                            strncpy(wb, wh.c_str(), sizeof(wb) - 1);
                            if (ImGui::InputText("Condición (when)", wb, sizeof(wb))) {
                                layer["when"] = std::string(wb); changed = true;
                            }
                            if (!wh.empty()) {
                                std::string werr;
                                if (Haruka::Planet::parsePropCond(wh, werr)) {
                                    ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "condición válida");
                                } else {
                                    ImGui::TextColored(ImVec4(0.95f, 0.4f, 0.35f, 1.0f), "%s", werr.c_str());
                                }
                            }
                        }
                        {
                            float smin = layer.value("scaleMin", 0.8f);
                            float smax = layer.value("scaleMax", 1.3f);
                            if (ImGui::DragFloat2("Escala min/max", &smin, 0.05f, 0.1f, 10.0f)) {
                                layer["scaleMin"] = smin; layer["scaleMax"] = smax; changed = true;
                            }
                        }
                        {
                            float cr = layer.value("claimRadius", 0.0f);
                            if (ImGui::DragFloat("Radio de exclusión (m)", &cr, 0.1f, 0.0f, 100.0f)) {
                                layer["claimRadius"] = cr; changed = true;
                            }
                        }
                        {
                            float hm[2] = {layer.value("humMin", 0.0f), layer.value("humMax", 1.0f)};
                            if (ImGui::DragFloat2("Humedad min/max", hm, 0.01f, 0.0f, 1.0f)) {
                                layer["humMin"] = hm[0]; layer["humMax"] = hm[1]; changed = true;
                            }
                        }
                        {
                            float tp[2] = {layer.value("tempMin", -1000.0f), layer.value("tempMax", 1000.0f)};
                            if (ImGui::DragFloat2("Temp min/max (°C)", tp, 1.0f, -100.0f, 100.0f)) {
                                layer["tempMin"] = tp[0]; layer["tempMax"] = tp[1]; changed = true;
                            }
                        }
                        {
                            float sp[2] = {layer.value("slopeMin", 0.0f), layer.value("slopeMax", 1.0f)};
                            if (ImGui::DragFloat2("Pendiente min/max", sp, 0.01f, 0.0f, 1.0f)) {
                                layer["slopeMin"] = sp[0]; layer["slopeMax"] = sp[1]; changed = true;
                            }
                        }
                        {
                            float fe = layer.value("feather", 0.08f);
                            if (ImGui::DragFloat("Feather", &fe, 0.01f, 0.0f, 1.0f)) {
                                layer["feather"] = fe; changed = true;
                            }
                        }
                        if (ImGui::Button("Borrar capa##p")) delIdx = pi;
                    }
                    ImGui::PopID();
                    ++pi;
                }
                if (moveFrom >= 0 && moveTo >= 0 && moveTo < (int)pl.size()) {
                    std::iter_swap(pl.begin() + moveFrom, pl.begin() + moveTo);
                    changed = true;
                }
                if (delIdx >= 0 && delIdx < (int)pl.size()) {
                    pl.erase(pl.begin() + delIdx);
                    changed = true;
                }
                if (ImGui::Button("+ Añadir capa de objeto")) {
                    pl.push_back(nlohmann::json::object({
                        {"name", "capa" + std::to_string(pl.size())},
                        {"mesh", "tree"},
                        {"density", 1.0},
                        {"scaleMin", 0.8}, {"scaleMax", 1.3},
                        {"claimRadius", 0.0},
                        {"humMin", 0.0}, {"humMax", 1.0},
                        {"tempMin", -1000.0}, {"tempMax", 1000.0},
                        {"slopeMin", 0.0}, {"slopeMax", 1.0},
                        {"feather", 0.08}
                    }));
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("Presets:");
                auto propPreset = [&](const nlohmann::json& jp) {
                    pl.push_back(jp); changed = true;
                };
                if (ImGui::SmallButton("Árbol")) propPreset(nlohmann::json::object({
                    {"name", "tree"}, {"mesh", "tree"},
                    {"humMin", 0.15}, {"humMax", 1.0},
                    {"tempMin", -2.0}, {"tempMax", 32.0},
                    {"slopeMin", 0.0}, {"slopeMax", 0.6},
                    {"density", 0.9}, {"scaleMin", 0.8}, {"scaleMax", 1.3},
                    {"claimRadius", 0.3}, {"feather", 0.08}}));
                if (ImGui::SmallButton("Roca")) propPreset(nlohmann::json::object({
                    {"name", "rock"}, {"mesh", "rock"},
                    {"humMin", 0.0}, {"humMax", 1.0},
                    {"tempMin", -1000.0}, {"tempMax", 1000.0},
                    {"slopeMin", 0.2}, {"slopeMax", 1.0},
                    {"density", 0.4}, {"scaleMin", 0.8}, {"scaleMax", 1.2},
                    {"claimRadius", 0.4}, {"feather", 0.08}}));
                if (ImGui::SmallButton("Casa")) propPreset(nlohmann::json::object({
                    {"name", "build"}, {"mesh", "house"},
                    {"humMin", 0.35}, {"humMax", 0.95},
                    {"tempMin", -5.0}, {"tempMax", 38.0},
                    {"slopeMin", 0.0}, {"slopeMax", 0.35},
                    {"density", 0.2}, {"scaleMin", 0.9}, {"scaleMax", 1.1},
                    {"claimRadius", 6.0}, {"feather", 0.08}}));
                if (ImGui::SmallButton("Camino")) propPreset(nlohmann::json::object({
                    {"name", "path"}, {"mesh", "path"},
                    {"humMin", 0.30}, {"humMax", 0.98},
                    {"tempMin", -10.0}, {"tempMax", 35.0},
                    {"slopeMin", 0.0}, {"slopeMax", 0.45},
                    {"density", 0.08}, {"scaleMin", 0.9}, {"scaleMax", 1.1},
                    {"claimRadius", 0.8}, {"feather", 0.08}}));
                if (changed && onSceneChanged) onSceneChanged();
            }

            ImGui::Separator();
            if (ImGui::Button("Regenerar planeta##planet")) {
                if (Application* app = MotorInstance::getInstance().getApplication()) {
                    app->regeneratePlanet(obj->name);
                }
                if (onSceneChanged) onSceneChanged();
            }
        }
    }

    if (playMode) ImGui::EndDisabled();

    ImGui::End();
}
