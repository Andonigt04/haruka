#include "init.h"
#include "player_input_controller.h"
#include "game_globals.h"
#include "game/planetary_system.h"
#include "game/character.h"
#include "core/camera.h"
#include <iostream>
#include <memory>

// Global state
Camera* g_gameCamera = nullptr;
Haruka::Character* g_playerCharacter = nullptr;
GameLogic::PlayerInputController* g_playerInputController = nullptr;
Haruka::PlanetarySystem* g_planetarySystem = nullptr;

// ============================================================================
// GAME INITIALIZATION
// ============================================================================

void gameOnInit(Haruka::Scene* scene) {
    if (!scene) return;
    
    std::cout << "\n🎮 Game Initializing..." << std::endl;
    
    // Initialize planetary system
    g_planetarySystem = new Haruka::PlanetarySystem();
    std::cout << "✓ Planetary system initialized" << std::endl;
    
    auto& objects = scene->getObjects();
    std::cout << "✓ Scene loaded with " << objects.size() << " objects" << std::endl;
    
    // Find and initialize player
    for (auto& obj : objects) {
        if (obj.type == "Character" && obj.name == "Player") {
            Haruka::WorldPos playerPos = obj.position;
            
            g_playerCharacter = new Haruka::Character(playerPos, "player1");
            g_gameCamera = g_playerCharacter->getCamera();
            
            g_playerInputController = new GameLogic::PlayerInputController(
                g_playerCharacter,
                g_planetarySystem
            );
            
            auto charPtr = std::make_unique<Haruka::Character>(playerPos, "player1");
            g_planetarySystem->setPlayer(std::move(charPtr));
            
            std::cout << "✓ Player initialized" << std::endl;
            std::cout << "✓ Game ready\n" << std::endl;
            return;
        }
    }
    
    std::cout << "⚠ No player found in scene" << std::endl;
}

// ============================================================================
// GAME UPDATE
// ============================================================================

void gameOnUpdate(GLFWwindow* window, float deltaTime) {
    if (g_planetarySystem) {
        g_planetarySystem->update(deltaTime);
    }
    
    if (g_playerInputController) {
        g_playerInputController->update(window, deltaTime);
    }
}

// ============================================================================
// GAME INTERFACE
// ============================================================================

Camera* gameGetCamera() {
    return g_gameCamera;
}

void gameOnShutdown() {
    std::cout << "\n🛑 Shutting down..." << std::endl;
    if (g_playerCharacter) {
        delete g_playerCharacter;
        g_playerCharacter = nullptr;
    }
    if (g_playerInputController) {
        delete g_playerInputController;
        g_playerInputController = nullptr;
    }
    if (g_planetarySystem) {
        delete g_planetarySystem;
        g_planetarySystem = nullptr;
    }
}

namespace GameLogic {
    Haruka::GameInterface gameInterface = {
        .onInit = gameOnInit,
        .onUpdate = gameOnUpdate,
        .onShutdown = gameOnShutdown,
        .getCamera = gameGetCamera,
        .getScene = nullptr,
        .name = "Game",
        .version = "0.1.0"
    };
}

extern "C" {
    Haruka::GameInterface* getGameInterface() {
        return &GameLogic::gameInterface;
    }
}


