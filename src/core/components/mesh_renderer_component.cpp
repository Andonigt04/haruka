#include "mesh_renderer_component.h"
#include "renderer/shader.h"
#include "renderer/simple_mesh.h"
#include "imgui.h"

MeshRendererComponent::MeshRendererComponent() 
    : mesh(nullptr), meshPath(""), materialPath("") {}

MeshRendererComponent::~MeshRendererComponent() = default;

void MeshRendererComponent::setMesh(const std::vector<glm::vec3>& vertices,
                                    const std::vector<glm::vec3>& normals,
                                    const std::vector<unsigned int>& indices) {
    mesh = std::make_shared<SimpleMesh>(vertices, normals, indices);
}

void MeshRendererComponent::render(Shader& shader) const {
    if (mesh) {
        mesh->draw();
    }
}

void MeshRendererComponent::renderInspector() {
    ImGui::Text("Mesh Renderer");
    char meshBuffer[128] = {};
    strcpy(meshBuffer, meshPath.c_str());
    if (ImGui::InputText("Mesh##path", meshBuffer, 128)) {
        meshPath = meshBuffer;
    }
    
    char matBuffer[128] = {};
    strcpy(matBuffer, materialPath.c_str());
    if (ImGui::InputText("Material##path", matBuffer, 128)) {
        materialPath = matBuffer;
    }
}