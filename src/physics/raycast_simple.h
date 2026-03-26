#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/intersect.hpp>
#include <string>
#include <vector>

/**
 * RaycastSimple - Raycast simple para detección de altura
 * 
 * Sistema simple y eficiente para:
 * - Raycast ray-triangle
 * - Detección de altura del terreno
 * - Sin overhead innecesario
 */

struct RaycastHit {
    bool hit = false;
    float distance = 0.0f;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    int triangleIndex = -1;
};

struct RaycastTriangle {
    glm::vec3 v0, v1, v2;
    glm::vec3 normal;
};

class RaycastSimple {
public:
    RaycastSimple() = default;
    ~RaycastSimple() = default;

    /**
     * Agregar malla para raycast
     */
    void addMesh(const std::string& id, 
                 const std::vector<glm::vec3>& vertices,
                 const std::vector<unsigned int>& indices);

    /**
     * Raycast simple
     */
    RaycastHit raycast(const glm::vec3& origin, 
                       const glm::vec3& direction,
                       float maxDistance = 1000.0f);

    /**
     * Raycast hacia abajo (detectar altura del terreno)
     */
    RaycastHit raycastDown(const glm::vec3& position, float maxDistance = 1000.0f) {
        return raycast(position, glm::vec3(0.0f, -1.0f, 0.0f), maxDistance);
    }

private:
    std::vector<RaycastTriangle> triangles;

    bool rayTriangleIntersect(const glm::vec3& rayOrigin,
                              const glm::vec3& rayDir,
                              const RaycastTriangle& tri,
                              float& distance,
                              glm::vec3& hitPoint);
};
