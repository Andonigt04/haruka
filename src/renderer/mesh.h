#ifndef MESH_H
#define MESH_H

#include <iostream>

#include <vector>
#include <string>
#include <glm/glm.hpp>
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

    Mesh(std::vector<Vertex> vertex, std::vector<unsigned int> idx, std::vector<MeshTexture> textures);
    void Draw(Shader &shader);

private:
    unsigned int VBO, EBO;
    void setupMesh();
};

#endif