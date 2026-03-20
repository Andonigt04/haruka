#pragma once
#include "../component.h"
#include <string>

namespace Haruka {

class ScriptComponent : public Component {
public:
    std::string scriptPath;

    static std::string staticType() { return "Script"; }
    std::string getType() const override { return staticType(); }
    void renderInspector() override;
};

}