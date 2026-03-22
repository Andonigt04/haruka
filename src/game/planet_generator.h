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

private:
    // Generar una cara del cubo
    static void generateFace(
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
    );
};

}
