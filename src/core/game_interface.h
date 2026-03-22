#pragma once

#include "scene.h"
#include "camera.h"
#include <GLFW/glfw3.h>

namespace Haruka {

/**
 * @brief Interfaz genérica para lógica de juego
 * 
 * Los proyectos definen esta estructura con sus callbacks
 * El editor carga dinámicamente y llama a estos métodos
 * Permite máxima versatilidad sin código custom en el editor
 */
struct GameInterface {
    // Ciclo de vida
    typedef void (*OnInitFunc)(Scene* scene);
    typedef void (*OnUpdateFunc)(GLFWwindow* window, float deltaTime);
    typedef void (*OnShutdownFunc)();
    
    // Getters
    typedef Camera* (*GetCameraFunc)();
    typedef Scene* (*GetSceneFunc)();
    
    // Callbacks
    OnInitFunc onInit = nullptr;
    OnUpdateFunc onUpdate = nullptr;
    OnShutdownFunc onShutdown = nullptr;
    GetCameraFunc getCamera = nullptr;
    GetSceneFunc getScene = nullptr;
    
    // Información
    const char* name = "UnnamedGame";
    const char* version = "1.0.0";
};

// Macro para simplificar la creación del interfaz
#define GAME_INTERFACE_EXPORT extern "C"

}
