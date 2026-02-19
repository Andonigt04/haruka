#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>
#include "shader.h"
#include "texture.h"

namespace Haruka {

struct TerrainPatch {
    glm::vec2 offset;
    int lod;
    unsigned int VAO, VBO, EBO;
    unsigned int indexCount;
};

class Terrain {
public:
    Terrain(int size = 1024, float scale = 100.0f);
    ~Terrain();
    
    void loadHeightmap(const std::string& filepath);
    void generatePerlin(int seed = 0);
    
    void setPosition(const glm::vec3& pos) { position = pos; }
    void setScale(const glm::vec3& scale) { terrainScale = scale; }
    
    void render(Shader& shader, const glm::vec3& cameraPos);
    
    float getHeight(float x, float z) const;
    glm::vec3 getNormal(float x, float z) const;

private:
    int size;
    float scale;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 terrainScale = glm::vec3(1.0f);
    
    std::vector<float> heightData;
    std::vector<TerrainPatch> patches;
    
    // LOD settings
    float lodDistance[4] = {50.0f, 100.0f, 200.0f, 400.0f};
    int patchSize = 64;
    
    void generateMesh();
    void createPatch(int x, int z, int lod);
    int calculateLOD(const glm::vec2& patchCenter, const glm::vec3& cameraPos);
    
    float getHeightNormalized(int x, int z) const;
};

}