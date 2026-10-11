// El núcleo no elige motor: lo carga al arrancar (backend/backend_loader.h).
#include "editor_app.h"
#include <iostream>

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    try {
        EditorApplication editor;
        editor.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
