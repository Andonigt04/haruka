#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Haruka {

/**
 * @brief Generador procedural de planetas con ruido
 * Crea una esfera con desniveles basados en ruido Perlin
 */
class PlanetGenerator {
public:
    enum class PlanetPreset {
        EARTH_LIKE,
        DESERT,
        ICE
    };

    struct PlanetConfig {
        float radius = 1.0f;
        int subdivisions = 4;

        // Seeds por capa
        int seedBase = 42;
        int seedContinents = 1337;
        int seedMacro = 2024;
        int seedDetail = 9001;

        // Switches globales
        bool enableContinents = true;

        // Continentes / océanos
        float seaLevel = 0.52f;
        float continentFrequency = 1.2f;
        float continentWarpStrength = 0.15f;
        float continentHeightStrength = 0.20f;

        // Macro relieve (cordilleras/mesetas)
        float macroFrequency = 3.5f;
        float macroHeightStrength = 0.12f;

        // Micro detalle
        float detailFrequency = 12.0f;
        float detailHeightStrength = 0.02f;

        // Ruido
        int octavesContinents = 4;
        int octavesMacro = 5;
        int octavesDetail = 4;
        float persistence = 0.5f;
        float lacunarity = 2.0f;
    };

    struct PlanetData {
        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;
        float radius = 0.0f;
        float minHeight = 0.0f;
        float maxHeight = 0.0f;
    };

    /**
     * @brief Generar planeta procedural
     * @param radius Radio base del planeta
     * @param subdivisions Subdivisiones por cara (2^n)
     * @param seed Semilla para ruido determinista
     * @param noiseScale Escala del ruido (frecuencia)
     * @param heightScale Amplitud de las montañas (0-1)
     * @param octaves Capas de ruido Perlin
     */
    static PlanetData generatePlanet(
        float radius,
        int subdivisions = 4,
        int seed = 42,
        float noiseScale = 1.0f,
        float heightScale = 0.1f,
        int octaves = 4
    );

    static PlanetData generatePlanet(const PlanetConfig& config);
    static PlanetConfig getPresetConfig(PlanetPreset preset);

private:
    // Generar una cara del cubo
    static void generateFace(
        PlanetData& data,
        glm::vec3 faceNormal,
        glm::vec3 right,
        glm::vec3 up,
        const PlanetConfig& config,
        int faceSeedOffset
    );
};

}
