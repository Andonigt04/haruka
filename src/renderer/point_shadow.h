#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

class PointShadow {
public:
    PointShadow(unsigned int resolution = 1024);
    ~PointShadow();

    void bindForWriting();
    void unbind();
    void bindForReading(unsigned int textureUnit);

    unsigned int getDepthCubemap() const { return depthCubemap; }
    unsigned int getFBO() const { return FBO; }

private:
    void setupFramebuffer();

    unsigned int FBO = 0;
    unsigned int depthCubemap = 0;
    unsigned int resolution;
};