#pragma once

#include <string>
#include <memory>

class EditorApplication;

/**
 * ExportPanel - Panel para exportar juegos
 * Permite configurar metadatos y opciones de export
 */
class ExportPanel {
public:
    ExportPanel();
    ~ExportPanel();
    
    void render(EditorApplication* editorApp);
    void show() { visible = true; }
    void hide() { visible = false; }
    bool isVisible() const { return visible; }

private:
    bool visible = false;
    
    // UI state
    char gameName[256] = {0};
    char version[32] = {0};
    char author[256] = {0};
    char description[512] = {0};

    char shadersPath[512] = {0}; // Ruta de shaders a exportar
    char exportPath[512] = {0};
    
    int buildTypeSelected = 1;  // Release por defecto
    int platformSelected = 0;   // Linux por defecto
    
    bool includeDebugSymbols = false;
    bool optimizeAssets = true;
    bool stripUnusedContent = true;
    bool compressAssets = true;
    
    std::string exportStatus;
    bool exportStatusError = false;
    
    void loadProjectSettings(EditorApplication* editorApp);
    void saveProjectSettings(EditorApplication* editorApp);
    bool validateSettings() const;
    void performExport(EditorApplication* editorApp);
};
