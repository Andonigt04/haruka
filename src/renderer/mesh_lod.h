#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <glad/glad.h>
#include "mesh_optimizer.h"

/**
 * @brief Mesh level-of-detail manager.
 *
 * Generates and renders simplified mesh variants based on camera distance.
 */
class MeshLOD {
public:
    /** @brief Per-LOD mesh buffer and range description. */
    struct LODLevel {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        float minDistance;
        float maxDistance;
        GLuint VAO = 0;
        GLuint VBO = 0;
        GLuint EBO = 0;
    };

    /** @brief Constructs an empty LOD manager. */
    MeshLOD();
    /** @brief Releases generated LOD resources. */
    ~MeshLOD();

    /**
     * @brief Generates LOD levels from source mesh buffers.
     * @param vertices Source vertices.
     * @param indices Source indices.
     * @param distances Split distances per LOD level.
     */
    void generateLODs(
        const std::vector<Vertex>& vertices,
        const std::vector<unsigned int>& indices,
        const std::vector<float>& distances = {10.0f, 30.0f, 100.0f}
    );

    /** @brief Returns the best LOD index for a given distance. */
    int selectLOD(float distance) const;

    /** @brief Renders the mesh using the selected LOD level. */
    void render(float distance);

    /** @brief Statistics for generated LOD levels. */
    struct LODStats {
        int totalLODLevels;
        int totalVertices;
        int totalIndices;
        float memoryReduction;
    };

    /** @brief Returns current LOD statistics. */
    LODStats getStats() const;

private:
    std::vector<LODLevel> lodLevels;
    MeshOptimizer optimizer;

    /** @brief Allocates OpenGL buffers for one LOD level. */
    void setupGL(LODLevel& level);
};
