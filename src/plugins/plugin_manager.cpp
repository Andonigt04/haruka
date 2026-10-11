#include "plugins/plugin_manager.h"

#include <imgui.h>
#include <dlfcn.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {

std::string exeDir() {
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    return ec ? std::string{} : exe.parent_path().string();
}

std::string stemOf(const std::string& path) {
    return fs::path(path).stem().string();
}

} // namespace

HarukaImGuiAbi PluginManager::hostImGuiAbi() {
    HarukaImGuiAbi a{};
    a.version = IMGUI_VERSION;
    a.sizeofIO = (int)sizeof(ImGuiIO);
    a.sizeofStyle = (int)sizeof(ImGuiStyle);
    a.sizeofDrawVert = (int)sizeof(ImDrawVert);
    a.sizeofDrawIdx = (int)sizeof(ImDrawIdx);
    a.context = ImGui::GetCurrentContext();
    ImGui::GetAllocatorFunctions(&a.alloc, &a.free, &a.allocUser);
    return a;
}

std::vector<std::string> PluginManager::defaultSearchDirs(const std::string& enginePluginsDir) {
    std::vector<std::string> dirs;
    if (const char* env = std::getenv("HARUKA_EDITOR_PLUGINS"); env && *env) {
        std::string v(env);
        size_t i = 0;
        while (i <= v.size()) {
            const size_t j = v.find(':', i);
            const std::string d = v.substr(i, j == std::string::npos ? std::string::npos : j - i);
            if (!d.empty()) dirs.push_back(d);
            if (j == std::string::npos) break;
            i = j + 1;
        }
    }
    if (const std::string d = exeDir(); !d.empty()) dirs.push_back((fs::path(d) / "plugins").string());

    const char* xdg = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    if (xdg && *xdg)        dirs.push_back((fs::path(xdg) / "haruka-editor" / "plugins").string());
    else if (home && *home) dirs.push_back((fs::path(home) / ".local" / "share" / "haruka-editor" / "plugins").string());

    if (!enginePluginsDir.empty()) dirs.push_back(enginePluginsDir);
    return dirs;
}

int PluginManager::scan(const std::vector<std::string>& dirs) {
    searchDirs = dirs;

    // Los que fallaron se reintentan: lo normal es que se haya recompilado el .so para arreglarlo.
    list.erase(std::remove_if(list.begin(), list.end(), [](const Plugin& p) { return !p.loaded(); }),
               list.end());

    // Orden estable (por nombre dentro de cada directorio): el orden de carga es el de los
    // paneles en el menú, y no debe depender de cómo liste el sistema de ficheros.
    std::vector<std::string> files;
    for (const auto& dir : dirs) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec)) continue;
        std::vector<std::string> here;
        for (const auto& f : fs::directory_iterator(dir, ec))
            if (f.is_regular_file(ec) && f.path().extension() == ".so") here.push_back(f.path().string());
        std::sort(here.begin(), here.end());
        files.insert(files.end(), here.begin(), here.end());
    }

    int loadedNow = 0;
    for (const auto& path : files) {
        const std::string name = stemOf(path);
        const auto same = std::find_if(list.begin(), list.end(), [&](const Plugin& p) { return p.name == name; });
        if (same != list.end()) {
            // Mismo nombre en dos directorios: gana el primero (es el de más prioridad). Cargar los
            // dos chocaría en los ids de panel y, peor, en los símbolos del propio plugin.
            if (same->path != path)
                std::cout << "[Plugins] " << path << " ignorado: ya cargado desde " << same->path << std::endl;
            continue;
        }
        list.push_back(Plugin{name, path});
        // `list` puede realojarse en el push_back de una carga POSTERIOR, no durante esta: el
        // puntero que usa `load` (vía `loading`) vale mientras dura.
        if (load(path, list.back())) ++loadedNow;
    }
    return loadedNow;
}

