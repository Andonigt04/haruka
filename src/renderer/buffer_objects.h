#pragma once
#include <glad/glad.h>
#include <vector>

class VertexBuffer {
public:
    unsigned int ID;
    VertexBuffer(const void* data, unsigned int size) {
        glGenBuffers(1, &ID);
        glBindBuffer(GL_ARRAY_BUFFER, ID);
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    }
    ~VertexBuffer() { glDeleteBuffers(1, &ID); }
    
    void bind() const { glBindBuffer(GL_ARRAY_BUFFER, ID); }
};

class VertexArray {
public:
    unsigned int ID;
    VertexArray() { glGenVertexArrays(1, &ID); }
    ~VertexArray() { glDeleteVertexArrays(1, &ID); }

    void add_buffer(const VertexBuffer& vbo, unsigned int index, int size, int stride, const void* pointer) {
        bind();
        vbo.bind();
        glVertexAttribPointer(index, size, GL_FLOAT, GL_FALSE, stride, pointer);
        glEnableVertexAttribArray(index);
    }

    void bind() const { glBindVertexArray(ID); }
    void unbind() const { glBindVertexArray(0); }
};

class IndexBuffer {
public:
    unsigned int ID;
    IndexBuffer(const unsigned int* indices, unsigned int count) {
        glGenBuffers(1, &ID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(unsigned int), indices, GL_STATIC_DRAW);
    }
    ~IndexBuffer() { glDeleteBuffers(1, &ID); }
    void bind() const { glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ID); }
};