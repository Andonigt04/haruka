#pragma once

#include <functional>
#include <string>

class EditorApplication;

/**
 * @brief Top-level editor menu bar renderer.
 *
 * Owns no state by itself; forwards actions to `EditorApplication`.
 */
class MenuBar {
public:
    /** @brief Binds the menu bar to the editor application. */
    MenuBar(EditorApplication* app) : editorApp(app) {}
    
    /** @brief Draws the menu bar and dispatches UI actions. */
    void render();
    
private:
    EditorApplication* editorApp;
    
    /** @brief Renders file menu items. */
    void renderFileMenu();
    /** @brief Renders edit menu items. */
    void renderEditMenu();
    /** @brief Renders object creation/management menu items. */
    void renderObjectsMenu();
    /** @brief Renders material / node-graph menu items. */
    void renderMaterialMenu();
    /** @brief Renders view/panel visibility menu items. */
    void renderViewMenu();
    /** @brief Renders play/pause controls. */
    void renderPlayControls();
    /** @brief Renders help/about menu items. */
    void renderHelpMenu();
};