bool PluginManager::load(const std::string& path, Plugin& p) {
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        const char* e = dlerror();
        p.error = e ? e : "dlopen fallo";
        std::cerr << "[Plugins] " << p.name << ": " << p.error << std::endl;
        return false;
    }

    const auto* api = static_cast<const int*>(dlsym(h, HARUKA_EDITOR_PLUGIN_API_SYMBOL));
    auto init = reinterpret_cast<HarukaEditorPluginInitFn>(dlsym(h, HARUKA_EDITOR_PLUGIN_INIT_SYMBOL));
    if (!api && init) {
        // Tiene init pero no la versión: casi siempre un `const int` sin enlace externo.
        p.error = "falta " HARUKA_EDITOR_PLUGIN_API_SYMBOL " (usa HARUKA_EDITOR_PLUGIN_DECLARE_API())";
    } else if (!api || !init) {
        p.error = std::string("no es un plugin del editor (falta ") +
                  (!api ? HARUKA_EDITOR_PLUGIN_API_SYMBOL : HARUKA_EDITOR_PLUGIN_INIT_SYMBOL) + ")";
    } else if (*api != HARUKA_EDITOR_PLUGIN_API_VERSION) {
        p.error = "contrato v" + std::to_string(*api) + ", el editor es v" +
                  std::to_string(HARUKA_EDITOR_PLUGIN_API_VERSION);
    }
    if (!p.error.empty()) {
        std::cerr << "[Plugins] " << p.name << ": " << p.error << std::endl;
        dlclose(h);
        return false;
    }

    host.apiVersion = HARUKA_EDITOR_PLUGIN_API_VERSION;
    host.engineName = engineName.c_str();
    host.imgui = hostImGuiAbi();
    host.self = this;
    host.registerPanel = &PluginManager::hostRegisterPanel;
    host.log = &PluginManager::hostLog;

    p.handle = h;   // antes de init: `hostRegisterPanel` lo cuenta como cargándose
    loading = &p;
    int ok = 0;
    try {
        ok = init(&host);
    } catch (const std::exception& e) {
        p.error = std::string("init lanzo: ") + e.what();
    } catch (...) {
        p.error = "init lanzo una excepcion";
    }
    loading = nullptr;

    if (!ok) {
        if (p.error.empty()) p.error = "init devolvio 0 (¿version de ImGui distinta?)";
        std::cerr << "[Plugins] " << p.name << ": " << p.error << std::endl;
        registry.removeOwner(p.name);
        p.panels = 0;
        p.handle = nullptr;
        dlclose(h);
        return false;
    }
    std::cout << "[Plugins] cargado " << p.name << " (" << p.panels << " panel"
              << (p.panels == 1 ? "" : "es") << ") desde " << p.path << std::endl;
    return true;
}

int PluginManager::hostRegisterPanel(void* self, const HarukaPanelDesc* d) {
    auto* pm = static_cast<PluginManager*>(self);
    Plugin* owner = pm->loading;
    if (!owner) {
        std::cerr << "[Plugins] registerPanel fuera de init: rechazado" << std::endl;
        return 0;
    }
    if (!d || !d->id || !*d->id || !d->draw) {
        std::cerr << "[Plugins] " << owner->name << ": panel sin id o sin draw" << std::endl;
        return 0;
    }
    if (pm->registry.find(d->id)) {
        std::cerr << "[Plugins] " << owner->name << ": el id '" << d->id << "' ya existe" << std::endl;
        return 0;
    }

    // Copia de la descripción: el plugin puede tenerla en la pila de su `init`.
    const HarukaPanelDesc desc = *d;
    void* user = desc.user;
    PanelEntry e;
    e.id = desc.id;
    e.label = desc.label && *desc.label ? desc.label : desc.id;
    e.menuGroup = desc.menuGroup;
    e.open = desc.openByDefault != 0;
    e.lockedInPlay = desc.lockedInPlay != 0;
    e.owner = owner->name;
    e.draw = [f = desc.draw, user] { f(user); };
    if (desc.whileClosed)      e.whileClosed = [f = desc.whileClosed, user] { f(user); };
    if (desc.onProjectChanged) e.onProjectChanged = [f = desc.onProjectChanged, user](const std::string& r) { f(user, r.c_str()); };
    if (desc.clearSelection)   e.clearSelection = [f = desc.clearSelection, user] { f(user); };
    if (desc.onSceneChanged)   e.onSceneChanged = [f = desc.onSceneChanged, user](void* s) { f(user, s); };

    PanelEntry& added = pm->registry.add(std::move(e));
    ++owner->panels;

    // Un panel que llega tarde recibe lo que ya hay. Sin esto se quedaría con proyecto y escena
    // vacíos hasta el siguiente cambio: el mismo fallo mudo que tuvo el panel de prefabricados.
    if (added.onProjectChanged && pm->queryProject) {
        const std::string root = pm->queryProject();
        if (!root.empty()) added.onProjectChanged(root);
    }
    if (added.onSceneChanged && pm->queryScene) {
        if (void* s = pm->queryScene()) added.onSceneChanged(s);
    }
    return 1;
}

