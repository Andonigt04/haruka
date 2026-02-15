#ifndef CAMERA_H
#define CAMERA_H

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "math_types.h"

struct GLFWwindow;

class Camera {
public:

    Haruka::WorldPos position;
    Haruka::Rotation orientation;
    float speed = 5.0f;
    float sensitivity = 0.1f;
    float zoom = 45.0f;

    Camera(Haruka::WorldPos startPos);
    
    glm::vec3 getFront() const;
    glm::vec3 getUp() const;
    glm::mat4 getViewMatrix() const;

    void rotate(float deltaX, float deltaY);
    void processInput(GLFWwindow* window, float deltaTime);
    
    void ProcessMouseScroll(float yoffset);
};
#endif