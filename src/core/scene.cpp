#include "scene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <dlfcn.h>
#include "renderer/primitive_shapes.h"

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

        // Detectar si es prefab por extensión
        std::string ext = filepath.substr(filepath.find_last_of(".") + 1);
        bool isPrefab = (ext == "prefab");

        nlohmann::json j;
        j["name"] = sceneName;
        
        if (isPrefab) {
            // Guardar como prefab con initializer
            j["initializer"] = initializerPath;
            j["components"] = nlohmann::json::array();
            for (const auto& obj : objects) {
                nlohmann::json comp;
                comp["name"] = obj.name;
                comp["type"] = obj.type;
                comp["modelPath"] = obj.modelPath;
                comp["position"] = {obj.position.x, obj.position.y, obj.position.z};
                comp["rotation"] = {obj.rotation.x, obj.rotation.y, obj.rotation.z};
                comp["scale"]    = {obj.scale.x, obj.scale.y, obj.scale.z};
                comp["color"]    = {obj.color.x, obj.color.y, obj.color.z};
                comp["intensity"] = obj.intensity;
                
                if (obj.material) {
                    comp["material"] = obj.material->toJSON();
                }
                j["components"].push_back(comp);
            }
        } else {
            // Guardar como escena normal
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
        if (j.contains("initializer")) initializerPath = j["initializer"].get<std::string>();
        objects.clear();

        // Detectar si es un prefab por extensión
        std::string ext = filepath.substr(filepath.find_last_of(".") + 1);
        bool isPrefab = (ext == "prefab");

        if (isPrefab) {
            // Cargar componentes del prefab como objetos principales
            if (j.contains("components") && j["components"].is_array()) {
                for (const auto& comp : j["components"]) {
                    SceneObject obj;
                    obj.name = comp.value("name", comp.value("type", ""));
                    obj.type = comp.value("type", "");
                    obj.properties = comp;
                    obj.modelPath = comp.value("modelPath", "");
                    obj.color = glm::dvec3(1.0);
                    obj.intensity = 1.0;

                    if (comp.contains("position") && comp["position"].size() == 3)
                        obj.position = {comp["position"][0], comp["position"][1], comp["position"][2]};
                    if (comp.contains("rotation") && comp["rotation"].size() == 3)
                        obj.rotation = {comp["rotation"][0], comp["rotation"][1], comp["rotation"][2]};
                    if (comp.contains("scale") && comp["scale"].size() == 3)
                        obj.scale = {comp["scale"][0], comp["scale"][1], comp["scale"][2]};
                    if (comp.contains("color") && comp["color"].size() == 3)
                        obj.color = {comp["color"][0], comp["color"][1], comp["color"][2]};
                    if (comp.contains("intensity"))
                        obj.intensity = comp["intensity"].get<double>();

                    // Cargar material si existe
                    if (comp.contains("material")) {
                        obj.material = std::make_shared<MaterialComponent>();
                        obj.material->fromJSON(comp["material"]);
                    }

                    objects.push_back(obj);
                }
            }
        } else {
            // Cargar escena normal
            if (j.contains("objects") && j["objects"].is_array()) {
                for (const auto& o : j["objects"]) {
                    SceneObject obj = parseSceneObject(o);
                    objects.push_back(obj);
                }
            }
        }

        // Ejecutar inicializador solo en escenas
        if (!isPrefab) {
            executeInitializer(filepath);
        }

        in.close();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error loading scene: " << e.what() << std::endl;
        return false;
    }
}

