/**
 * @file panels/world_panel.cpp
 * @brief Anfitrión del pincel del mundo. Ver `world_panel.h`.
 */
#include "panels/world_panel.h"

#include "commands/scene_commands.h"
#include "commands/command_history.h"

#include "Haruka/Core/Application.h"
#include "Haruka/Renderer/MotorInstance.h"
#include "Haruka/Core/Scene/SceneManager.h"
#include "Haruka/World/Planet/PlanetarySystem.h"
#include "Haruka/World/Planet/Planet.h"

#include <imgui.h>
#include <cstdio>
#include <nlohmann/json.hpp>

void WorldPanel::setProjectPath(const std::string& root) {
    m_projectPath = root;
    // Los PNG de cuevas/islas nuevas: `assets/vox/` del proyecto (ruta RELATIVA en la escena, como
    // `surface.elevationMap`; `caveResolveAssetPath` la resuelve bajo la raíz del proyecto).
    m_panel.setAssetDir("assets/vox/");
}

void WorldPanel::attachWorld() {
    Haruka::Core::Application* app = Haruka::Renderer::MotorInstance::getInstance().getApplication();
    Haruka::World::Planet::PlanetarySystem* ps = app ? app->getPlanetarySystem() : nullptr;
    Haruka::World::Planet::TerrestrialPlanet* tp = ps ? ps->activeTerrestrialMut() : nullptr;
    Haruka::World::Vox::VoxWorld* w = (tp && tp->vox().configured()) ? &tp->vox() : nullptr;
    if (m_panel.world() != w) {
        m_panel.setWorld(w, tp ? tp->position() : glm::dvec3(0.0), tp ? tp->radius() : 0.0);
        m_panel.setDesignerMode(true);   // el editor escribe en el MUNDO, nunca en una partida
        if (w) std::printf("[WorldPanel] campo del planeta '%s' enganchado: %zu cuevas · %zu islas · modo disenador\n",
                           tp->name().c_str(), w->caveDefs().size(), w->islandDefs().size());
    }
    if (m_wired) return;
    m_wired = true;
    m_panel.setOnCaveCreated([this](const Haruka::World::Vox::CaveDef& d) {
        nlohmann::json p;
        p["radiusM"] = d.radiusM; p["depthM"] = d.depthM;
        p["mouth"] = { d.mouth.x, d.mouth.y, d.mouth.z };
        if (!d.layers.empty()) {
            nlohmann::json layers = nlohmann::json::array();
            for (const auto& l : d.layers) layers.push_back({ { "ceiling", l.ceiling }, { "floor", l.floor } });
            p["layers"] = layers;
        } else { p["planta"] = d.planta; p["perfil"] = d.perfil; }   // legacy format, only if it has no layers
        p["draftSeed"] = d.draftSeed;
        addSceneEntry("Cave", d.name, d.centerDir, p);
    });
    m_panel.setOnIslandCreated([this](const Haruka::World::Vox::IslandDef& d) {
        nlohmann::json p;
        p["radiusM"] = d.radiusM; p["topM"] = d.topM; p["rootM"] = d.rootM; p["altitudeM"] = d.altitudeM;
        p["planta"] = d.planta; p["cima"] = d.cima; p["raiz"] = d.raiz; p["draftSeed"] = d.draftSeed;
        addSceneEntry("Island", d.name, d.centerDir, p);
    });
    m_panel.setOnCaveRemoved([this](const std::string& n) { removeSceneEntry(n); });
    m_panel.setOnIslandRemoved([this](const std::string& n) { removeSceneEntry(n); });
    m_panel.setOnChanged([this]() { if (m_onChanged) m_onChanged(); });
}

void WorldPanel::addSceneEntry(const std::string& type, const std::string& name, const glm::dvec3& dir,
                               const nlohmann::json& props) {
    if (!m_scene) return;
    Haruka::Core::Application* app = Haruka::Renderer::MotorInstance::getInstance().getApplication();
    Haruka::World::Planet::PlanetarySystem* ps = app ? app->getPlanetarySystem() : nullptr;
    const Haruka::World::Planet::TerrestrialPlanet* tp = ps ? ps->activeTerrestrial() : nullptr;
    if (!tp) return;
    Haruka::Core::Scene::SceneObject obj;
    obj.name = name;
    obj.type = type;
    obj.objectType = Haruka::Tools::classifyObjectType(type);
    // `position` = centro de la caja EN EL MUNDO (para la cueva, sobre la superficie de referencia;
    // la cota real la pone `caveBoxFromDef` al cargar). Es lo que `PlanetarySystem::buildFromScene`
    // convierte otra vez en dirección.
    obj.position = tp->position() + dir * tp->radius();
    obj.rotation = glm::dquat(1.0, 0.0, 0.0, 0.0);
    obj.scale = glm::dvec3(1.0);
    obj.properties = props;
    if (m_history) m_history->execute(std::make_unique<AddObjectCommand>(m_scene, obj));
    else m_scene->addLoadedObject(std::make_shared<Haruka::Core::Scene::SceneObject>(obj));
}

void WorldPanel::removeSceneEntry(const std::string& name) {
    if (!m_scene || !m_scene->getObject(name)) return;
    if (m_history) m_history->execute(std::make_unique<DeleteObjectCommand>(m_scene, name));
    else m_scene->removeObject(name);
}

