#pragma once

#include "../component.h"
#include <string>

namespace Haruka {

class MeshRendererComponent : public Component {
public:
    std::string meshPath;
    std::string materialPath;

    static std::string staticType() { return "MeshRenderer"; }
    std::string getType() const override { return staticType(); }
    void renderInspector() override;
};

}