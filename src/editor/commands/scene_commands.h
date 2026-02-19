#pragma once

#include "command.h"
#include "core/scene.h"
#include <string>

// Añadir objeto
class AddObjectCommand : public ICommand {
public:
    AddObjectCommand(Haruka::Scene* scene, const Haruka::SceneObject& obj)
        : scene(scene), object(obj) {}
    
    void execute() override {
        scene->addObject(object);
    }
    
    void undo() override {
        scene->removeObject(object.name);
    }

private:
    Haruka::Scene* scene;
    Haruka::SceneObject object;
};

// Eliminar objeto
class DeleteObjectCommand : public ICommand {
public:
    DeleteObjectCommand(Haruka::Scene* scene, const std::string& name)
        : scene(scene), objectName(name) {
        // Guardar objeto antes de eliminar
        auto* obj = scene->getObject(name);
        if (obj) savedObject = *obj;
    }
    
    void execute() override {
        scene->removeObject(objectName);
    }
    
    void undo() override {
        scene->addObject(savedObject);
    }

private:
    Haruka::Scene* scene;
    std::string objectName;
    Haruka::SceneObject savedObject;
};

// Modificar transformación
class TransformObjectCommand : public ICommand {
public:
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
    
    void execute() override {
        auto* obj = scene->getObject(objectName);
        if (obj) {
            obj->position = newPosition;
            obj->rotation = newRotation;
            obj->scale = newScale;
        }
    }
    
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