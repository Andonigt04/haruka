#include "terrain.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <cmath>
#include <algorithm>
#include "stb_image.h"

namespace Haruka {

Terrain::Terrain(int size, float scale) 
    : size(size), scale(scale) {
    heightData.resize(size * size, 0.0f);
}

Terrain::~Terrain() {
    for (auto& patch : patches) {
        if (patch.VAO) glDeleteVertexArrays(1, &patch.VAO);
        if (patch.VBO) glDeleteBuffers(1, &patch.VBO);
        if (patch.EBO) glDeleteBuffers(1, &patch.EBO);
    }
}

void Terrain::loadHeightmap(const std::string& filepath) {
    int width, height, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 1);
    
    if (!data) {
        std::cerr << "[Terrain] Failed to load heightmap: " << filepath << std::endl;
        return;
    }
    
    // Resize if needed
    if (width != size || height != size) {
        std::cerr << "[Terrain] Heightmap size mismatch. Expected " << size << "x" << size 
                  << ", got " << width << "x" << height << std::endl;
    }
    
    int actualSize = std::min(width, size);
    for (int z = 0; z < actualSize; z++) {
        for (int x = 0; x < actualSize; x++) {
            heightData[z * size + x] = data[z * width + x] / 255.0f;
        }
    }
    
    stbi_image_free(data);
    
    std::cout << "[Terrain] Loaded heightmap: " << filepath << std::endl;
    generateMesh();
}

void Terrain::generatePerlin(int seed) {
    // Simple Perlin-like noise
    srand(seed);
    
    for (int z = 0; z < size; z++) {
        for (int x = 0; x < size; x++) {
            float height = 0.0f;
            float amplitude = 1.0f;
            float frequency = 0.005f;
            
            // Multiple octaves
            for (int octave = 0; octave < 6; octave++) {
                float sampleX = x * frequency;
                float sampleZ = z * frequency;
                
                float noise = sin(sampleX) * cos(sampleZ) + 
                             sin(sampleX * 2.0f) * cos(sampleZ * 2.0f) * 0.5f;
                
                height += noise * amplitude;
                
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }
            
            heightData[z * size + x] = (height + 1.0f) * 0.5f; // Normalize to 0-1
        }
    }
    
    std::cout << "[Terrain] Generated procedural terrain" << std::endl;
    generateMesh();
}

void Terrain::generateMesh() {
    patches.clear();
    
    int numPatches = size / patchSize;
    
    for (int pz = 0; pz < numPatches; pz++) {
        for (int px = 0; px < numPatches; px++) {
            createPatch(px * patchSize, pz * patchSize, 0);
        }
    }
    
    std::cout << "[Terrain] Generated " << patches.size() << " patches" << std::endl;
}

void Terrain::createPatch(int startX, int startZ, int lod) {
    int step = 1 << lod; // 1, 2, 4, 8...
    int verticesPerSide = (patchSize / step) + 1;
    
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    // Generate vertices
    for (int z = 0; z < verticesPerSide; z++) {
        for (int x = 0; x < verticesPerSide; x++) {
            int actualX = startX + x * step;
            int actualZ = startZ + z * step;
            
            if (actualX >= size) actualX = size - 1;
            if (actualZ >= size) actualZ = size - 1;
            
            float height = getHeightNormalized(actualX, actualZ) * scale;
            
            // Position
            vertices.push_back(actualX * terrainScale.x);
            vertices.push_back(height * terrainScale.y);
            vertices.push_back(actualZ * terrainScale.z);
            
            // Normal (calculated from neighbors)
            glm::vec3 normal = getNormal(actualX, actualZ);
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);
            
            // TexCoord
            vertices.push_back((float)x / (verticesPerSide - 1));
            vertices.push_back((float)z / (verticesPerSide - 1));
        }
    }
    
    // Generate indices
    for (int z = 0; z < verticesPerSide - 1; z++) {
        for (int x = 0; x < verticesPerSide - 1; x++) {
            int topLeft = z * verticesPerSide + x;
            int topRight = topLeft + 1;
            int bottomLeft = (z + 1) * verticesPerSide + x;
            int bottomRight = bottomLeft + 1;
            
            // Triangle 1
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);
            
            // Triangle 2
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    
    // Create OpenGL buffers
    TerrainPatch patch;
    patch.offset = glm::vec2(startX, startZ);
    patch.lod = lod;
    patch.indexCount = indices.size();
    
    glGenVertexArrays(1, &patch.VAO);
    glGenBuffers(1, &patch.VBO);
    glGenBuffers(1, &patch.EBO);
    
    glBindVertexArray(patch.VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, patch.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, patch.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // TexCoord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    
    glBindVertexArray(0);
    
    patches.push_back(patch);
}

int Terrain::calculateLOD(const glm::vec2& patchCenter, const glm::vec3& cameraPos) {
    float distance = glm::length(glm::vec2(cameraPos.x, cameraPos.z) - patchCenter);
    
    for (int i = 0; i < 4; i++) {
        if (distance < lodDistance[i]) {
            return i;
        }
    }
    
    return 3; // Max LOD
}

void Terrain::render(Shader& shader, const glm::vec3& cameraPos) {
    shader.use();
    
    for (auto& patch : patches) {
        glm::vec2 patchCenter = patch.offset + glm::vec2(patchSize / 2.0f);
        int targetLOD = calculateLOD(patchCenter * terrainScale.x, cameraPos);
        
        // Recreate patch if LOD changed
        if (targetLOD != patch.lod) {
            if (patch.VAO) glDeleteVertexArrays(1, &patch.VAO);
            if (patch.VBO) glDeleteBuffers(1, &patch.VBO);
            if (patch.EBO) glDeleteBuffers(1, &patch.EBO);
            
            createPatch(patch.offset.x, patch.offset.y, targetLOD);
            patch = patches.back();
        }
        
        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        shader.setMat4("model", model);
        
        glBindVertexArray(patch.VAO);
        glDrawElements(GL_TRIANGLES, patch.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

float Terrain::getHeightNormalized(int x, int z) const {
    if (x < 0 || x >= size || z < 0 || z >= size) return 0.0f;
    return heightData[z * size + x];
}

float Terrain::getHeight(float x, float z) const {
    x /= terrainScale.x;
    z /= terrainScale.z;
    
    if (x < 0 || x >= size - 1 || z < 0 || z >= size - 1) return 0.0f;
    
    int ix = (int)x;
    int iz = (int)z;
    float fx = x - ix;
    float fz = z - iz;
    
    // Bilinear interpolation
    float h00 = getHeightNormalized(ix, iz);
    float h10 = getHeightNormalized(ix + 1, iz);
    float h01 = getHeightNormalized(ix, iz + 1);
    float h11 = getHeightNormalized(ix + 1, iz + 1);
    
    float h0 = h00 * (1.0f - fx) + h10 * fx;
    float h1 = h01 * (1.0f - fx) + h11 * fx;
    
    return (h0 * (1.0f - fz) + h1 * fz) * scale * terrainScale.y;
}

glm::vec3 Terrain::getNormal(float x, float z) const {
    float heightL = getHeight(x - 1, z);
    float heightR = getHeight(x + 1, z);
    float heightD = getHeight(x, z - 1);
    float heightU = getHeight(x, z + 1);
    
    glm::vec3 normal = glm::normalize(glm::vec3(heightL - heightR, 2.0f, heightD - heightU));
    return normal;
}

}