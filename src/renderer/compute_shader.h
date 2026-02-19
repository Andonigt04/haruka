#ifndef COMPUTE_SHADER_H
#define COMPUTE_SHADER_H

#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>

class ComputeShader
{
public:
    ComputeShader(const std::string& computePath);
    ~ComputeShader();

    void use() const;
    void dispatch(GLuint x, GLuint y, GLuint z) const;

    // Setters
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;
    
    GLuint getID() const { return ID; }
private:
    GLuint ID;
    std::string readFile(const std::string& filePath);
    GLint getUniformLocation(const std::string& name) const;
};
#endif