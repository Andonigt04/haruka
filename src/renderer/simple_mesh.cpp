#include "simple_mesh.h"

SimpleMesh::SimpleMesh(const std::vector<glm::vec3>& vertices,
                       const std::vector<glm::vec3>& normals,
                       const std::vector<unsigned int>& indices) 
    : indexCount(indices.size()) {
    
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Positions
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    // Normals
    glGenBuffers(1, &nbo);
    glBindBuffer(GL_ARRAY_BUFFER, nbo);
    glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), normals.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(1);

    // Indices
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glBindVertexArray(0);
}

SimpleMesh::~SimpleMesh() {
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &nbo);
    glDeleteBuffers(1, &ebo);
    glDeleteVertexArrays(1, &vao);
}

void SimpleMesh::draw() const {
    glBindVertexArray(vao);

    // Atributos requeridos por simple.vert (defaults)
    glDisableVertexAttribArray(2);
    glVertexAttrib2f(2, 0.0f, 0.0f);
    glDisableVertexAttribArray(3);
    glVertexAttrib3f(3, 1.0f, 0.0f, 0.0f);
    glDisableVertexAttribArray(4);
    glVertexAttrib3f(4, 0.0f, 1.0f, 0.0f);

    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}