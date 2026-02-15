#pragma once
#include <glad/glad.h>

class GBuffer {
public:
    GBuffer(unsigned int width, unsigned int height);
    ~GBuffer();

    void bindForWriting();
    void unbind();
    void bindForReading(int index, unsigned int textureUnit);

    unsigned int getPositionTex() const { return gPosition; }
    unsigned int getNormalTex() const { return gNormal; }
    unsigned int getAlbedoSpecTex() const { return gAlbedoSpec; }
    unsigned int getEmissiveTex() const { return gEmissive; }

private:
    void setupFramebuffer();

    unsigned int gBufferFBO = 0;
    unsigned int gPosition = 0;
    unsigned int gNormal = 0;
    unsigned int gAlbedoSpec = 0;
    unsigned int gEmissive = 0;
    unsigned int rboDepth = 0;
    unsigned int width, height;
};