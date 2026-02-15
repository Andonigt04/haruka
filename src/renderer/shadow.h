#ifndef SHADOW_H
#define SHADOW_H

#include <glad/glad.h>
#include <glm/glm.hpp>

class Shadow
{
public:
    unsigned int depthMapFBO;
    unsigned int depthMap;
    unsigned int shadowWidth, shadowHeight;

    Shadow(unsigned int width = 1024, unsigned int height = 1024);
    ~Shadow();

    void bindForWriting();
    void bindForReading(unsigned int textureUnit = 2);
    void unbind();
private:
    void setupFramebuffer();
};
#endif