void PluginManager::hostLog(void* self, int level, const char* message) {
    const auto* pm = static_cast<PluginManager*>(self);
    const std::string who = pm->loading ? pm->loading->name : "plugin";
    (level >= 2 ? std::cerr : std::cout) << "[" << who << "] " << (message ? message : "") << std::endl;
}

bool PluginManager::unload(const std::string& name) {
    const auto it = std::find_if(list.begin(), list.end(), [&](const Plugin& p) { return p.name == name; });
    if (it == list.end()) return false;
    if (it->loaded()) {
        registry.removeOwner(it->name);   // primero: sus std::function apuntan a su código
        if (auto sd = reinterpret_cast<HarukaEditorPluginShutdownFn>(dlsym(it->handle, HARUKA_EDITOR_PLUGIN_SHUTDOWN_SYMBOL))) {
            try { sd(); } catch (...) { std::cerr << "[Plugins] " << name << ": shutdown lanzo" << std::endl; }
        }
        dlclose(it->handle);
        std::cout << "[Plugins] descargado " << name << std::endl;
    }
    list.erase(it);
    return true;
}

void PluginManager::unloadAll() {
    // Al revés de como se cargaron, por si uno usa algo de otro.
    while (!list.empty()) unload(list.back().name);
}

void PluginManager::processPending() {
    for (const auto& n : pendingUnload) unload(n);
    pendingUnload.clear();
    if (pendingRescan) {
        pendingRescan = false;
        scan(searchDirs);
    }
}

void PluginManager::drawPanel() {
    if (!ImGui::Begin("Plugins")) { ImGui::End(); return; }

    if (ImGui::Button("Buscar plugins nuevos")) requestRescan();
    ImGui::SameLine();
    ImGui::TextDisabled("contrato v%d · ImGui %s", HARUKA_EDITOR_PLUGIN_API_VERSION, IMGUI_VERSION);

    if (list.empty()) ImGui::TextDisabled("Ningun plugin encontrado.");
    if (!list.empty() && ImGui::BeginTable("##plugins", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Plugin");
        ImGui::TableSetupColumn("Estado");
        ImGui::TableSetupColumn("Paneles", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();
        for (const auto& p : list) {
            ImGui::PushID(p.name.c_str());
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(p.name.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", p.path.c_str());
            ImGui::TableNextColumn();
            if (p.loaded()) ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1), "cargado");
            else            ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.4f, 1), "%s", p.error.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", p.panels);
            ImGui::TableNextColumn();
            if (p.loaded() && ImGui::SmallButton("Descargar")) requestUnload(p.name);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (ImGui::TreeNode("Directorios")) {
        for (const auto& d : searchDirs) {
            std::error_code ec;
            const bool exists = fs::is_directory(d, ec);
            ImGui::BulletText("%s%s", d.c_str(), exists ? "" : "  (no existe)");
        }
        ImGui::TreePop();
    }
    ImGui::End();
}
