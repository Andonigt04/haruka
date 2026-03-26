#pragma once

#include <glm/glm.hpp>
#include <vector>
#include "core/scene.h"

/**
 * LightCuller - Culling dinámico de luces
 * 
 * Reduce el número de luces enviadas a GPU basado en:
 * - Frustum culling (¿está la luz dentro del frustum?)
 * - Distance culling (¿está la luz demasiado lejos?)
 * - Light range checking (¿afecta esta luz al viewport?)
 */
class LightCuller {
public:
    struct CulledLight {
        glm::vec3 position;
        glm::vec3 color;
        float radius;  // Para point lights
    };

    LightCuller();
    ~LightCuller() = default;

    /**
     * Cull lights basado en cámara y escena
     * @param scene Escena con los objetos
     * @param viewMatrix Matriz de vista
     * @param projMatrix Matriz de proyección
     * @param maxLights Máximo de luces a retornar
     * @return Vector de luces filtradas
     */
    std::vector<CulledLight> cullLights(
        Haruka::Scene* scene,
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        int maxLights = 256
    );

    /**
     * Obtiene estadísticas de culling
     */
    int getTotalLights() const { return totalLights; }
    int getCulledLights() const { return culledLights; }

private:
    // Frustum planes
    struct Frustum {
        glm::vec4 planes[6];  // left, right, top, bottom, near, far
    };

    Frustum extractFrustum(const glm::mat4& viewProj);
    bool isPointInFrustum(const glm::vec3& point, const Frustum& frustum);
    bool isSphereInFrustum(const glm::vec3& center, float radius, const Frustum& frustum);

    int totalLights = 0;
    int culledLights = 0;
};
