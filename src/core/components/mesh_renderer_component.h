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
    void releaseMesh();
    bool isResident() const { return mesh != nullptr; }
    
    void render(Shader& shader) const;
    void renderInspector();
    
    std::shared_ptr<SimpleMesh> getMesh() const { return mesh; }
    int getVertexCount() const { return cachedVertexCount; }
    int getTriangleCount() const { return cachedTriangleCount; }
    int getResidentVertexCount() const { return mesh ? mesh->getVertexCount() : 0; }
    int getResidentTriangleCount() const { return mesh ? mesh->getTriangleCount() : 0; }
    const std::vector<glm::vec3>& getSourceVertices() const { return sourceVertices; }
    const std::vector<glm::vec3>& getSourceNormals() const { return sourceNormals; }
    const std::vector<unsigned int>& getSourceIndices() const { return sourceIndices; }
    
private:
    std::shared_ptr<SimpleMesh> mesh;
    std::string meshPath;
    std::string materialPath;
    int cachedVertexCount = 0;
    int cachedTriangleCount = 0;
    std::vector<glm::vec3> sourceVertices;
    std::vector<glm::vec3> sourceNormals;
    std::vector<unsigned int> sourceIndices;
};