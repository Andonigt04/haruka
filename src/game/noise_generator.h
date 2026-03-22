#pragma once

#include <glm/glm.hpp>

namespace Haruka {

/**
 * @brief Generador de ruido Perlin simplex con semilla
 * Crea ruido determinista basado en semilla para terreno procedural
 */
class NoiseGenerator {
public:
    /**
     * @brief Generar ruido Perlin en una posición 3D
     * @param pos Posición del espacio 3D
     * @param seed Semilla para determinismo
     * @param scale Escala del ruido
     * @return Valor entre -1 y 1
     */
    static float perlin3D(const glm::vec3& pos, int seed = 0, float scale = 1.0f);
    
    /**
     * @brief Ruido Perlin multi-octava (Fractal Brownian Motion)
     * @param pos Posición
     * @param seed Semilla
     * @param octaves Número de capas
     * @param persistence Cuánto aporta cada octava (0-1)
     * @param lacunarity Multiplicador de frecuencia entre octavas
     * @param scale Escala base
     * @return Valor entre -1 y 1
     */
    static float fBm(
        const glm::vec3& pos,
        int seed = 0,
        int octaves = 4,
        float persistence = 0.5f,
        float lacunarity = 2.0f,
        float scale = 1.0f
    );

private:
    static float smoothstep(float t);
    static float lerp(float a, float b, float t);
    static float grad(int hash, float x, float y, float z);
    static int hash(int x, int y, int z, int seed);
};

}
