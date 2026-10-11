#pragma once

#include "commands/command.h"
#include "Haruka/Core/Scene/SceneManager.h"
#include <string>

/** @brief Command that adds one object to a scene. */
class AddObjectCommand : public ICommand {
public:
    AddObjectCommand(Haruka::Core::Scene::SceneManager* scene, const Haruka::Core::Scene::SceneObject& obj)
        : scene(scene), object(obj) {}
    
    void execute() override {
        scene->addLoadedObject(std::make_shared<Haruka::Core::Scene::SceneObject>(object));
    }
    
    void undo() override {
        scene->removeObject(object.name);
    }

private:
    Haruka::Core::Scene::SceneManager* scene;
    Haruka::Core::Scene::SceneObject object;
};

/** @brief Command that deletes one object from a scene. */
class DeleteObjectCommand : public ICommand {
public:
    DeleteObjectCommand(Haruka::Core::Scene::SceneManager* scene, const std::string& name)
        : scene(scene), objectName(name) {
        auto obj = scene->getObject(name);
        if (obj) savedObject = *obj;
    }
    
    void execute() override {
        scene->removeObject(objectName);
    }
    
    void undo() override {
        scene->addLoadedObject(std::make_shared<Haruka::Core::Scene::SceneObject>(savedObject));
    }

private:
    Haruka::Core::Scene::SceneManager* scene;
    std::string objectName;
    Haruka::Core::Scene::SceneObject savedObject;
};

/** @brief Command that modifies object transform. */
class TransformObjectCommand : public ICommand {
public:
    TransformObjectCommand(Haruka::Core::Scene::SceneManager* scene, const std::string& name,
                          const glm::dvec3& newPos, const Haruka::Tools::Rotation& newRot, const glm::dvec3& newScale)
        : scene(scene), objectName(name), newPosition(newPos), newRotation(newRot), newScale(newScale) {
        auto obj = scene->getObject(name);
        if (obj) {
            oldPosition = obj->position;
            oldRotation = obj->rotation;
            oldScale = obj->scale;
        }
    }
    
    void execute() override {
        auto obj = scene->getObject(objectName);
        if (obj) {
            obj->position = newPosition;
            obj->rotation = newRotation;
            obj->scale = newScale;
        }
    }
    
    void undo() override {
        auto obj = scene->getObject(objectName);
        if (obj) {
            obj->position = oldPosition;
            obj->rotation = oldRotation;
            obj->scale = oldScale;
        }
    }

private:
    Haruka::Core::Scene::SceneManager* scene;
    std::string objectName;
    glm::dvec3 oldPosition, oldScale;
    Haruka::Tools::Rotation oldRotation;
    glm::dvec3 newPosition, newScale;
    Haruka::Tools::Rotation newRotation;
};
