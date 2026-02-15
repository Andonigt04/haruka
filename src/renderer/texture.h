#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/glad.h>

class Texture {
public:
    unsigned int ID;
    int width, height, nrChannels;

    Texture(const char* path);
    void use(unsigned int unit = 0);
    void cleanup();
};
#endif