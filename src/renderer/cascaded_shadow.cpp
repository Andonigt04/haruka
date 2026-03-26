#include "cascaded_shadow.h"
#include "core/error_reporter.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

CascadedShadowMap::CascadedShadowMap() {}

CascadedShadowMap::~CascadedShadowMap() {
    for (auto fbo : shadowMapFramebuffers) {
        glDeleteFramebuffers(1, &fbo);
    }
    for (auto tex : shadowMapTextures) {
        glDeleteTextures(1, &tex);
    }
}

void CascadedShadowMap::init(float zNear, float zFar, float lambda) {
    this->zNear = zNear;
    this->zFar = zFar;
    this->lambda = lambda;

    shadowMapTextures.resize(NUM_CASCADES);
    shadowMapFramebuffers.resize(NUM_CASCADES);
    cascades.resize(NUM_CASCADES);

    for (int i = 0; i < NUM_CASCADES; i++) {
        createShadowMap(i);
    }

    std::cout << "✓ Cascaded Shadow Maps initialized\n";
    std::cout << "  Cascades: " << NUM_CASCADES << "\n";
    std::cout << "  Resolution: " << SHADOW_MAP_RESOLUTION << "x" << SHADOW_MAP_RESOLUTION << "\n";
}

void CascadedShadowMap::createShadowMap(int cascade) {
    // Crear texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F,
                 SHADOW_MAP_RESOLUTION, SHADOW_MAP_RESOLUTION,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    shadowMapTextures[cascade] = texture;

    // Crear framebuffer
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        HARUKA_MOTOR_ERROR(ErrorCode::RENDER_TARGET_FAILED, "Shadow map framebuffer incomplete!");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    shadowMapFramebuffers[cascade] = fbo;
}

void CascadedShadowMap::updateCascades(
    const glm::vec3& lightDir,
    const glm::vec3& cameraPos,
    const glm::vec3& cameraForward,
    float zNear,
    float zFar,
    float fov) {

    this->zNear = zNear;
    this->zFar = zFar;

    // Calcular splits logarítmicos
    float clipRange = zFar - zNear;

    for (int i = 0; i < NUM_CASCADES; i++) {
        float p = (i + 1) / static_cast<float>(NUM_CASCADES);
        
        // Logarithmic split
        float near = zNear + lambda * (p * clipRange) * (p * clipRange) + 
                           (1.0f - lambda) * p * clipRange;
        float far = zNear + lambda * ((p + 1) / static_cast<float>(NUM_CASCADES) * clipRange * 
                   (p + 1) / static_cast<float>(NUM_CASCADES) * clipRange) +
                    (1.0f - lambda) * (p + 1) / static_cast<float>(NUM_CASCADES) * clipRange;

        // Light view matrix
        glm::vec3 lightPos = cameraPos - lightDir * (far + zFar);
        glm::mat4 lightView = glm::lookAt(lightPos, cameraPos, glm::vec3(0.0f, 1.0f, 0.0f));

        // Light projection (ortho)
        float orthoSize = far * 1.5f;
        glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, far * 2.0f);

        cascades[i].zNear = near;
        cascades[i].zFar = far;
        cascades[i].viewProj = lightProj * lightView;
    }
}

glm::mat4 CascadedShadowMap::getCascadeMatrix(int cascade) const {
    if (cascade >= 0 && cascade < NUM_CASCADES) {
        return cascades[cascade].viewProj;
    }
    return glm::mat4(1.0f);
}

GLuint CascadedShadowMap::getShadowMapTexture(int cascade) const {
    if (cascade >= 0 && cascade < NUM_CASCADES) {
        return shadowMapTextures[cascade];
    }
    return 0;
}

CascadedShadowMap::CascadeInfo CascadedShadowMap::getCascadeInfo(int cascade) const {
    if (cascade >= 0 && cascade < NUM_CASCADES) {
        return cascades[cascade];
    }
    return CascadeInfo{zNear, zFar, glm::mat4(1.0f)};
}
