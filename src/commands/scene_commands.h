#pragma once

#include "command.h"
#include "core/scene.h"
#include <string>
#include <glm/glm.hpp>

// Adds one object to the scene; undo removes it by name.
class AddObjectCommand : public ICommand {
public:
    AddObjectCommand(Haruka::Scene* scene, Haruka::SceneObject obj)
        : scene_(scene), obj_(std::move(obj)) {}

    void execute() override { scene_->addObject(obj_); }
    void undo()    override { scene_->removeObject(obj_.name); }

private:
    Haruka::Scene* scene_;
    Haruka::SceneObject obj_;
};

// Removes an object by name; undo re-adds it.
class DeleteObjectCommand : public ICommand {
public:
    DeleteObjectCommand(Haruka::Scene* scene, std::string name)
        : scene_(scene), name_(std::move(name)) {}

    void execute() override {
        auto* o = scene_->getObject(name_);
        if (o) backup_ = *o;
        scene_->removeObject(name_);
    }
    void undo() override {
        if (!backup_.name.empty()) scene_->addObject(backup_);
    }

private:
    Haruka::Scene* scene_;
    std::string name_;
    Haruka::SceneObject backup_;
};

// Records a transform change; undo restores the previous transform.
class TransformObjectCommand : public ICommand {
public:
    TransformObjectCommand(Haruka::Scene* scene, std::string name,
                           glm::dvec3 newPos, glm::dvec3 newRot, glm::dvec3 newScale)
        : scene_(scene), name_(std::move(name)),
          newPos_(newPos), newRot_(newRot), newScale_(newScale) {}

    void execute() override {
        auto* o = scene_->getObject(name_);
        if (!o) return;
        oldPos_   = o->position; oldRot_   = o->rotation; oldScale_ = o->scale;
        o->position = newPos_;   o->rotation = newRot_;   o->scale  = newScale_;
    }
    void undo() override {
        auto* o = scene_->getObject(name_);
        if (!o) return;
        o->position = oldPos_; o->rotation = oldRot_; o->scale = oldScale_;
    }

private:
    Haruka::Scene* scene_;
    std::string name_;
    glm::dvec3 newPos_, newRot_, newScale_;
    glm::dvec3 oldPos_{}, oldRot_{}, oldScale_{};
};
