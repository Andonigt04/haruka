#include "project.h"

#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

namespace Haruka
{
    Project::Project() {}
    Project::~Project() {}

    bool Project::create(const std::string& path, const std::string& name) {
        projectPath = path;
        config.name = name;
        config.version = "0.1";
        config.engineVersion = "0.1";
        config.startScene = "main";
        config.assetsPath = "assets/";
        config.outputPath = "build/";
        
        try {
            // Crear estructura de carpetas
            namespace fs = std::filesystem;
            fs::create_directories(path);
            fs::create_directories(path + "/scenes");
            fs::create_directories(path + "/assets/models");
            fs::create_directories(path + "/assets/textures");
            fs::create_directories(path + "/assets/scripts");
            fs::create_directories(path + "/build");
            
            config.scenes.push_back("main.scene");
            
            bool ok = saveToJSON(path + "/project.hrk");
            std::cout << "Project created at: " << path << std::endl;
            return ok;
        } catch (const std::exception& e) {
            std::cerr << "Error creating project: " << e.what() << std::endl;
            return false;
        }
    }

    bool Project::load(const std::string& path) {
        projectPath = path;
        return loadFromJSON(path + "/project.hrk");
    }

    bool Project::save() {
        return saveToJSON(projectPath + "/project.hrk");
    }

    bool Project::addScene(const std::string& sceneName) {
        std::string scenePath = sceneName;
        if (scenePath.find(".scene") == std::string::npos) {
            scenePath += ".scene";
        }
        
        config.scenes.push_back(scenePath);
        return save();
    }

    bool Project::removeScene(const std::string& sceneName) {
        auto it = std::find(config.scenes.begin(), config.scenes.end(), sceneName);
        if (it != config.scenes.end()) {
            config.scenes.erase(it);
            return save();
        }
        return false;
    }

    bool Project::loadFromJSON(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Cannot open project file: " << filepath << std::endl;
            return false;
        }
        
        // JSON parsing simple (manual)
        std::string line;
        while (std::getline(file, line)) {
            // Parse name
            if (line.find("\"name\"") != std::string::npos) {
                size_t start = line.find(":") + 1;
                size_t firstQuote = line.find("\"", start) + 1;
                size_t lastQuote = line.find("\"", firstQuote);
                config.name = line.substr(firstQuote, lastQuote - firstQuote);
            }
            // Parse version
            else if (line.find("\"version\"") != std::string::npos) {
                size_t start = line.find(":") + 1;
                size_t firstQuote = line.find("\"", start) + 1;
                size_t lastQuote = line.find("\"", firstQuote);
                config.version = line.substr(firstQuote, lastQuote - firstQuote);
            }
            // Parse startScene
            else if (line.find("\"startScene\"") != std::string::npos) {
                size_t start = line.find(":") + 1;
                size_t firstQuote = line.find("\"", start) + 1;
                size_t lastQuote = line.find("\"", firstQuote);
                config.startScene = line.substr(firstQuote, lastQuote - firstQuote);
            }
        }
        
        file.close();
        return true;
    }

    bool Project::saveToJSON(const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Cannot create project file: " << filepath << std::endl;
            return false;
        }
        
        // Escribir JSON manual
        file << "{\n";
        file << "  \"name\": \"" << config.name << "\",\n";
        file << "  \"version\": \"" << config.version << "\",\n";
        file << "  \"engineVersion\": \"" << config.engineVersion << "\",\n";
        file << "  \"startScene\": \"" << config.startScene << "\",\n";
        file << "  \"scenes\": [\n";
        
        for (size_t i = 0; i < config.scenes.size(); i++) {
            file << "    \"" << config.scenes[i] << "\"";
            if (i < config.scenes.size() - 1) file << ",";
            file << "\n";
        }
        
        file << "  ],\n";
        file << "  \"assetsPath\": \"" << config.assetsPath << "\",\n";
        file << "  \"outputPath\": \"" << config.outputPath << "\"\n";
        file << "}\n";
        
        file.close();
        return true;
    }
}