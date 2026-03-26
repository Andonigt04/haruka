#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <glad/glad.h>
#include "mesh_optimizer.h"

/**
 * MeshLOD - Sistema de Level of Detail para meshes
 * 
 * Automáticamente renderiza versiones simplificadas
 * según la distancia a la cámara
 */
class MeshLOD {
public:
    struct LODLevel {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        float minDistance;
        float maxDistance;
        GLuint VAO = 0;
        GLuint VBO = 0;
        GLuint EBO = 0;
    };

    MeshLOD();
    ~MeshLOD();

    /**
     * Generar LOD levels automáticamente
     * @param vertices Vértices originales
     * @param indices Índices originales
     * @param distances Distancias para cada LOD {0-10m, 10-30m, 30-100m, 100m+}
     */
    void generateLODs(
        const std::vector<Vertex>& vertices,
        const std::vector<unsigned int>& indices,
        const std::vector<float>& distances = {10.0f, 30.0f, 100.0f}
    );

    /**
     * Obtener LOD apropiado según distancia
     */
    int selectLOD(float distance) const;

    /**
     * Renderizar mesh con LOD apropiado
     */
    void render(float distance);

    /**
     * Obtener estadísticas de LOD
     */
    struct LODStats {
        int totalLODLevels;
        int totalVertices;
        int totalIndices;
        float memoryReduction;
    };

    LODStats getStats() const;

private:
    std::vector<LODLevel> lodLevels;
    MeshOptimizer optimizer;

    void setupGL(LODLevel& level);
};
