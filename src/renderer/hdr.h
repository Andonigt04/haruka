#pragma once

#include <glad/glad.h>

class HDR
{
public:
    HDR(unsigned int width, unsigned int height);
    ~HDR();

    void bindForWriting();
    void unbind();
    void bindForReading(unsigned int textureUnit, int index);

    unsigned int getColorTexture() { return colorTexture; }
    unsigned int getBrightTexture() { return brightTexture; }
    unsigned int getFBO() { return hdrFBO; }
private:
    void setupFramebuffer();
    unsigned int hdrFBO;
    unsigned int colorTexture;
    unsigned int brightTexture;
    unsigned int rboDepth;
    unsigned int width, height;
};