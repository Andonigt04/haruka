#pragma once

#include "command.h"
#include "core/scene/scene_manager.h"
#include <string>
#include <memory>
#include <glm/glm.hpp>

// Adds one object to the scene; undo removes it by name.
class AddObjectCommand : public ICommand {
public:
    AddObjectCommand(Haruka::SceneManager* scene, Haruka::SceneObject obj)
        : scene_(scene), obj_(std::move(obj)) {}

    void execute() override { scene_->addLoadedObject(std::make_shared<Haruka::SceneObject>(obj_)); }
    void undo()    override { scene_->removeObject(obj_.name); }

private:
    Haruka::SceneManager* scene_;
    Haruka::SceneObject obj_;
};

// Removes an object by name; undo re-adds it.
class DeleteObjectCommand : public ICommand {
public:
    DeleteObjectCommand(Haruka::SceneManager* scene, std::string name)
        : scene_(scene), name_(std::move(name)) {}

    void execute() override {
        auto o = scene_->getObjectByName(name_);
        if (o) backup_ = o;
        scene_->removeObject(name_);
    }
    void undo() override {
        if (backup_) scene_->addLoadedObject(backup_);
    }

private:
    Haruka::SceneManager* scene_;
    std::string name_;
    std::shared_ptr<Haruka::SceneObject> backup_;
};

// Records a transform change; undo restores the previous transform.
class TransformObjectCommand : public ICommand {
public:
    TransformObjectCommand(Haruka::SceneManager* scene, std::string name,
                           glm::dvec3 newPos, Haruka::Rotation newRot, glm::dvec3 newScale)
        : scene_(scene), name_(std::move(name)),
          newPos_(newPos), newRot_(newRot), newScale_(newScale) {}

    void execute() override {
        auto o = scene_->getObjectByName(name_);
        if (!o) return;
        oldPos_   = o->position; oldRot_ = o->rotation; oldScale_ = o->scale;
        o->position = newPos_;   o->rotation = newRot_;   o->scale  = newScale_;
    }
    void undo() override {
        auto o = scene_->getObjectByName(name_);
        if (!o) return;
        o->position = oldPos_; o->rotation = oldRot_; o->scale = oldScale_;
    }

private:
    Haruka::SceneManager* scene_;
    std::string name_;
    Haruka::WorldPos newPos_;
    Haruka::Rotation newRot_;
    glm::dvec3 newScale_;
    Haruka::WorldPos oldPos_;
    Haruka::Rotation oldRot_;
    glm::dvec3 oldScale_;
};
