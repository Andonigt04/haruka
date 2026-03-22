#include "planet_generator.h"
#include "noise_generator.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>

namespace Haruka {

void PlanetGenerator::generateFace(
    PlanetData& data,
    glm::vec3 faceNormal,
    glm::vec3 right,
    glm::vec3 up,
    int subdivisions,
    float radius,
    int seed,
    float noiseScale,
    float heightScale,
    int octaves
) {
    int gridSize = 1 << subdivisions;
    float step = 2.0f / gridSize;
    int vertexOffset = data.vertices.size();
    
    // Generar vértices para esta cara
    for (int i = 0; i <= gridSize; ++i) {
        for (int j = 0; j <= gridSize; ++j) {
            float u = -1.0f + i * step;
            float v = -1.0f + j * step;
            
            // Posición en el cubo
            glm::vec3 cubePos = faceNormal + right * u + up * v;
            glm::vec3 normalized = glm::normalize(cubePos);
            
            // Generar ruido multi-octava
            float noiseValue = NoiseGenerator::fBm(
                normalized,
                seed,
                octaves,
                0.5f,          // persistence
                2.0f,          // lacunarity
                noiseScale
            );
            
            // Normalizar ruido a [0, 1]
            noiseValue = (noiseValue + 1.0f) * 0.5f;
            
            // Aplicar ruido como altura (con suavizado)
            float heightVariation = noiseValue * heightScale;
            float height = 1.0f + heightVariation;
            
            glm::vec3 vertex = normalized * radius * height;
            glm::vec3 normal = glm::normalize(vertex);
            
            data.vertices.push_back(vertex);
            data.normals.push_back(normal);
            
            // Actualizar min/max altura
            float vertexHeight = glm::length(vertex);
            data.minHeight = std::min(data.minHeight, vertexHeight);
            data.maxHeight = std::max(data.maxHeight, vertexHeight);
        }
    }
    
    // Generar índices para esta cara
    int stride = gridSize + 1;
    for (int i = 0; i < gridSize; ++i) {
        for (int j = 0; j < gridSize; ++j) {
            int a = vertexOffset + i * stride + j;
            int b = a + 1;
            int c = a + stride;
            int d = c + 1;
            
            // Triángulo 1
            data.indices.push_back(a);
            data.indices.push_back(c);
            data.indices.push_back(b);
            
            // Triángulo 2
            data.indices.push_back(b);
            data.indices.push_back(c);
            data.indices.push_back(d);
        }
    }
}

PlanetGenerator::PlanetData PlanetGenerator::generatePlanet(
    float radius,
    int subdivisions,
    int seed,
    float noiseScale,
    float heightScale,
    int octaves
) {
    PlanetData planet;
    planet.radius = radius;
    planet.minHeight = radius * (1.0f - heightScale);
    planet.maxHeight = radius * (1.0f + heightScale);
    
    // Generar 6 caras del cubo (proyectado a esfera)
    // Cara +X
    generateFace(planet, glm::vec3(1, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0),
                 subdivisions, radius, seed, noiseScale, heightScale, octaves);
    
    // Cara -X
    generateFace(planet, glm::vec3(-1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0),
                 subdivisions, radius, seed + 1, noiseScale, heightScale, octaves);
    
    // Cara +Y
    generateFace(planet, glm::vec3(0, 1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, -1),
                 subdivisions, radius, seed + 2, noiseScale, heightScale, octaves);
    
    // Cara -Y
    generateFace(planet, glm::vec3(0, -1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1),
                 subdivisions, radius, seed + 3, noiseScale, heightScale, octaves);
    
    // Cara +Z
    generateFace(planet, glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0),
                 subdivisions, radius, seed + 4, noiseScale, heightScale, octaves);
    
    // Cara -Z
    generateFace(planet, glm::vec3(0, 0, -1), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
                 subdivisions, radius, seed + 5, noiseScale, heightScale, octaves);
    
    return planet;
}

}
