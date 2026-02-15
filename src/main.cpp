#include "core/application.h"
#include <iostream>
#include <stdexcept>

int main() {
    Application app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Excepcion capturada: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}