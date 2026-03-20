#include "script_component.h"
#include <imgui.h>

namespace Haruka {

void ScriptComponent::renderInspector() {
    ImGui::InputText("Script", &scriptPath[0], 128);
}

}