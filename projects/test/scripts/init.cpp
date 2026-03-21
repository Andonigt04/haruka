#include "init.h"
#include "player/controller/player_controller.h"
#include "game_globals.h"
#include <iostream>

Camera* g_gameCamera = nullptr;
Haruka::Character* g_playerCharacter = nullptr;

void GameLogic::GameInitializer::initializeGame(Haruka::Scene* scene) {
    if (!scene) return;
    
    std::cout << "Game initialized - Main scene loaded" << std::endl;
    
    auto& objects = scene->getObjects();
    
    for (auto& obj : objects) {
        // Si es un Prefab, buscar componentes en children
        if (obj.type == "Prefab") {
            for (auto& child : obj.children) {
                if (child.properties.is_null()) {
                    continue;
                }
                
                std::string componentClass = child.properties.value("class", "");
                
                if (componentClass == "Haruka::Character" || child.type == "Character") {
                    Haruka::WorldPos charPos = child.position;
                    g_playerCharacter = new Haruka::Character(charPos, "player1");
                    g_gameCamera = g_playerCharacter->getCamera();
                    
                    std::cout << "Player character created at (" << charPos.x << ", " 
                              << charPos.y << ", " << charPos.z << ")" << std::endl;
                }
                
                if (componentClass == "GameLogic::PlayerController") {
                    if (g_playerCharacter) {
                        auto controller = std::make_unique<GameLogic::PlayerController>(
                            g_playerCharacter
                        );
                        std::cout << "Player controller initialized" << std::endl;
                    }
                }
            }
        }
    }
    
    std::cout << "Game initialization complete" << std::endl;
}