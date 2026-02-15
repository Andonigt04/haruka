#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class IBL {
public:
    IBL();
    ~IBL();

    void loadHDRI(const std::string& imagePath);
    void generateIrradianceMap();
    void generatePrefilterMap();
    void generateBRDFLUT();

    unsigned int getIrradianceMap() const { return irradianceMap; }
    unsigned int getPrefilterMap() const { return prefilterMap; }
    unsigned int getBRDFLUT() const { return brdfLUT; }
    unsigned int getEnvCubemap() const { return envCubemap; }

    void bindPrefilterMap(unsigned int textureUnit);
    void bindBRDFLUT(unsigned int textureUnit);

private:
    void setupCubemap();
    void renderCube();
    void renderQuad();

    unsigned int envCubemap = 0;
    unsigned int irradianceMap = 0;
    unsigned int prefilterMap = 0;
    unsigned int brdfLUT = 0;

    unsigned int cubeVAO = 0, cubeVBO = 0;
    unsigned int quadVAO = 0, quadVBO = 0;
};