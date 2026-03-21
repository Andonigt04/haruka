#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>

class SimpleMesh;
class Shader;

class MeshRendererComponent {
public:
    MeshRendererComponent();
    ~MeshRendererComponent();

    void setMesh(const std::vector<glm::vec3>& vertices, 
                 const std::vector<glm::vec3>& normals,
                 const std::vector<unsigned int>& indices);
    
    void render(Shader& shader) const;
    void renderInspector();
    
    std::shared_ptr<SimpleMesh> getMesh() const { return mesh; }
    
private:
    std::shared_ptr<SimpleMesh> mesh;
    std::string meshPath;
    std::string materialPath;
};