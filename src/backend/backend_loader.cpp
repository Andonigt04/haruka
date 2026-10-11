#include "backend/backend_loader.h"
#include "plugins/plugin_manager.h"   // hostImGuiAbi

#include <dlfcn.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

std::vector<std::string> BackendLoader::candidates(const std::string& engineBackend) {
    std::vector<std::string> out;
    if (const char* env = std::getenv("HARUKA_EDITOR_BACKEND"); env && *env) out.emplace_back(env);
    if (!engineBackend.empty()) out.push_back(engineBackend);

    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        const fs::path dir = exe.parent_path() / "backends";
        std::vector<std::string> here;
        if (fs::is_directory(dir, ec))
            for (const auto& f : fs::directory_iterator(dir, ec))
                if (f.is_regular_file(ec) && f.path().extension() == ".so") here.push_back(f.path().string());
        std::sort(here.begin(), here.end());
        out.insert(out.end(), here.begin(), here.end());
    }
    return out;
}

std::unique_ptr<IEngineBackend> BackendLoader::loadFile(const std::string& path, std::string& error) {
    // RTLD_NOW: un símbolo que falta (otra versión del editor, del motor o de ImGui) falla AQUÍ con
    // su nombre, no a mitad de un frame. RTLD_LOCAL: el backend no ensucia el espacio global.
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        const char* e = dlerror();
        error = e ? e : "dlopen fallo";
        return nullptr;
    }
    const auto* api = static_cast<const int*>(dlsym(h, HARUKA_EDITOR_BACKEND_API_SYMBOL));
    auto create = reinterpret_cast<HarukaEditorBackendCreateFn>(dlsym(h, HARUKA_EDITOR_BACKEND_CREATE_SYMBOL));
    if (!api || !create) {
        error = std::string("no es un backend del editor (falta ") +
                (!api ? HARUKA_EDITOR_BACKEND_API_SYMBOL : HARUKA_EDITOR_BACKEND_CREATE_SYMBOL) + ")";
        dlclose(h);
        return nullptr;
    }
    if (*api != HARUKA_EDITOR_BACKEND_API_VERSION) {
        // Antes de llamar a NADA: con otra versión, las posiciones de la vtable no son las nuestras.
        error = "contrato de backend v" + std::to_string(*api) + ", el editor es v" +
                std::to_string(HARUKA_EDITOR_BACKEND_API_VERSION);
        dlclose(h);
        return nullptr;
    }

    const HarukaImGuiAbi abi = PluginManager::hostImGuiAbi();
    char msg[512] = {0};
    IEngineBackend* b = nullptr;
    try {
        b = create(&abi, msg, sizeof msg);
    } catch (const std::exception& e) {
        error = std::string("create lanzo: ") + e.what();
    }
    if (!b) {
        if (error.empty()) error = msg[0] ? msg : "create devolvio nullptr";
        dlclose(h);
        return nullptr;
    }
    // `h` se queda abierto a propósito (ver la cabecera): el backend vive en ese código.
    return std::unique_ptr<IEngineBackend>(b);
}

std::unique_ptr<IEngineBackend> BackendLoader::load(const std::string& engineBackend) {
    tried.clear();
    if (const char* nb = std::getenv("HARUKA_EDITOR_NO_BACKEND"); nb && nb[0] == '1') {
        std::cout << "[Backend] HARUKA_EDITOR_NO_BACKEND=1: editor sin motor" << std::endl;
        return nullptr;
    }
    for (const auto& path : candidates(engineBackend)) {
        std::string error;
        auto b = loadFile(path, error);
        tried.push_back({path, error});
        if (b) {
            std::cout << "[Backend] " << b->name() << " desde " << path << std::endl;
            return b;
        }
        std::cerr << "[Backend] " << path << ": " << error << std::endl;
    }
    std::cerr << "[Backend] ninguno cargado: editor sin motor (solo paneles genericos)" << std::endl;
    return nullptr;
}
