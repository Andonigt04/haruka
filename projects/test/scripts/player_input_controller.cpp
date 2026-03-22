#include "player_input_controller.h"
#include <GLFW/glfw3.h>
#include <iostream>

namespace GameLogic {

PlayerInputController::PlayerInputController(Haruka::Character* character, Haruka::PlanetarySystem* planetarySystem)
    : character(character), planetarySystem(planetarySystem) {}

void PlayerInputController::update(GLFWwindow* window, float deltaTime) {
    if (!character || !window) return;
    
    // WASD - Movimiento horizontal
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        character->moveForward(moveSpeed * deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        character->moveForward(-moveSpeed * deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        character->moveRight(-moveSpeed * deltaTime);
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        character->moveRight(moveSpeed * deltaTime);
    }
    
    // Space - Salto
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !spacePressedLastFrame) {
        if (character->isGrounded()) {
            character->jump();
        } else if (!character->isInFlightMode()) {
            // Activar modo vuelo si está en el aire (presionar space 2 veces)
            if (spacePressed && timeSinceLastSpacePress < 0.3f) {
                planetarySystem->setPlayerFlightMode(true);
                character->setState(Haruka::CharacterState::FALLING);
            }
            spacePressed = true;
            timeSinceLastSpacePress = 0.0f;
        }
    }
    
    spacePressedLastFrame = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
    
    if (spacePressed) {
        timeSinceLastSpacePress += deltaTime;
        if (timeSinceLastSpacePress > 0.3f) {
            spacePressed = false;
        }
    }
    
    // Shift - Sprint
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        character->sprint(true);
    } else {
        character->sprint(false);
    }
    
    // Ctrl - Crouch
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        character->crouch(true);
    } else {
        character->crouch(false);
    }
    
    // F - Modo vuelo toggle
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !fPressedLastFrame) {
        bool currentMode = character->isInFlightMode();
        planetarySystem->setPlayerFlightMode(!currentMode);
        
        if (!currentMode) {
            std::cout << "🚀 Flight Mode ENABLED" << std::endl;
        } else {
            std::cout << "🚶 Flight Mode DISABLED" << std::endl;
        }
    }
    fPressedLastFrame = (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS);
    
    // Mouse look - Cámara
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    
    if (firstMouse) {
        lastMouseX = mouseX;
        lastMouseY = mouseY;
        firstMouse = false;
    }
    
    double deltaX = mouseX - lastMouseX;
    double deltaY = mouseY - lastMouseY;
    lastMouseX = mouseX;
    lastMouseY = mouseY;
    
    character->rotate(static_cast<float>(deltaX * mouseSensitivity), static_cast<float>(-deltaY * mouseSensitivity));
}

void PlayerInputController::setMoveSpeed(float speed) {
    moveSpeed = speed;
}

void PlayerInputController::setMouseSensitivity(float sensitivity) {
    mouseSensitivity = sensitivity;
}

}
