#pragma once

#include "../component.h"
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace Haruka {

class TransformComponent : public Component {
public:
    glm::dvec3 position{0};
    glm::dvec3 rotation{0};
    glm::dvec3 scale{1,1,1};

    nlohmann::json toJson() const override {
        nlohmann::json j;
        j["type"] = getType();
        j["position"] = { position.x, position.y, position.z };
        j["rotation"] = { rotation.x, rotation.y, rotation.z };
        j["scale"] = { scale.x, scale.y, scale.z };
        return j;
    }

    static std::shared_ptr<TransformComponent> fromJson(const nlohmann::json& j) {
        auto comp = std::make_shared<TransformComponent>();
        auto pos = j["position"];
        comp->position = glm::dvec3(pos[0], pos[1], pos[2]);
        auto rot = j["rotation"];
        comp->rotation = glm::dvec3(rot[0], rot[1], rot[2]);
        auto scl = j["scale"];
        comp->scale = glm::dvec3(scl[0], scl[1], scl[2]);
        return comp;
    }

    static std::string staticType() { return "Transform"; }
    std::string getType() const override { return staticType(); }
    void renderInspector() override;
};

}