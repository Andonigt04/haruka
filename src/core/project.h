#pragma once

#include <string>
#include <vector>

namespace Haruka
{
    struct ProjectConfig
    {
        std::string name;
        std::string version;
        std::string engineVersion;
        std::string startScene;
        std::vector<std::string> scenes;
        std::string assetsPath;
        std::string outputPath;
    };

    class Project
    {
    public:
        Project();
        ~Project();

        bool create(const std::string& path, const std::string& name);
        bool load(const std::string& path);
        bool save();

        const ProjectConfig& getConfig() const { return config; }
        const std::string& getPath() const { return projectPath; }

        bool addScene(const std::string& sceneName);
        bool removeScene(const std::string& sceneName);

    private:
        ProjectConfig config;
        std::string projectPath;

        bool loadFromJSON(const std::string& filepath);
        bool saveToJSON(const std::string& filepath);
    };

}