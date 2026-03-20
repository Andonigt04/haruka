#pragma once

#include "core/component.h"
#include <string>
#include <nlohmann/json.hpp>

namespace Haruka {

class MeshComponent : public Component {
public:
    std::string meshPath;

    nlohmann::json toJson() const override {
        nlohmann::json j;
        j["type"] = getType();
        j["meshPath"] = meshPath;
        return j;
    }

    static std::shared_ptr<MeshComponent> fromJson(const nlohmann::json& j) {
        auto comp = std::make_shared<MeshComponent>();
        comp->meshPath = j.value("meshPath", "");
        return comp;
    }

    static std::string staticType() { return "Mesh"; }
    std::string getType() const override { return staticType(); }
};

}