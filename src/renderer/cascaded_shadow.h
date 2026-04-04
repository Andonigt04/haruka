#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <vector>

/**
 * CascadedShadowMap - Sombras en cascadas (4 niveles)
 * 
 * Ventajas:
 * - Sombras bien distribuidas (cercano y lejano)
 * - PCF filtering para suavidad
 * - Transiciones suaves entre cascadas
 */

class CascadedShadowMap {
public:
    static constexpr int NUM_CASCADES = 4;
    static constexpr int SHADOW_MAP_RESOLUTION = 2048;

    CascadedShadowMap();
    ~CascadedShadowMap();

    /**
     * Inicializar cascaded shadows
     * @param zNear Near plane
     * @param zFar Far plane
     * @param lambda Parámetro de distribución (0.5 recomendado)
     */
    void init(float zNear, float zFar, float lambda = 0.5f);

    /**
     * Actualizar cascadas basado en luz y cámara
     */
    void updateCascades(
        const glm::vec3& lightDir,
        const glm::vec3& cameraPos,
        const glm::vec3& cameraForward,
        const glm::vec3& cameraUp,
        float aspect,
        float zNear,
        float zFar,
        float fov
    );

    /**
     * Obtener matrices de proyección para cada cascada
     */
    glm::mat4 getCascadeMatrix(int cascade) const;

    /**
     * Obtener texture de shadow map para cascada
     */
    GLuint getShadowMapTexture(int cascade) const;
    GLuint getFramebuffer(int cascade) const;
    void bindForWriting(int cascade) const;
    void bindForReading(int cascade, unsigned int textureUnit) const;

    /**
     * Obtener información de cascadas
     */
    struct CascadeInfo {
        float zNear;
        float zFar;
        glm::mat4 viewProj;
    };

    CascadeInfo getCascadeInfo(int cascade) const;

    /**
     * Estadísticas
     */
    int getNumCascades() const { return NUM_CASCADES; }
    int getShadowMapResolution() const { return SHADOW_MAP_RESOLUTION; }

private:
    std::vector<GLuint> shadowMapTextures;
    std::vector<GLuint> shadowMapFramebuffers;
    std::vector<CascadeInfo> cascades;

    float zNear, zFar, lambda;

    void createShadowMap(int cascade);
};
