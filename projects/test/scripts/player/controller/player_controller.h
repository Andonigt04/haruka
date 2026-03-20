#pragma once

#include "core/scene.h"
#include "game/character.h"
#include <glm/glm.hpp>
#include <memory>

namespace GameLogic {
    class PlayerController {
    public:
        PlayerController(Haruka::Character* character);
        ~PlayerController() = default;

        void update(float deltaTime);
        
        void setMoveSpeed(float speed) { moveSpeed = speed; }
        void setJumpForce(float force) { jumpForce = force; }
        void setMouseSensitivity(float sens) { mouseSensitivity = sens; }

    private:
        Haruka::Character* character = nullptr;
        
        float moveSpeed = 5.0f;
        float jumpForce = 5.0f;
        float mouseSensitivity = 1.0f;
        float groundDrag = 0.1f;
        float airDrag = 0.01f;
        
        bool isGrounded = false;
        float groundCheckDistance = 0.1f;
        
        void handleMovementInput(float deltaTime);
        void handleRotationInput(float deltaTime);
        void handleJumpInput();
        void updateGroundState();
        void applyRadialGravity(float deltaTime);
    };
}