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
    Character(const glm::vec3& position, const std::string& userId = "");
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
    Camera* getCamera() { return camera.get(); }
    
    // Physics
    void setPhysicsBody(std::shared_ptr<RigidBody> body) { physicsBody = body; }
    std::shared_ptr<RigidBody> getPhysicsBody() { return physicsBody; }
    
    // Network
    void setNetworkClient(NetworkClient* client) { networkClient = client; }
    void syncToServer();
    void applyServerUpdate(const glm::dvec3& serverPos, const glm::vec3& serverRot);
    
    // State
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getVelocity() const { return velocity; }
    CharacterState getState() const { return state; }
    std::string getUserId() const { return userId; }
    
    bool isGrounded() const { return grounded; }
    bool isSprinting() const { return sprinting; }
    bool isCrouching() const { return crouched; }
    bool isLocalPlayer() const { return localPlayer; }

private:
    std::string userId;
    bool localPlayer = true;
    
    glm::vec3 position;
    glm::vec3 velocity;
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
    float syncTimer = 0.0f;
    float syncInterval = 0.05f; // 20 Hz
    glm::vec3 lastSyncPos;
    glm::vec3 lastSyncRot;
    
    // Interpolation for remote players
    glm::vec3 targetPosition;
    glm::vec3 targetRotation;
    float interpolationSpeed = 10.0f;
    
    void updateCamera();
    void updateState();
    void checkGrounded();
    bool shouldSyncToServer();
};

}