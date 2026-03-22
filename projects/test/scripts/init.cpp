#include "init.h"
#include "player_input_controller.h"
#include "game_globals.h"
#include "game/planetary_system.h"
#include "game/character.h"
#include "core/camera.h"
#include <iostream>

Camera* g_gameCamera = nullptr;
Haruka::Character* g_playerCharacter = nullptr;
GameLogic::PlayerInputController* g_playerInputController = nullptr;
Haruka::PlanetarySystem* g_planetarySystem = nullptr;

// Callbacks del juego
void gameOnInit(Haruka::Scene* scene) {
    if (!scene) return;
    
    std::cout << "Game initialized - Planetary system loaded" << std::endl;
    
    // Inicializar sistema planetario
    if (!g_planetarySystem) {
        g_planetarySystem = new Haruka::PlanetarySystem();
        // Nota: init() requiere WorldSystem que no tenemos aquí
        // La gravedad se aplicará automáticamente basada en la escena
    }
    
    auto& objects = scene->getObjects();
    
    // Buscar el jugador en la escena
    for (auto& obj : objects) {
        if (obj.type == "Character" && obj.name == "Player") {
            Haruka::WorldPos playerPos = obj.position;
            g_playerCharacter = new Haruka::Character(playerPos, "player1");
            g_gameCamera = g_playerCharacter->getCamera();
            
            // Crear controlador de entrada
            g_playerInputController = new GameLogic::PlayerInputController(g_playerCharacter, g_planetarySystem);
            
            // Establecer el jugador en el sistema planetario
            if (g_planetarySystem) {
                auto charPtr = std::make_unique<Haruka::Character>(playerPos, "player1");
                g_planetarySystem->setPlayer(std::move(charPtr));
            }
            
            std::cout << "✓ Player initialized at position: " 
                      << playerPos.x << ", " << playerPos.y << ", " << playerPos.z << std::endl;
            return;
        }
    }
    
    std::cout << "⚠ No player found in scene" << std::endl;
}

void gameOnUpdate(GLFWwindow* window, float deltaTime) {
    // Actualizar sistema planetario (física, órbitas, etc)
    if (g_planetarySystem) {
        g_planetarySystem->update(deltaTime);
    }
    
    // Procesar input del jugador
    if (g_playerInputController) {
        g_playerInputController->update(window, deltaTime);
    }
}

Camera* gameGetCamera() {
    return g_gameCamera;
}

void gameOnShutdown() {
    std::cout << "Game shutting down..." << std::endl;
    if (g_playerCharacter) {
        delete g_playerCharacter;
        g_playerCharacter = nullptr;
    }
    if (g_playerInputController) {
        delete g_playerInputController;
        g_playerInputController = nullptr;
    }
}

// Interfaz de juego - EXPORT para que el editor lo cargue
namespace GameLogic {
    Haruka::GameInterface gameInterface = {
        .onInit = gameOnInit,
        .onUpdate = gameOnUpdate,
        .onShutdown = gameOnShutdown,
        .getCamera = gameGetCamera,
        .getScene = nullptr,
        .name = "Planetary Exploration System",
        .version = "0.1.0"
    };
}

// Función que el editor carga dinámicamente
extern "C" {
    Haruka::GameInterface* getGameInterface() {
        return &GameLogic::gameInterface;
    }
}
