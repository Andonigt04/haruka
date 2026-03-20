#include "character.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace Haruka {

Character::Character(const glm::dvec3& position, const std::string& userId)
    : userId(userId), position(position), velocity(0.0) {
    
    localPlayer = !userId.empty();
    
    if (localPlayer) {
        camera = std::make_unique<Camera>(WorldPos(position.x, position.y + currentHeight * 0.9, position.z));
    }
    
    forward = glm::vec3(0, 0, -1);
    right = glm::vec3(1, 0, 0);
    
    lastSyncPos = position;
    lastSyncRot = glm::vec3(yaw, pitch, 0);
    targetPosition = position;
    targetRotation = glm::vec3(yaw, pitch, 0);
    
    std::cout << "[Character] Created" << (localPlayer ? " (local)" : " (remote)") << " at " 
              << position.x << ", " << position.y << ", " << position.z << std::endl;
}

Character::~Character() {}

void Character::update(float deltaTime) {
    // Sincronizar con física si existe
    if (physicsBody) {
        position = glm::dvec3(physicsBody->position);
        velocity = glm::dvec3(physicsBody->velocity);
    }
    
    // Interpolación para jugadores remotos
    if (!localPlayer) {
        position = glm::mix(position, targetPosition, deltaTime * interpolationSpeed);
        
        float targetYaw = targetRotation.x;
        float targetPitch = targetRotation.y;
        yaw = glm::mix(yaw, targetYaw, deltaTime * interpolationSpeed);
        pitch = glm::mix(pitch, targetPitch, deltaTime * interpolationSpeed);
    }
    
    checkGrounded();
    updateState();
    
    if (localPlayer) {
        updateCamera();
        
        // Sync con servidor
        syncTimer += deltaTime;
        if (syncTimer >= syncInterval && networkClient) {
            if (shouldSyncToServer()) {
                syncToServer();
                syncTimer = 0.0f;
            }
        }
    }
    
    // Smooth crouching
    float targetHeight = crouched ? crouchingHeight : standingHeight;
    currentHeight += (targetHeight - currentHeight) * deltaTime * 10.0f;
}

void Character::processInput(GLFWwindow* window, float deltaTime) {
    if (!localPlayer) return;
    
    // Movement
    float speed = sprinting ? runSpeed : (crouched ? crouchSpeed : walkSpeed);
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        moveForward(speed * deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        moveForward(-speed * deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        moveRight(-speed * deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        moveRight(speed * deltaTime);
    }
    
    // Jump
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && grounded) {
        jump();
    }
    
    // Sprint
    sprint(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
    
    // Crouch
    crouch(glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS);
}

void Character::moveForward(float amount) {
    if (physicsBody) {
        glm::dvec3 move = glm::dvec3(forward.x, 0, forward.z) * (double)amount;
        physicsBody->velocity.x = move.x;
        physicsBody->velocity.z = move.z;
    } else {
        position += glm::dvec3(forward) * (double)amount;
    }
}

void Character::moveRight(float amount) {
    if (physicsBody) {
        glm::dvec3 move = glm::dvec3(right.x, 0, right.z) * (double)amount;
        physicsBody->velocity.x = move.x;
        physicsBody->velocity.z = move.z;
    } else {
        position += glm::dvec3(right) * (double)amount;
    }
}

void Character::jump() {
    if (grounded && physicsBody) {
        physicsBody->velocity.y = jumpForce;
        grounded = false;
    }
}

void Character::crouch(bool enabled) {
    crouched = enabled;
}

void Character::sprint(bool enabled) {
    sprinting = enabled && !crouched;
}

void Character::rotate(float yawDelta, float pitchDelta) {
    if (!localPlayer) return;
    
    yaw += yawDelta * mouseSensitivity;
    pitch += pitchDelta * mouseSensitivity;
    
    pitch = glm::clamp(pitch, -89.0f, 89.0f);
    
    // Update forward/right vectors
    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward = glm::normalize(direction);
    
    right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
}

void Character::updateCamera() {
    if (!camera) return;
    
    glm::dvec3 cameraPos = position + glm::dvec3(0, currentHeight * 0.9, 0);
    camera->position = WorldPos(cameraPos.x, cameraPos.y, cameraPos.z);
    
    // Update camera orientation
    glm::quat qPitch = glm::angleAxis(glm::radians(pitch), glm::vec3(1, 0, 0));
    glm::quat qYaw = glm::angleAxis(glm::radians(yaw), glm::vec3(0, 1, 0));
    camera->orientation = qYaw * qPitch;
}

void Character::updateState() {
    float horizontalSpeed = glm::length(glm::vec2(velocity.x, velocity.z));
    
    if (!grounded) {
        state = velocity.y > 0 ? CharacterState::JUMPING : CharacterState::FALLING;
    } else if (crouched) {
        state = CharacterState::CROUCHING;
    } else if (horizontalSpeed > 0.1f) {
        state = sprinting ? CharacterState::RUNNING : CharacterState::WALKING;
    } else {
        state = CharacterState::IDLE;
    }
}

void Character::checkGrounded() {
    if (physicsBody) {
        grounded = (physicsBody->velocity.y < 0.1f && physicsBody->velocity.y > -0.1f);
    }
}

bool Character::shouldSyncToServer() {
    float posDelta = glm::length(glm::dvec3(position) - lastSyncPos);
    float rotDelta = glm::length(glm::vec3(yaw, pitch, 0) - lastSyncRot);
    
    return posDelta > syncThreshold || rotDelta > syncThreshold;
}

void Character::syncToServer() {
    if (!networkClient || !localPlayer) return;
    
    networkClient->sendPositionUpdate(
        glm::dvec3(position.x, position.y, position.z),
        glm::vec3(yaw, pitch, 0)
    );
    
    lastSyncPos = position;
    lastSyncRot = glm::vec3(yaw, pitch, 0);
    
    std::cout << "[Character] Synced to server: " 
              << position.x << ", " << position.y << ", " << position.z << std::endl;
}

void Character::applyServerUpdate(const glm::dvec3& serverPos, const glm::vec3& serverRot) {
    double distance = glm::length(serverPos - position);
    
    if (distance > 1.0) {
        targetPosition = serverPos;
        targetRotation = serverRot;
    }
}

}