#include "panels/generic_scene_panels.h"
#include "commands/command_history.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_set>

namespace {

void drawNode(ISceneModel* model, const std::vector<SceneNode>& nodes, const SceneNode& n,
              const std::string& selected, std::unordered_set<std::string>& visited) {
    if (!visited.insert(n.id).second) return;   // un ciclo de padres no debe colgar el panel
    bool hasChildren = false;
    for (const auto& c : nodes) if (c.parent == n.id) { hasChildren = true; break; }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth |
                               ImGuiTreeNodeFlags_DefaultOpen;
    if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (n.id == selected) flags |= ImGuiTreeNodeFlags_Selected;

    const bool open = ImGui::TreeNodeEx(n.id.c_str(), flags, "%s  (%s)", n.name.c_str(), n.type.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) model->select(n.id);
    if (open && hasChildren) {
        for (const auto& c : nodes) if (c.parent == n.id) drawNode(model, nodes, c, selected, visited);
        ImGui::TreePop();
    }
}

} // namespace

void GenericHierarchyPanel::onImGuiRender() {
    if (!ImGui::Begin("Scene Hierarchy")) { ImGui::End(); return; }
    if (!model) { ImGui::TextDisabled("Sin escena."); ImGui::End(); return; }

    const std::vector<SceneNode> nodes = model->nodes();
    const std::string selected = model->selected();
    std::unordered_set<std::string> ids, visited;
    for (const auto& n : nodes) ids.insert(n.id);
    // Raíz = sin padre o con un padre que no está (el motor puede mandar un subárbol).
    for (const auto& n : nodes)
        if (n.parent.empty() || !ids.count(n.parent)) drawNode(model, nodes, n, selected, visited);
    if (nodes.empty()) ImGui::TextDisabled("La escena esta vacia.");
    ImGui::End();
}

void GenericInspectorPanel::onImGuiRender() {
    if (!ImGui::Begin("Inspector")) { ImGui::End(); return; }
    if (!model) { ImGui::TextDisabled("Sin escena."); ImGui::End(); return; }

    const std::string sel = model->selected();
    const std::vector<SceneNode> nodes = model->nodes();
    const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const SceneNode& n) { return n.id == sel; });
    if (it == nodes.end()) {
        ImGui::TextDisabled("Nada seleccionado.");
    } else {
        ImGui::Text("%s", it->name.c_str());
        ImGui::TextDisabled("%s · id %s", it->type.c_str(), it->id.c_str());
        ImGui::Separator();
        for (const auto& p : it->properties) editProperty(*it, p);
    }
    ImGui::Separator();
    ImGui::TextDisabled("revision %llu", (unsigned long long)model->revision());
    ImGui::End();
}

void GenericInspectorPanel::editProperty(const SceneNode& node, const SceneProperty& prop) {
    const std::string key = node.id + "/" + prop.name;
    SceneProperty cur = (key == activeKey) ? activeCurrent : prop;
    if ((cur.type == "vec3" || cur.type == "color") && cur.num.size() < 3) cur.num.resize(3, 0.0);
    if ((cur.type == "float" || cur.type == "int" || cur.type == "bool") && cur.num.empty()) cur.num.push_back(0.0);

    ImGui::PushID(key.c_str());
    if (prop.readOnly) ImGui::BeginDisabled();
    bool changed = false;
    const char* label = prop.name.c_str();
    if (cur.type == "float") {
        float v = (float)cur.num[0];
        if (ImGui::DragFloat(label, &v, 0.01f)) { cur.num[0] = v; changed = true; }
    } else if (cur.type == "int") {
        int v = (int)cur.num[0];
        if (ImGui::DragInt(label, &v)) { cur.num[0] = v; changed = true; }
    } else if (cur.type == "bool") {
        bool v = cur.num[0] != 0.0;
        if (ImGui::Checkbox(label, &v)) { cur.num[0] = v ? 1.0 : 0.0; changed = true; }
    } else if (cur.type == "vec3") {
        float v[3] = {(float)cur.num[0], (float)cur.num[1], (float)cur.num[2]};
        if (ImGui::DragFloat3(label, v, 0.01f)) { for (int i = 0; i < 3; ++i) cur.num[i] = v[i]; changed = true; }
    } else if (cur.type == "color") {
        float v[3] = {(float)cur.num[0], (float)cur.num[1], (float)cur.num[2]};
        if (ImGui::ColorEdit3(label, v)) { for (int i = 0; i < 3; ++i) cur.num[i] = v[i]; changed = true; }
    } else if (cur.type == "string") {
        // Búfer local por frame: mientras el campo está activo ImGui guarda su propio estado del
        // texto y solo escribe aquí lo que cambia.
        char buf[512];
        std::snprintf(buf, sizeof buf, "%s", cur.str.c_str());
        if (ImGui::InputText(label, buf, sizeof buf)) { cur.str = buf; changed = true; }
    } else {
        ImGui::TextDisabled("%s: tipo '%s' sin widget", label, cur.type.c_str());
    }

    if (ImGui::IsItemActivated()) {
        activeKey = key;
        activeBefore = prop;
        activeCurrent = cur;
    }
    if (changed) {
        if (key == activeKey) activeCurrent = cur;
        model->setProperty(node.id, cur);   // en vivo: el motor lo pinta ya
        if (onChanged) onChanged();
    }
    if (ImGui::IsItemDeactivated() && key == activeKey) {
        // Un comando por gesto completo, con el valor de ANTES de empezar a arrastrar.
        if (ImGui::IsItemDeactivatedAfterEdit() && history && activeBefore != activeCurrent)
            history->execute(std::make_unique<SetPropertyCommand>(model, node.id, activeBefore, activeCurrent));
        activeKey.clear();
    }
    if (prop.readOnly) ImGui::EndDisabled();
    ImGui::PopID();
}
