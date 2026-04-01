#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>
#include "renderer/simple_mesh.h"

class Shader;

class MeshRendererComponent {
public:
    MeshRendererComponent();
    ~MeshRendererComponent();

    void setMesh(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& normals, const std::vector<unsigned int>& indices);
    
    void render(Shader& shader) const;
    void renderInspector();
    
    std::shared_ptr<SimpleMesh> getMesh() const { return mesh; }
    int getVertexCount() const { return mesh ? mesh->getVertexCount() : 0; }
    int getTriangleCount() const { return mesh ? mesh->getTriangleCount() : 0; }
    
private:
    std::shared_ptr<SimpleMesh> mesh;
    std::string meshPath;
    std::string materialPath;
};