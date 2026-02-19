#include "camera.h"

#include <iostream>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

Camera::Camera(Haruka::WorldPos startPos)
    : position(startPos), orientation(glm::dvec3(0.0, 0.0, 0.0)) {};

glm::vec3 Camera::getFront() const {
    return glm::normalize(orientation * glm::dvec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 Camera::getUp() const {
    return glm::normalize(orientation * glm::dvec3(0.0f, -1.0f, 0.0f));
}

glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(Haruka::LocalPos(position), Haruka::LocalPos(position) + getFront(), getUp());
}

void Camera::rotate(float deltaX, float deltaY) {
    // Change to radians and apply sensitivity
    double xRad = glm::radians(-(double)deltaX* sensitivity);
    double yRad = glm::radians(-(double)deltaY* sensitivity);
    
    // Where rotate camera to
    Haruka::Rotation yaw = glm::angleAxis(xRad, glm::dvec3(0, 1, 0));
    Haruka::Rotation pitch = glm::angleAxis(yRad, glm::dvec3(1, 0, 0));

    // Apply the rotation with the yaw * orientation * pitch
    orientation = yaw * orientation * pitch;
    orientation = glm::normalize(orientation);
    //std::cout << "Rotando: " << deltaX << ", " << deltaY << std::endl;
}

void Camera::processInput(GLFWwindow* window, float deltaTime) {
    double velocity = (double)speed * (double)deltaTime;
    glm::vec3 front = getFront();
    glm::vec3 right = glm::normalize(glm::cross(front, getUp()));
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // Siempre hacia arriba en mundo
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += Haruka::WorldPos(front) * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= Haruka::WorldPos(front) * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position -= Haruka::WorldPos(right) * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position += Haruka::WorldPos(right) * velocity;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) position += Haruka::WorldPos(up) * velocity;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) position -= Haruka::WorldPos(up) * velocity;
}

void Camera::ProcessMouseScroll(float yoffset) {
    zoom -= (float)yoffset;
    if (zoom < 1.0f) zoom = 1.0f;
    if (zoom > 45.0f) zoom = 45.0f;
}