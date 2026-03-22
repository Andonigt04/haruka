#include "init.h"
#include "player/controller/player_controller.h"
#include "game_globals.h"
#include "game/planetary_system.h"
#include <iostream>

Camera* g_gameCamera = nullptr;
Haruka::Character* g_playerCharacter = nullptr;

void GameLogic::GameInitializer::initializeGame(Haruka::Scene* scene) {
    if (!scene) return;
    
    std::cout << "Game initialized - Planetary system loaded" << std::endl;
    
    auto& objects = scene->getObjects();
    
    // Buscar el jugador en la escena
    for (auto& obj : objects) {
        if (obj.type == "Character" && obj.name == "Player") {
            Haruka::WorldPos playerPos = obj.position;
            g_playerCharacter = new Haruka::Character(playerPos, "player1");
            g_gameCamera = g_playerCharacter->getCamera();
            
            std::cout << "✓ Player initialized at position: " 
                      << playerPos.x << ", " << playerPos.y << ", " << playerPos.z << std::endl;
            return;
        }
    }
    
    std::cout << "⚠ No player found in scene" << std::endl;
}
