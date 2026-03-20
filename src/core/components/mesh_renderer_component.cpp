#include "mesh_renderer_component.h"
#include <imgui.h>

namespace Haruka {

void MeshRendererComponent::renderInspector() {
    ImGui::InputText("Mesh", &meshPath[0], 128);
    ImGui::InputText("Material", &materialPath[0], 128);
}

}