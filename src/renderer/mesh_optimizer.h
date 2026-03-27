#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "mesh.h"

/**
 * MeshOptimizer - Optimizaciones de mesh para mejor performance
 * 
 * Características:
 * - LOD generation (multiple levels of detail)
 * - Vertex deduplication
 * - Index buffer optimization
 * - Mesh decimation
 */
class MeshOptimizer {
public:
    struct OptimizedMesh {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        float decimationRatio;  // Cuánto se redujo (0.0-1.0)
    };

    MeshOptimizer();
    ~MeshOptimizer() = default;

    /**
     * Generar LOD (Level of Detail) para un mesh
     * @param vertices Vértices originales
     * @param indices Índices originales
     * @param lodLevel 0=original, 1=50% verts, 2=25% verts, 3=12.5% verts
     * @return Mesh optimizado
     */
    OptimizedMesh generateLOD(
        const std::vector<Vertex>& vertices,
        const std::vector<unsigned int>& indices,
        int lodLevel
    );

    /**
     * Optimizar índices para mejor cache locality
     */
    std::vector<unsigned int> optimizeIndices(
        const std::vector<unsigned int>& indices
    );

    /**
     * Deduplicar vértices (merging de vértices cercanos)
     */
    OptimizedMesh deduplicateVertices(
        const std::vector<Vertex>& vertices,
        const std::vector<unsigned int>& indices,
        float mergeThreshold = 0.001f
    );

    /**
     * Decimación simple usando quadric error metrics
     */
    OptimizedMesh decimate(
        const std::vector<Vertex>& vertices,
        const std::vector<unsigned int>& indices,
        float targetRatio  // 0.5 = 50% de los triángulos originales
    );

    /**
     * Estadísticas de optimización
     */
    struct Stats {
        int originalVertices;
        int originalIndices;
        int optimizedVertices;
        int optimizedIndices;
        float reductionPercent;
        float estimatedFpsGain;
    };

    Stats getStats() const { return stats; }

private:
    struct Vertex_internal {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;
        int originalIndex;
    };

    // Cuadric error metrics
    struct QEM {
        glm::mat4 Q;  // 4x4 matriz de error cuadrático
    };

    bool areVerticesSimilar(
        const Vertex& v1,
        const Vertex& v2,
        float threshold
    ) const;

    float calculateVertexError(
        const glm::vec3& vertex,
        const glm::mat4& quadric
    ) const;

    Stats stats;
};