SceneObject Scene::parseSceneObject(const nlohmann::json& o) {
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
    if (o.contains("childrenIndices")) 
        obj.childrenIndices = o["childrenIndices"].get<std::vector<int>>();

    // Cargar material
    if (o.contains("material")) {
        obj.material = std::make_shared<MaterialComponent>();
        obj.material->fromJSON(o["material"]);
    }

    // Cargar meshRenderer
    if (o.contains("meshRenderer")) {
        obj.meshRenderer = std::make_shared<MeshRendererComponent>();
        std::string meshType = o["meshRenderer"].value("meshType", "cube");
        
        std::vector<glm::vec3> verts, norms;
        std::vector<unsigned int> indices;
        
        if (meshType == "sphere") {
            float radius = o["meshRenderer"].value("radius", 1.0f);
            int segments = o["meshRenderer"].value("segments", 32);
            PrimitiveShapes::createSphere(radius, segments, segments, verts, norms, indices);
        } 
        else if (meshType == "cube") {
            float size = o["meshRenderer"].value("size", 1.0f);
            PrimitiveShapes::createCube(size, verts, norms, indices);
        }
        
        if (!verts.empty()) {
            obj.meshRenderer->setMesh(verts, norms, indices);
        }
    }

    // Detectar tipo por extensión si tiene path (Prefab)
    if (o.contains("path")) {
        std::string filePath = o.value("path", "");
        std::string fileExt = filePath.substr(filePath.find_last_of(".") + 1);
        
        if (fileExt == "prefab") {
            obj.type = "Prefab";
            loadPrefabComponents(filePath, obj);
        } else if (fileExt == "scene") {
            obj.type = "Scene";
        }
    }
    // Si tipo es Prefab pero sin path, buscar por nombre
    else if (obj.type == "Prefab") {
        std::string prefabPath = "Assets/Prefabs/" + obj.name + ".prefab";
        loadPrefabComponents(prefabPath, obj);
    }

    return obj;
}

void Scene::loadPrefabComponents(const std::string& prefabPath, SceneObject& obj) {
    std::ifstream prefabFile(prefabPath);
    if (!prefabFile.is_open()) return;

    nlohmann::json prefabData;
    prefabFile >> prefabData;
    prefabFile.close();
    
    if (prefabData.contains("components")) {
        for (const auto& comp : prefabData["components"]) {
            SceneObject child;
            child.name = comp.value("type", "");
            child.type = comp.value("type", "");
            child.properties = comp;
            child.modelPath = "";
            
            // Heredar posición/rotación/escala del prefab padre
            if (comp.contains("position") && comp["position"].size() == 3) {
                child.position = {comp["position"][0], comp["position"][1], comp["position"][2]};
            } else {
                child.position = obj.position;
            }
            
            if (comp.contains("rotation") && comp["rotation"].size() == 3) {
                child.rotation = {comp["rotation"][0], comp["rotation"][1], comp["rotation"][2]};
            } else {
                child.rotation = obj.rotation;
            }
            
            if (comp.contains("scale") && comp["scale"].size() == 3) {
                child.scale = {comp["scale"][0], comp["scale"][1], comp["scale"][2]};
            } else {
                child.scale = obj.scale;
            }
            
            obj.children.push_back(child);
        }
    }
}

void Scene::executeInitializer(const std::string& scenePath) {
    std::ifstream in(scenePath);
    if (!in.is_open()) return;
    nlohmann::json j;
    in >> j;
    in.close();
    if (!j.contains("initializer")) return;
    std::filesystem::path scenePath_fs(scenePath);
    std::filesystem::path projectRoot = scenePath_fs.parent_path().parent_path();
    std::string projectName = "";
    std::filesystem::path projectFile = projectRoot / "project.hrk";
    std::ifstream pj(projectFile);
    if (pj.is_open()) {
        std::string line;
        while (std::getline(pj, line)) {
            if (line.find("\"name\"") != std::string::npos) {
                size_t start = line.find(":") + 1;
                size_t firstQuote = line.find("\"", start) + 1;
                size_t lastQuote = line.find("\"", firstQuote);
                projectName = line.substr(firstQuote, lastQuote - firstQuote);
                break;
            }
        }
        pj.close();
    }
    std::string logicLib = "lib" + projectName + ".so";
    std::filesystem::path libPath = projectRoot / logicLib;
    std::cout << "Loading initializer: " << libPath.string() << std::endl;
    void* handle = dlopen(libPath.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Could not load initializer library: " << dlerror() << std::endl;
        return;
    }

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
            initFunc(this);
            std::cout << "Initializer executed successfully" << std::endl;
            break;
        }
    }

    if (!initFunc) {
        std::cerr << "Initializer function not found" << std::endl;
    }
    
    dlclose(handle);
}

}