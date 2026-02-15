#include "mesh.h"

#include <glad/glad.h>

Mesh::Mesh(std::vector<Vertex> vertex, std::vector<unsigned int> idx, std::vector<MeshTexture> textures) {
    if (vertex.empty()) {
        std::cout << "ERROR::MESH::Vértices vacíos al crear la malla" << std::endl;
        return; 
    }
    this->vertex = vertex;
    this->index = idx;
    this->textures = textures;
    setupMesh();
}

void Mesh::Draw(Shader &shader) 
{
    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    unsigned int normalNr = 1;
    unsigned int heightNr = 1;
    unsigned int roughnessNr = 1;
    unsigned int metallicNr = 1;
    unsigned int aoNr = 1;
    unsigned int emissiveNr = 1;
    unsigned int metallicRoughnessNr = 1;

    for(unsigned int i = 0; i < textures.size(); i++)
    {
        glActiveTexture(GL_TEXTURE0 + i); 
        
        std::string number;
        std::string name = textures[i].type;

        if(name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if(name == "texture_specular")
            number = std::to_string(specularNr++);
        else if(name == "texture_normal")
            number = std::to_string(normalNr++);
        else if(name == "texture_height")
            number = std::to_string(heightNr++);
        else if(name == "texture_roughness")
            number = std::to_string(roughnessNr++);
        else if(name == "texture_metallic")
            number = std::to_string(metallicNr++);
        else if(name == "texture_ao")
            number = std::to_string(aoNr++);
        else if(name == "texture_emissive")
            number = std::to_string(emissiveNr++);
        else if(name == "texture_metallic_roughness")
            number = std::to_string(metallicRoughnessNr++);

        if (!number.empty()) {
            shader.setInt(("material." + name + number).c_str(), i); 
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }
    }
    
    // Clean not used textures 
    for(unsigned int i = textures.size(); i < 16; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, 0);  // Unbind
    }
    
    glActiveTexture(GL_TEXTURE0);

    // draw mesh
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, index.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
} 

void Mesh::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertex.size() * sizeof(Vertex), &vertex[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index.size() * sizeof(unsigned int), &index[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));
    
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Bitangent));
    
    glBindVertexArray(0);
}