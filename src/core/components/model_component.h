#pragma once
#include "core/component.h"
#include <string>
#include <nlohmann/json.hpp>

namespace Haruka {

class ModelComponent : public Component {
public:
    std::string modelPath;

    nlohmann::json toJson() const override {
        nlohmann::json j;
        j["type"] = getType();
        j["modelPath"] = modelPath;
        return j;
    }

    static std::shared_ptr<Component> fromJson(const nlohmann::json& j) {
        auto comp = std::make_shared<ModelComponent>();
        comp->modelPath = j.value("modelPath", "");
        return comp;
    }

    static std::string staticType() { return "Model"; }
    std::string getType() const override { return staticType(); }
    void renderInspector() override {}
};

}