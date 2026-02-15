#pragma once
#include <glad/glad.h>

class RenderTarget {
public:
    RenderTarget(unsigned int width, unsigned int height);
    ~RenderTarget();

    void bindForWriting();
    void unbind();
    void bindForReading(unsigned int textureUnit);

    unsigned int getColorTexture() const { return colorTexture; }
    unsigned int getFBO() const { return FBO; }

private:
    void setupFramebuffer();

    unsigned int FBO = 0;
    unsigned int colorTexture = 0;
    unsigned int rboDepth = 0;
    unsigned int width, height;
};