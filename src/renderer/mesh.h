#ifndef MESH_H
#define MESH_H

#include <iostream>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "shader.h"

#pragma pack(push, 1)
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
};
#pragma pack(pop)

struct MeshTexture {
    unsigned int id;
    std::string type;
    std::string path;
};

class Mesh {
public:
    std::vector<Vertex>       vertex;
    std::vector<unsigned int> index;
    std::vector<MeshTexture>  textures;
    unsigned int VAO;

    // Constructor para modelos complejos (con texturas)
    Mesh(std::vector<Vertex> vertex, std::vector<unsigned int> idx, std::vector<MeshTexture> textures);
    
    // Constructor simplificado (solo geometria)
    Mesh(const std::vector<glm::vec3>& vertices,
         const std::vector<glm::vec3>& normals,
         const std::vector<unsigned int>& indices);
    
    ~Mesh();

    void Draw(Shader &shader);
    void draw() const;  // Alias para compatibilidad
    size_t getIndexCount() const { return index.size(); }

private:
    unsigned int VBO, EBO;
    GLuint nbo = 0;  // Normal buffer para geometria simple
    bool isSimpleGeometry = false;
    
    void setupMesh();
    void setupSimpleMesh(const std::vector<glm::vec3>& vertices,
                         const std::vector<glm::vec3>& normals,
                         const std::vector<unsigned int>& indices);
};

#endif