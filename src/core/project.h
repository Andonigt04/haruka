#pragma once

#include <string>
#include <vector>

namespace Haruka
{
    struct ProjectConfig
    {
        // Configuración base del proyecto
        std::string name;
        std::string version;
        std::string engineVersion;
        std::string startScene;
        std::string engineBinary;
        std::string editorBinary;
        std::vector<std::string> scenes;
        std::string assetsPath;
        std::string outputPath;
        std::string shadersPath = "";
        
        // Configuración de export (persistente por proyecto)
        struct ExportSettings {
            std::string author = "Developer";
            std::string description = "";
            
            // Build defaults
            std::string defaultBuildType = "Release";  // Debug, Release, Shipping
            std::string defaultPlatform = "Linux";      // Linux, Windows, macOS
            
            // Compilation options
            bool includeDebugSymbols = false;
            bool optimizeAssets = true;
            bool stripUnusedContent = true;
            bool compressAssets = true;
            
            // Custom paths
            std::string customBuildOutputPath = "";
            std::string customLaunchScript = "";
        } exportSettings;
    };

    class Project
    {
    public:
        Project();
        ~Project();

        bool create(const std::string& path, const std::string& name);
        bool load(const std::string& path);
        bool save();

        Haruka::ProjectConfig& getConfig() { return config; }
        const std::string& getPath() const { return projectPath; }

        bool addScene(const std::string& sceneName);
        bool removeScene(const std::string& sceneName);

    private:
        Haruka::ProjectConfig config;
        std::string projectPath;

        bool loadFromJSON(const std::string& filepath);
        bool saveToJSON(const std::string& filepath);
    };

}