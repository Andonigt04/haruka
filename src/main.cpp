/**
 * @file main.cpp
 * @brief Punto de entrada del editor de Haruka.
 *
 * AQUÍ SE DECIDE QUÉ MOTOR SE USA.
 * Para cambiar de motor solo hay que cambiar qué IEngine* se crea.
 * El resto del editor (EditorApplication) no cambia.
 *
 * Ejemplo futuro con motor OpenGL:
 *   auto engine = std::make_unique<HarukaGLEngine>();
 */

#include "editor_app.h"
#include "engine/HarukaVulkanEngine.h"

#include <iostream>
#include <memory>

int main(int /*argc*/, char** /*argv*/) {
    try {
        // 1. Crear el motor (única línea que sabe qué API gráfica se usa)
        auto engine = std::make_unique<HarukaVulkanEngine>();

        // 2. Crear el editor inyectando el motor
        EditorApplication editor(engine.get());

        // 3. Ejecutar
        editor.run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}