void WorldPanel::saveWorldEdits() {
    if (m_panel.world()) m_panel.world()->saveEdits();
}

void WorldPanel::syncFromScene() {
    Haruka::World::Vox::VoxWorld* w = m_panel.world();
    if (!w || !m_scene) return;
    Haruka::Core::Application* app = Haruka::Renderer::MotorInstance::getInstance().getApplication();
    Haruka::World::Planet::PlanetarySystem* ps = app ? app->getPlanetarySystem() : nullptr;
    const Haruka::World::Planet::TerrestrialPlanet* tp = ps ? ps->activeTerrestrial() : nullptr;
    if (!tp) return;
    std::vector<Haruka::World::Vox::CaveDef> caves; std::vector<Haruka::World::Vox::IslandDef> islands;
    for (const auto& o : m_scene->getObjectsMutable()) {
        if (!o) continue;
        if (o->type == "Cave")   caves.push_back(Haruka::World::Planet::caveDefFromSceneObject(*o, tp->position()));
        if (o->type == "Island") islands.push_back(Haruka::World::Planet::islandDefFromSceneObject(*o, tp->position()));
    }
    // Quitadas o cambiadas: fuera del mundo (y se vuelven a poner abajo si cambiaron).
    for (const Haruka::World::Vox::CaveDef& d : std::vector<Haruka::World::Vox::CaveDef>(w->caveDefs())) {
        bool same = false;
        for (const auto& c : caves) if (c.name == d.name) { same = Haruka::World::Planet::sameCaveDef(c, d); break; }
        if (!same) w->removeCave(d.name);
    }
    for (const Haruka::World::Vox::IslandDef& d : std::vector<Haruka::World::Vox::IslandDef>(w->islandDefs())) {
        bool same = false;
        for (const auto& c : islands) if (c.name == d.name) { same = Haruka::World::Planet::sameIslandDef(c, d); break; }
        if (!same) w->removeIsland(d.name);
    }
    for (const auto& c : caves)   w->placeCave(c);     // no-op si ya está igual
    for (const auto& c : islands) w->placeIsland(c);
}

// ── VISTA DE EDICIÓN Y DÓNDE ESTÁS ───────────────────────────────────────────────────────────────
// Para esculpir viendo la ROCA: sin clima y sin agua, con el terreno teñido por su cota sobre el
// nivel del mar (lo azul es donde habrá agua). El esculpido es el pincel de siempre, debajo: los
// mismos trazos sobre el campo que las cuevas y las islas. Ver `Application::EditView`.
static void drawProbe(const char* label, const Haruka::Core::Application::SurfaceProbe& p) {
    if (!p.ok) { ImGui::TextDisabled("%s: sin planeta", label); return; }
    ImGui::Text("%s", label);
    ImGui::Indent();
    ImGui::Text("lat %.5f  lon %.5f", p.latDeg, p.lonDeg);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("La convención de SURVIVAL_SPAWN=lat,lon: con estas coordenadas el juego aparece aquí.");
    ImGui::Text("sobre el mar %.1f m", p.altSeaM);
    ImGui::Text("suelo %.1f m%s  ·  sobre el suelo %.1f m", p.groundSeaM,
                p.underWater ? " (bajo el mar)" : "", p.aboveGroundM);
    ImGui::Unindent();
}

void WorldPanel::drawEditView() {
    Haruka::Core::Application* app = Haruka::Renderer::MotorInstance::getInstance().getApplication();
    if (!app) return;
    if (!ImGui::CollapsingHeader("Vista de edición", ImGuiTreeNodeFlags_DefaultOpen)) return;
    Haruka::Core::Application::EditView v = app->editView();
    bool changed = false;
    if (ImGui::Button("Modo terreno")) { v.weather = false; v.water = false; v.seaTint = true; changed = true; }
    ImGui::SameLine();
    if (ImGui::Button("Normal")) { v = Haruka::Core::Application::EditView{}; changed = true; }
    changed |= ImGui::Checkbox("Clima (nubes, lluvia, nieve)", &v.weather);
    changed |= ImGui::Checkbox("Agua (mar, lagos)", &v.water);
    changed |= ImGui::Checkbox("Cota sobre el mar", &v.seaTint);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Verde/ocre/marrón/blanco sobre el nivel del mar, azul debajo (ahí habrá agua).\n"
                          "Línea amarilla = costa (cota 0). Isolíneas cada 10/50/250/1000 m según el zoom.");
    if (changed) app->setEditView(v);

    if (const Haruka::Core::Camera* cam = app->getCamera())
        drawProbe("Cámara", app->probeSurface(cam->position));
    if (m_panel.hoverHit())
        drawProbe("Cursor", app->probeSurface(m_panel.hoverPos()));
    else if (v.seaTint || m_panel.tool() != Haruka::Tools::WorldEditPanel::Tool::None)
        ImGui::TextDisabled("Cursor: fuera del terreno");
    else
        ImGui::TextDisabled("Cursor: activa \"Cota sobre el mar\" o un pincel para leerlo");
    ImGui::Separator();
}

void WorldPanel::onImGuiRender() {
    attachWorld();
    syncFromScene();
    drawEditView();
    m_panel.onImGuiRender();
}
