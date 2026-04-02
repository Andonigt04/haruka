#include "planet_generator.h"
#include "noise_generator.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <limits>

namespace Haruka {

namespace {
float saturate(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

float smoothstep(float edge0, float edge1, float x) {
    float t = saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}
}

void PlanetGenerator::generateFace(
    PlanetData& data,
    glm::vec3 faceNormal,
    glm::vec3 right,
    glm::vec3 up,
    const PlanetConfig& config,
    int faceSeedOffset
) {
    int gridSize = 1 << config.subdivisions;
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

            // ---- Capa 1: continentes y océanos ----
            float continentHeight = 0.0f;
            float continentMask = 1.0f;
            float coastMask = 1.0f;
            if (config.enableContinents) {
                float wx = NoiseGenerator::fBm(normalized,
                                               config.seedBase + faceSeedOffset + 11,
                                               3,
                                               config.persistence,
                                               config.lacunarity,
                                               config.continentFrequency * 0.7f);
                float wy = NoiseGenerator::fBm(normalized,
                                               config.seedBase + faceSeedOffset + 17,
                                               3,
                                               config.persistence,
                                               config.lacunarity,
                                               config.continentFrequency * 0.7f);
                float wz = NoiseGenerator::fBm(normalized,
                                               config.seedBase + faceSeedOffset + 23,
                                               3,
                                               config.persistence,
                                               config.lacunarity,
                                               config.continentFrequency * 0.7f);

                glm::vec3 warp = glm::vec3(wx, wy, wz) * config.continentWarpStrength;
                glm::vec3 cpos = glm::normalize(normalized + warp);

                float c = NoiseGenerator::fBm(cpos,
                                              config.seedContinents + faceSeedOffset,
                                              config.octavesContinents,
                                              config.persistence,
                                              config.lacunarity,
                                              config.continentFrequency);
                float c01 = (c + 1.0f) * 0.5f;

                // Costa ancha para transiciones suaves tierra/mar
                float coastWidth = 0.22f;
                continentMask = smoothstep(config.seaLevel - coastWidth, config.seaLevel + coastWidth, c01);

                float signedContinent = c01 - config.seaLevel;

                // Máscara de costa: cerca de costa reducimos amplitudes de macro/micro
                coastMask = smoothstep(0.03f, 0.18f, std::abs(signedContinent));

                // Tierra positiva y océano ligeramente hundido, sin romper la base esférica
                float landPart = std::max(0.0f, signedContinent);
                float oceanPart = std::min(0.0f, signedContinent) * 0.18f;
                continentHeight = (landPart + oceanPart) * config.continentHeightStrength;
            }

            // ---- Capa 2: montañas principales ----
            float macro = NoiseGenerator::fBm(normalized,
                                              config.seedMacro + faceSeedOffset,
                                              config.octavesMacro,
                                              config.persistence,
                                              config.lacunarity,
                                              config.macroFrequency);
            float macro01 = (macro + 1.0f) * 0.5f;
            float mountainMask = smoothstep(0.48f, 0.72f, macro01);
            float mountainRidge = 1.0f - std::abs(2.0f * macro01 - 1.0f);
            float macroHeight = mountainMask * mountainRidge * config.macroHeightStrength * (0.25f + 0.75f * continentMask) * coastMask;

            // ---- Capa 3: detalle fino de montañas ----
            float detail = NoiseGenerator::fBm(normalized,
                                               config.seedDetail + faceSeedOffset,
                                               config.octavesDetail,
                                               config.persistence,
                                               config.lacunarity,
                                               config.detailFrequency);
            float detailSigned = detail * 0.5f + 0.5f;
            float detailMask = smoothstep(0.35f, 0.80f, macro01);
            float detailHeight = detailMask * detailSigned * config.detailHeightStrength * (0.4f + 0.6f * continentMask) * coastMask;

            float totalDelta = continentHeight + macroHeight + detailHeight;
            // Compresión muy suave para conservar definición sin picos extremos
            totalDelta = std::tanh(totalDelta * 1.4f) / 1.4f;

            // La esfera base (radio = 1.0) es el mínimo absoluto del terreno
            float height = 1.0f + std::max(0.0f, totalDelta);

            glm::vec3 vertex = normalized * config.radius * height;
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
    PlanetConfig cfg;
    cfg.radius = radius;
    cfg.subdivisions = subdivisions;
    cfg.seedBase = seed;
    cfg.seedContinents = seed + 100;
    cfg.seedMacro = seed + 200;
    cfg.seedDetail = seed + 300;
    cfg.enableContinents = false;
    cfg.detailFrequency = std::max(1.0f, noiseScale);
    cfg.detailHeightStrength = heightScale;
    cfg.octavesDetail = octaves;
    return generatePlanet(cfg);
}

PlanetGenerator::PlanetData PlanetGenerator::generatePlanet(const PlanetConfig& config) {
    PlanetData planet;
    planet.radius = config.radius;
    planet.minHeight = std::numeric_limits<float>::max();
    planet.maxHeight = std::numeric_limits<float>::lowest();
    
    // Generar 6 caras del cubo (proyectado a esfera)
    // Cara +X
    generateFace(planet, glm::vec3(1, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, 1, 0),
                 config, 0);
    
    // Cara -X
    generateFace(planet, glm::vec3(-1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, 1, 0),
                 config, 1);
    
    // Cara +Y
    generateFace(planet, glm::vec3(0, 1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, -1),
                 config, 2);
    
    // Cara -Y
    generateFace(planet, glm::vec3(0, -1, 0), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1),
                 config, 3);
    
    // Cara +Z
    generateFace(planet, glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 1, 0),
                 config, 4);
    
    // Cara -Z
    generateFace(planet, glm::vec3(0, 0, -1), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
                 config, 5);

    if (planet.minHeight == std::numeric_limits<float>::max()) {
        planet.minHeight = config.radius;
        planet.maxHeight = config.radius;
    }
    
    return planet;
}

PlanetGenerator::PlanetConfig PlanetGenerator::getPresetConfig(PlanetPreset preset) {
    PlanetConfig cfg;

    switch (preset) {
        case PlanetPreset::EARTH_LIKE:
            cfg.enableContinents = true;
            cfg.seaLevel = 0.52f;
            cfg.continentFrequency = 1.1f;
            cfg.continentWarpStrength = 0.14f;
            cfg.continentHeightStrength = 0.18f;
            cfg.macroFrequency = 3.2f;
            cfg.macroHeightStrength = 0.12f;
            cfg.detailFrequency = 12.0f;
            cfg.detailHeightStrength = 0.015f;
            break;
        case PlanetPreset::DESERT:
            cfg.enableContinents = true;
            cfg.seaLevel = 0.42f;
            cfg.continentFrequency = 1.4f;
            cfg.continentWarpStrength = 0.09f;
            cfg.continentHeightStrength = 0.14f;
            cfg.macroFrequency = 4.5f;
            cfg.macroHeightStrength = 0.08f;
            cfg.detailFrequency = 16.0f;
            cfg.detailHeightStrength = 0.02f;
            break;
        case PlanetPreset::ICE:
            cfg.enableContinents = true;
            cfg.seaLevel = 0.58f;
            cfg.continentFrequency = 0.95f;
            cfg.continentWarpStrength = 0.12f;
            cfg.continentHeightStrength = 0.10f;
            cfg.macroFrequency = 2.8f;
            cfg.macroHeightStrength = 0.10f;
            cfg.detailFrequency = 10.0f;
            cfg.detailHeightStrength = 0.012f;
            break;
    }

    return cfg;
}

}
