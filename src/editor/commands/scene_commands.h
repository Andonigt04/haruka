#pragma once

#include "command.h"
#include "core/scene.h"
#include <string>

/** @brief Command that adds one object to a scene. */
class AddObjectCommand : public ICommand {
public:
    /** @brief Stores the target scene and object snapshot. */
    AddObjectCommand(Haruka::Scene* scene, const Haruka::SceneObject& obj)
        : scene(scene), object(obj) {}
    
    /** @brief Adds the object to the scene. */
    void execute() override {
        scene->addObject(object);
    }
    
    /** @brief Removes the object from the scene. */
    void undo() override {
        scene->removeObject(object.name);
    }

private:
    Haruka::Scene* scene;
    Haruka::SceneObject object;
};

/** @brief Command that deletes one object from a scene. */
class DeleteObjectCommand : public ICommand {
public:
    /** @brief Stores the scene and object name, capturing a backup copy if found. */
    DeleteObjectCommand(Haruka::Scene* scene, const std::string& name)
        : scene(scene), objectName(name) {
        // Capture object snapshot before deletion
        auto* obj = scene->getObject(name);
        if (obj) savedObject = *obj;
    }
    
    /** @brief Deletes the object by name. */
    void execute() override {
        scene->removeObject(objectName);
    }
    
    /** @brief Restores the previously saved object snapshot. */
    void undo() override {
        scene->addObject(savedObject);
    }

private:
    Haruka::Scene* scene;
    std::string objectName;
    Haruka::SceneObject savedObject;
};

/** @brief Command that modifies object transform. */
class TransformObjectCommand : public ICommand {
public:
    /** @brief Captures old/new transform state for undo/redo. */
    TransformObjectCommand(Haruka::Scene* scene, const std::string& name,
                          const glm::vec3& newPos, const glm::vec3& newRot, const glm::vec3& newScale)
        : scene(scene), objectName(name), newPosition(newPos), newRotation(newRot), newScale(newScale) {
        auto* obj = scene->getObject(name);
        if (obj) {
            oldPosition = obj->position;
            oldRotation = obj->rotation;
            oldScale = obj->scale;
        }
    }
    
    /** @brief Applies the new transform values. */
    void execute() override {
        auto* obj = scene->getObject(objectName);
        if (obj) {
            obj->position = newPosition;
            obj->rotation = newRotation;
            obj->scale = newScale;
        }
    }
    
    /** @brief Restores the original transform values. */
    void undo() override {
        auto* obj = scene->getObject(objectName);
        if (obj) {
            obj->position = oldPosition;
            obj->rotation = oldRotation;
            obj->scale = oldScale;
        }
    }

private:
    Haruka::Scene* scene;
    std::string objectName;
    glm::vec3 oldPosition, oldRotation, oldScale;
    glm::vec3 newPosition, newRotation, newScale;
};