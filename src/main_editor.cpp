// src/main_editor.cpp
#include "editor/editor_app.h"
#include <iostream>

int main(int argc, char** argv) {
    try {
        EditorApplication editor;
        editor.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}