#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>

namespace Haruka {

class SceneObject;

class Component {
public:
    virtual ~Component() = default;
    virtual nlohmann::json toJson() const { return {}; }
    virtual std::string getType() const = 0;
    virtual void renderInspector() {}
    SceneObject* owner = nullptr;
};

using ComponentPtr = std::shared_ptr<Component>;

}