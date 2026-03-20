#include "init.h"
#include "player/controller/player_controller.h"
#include "game_globals.h"
#include <iostream>

Camera* g_gameCamera = nullptr;

void GameLogic::GameInitializer::initializeGame(Haruka::Scene* scene) {
    if (!scene) return;
    
    std::cout << "Game initialized - Main scene loaded" << std::endl;
    
}