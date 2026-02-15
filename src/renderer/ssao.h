#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

class SSAO {
public:
    SSAO(unsigned int width, unsigned int height);
    ~SSAO();

    void bindForWriting();
    void unbind();
    void bindForReading(unsigned int textureUnit);

    unsigned int getSSAOTexture() const { return ssaoColorBuffer; }

private:
    void setupFramebuffer();
    void setupSamples();

    unsigned int ssaoFBO = 0;
    unsigned int ssaoColorBuffer = 0;
    unsigned int noiseTexture = 0;
    
    std::vector<glm::vec3> ssaoKernel;
    std::vector<glm::vec3> ssaoNoise;
    
    unsigned int width, height;
};