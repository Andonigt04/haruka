#include "panels/panel_registry.h"

#include <imgui.h>
#include <algorithm>
#include <exception>
#include <iostream>

PanelEntry& PanelRegistry::add(PanelEntry entry) {
    if (find(entry.id)) {
        // Dos con el mismo id: el segundo no se podría abrir por id ni buscar desde un menú.
        std::cerr << "[PanelRegistry] id repetido '" << entry.id << "': se registra igualmente" << std::endl;
    }
    entries.push_back(std::move(entry));
    return entries.back();
}

PanelEntry* PanelRegistry::find(const std::string& id) {
    for (auto& e : entries)
        if (e.id == id) return &e;
    return nullptr;
}

bool* PanelRegistry::openFlag(const std::string& id) {
    PanelEntry* e = find(id);
    return e ? &e->open : nullptr;
}

size_t PanelRegistry::removeOwner(const std::string& owner) {
    const size_t before = entries.size();
    for (auto it = entries.begin(); it != entries.end();)
        it = (it->owner == owner) ? entries.erase(it) : it + 1;
    return before - entries.size();
}

void PanelRegistry::drawAll(bool playMode) {
    // Por ÍNDICE y no con iteradores: un panel puede registrar otro mientras se dibuja (el de
    // plugins al cargar uno), y push_back en un deque invalida iteradores, no índices.
    for (size_t i = 0; i < entries.size(); ++i) {
        PanelEntry& e = entries[i];
        if (!e.open) {
            if (e.whileClosed) e.whileClosed();
            continue;
        }
        if (!e.draw) continue;

        const bool disabled = playMode && e.lockedInPlay;
        if (disabled) ImGui::BeginDisabled();
        try {
            e.draw();
        } catch (const std::exception& ex) {
            std::cerr << "[Editor] panel '" << e.label << "' lanzo: " << ex.what() << std::endl;
        }
        if (disabled) ImGui::EndDisabled();
    }
}

void PanelRegistry::drawViewMenuItems() {
    // El menú agrupa por `menuGroup` y, dentro de cada grupo, respeta el orden de registro: el de
    // dibujo lo fija quién tiene que ir antes que quién, no lo que se lee mejor en el menú.
    int lo = 0, hi = 0;
    bool any = false;
    for (const auto& e : entries) {
        if (!any) { lo = hi = e.menuGroup; any = true; }
        lo = std::min(lo, e.menuGroup);
        hi = std::max(hi, e.menuGroup);
    }
    bool first = true;
    for (int g = lo; any && g <= hi; ++g) {
        bool groupStarted = false;
        for (auto& e : entries) {
            if (e.menuGroup != g) continue;
            if (!groupStarted && !first) ImGui::Separator();
            groupStarted = true;
            first = false;
            ImGui::MenuItem(e.label.c_str(), nullptr, &e.open);
        }
    }
}

void PanelRegistry::openFromList(const std::string& list) {
    size_t i = 0;
    while (i < list.size()) {
        const size_t j = list.find_first_of(", ", i);
        const std::string id = list.substr(i, j == std::string::npos ? std::string::npos : j - i);
        if (!id.empty()) {
            if (PanelEntry* e = find(id)) e->open = true;
            else std::cerr << "[PanelRegistry] HARUKA_EDITOR_OPEN: no hay panel '" << id << "'" << std::endl;
        }
        if (j == std::string::npos) break;
        i = j + 1;
    }
}

void PanelRegistry::projectChanged(const std::string& root) {
    for (auto& e : entries)
        if (e.onProjectChanged) e.onProjectChanged(root);
}

void PanelRegistry::sceneChanged(void* scene) {
    for (auto& e : entries)
        if (e.clearSelection) e.clearSelection();
    for (auto& e : entries)
        if (e.onSceneChanged) e.onSceneChanged(scene);
}
