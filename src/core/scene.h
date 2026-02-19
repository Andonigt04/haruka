#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace Haruka
{
    struct SceneObject {
        std::string name;
        std::string type; // Type of Object
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 rotation = glm::vec3(0.0f);
        glm::vec3 scale = glm::vec3(1.0f);
        std::string modelPath;
        
        // Light properties
        glm::vec3 color = glm::vec3(1.0f);
        float intensity = 1.0f;
    };

    class Scene
    {
    public:
        Scene();
        Scene(const std::string& name);
        ~Scene();
        
        bool load(const std::string& filepath);
        bool save(const std::string& filepath);
        
        void addObject(const SceneObject& obj);
        void removeObject(const std::string& name);
        SceneObject* getObject(const std::string& name);
        
        const std::vector<SceneObject>& getObjects() const { return objects; }
        const std::string& getName() const { return sceneName; }
        
    private:
        std::string sceneName;
        std::vector<SceneObject> objects;
        
        bool loadFromJSON(const std::string& filepath);
        bool saveToJSON(const std::string& filepath);
    };
}