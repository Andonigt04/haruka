#ifndef SIMPLE_MESH_H
#define SIMPLE_MESH_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

class SimpleMesh {
public:
    SimpleMesh(const std::vector<glm::vec3>& vertices,
               const std::vector<glm::vec3>& normals,
               const std::vector<unsigned int>& indices);
    ~SimpleMesh();

    void draw() const;
    size_t getIndexCount() const { return indexCount; }

private:
    GLuint vao, vbo, nbo, ebo;
    size_t indexCount;
};

#endif