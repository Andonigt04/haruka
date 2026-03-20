#pragma once

#include <glm/glm.hpp>
#include <memory>
#include "core/camera.h"
#include "physics/physics_engine.h"
#include "network/network_manager.h"

namespace Haruka {

enum class CharacterState {
    IDLE,
    WALKING,
    RUNNING,
    JUMPING,
    FALLING,
    CROUCHING
};

class Character {
public:
    Character(const glm::dvec3& position, const std::string& userId = "");
    ~Character();
    
    void update(float deltaTime);
    void processInput(GLFWwindow* window, float deltaTime);
    
    // Movement
    void moveForward(float amount);
    void moveRight(float amount);
    void jump();
    void crouch(bool enabled);
    void sprint(bool enabled);
    
    // Camera
    void rotate(float yaw, float pitch);
    Camera* getCamera() const { return camera.get(); }
    
    // Physics
    void setPhysicsBody(std::shared_ptr<RigidBody> body) { physicsBody = body; }
    std::shared_ptr<RigidBody> getPhysicsBody() { return physicsBody; }
    
    // Network
    void setNetworkClient(NetworkClient* client) { networkClient = client; }
    void syncToServer();
    void applyServerUpdate(const glm::dvec3& serverPos, const glm::vec3& serverRot);
    
    // State
    glm::dvec3 getPosition() const { return position; }
    glm::dvec3 getVelocity() const { return velocity; }
    CharacterState getState() const { return state; }
    std::string getUserId() const { return userId; }

    void setPosition(glm::dvec3 newPosition) { position = newPosition; }
    void setVelocity(glm::dvec3 newVelocity) { velocity = newVelocity; }
    void setState(CharacterState newState) { state = newState; }
    
    bool isGrounded() const { return grounded; }
    bool isSprinting() const { return sprinting; }
    bool isCrouching() const { return crouched; }
    bool isLocalPlayer() const { return localPlayer; }

private:
    std::string userId;
    bool localPlayer = true;
    
    glm::dvec3 position;
    glm::dvec3 velocity;
    glm::vec3 forward;
    glm::vec3 right;
    
    std::unique_ptr<Camera> camera;
    std::shared_ptr<RigidBody> physicsBody;
    NetworkClient* networkClient = nullptr;
    
    CharacterState state = CharacterState::IDLE;
    
    // Movement parameters
    float walkSpeed = 5.0f;
    float runSpeed = 10.0f;
    float crouchSpeed = 2.5f;
    float jumpForce = 8.0f;
    float mouseSensitivity = 0.1f;
    
    float yaw = -90.0f;
    float pitch = 0.0f;
    
    bool grounded = false;
    bool sprinting = false;
    bool crouched = false;
    
    float standingHeight = 1.8f;
    float crouchingHeight = 1.2f;
    float currentHeight = 1.8f;
    
    // Network sync
    glm::dvec3 lastSyncPos;
    glm::vec3 lastSyncRot;
    glm::dvec3 targetPosition;
    glm::vec3 targetRotation;
    float syncThreshold = 0.1f;
    float syncInterval = 0.1f;
    float syncTimer = 0.0f;
    float interpolationSpeed = 5.0f;
    
    void updateCamera();
    void updateState();
    void checkGrounded();
    bool shouldSyncToServer();
};

}