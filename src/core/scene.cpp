#include "scene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <dlfcn.h>

namespace Haruka {

Scene::Scene() : sceneName("Untitled") {}

Scene::Scene(const std::string& name) : sceneName(name) {}

Scene::~Scene() {}

void Scene::addObject(const SceneObject& obj) {
    objects.push_back(obj);
    std::cout << "Object added: " << obj.name << std::endl;
}

void Scene::removeObject(const std::string& name) {
    auto it = std::find_if(objects.begin(), objects.end(),
        [&name](const SceneObject& o) { return o.name == name; });
    if (it != objects.end()) {
        objects.erase(it);
        std::cout << "Object removed: " << name << std::endl;
    }
}

SceneObject* Scene::getObject(const std::string& name) {
    auto it = std::find_if(objects.begin(), objects.end(),
        [&name](const SceneObject& o) { return o.name == name; });
    if (it != objects.end()) {
        return &(*it);
    }
    return nullptr;
}

bool Scene::save(const std::string& filepath) {
    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        nlohmann::json j;
        j["name"] = sceneName;
        j["objects"] = nlohmann::json::array();

        for (const auto& obj : objects) {
            nlohmann::json o;
            o["name"] = obj.name;
            o["type"] = obj.type;
            o["modelPath"] = obj.modelPath;
            o["position"] = {obj.position.x, obj.position.y, obj.position.z};
            o["rotation"] = {obj.rotation.x, obj.rotation.y, obj.rotation.z};
            o["scale"]    = {obj.scale.x, obj.scale.y, obj.scale.z};
            o["color"]    = {obj.color.x, obj.color.y, obj.color.z};
            o["intensity"] = obj.intensity;
            o["parentIndex"] = obj.parentIndex;
            o["childrenIndices"] = obj.childrenIndices;

            if (obj.material) {
                o["material"] = obj.material->toJSON();
            }

            j["objects"].push_back(o);
        }

        std::ofstream out(filepath, std::ios::trunc);
        if (!out.is_open()) return false;
        out << j.dump(4);
        return out.good();
    } catch (...) {
        return false;
    }
}

bool Scene::load(const std::string& filepath) {
    try {
        std::ifstream in(filepath);
        if (!in.is_open()) return false;

        nlohmann::json j;
        in >> j;

        if (j.contains("name")) sceneName = j["name"].get<std::string>();
        objects.clear();

        // Descubrir inicializador
        if (j.contains("initializer")) {
            std::string initPath = j["initializer"].get<std::string>();
            std::filesystem::path scenePath(filepath);
            std::filesystem::path projectRoot = scenePath.parent_path().parent_path();
            std::filesystem::path fullInitPath = projectRoot / initPath;
            
            initializerPath = fullInitPath.string();
            std::cout << "Scene initializer discovered: " << initializerPath << std::endl;
        }

        if (j.contains("objects") && j["objects"].is_array()) {
            for (const auto& o : j["objects"]) {
                SceneObject obj;
                obj.name = o.value("name", "");
                obj.type = o.value("type", "");
                obj.modelPath = o.value("modelPath", "");
                if (o.contains("position") && o["position"].size() == 3)
                    obj.position = {o["position"][0], o["position"][1], o["position"][2]};
                if (o.contains("rotation") && o["rotation"].size() == 3)
                    obj.rotation = {o["rotation"][0], o["rotation"][1], o["rotation"][2]};
                if (o.contains("scale") && o["scale"].size() == 3)
                    obj.scale = {o["scale"][0], o["scale"][1], o["scale"][2]};
                if (o.contains("color") && o["color"].size() == 3)
                    obj.color = {o["color"][0], o["color"][1], o["color"][2]};
                obj.intensity = o.value("intensity", 1.0);
                obj.parentIndex = o.value("parentIndex", -1);
                if (o.contains("childrenIndices")) obj.childrenIndices = o["childrenIndices"].get<std::vector<int>>();

                if (o.contains("material")) {
                    obj.material = std::make_shared<MaterialComponent>();
                    obj.material->fromJSON(o["material"]);
                }

                objects.push_back(obj);
            }
        }

        // Ejecutar inicializador si existe
        if (j.contains("initializer")) {
            std::string initPath = j["initializer"].get<std::string>();
            std::filesystem::path scenePath(filepath);
            std::filesystem::path projectRoot = scenePath.parent_path().parent_path();
            std::filesystem::path libPath = projectRoot / "build" / "libTestGameLogic.so";
            
            std::cout << "Loading initializer: " << libPath.string() << std::endl;
            
            void* handle = dlopen(libPath.c_str(), RTLD_LAZY);
            if (handle) {
                typedef void (*InitFunc)(Haruka::Scene*);
                InitFunc initFunc = nullptr;
                
                const char* symbols[] = {
                    "_ZN9GameLogic15GameInitializer14initializeGameEPN6Haruka5SceneE",
                    "_ZN9GameLogic16GameInitializer16initializeGameEPN6Haruka5SceneE",
                    "initializeGame",
                    nullptr
                };
                
                for (int i = 0; symbols[i] != nullptr; i++) {
                    initFunc = (InitFunc)dlsym(handle, symbols[i]);
                    if (initFunc) {
                        std::cout << "Found symbol: " << symbols[i] << std::endl;
                        break;
                    }
                }
                
                if (initFunc) {
                    initFunc(this);
                    std::cout << "Initializer executed successfully" << std::endl;
                } else {
                    std::cerr << "Initializer function not found" << std::endl;
                }
                
                dlclose(handle);
            } else {
                std::cerr << "Could not load initializer library: " << dlerror() << std::endl;
            }
        }

        in.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading scene: " << e.what() << std::endl;
        return false;
    }
}

} // namespace Haruka