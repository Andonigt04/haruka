#include "player_controller.h"
#include "../../game_globals.h"
#include <glm/glm.hpp>
#include <iostream>

namespace GameLogic {

    PlayerController::PlayerController(Haruka::Character* character)
        : character(character) {
    }

    void PlayerController::update(float deltaTime) {
        if (!character) return;
        
        // Por ahora solo aplicar gravedad
        applyRadialGravity(deltaTime);
    }

    void PlayerController::handleMovementInput(float deltaTime) {
        // TODO: Implementar cuando Input esté disponible
    }

    void PlayerController::handleRotationInput(float deltaTime) {
        // TODO: Implementar cuando Character tenga setYaw/setPitch
    }

    void PlayerController::handleJumpInput() {
        // TODO: Implementar cuando Input esté disponible
    }

    void PlayerController::updateGroundState() {
        isGrounded = true;
    }

    void PlayerController::applyRadialGravity(float deltaTime) {
        // Gravedad radial hacia el origen
        glm::dvec3 gravityDirection = -glm::normalize(character->getPosition());
        double gravityStrength = 9.81;
        
        character->setVelocity(character->getVelocity() + gravityDirection * gravityStrength * static_cast<double>(deltaTime));
    }

}