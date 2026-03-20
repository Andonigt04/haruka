#pragma once

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "core/component.h"
#include <nlohmann/json.hpp>
#include "core/components/transform_component.h"
#include "core/components/mesh_component.h"
#include "core/components/model_component.h"
#include "core/components/material_component.h"

namespace Haruka
{
    class Scene;
    struct SceneObject {
        std::string name;
        std::string type;
        std::string modelPath;
        
        glm::dvec3 position = glm::dvec3(0.0);
        glm::dvec3 rotation = glm::dvec3(0.0);
        glm::dvec3 scale = glm::dvec3(1.0);
        glm::dvec3 color = glm::dvec3(1.0);
        double intensity = 1.0;
        
        int parentIndex = -1;
        std::vector<int> childrenIndices;
        
        std::shared_ptr<MaterialComponent> material;
        
        glm::mat4 getWorldTransform(const Scene* scene) const;
        glm::dvec3 getWorldPosition(const Scene* scene) const;
        glm::dvec3 getWorldRotation(const Scene* scene) const;
        glm::dvec3 getWorldScale(const Scene* scene) const;
    };
    
    class Scene {
    public:
        Scene();
        Scene(const std::string& name);
        ~Scene();
        
        void addObject(const SceneObject& obj);
        void removeObject(const std::string& name);
        SceneObject* getObject(const std::string& name);
        
        std::vector<SceneObject>& getObjects() { return objects; }
        const std::vector<SceneObject>& getObjects() const { return objects; }
        std::vector<SceneObject>& getObjectsMutable() { return objects; }
        
        std::string getName() const { return sceneName; }
        void setName(const std::string& name) { sceneName = name; }
        
        bool save(const std::string& filepath);
        bool load(const std::string& filepath);
        bool loadFromJSON(const std::string& filepath);
        
        std::string sceneName;

        std::string getInitializerPath() const { return initializerPath; }
        
    private:
        std::vector<SceneObject> objects;
        std::string initializerPath;

    };
}