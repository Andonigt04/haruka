#include "scene.h"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <sstream>

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
        namespace fs = std::filesystem;
        
        // Crear carpeta padre si no existe
        fs::path path(filepath);
        fs::create_directories(path.parent_path());
        
        // Guardar JSON
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Cannot open file: " << filepath << std::endl;
            return false;
        }
        
        file << "{\n";
        file << "  \"name\": \"" << sceneName << "\",\n";
        file << "  \"objects\": [\n";
        
        for (size_t i = 0; i < objects.size(); i++) {
            const auto& obj = objects[i];
            file << "    {\n";
            file << "      \"name\": \"" << obj.name << "\",\n";
            file << "      \"type\": \"" << obj.type << "\",\n";
            file << "      \"position\": [" << obj.position.x << ", " << obj.position.y << ", " << obj.position.z << "],\n";
            file << "      \"rotation\": [" << obj.rotation.x << ", " << obj.rotation.y << ", " << obj.rotation.z << "],\n";
            file << "      \"scale\": [" << obj.scale.x << ", " << obj.scale.y << ", " << obj.scale.z << "],\n";
            file << "      \"modelPath\": \"" << obj.modelPath << "\",\n";
            file << "      \"color\": [" << obj.color.x << ", " << obj.color.y << ", " << obj.color.z << "],\n";
            file << "      \"intensity\": " << obj.intensity << "\n";
            file << "    }";
            if (i < objects.size() - 1) file << ",";
            file << "\n";
        }
        
        file << "  ]\n";
        file << "}\n";
        
        file.close();
        std::cout << "Scene saved: " << filepath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving scene: " << e.what() << std::endl;
        return false;
    }
}

static std::string extractString(const std::string& line) {
    size_t firstQuote = line.find("\"", line.find(":") + 1);
    size_t lastQuote = line.find("\"", firstQuote + 1);
    if (firstQuote == std::string::npos || lastQuote == std::string::npos) return "";
    return line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
}

static float extractFloat(const std::string& line) {
    size_t colon = line.find(":");
    if (colon == std::string::npos) return 0.0f;
    return std::stof(line.substr(colon + 1));
}

static glm::vec3 extractVec3(const std::string& line) {
    size_t lbr = line.find("[");
    size_t rbr = line.find("]");
    if (lbr == std::string::npos || rbr == std::string::npos) return glm::vec3(0.0f);
    std::string content = line.substr(lbr + 1, rbr - lbr - 1);
    std::stringstream ss(content);
    float x = 0, y = 0, z = 0;
    char comma;
    ss >> x >> comma >> y >> comma >> z;
    return glm::vec3(x, y, z);
}

bool Scene::load(const std::string& filepath) {
    return loadFromJSON(filepath);
}

bool Scene::loadFromJSON(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Cannot open scene file: " << filepath << std::endl;
        return false;
    }

    objects.clear();

    std::string line;
    bool inObjects = false;
    bool inObject = false;
    SceneObject current;

    while (std::getline(file, line)) {
        if (line.find("\"objects\"") != std::string::npos) {
            inObjects = true;
        }

        if (!inObjects && line.find("\"name\"") != std::string::npos) {
            sceneName = extractString(line);
        }

        if (inObjects && line.find("{") != std::string::npos) {
            inObject = true;
            current = SceneObject();
        }

        if (inObject) {
            if (line.find("\"name\"") != std::string::npos) current.name = extractString(line);
            else if (line.find("\"type\"") != std::string::npos) current.type = extractString(line);
            else if (line.find("\"position\"") != std::string::npos) current.position = extractVec3(line);
            else if (line.find("\"rotation\"") != std::string::npos) current.rotation = extractVec3(line);
            else if (line.find("\"scale\"") != std::string::npos) current.scale = extractVec3(line);
            else if (line.find("\"modelPath\"") != std::string::npos) current.modelPath = extractString(line);
            else if (line.find("\"color\"") != std::string::npos) current.color = extractVec3(line);
            else if (line.find("\"intensity\"") != std::string::npos) current.intensity = extractFloat(line);

            if (line.find("}") != std::string::npos) {
                objects.push_back(current);
                inObject = false;
            }
        }
    }

    file.close();
    std::cout << "Scene loaded: " << filepath << " (" << objects.size() << " objects)" << std::endl;
    return true;
}

}