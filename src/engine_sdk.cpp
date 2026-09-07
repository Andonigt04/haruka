#include "engine_sdk.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

namespace {

/// @brief Directorio del EJECUTABLE (no el cwd: el editor se lanza desde el menú, un
///        script o una terminal cualquiera, y el cwd cambia con quien lo arranque).
std::string exeDir() {
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (ec) return {};
    return exe.parent_path().string();
}

/// @brief Rutas candidatas a raíz del SDK, en orden de prioridad. Ver la cabecera.
std::vector<std::string> candidateRoots() {
    std::vector<std::string> roots;

    if (const char* env = std::getenv("HARUKA_ENGINE_ROOT"); env && *env) {
        roots.emplace_back(env);
    }

    // Elección persistida del usuario. Se lee con tolerancia: un config a medio escribir
    // no debe impedir arrancar el editor, solo pasa al siguiente candidato.
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    fs::path cfg;
    if (xdg && *xdg)        cfg = fs::path(xdg) / "haruka-editor" / "engine.json";
    else if (home && *home) cfg = fs::path(home) / ".config" / "haruka-editor" / "engine.json";
    if (!cfg.empty()) {
        std::ifstream in(cfg);
        if (in) {
            try {
                const auto j = nlohmann::json::parse(in);
                if (const auto it = j.find("root"); it != j.end() && it->is_string()) {
                    roots.emplace_back(it->get<std::string>());
                }
            } catch (const std::exception& e) {
                std::cerr << "[EngineSdk] " << cfg << " ilegible: " << e.what() << std::endl;
            }
        }
    }

#ifdef HARUKA_EDITOR_ENGINE_DIR
    roots.emplace_back(HARUKA_EDITOR_ENGINE_DIR);   // SDK instalado por paquete
#endif

    // Layout portable: el SDK viaja junto al ejecutable (zip descomprimido en $HOME).
    if (const std::string dir = exeDir(); !dir.empty()) {
        roots.push_back((fs::path(dir) / "engine").string());
        roots.push_back((fs::path(dir).parent_path() / "engine").string());
    }

#ifdef HARUKA_ENGINE_ROOT
    roots.emplace_back(HARUKA_ENGINE_ROOT);         // build de desarrollo: ../haruka-cpp
#endif

    return roots;
}

/// @brief Un directorio vale como SDK si trae descriptor, o si al menos es un árbol CMake
///        (motor antiguo sin descriptor: se le aplican los valores por defecto).
bool looksLikeEngine(const fs::path& root) {
    std::error_code ec;
    if (!fs::is_directory(root, ec)) return false;
    return fs::exists(root / "haruka-engine.json", ec) || fs::exists(root / "CMakeLists.txt", ec);
}

/// @brief Resuelve `value` contra la raíz del SDK si es relativa, y solo devuelve la
///        ruta si existe: el descriptor puede declarar cosas que ese checkout no tiene
///        (un runtime sin compilar, por ejemplo), y una ruta muerta confunde más que un vacío.
std::string resolveExisting(const fs::path& root, const std::string& value) {
    if (value.empty()) return {};
    fs::path p(value);
    if (p.is_relative()) p = root / p;
    std::error_code ec;
    return fs::exists(p, ec) ? fs::weakly_canonical(p, ec).string() : std::string{};
}

/// @brief Lee haruka-engine.json sobre los valores por defecto. Los campos ausentes se
///        quedan como estaban: el descriptor es un conjunto de OVERRIDES, no un esquema
///        obligatorio, para que un motor pueda declarar solo aquello en lo que difiere.
void applyDescriptor(EngineSdk& sdk, const fs::path& file) {
    std::ifstream in(file);
    if (!in) return;
    try {
        const auto j = nlohmann::json::parse(in);
        if (auto it = j.find("name"); it != j.end() && it->is_string())
            sdk.name = it->get<std::string>();
        if (auto it = j.find("version"); it != j.end() && it->is_string())
            sdk.version = it->get<std::string>();
        if (auto it = j.find("template"); it != j.end() && it->is_string())
            sdk.templateDir = it->get<std::string>();
        if (auto it = j.find("build"); it != j.end() && it->is_object()) {
            if (auto s = it->find("script"); s != it->end() && s->is_string())
                sdk.buildScript = s->get<std::string>();
            if (auto v = it->find("cmakeEngineVar"); v != it->end() && v->is_string())
                sdk.cmakeEngineVar = v->get<std::string>();
        }
        if (auto it = j.find("runtime"); it != j.end() && it->is_object()) {
            if (auto b = it->find("binary"); b != it->end() && b->is_string())
                sdk.runtimeBinary = b->get<std::string>();
        }
    } catch (const std::exception& e) {
        // Descriptor roto: se sigue con los valores por defecto en vez de dejar al editor
        // sin motor. Se avisa porque el motor sí está, pero se ignora lo que declaraba.
        std::cerr << "[EngineSdk] descriptor invalido en " << file << ": " << e.what()
                  << " (se usan los valores por defecto)" << std::endl;
    }
}

EngineSdk locate() {
    for (const std::string& candidate : candidateRoots()) {
        if (candidate.empty()) continue;
        std::error_code ec;
        const fs::path root = fs::weakly_canonical(fs::path(candidate), ec);
        if (ec || !looksLikeEngine(root)) continue;

        EngineSdk sdk;
        sdk.root = root.string();
        sdk.name = root.filename().string();
        sdk.templateDir = "template";        // convención por defecto (haruka-cpp)
        sdk.runtimeBinary = "build/HarukaEngine";

        if (const fs::path descriptor = root / "haruka-engine.json"; fs::exists(descriptor, ec)) {
            applyDescriptor(sdk, descriptor);
        }

        sdk.templateDir   = resolveExisting(root, sdk.templateDir);
        sdk.runtimeBinary = resolveExisting(root, sdk.runtimeBinary);

        std::cout << "[EngineSdk] motor: " << sdk.name
                  << (sdk.version.empty() ? "" : " " + sdk.version)
                  << " @ " << sdk.root
                  << (sdk.templateDir.empty() ? " (sin template)" : "") << std::endl;
        return sdk;
    }

    std::cerr << "[EngineSdk] no se encontro ningun motor. Buscado en:\n"
              << EngineSdk::searchPathsDescription()
              << "Crear y compilar proyectos no funcionara hasta que haya uno." << std::endl;
    return {};
}

} // namespace

const EngineSdk& EngineSdk::current() {
    static const EngineSdk sdk = locate();
    return sdk;
}

std::string EngineSdk::searchPathsDescription() {
    std::string out;
    for (const std::string& root : candidateRoots()) {
        if (!root.empty()) out += "  - " + root + "\n";
    }
    if (out.empty()) out = "  (ninguna: define $HARUKA_ENGINE_ROOT)\n";
    return out;
}
