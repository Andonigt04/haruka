#pragma once
#include <glad/glad.h>

class Bloom
{
public:
    Bloom(unsigned int width, unsigned int height);
    ~Bloom();
    
    void bindForWriting();
    void unbind();
    void bindForReading(unsigned int textureUnit, int index);  // 0=bright, 1=blurred
    
    unsigned int getBrightTexture() { return brightTexture; }
    unsigned int getBlurredTexture() { return blurredTexture; }
    
private:
    void setupFramebuffer();
    unsigned int bloomFBO;
    unsigned int brightTexture;
    unsigned int blurredTexture;
    unsigned int rboDepth;
    unsigned int width, height;
};