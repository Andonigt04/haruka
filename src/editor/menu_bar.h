#pragma once

#include <functional>
#include <string>

class EditorApplication;

class MenuBar {
public:
    MenuBar(EditorApplication* app) : editorApp(app) {}
    
    void render();
    
private:
    EditorApplication* editorApp;
    
    void renderFileMenu();
    void renderEditMenu();
    void renderObjectsMenu();
    void renderViewMenu();
    void renderPlayControls();
    void renderHelpMenu();
};
