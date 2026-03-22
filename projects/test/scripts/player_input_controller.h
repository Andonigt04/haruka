#pragma once

#include "game/character.h"
#include "game/planetary_system.h"
#include <GLFW/glfw3.h>

namespace GameLogic {

class PlayerInputController {
public:
    PlayerInputController(Haruka::Character* character, Haruka::PlanetarySystem* planetarySystem);
    
    void update(GLFWwindow* window, float deltaTime);
    
    void setMoveSpeed(float speed);
    void setMouseSensitivity(float sensitivity);
    
private:
    Haruka::Character* character = nullptr;
    Haruka::PlanetarySystem* planetarySystem = nullptr;
    
    float moveSpeed = 15.0f;
    float mouseSensitivity = 0.01f;
    
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool firstMouse = true;
    
    bool spacePressedLastFrame = false;
    bool fPressedLastFrame = false;
    bool spacePressed = false;
    float timeSinceLastSpacePress = 0.0f;
};

}
