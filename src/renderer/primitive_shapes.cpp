#include "primitive_shapes.h"

#include <glm/gtc/constants.hpp>
#include <cmath>

void PrimitiveShapes::createSphere(float radius, int sectors, int stacks, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices) {
    vertices.clear();
    normals.clear();
    indices.clear();

    float x, y, z, xy;
    float sectorStep = 2 * glm::pi<float>() / sectors;
    float stackStep = glm::pi<float>() / stacks;
    float sectorAngle, stackAngle;

    for (int i = 0; i <= stacks; ++i) {
        stackAngle = glm::pi<float>() / 2 - i * stackStep;
        xy = radius * cos(stackAngle);
        z = radius * sin(stackAngle);

        for (int j = 0; j <= sectors; ++j) {
            sectorAngle = j * sectorStep;
            x = xy * cos(sectorAngle);
            y = xy * sin(sectorAngle);

            vertices.push_back(glm::vec3(x, y, z));
            glm::vec3 normal = glm::normalize(glm::vec3(x, y, z));
            normals.push_back(normal);
        }
    }

    unsigned int k1, k2;
    for (int i = 0; i < stacks; ++i) {
        k1 = i * (sectors + 1);
        k2 = k1 + sectors + 1;

        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
}

void PrimitiveShapes::createSphereLOD(float radius, int sectors, int stacks, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices)
{
    createSphere(radius, sectors, stacks, vertices, normals, indices);
}

void PrimitiveShapes::createCube(float size, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices)
{
    vertices.clear();
    normals.clear();
    indices.clear();

    float s = size / 2.0f;

    // Vertices (8 esquinas)
    vertices = {
        {-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s},  // Back
        {-s, -s, s},  {s, -s, s},  {s, s, s},  {-s, s, s}    // Front
    };

    // Normals (por cara)
    glm::vec3 faceNormals[6] = {
        {0, 0, -1}, {0, 0, 1},   // Back, Front
        {-1, 0, 0}, {1, 0, 0},   // Left, Right
        {0, -1, 0}, {0, 1, 0}    // Bottom, Top
    };

    // Índices (6 caras, 2 triángulos cada una)
    unsigned int cubeIndices[36] = {
        0, 2, 1, 0, 3, 2,  // Back
        4, 5, 6, 4, 6, 7,  // Front
        4, 0, 3, 4, 3, 7,  // Left
        1, 2, 6, 1, 6, 5,  // Right
        4, 1, 5, 4, 0, 1,  // Bottom
        3, 6, 2, 3, 7, 6   // Top
    };

    indices.assign(cubeIndices, cubeIndices + 36);

    // Asignar normals por vértice (repetir por cada cara)
    for (int i = 0; i < 36; i += 6) {
        for (int j = 0; j < 6; ++j) {
            normals.push_back(faceNormals[i / 6]);
        }
    }
}

void PrimitiveShapes::createPlane(float width, float height, int subdivisions, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices)
{
    vertices.clear();
    normals.clear();
    indices.clear();

    float w = width / 2.0f;
    float h = height / 2.0f;
    float stepX = width / subdivisions;
    float stepY = height / subdivisions;

    // Generate vertices
    for (int y = 0; y <= subdivisions; ++y) {
        for (int x = 0; x <= subdivisions; ++x) {
            float px = -w + x * stepX;
            float py = 0.0f;
            float pz = -h + y * stepY;
            vertices.push_back({px, py, pz});
            normals.push_back({0, 1, 0});  // Up normal
        }
    }

    // Generate indices
    for (int y = 0; y < subdivisions; ++y) {
        for (int x = 0; x < subdivisions; ++x) {
            unsigned int a = y * (subdivisions + 1) + x;
            unsigned int b = a + 1;
            unsigned int c = a + (subdivisions + 1);
            unsigned int d = c + 1;

            indices.push_back(a);
            indices.push_back(c);
            indices.push_back(b);

            indices.push_back(b);
            indices.push_back(c);
            indices.push_back(d);
        }
    }
}

void PrimitiveShapes::createCubeSphere(float radius, int subdivisions, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals, std::vector<unsigned int>& indices)
{
    vertices.clear();
    normals.clear();
    indices.clear();

    // Crear las 6 caras de un cubo
    std::vector<glm::vec3> faceVertices[6];
    std::vector<unsigned int> faceIndices[6];
    
    // Definir las 6 caras del cubo (cada cara es un grid)
    glm::vec3 faceNormals[6] = {
        glm::vec3(1, 0, 0),   // Derecha
        glm::vec3(-1, 0, 0),  // Izquierda
        glm::vec3(0, 1, 0),   // Arriba
        glm::vec3(0, -1, 0),  // Abajo
        glm::vec3(0, 0, 1),   // Frente
        glm::vec3(0, 0, -1)   // Atrás
    };
    
    int gridSize = 2 << subdivisions; // 2^(subdivisions+1)
    float step = 2.0f / gridSize;
    
    // Para cada cara del cubo
    for (int face = 0; face < 6; face++) {
        std::vector<glm::vec3> faceVerts;
        
        // Generar grid de vértices para esta cara
        for (int i = 0; i <= gridSize; i++) {
            for (int j = 0; j <= gridSize; j++) {
                float u = -1.0f + i * step;
                float v = -1.0f + j * step;
                
                glm::vec3 p;
                if (face == 0) p = glm::vec3(1, v, -u);      // Derecha
                else if (face == 1) p = glm::vec3(-1, v, u); // Izquierda
                else if (face == 2) p = glm::vec3(u, 1, v);  // Arriba
                else if (face == 3) p = glm::vec3(u, -1, -v);// Abajo
                else if (face == 4) p = glm::vec3(u, v, 1);  // Frente
                else p = glm::vec3(-u, v, -1);               // Atrás
                
                // Normalizar para convertir a esfera
                glm::vec3 normalized = glm::normalize(p) * radius;
                faceVerts.push_back(normalized);
                vertices.push_back(normalized);
                normals.push_back(glm::normalize(normalized));
            }
        }
        
        // Generar índices para esta cara (quads -> triangles)
        int stride = gridSize + 1;
        for (int i = 0; i < gridSize; i++) {
            for (int j = 0; j < gridSize; j++) {
                int a = i * stride + j;
                int b = a + 1;
                int c = a + stride;
                int d = c + 1;
                
                unsigned int baseIndex = vertices.size() - faceVerts.size() + a;
                unsigned int baseB = baseIndex + 1;
                unsigned int baseC = baseIndex + stride;
                unsigned int baseD = baseC + 1;
                
                // Primer triángulo
                indices.push_back(baseIndex);
                indices.push_back(baseC);
                indices.push_back(baseB);
                
                // Segundo triángulo
                indices.push_back(baseB);
                indices.push_back(baseC);
                indices.push_back(baseD);
            }
        }
    }
}