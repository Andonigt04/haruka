#pragma once

#include <imgui.h>
#include <string>
#include <unordered_map>

class SettingsPanel {
public:
    SettingsPanel() = default;
    
    void onImGuiRender();
    
    // Graphics
    bool vsync = true;
    int maxFPS = 60;
    float masterVolume = 1.0f;
    
    // Editor
    bool autoSave = true;
    float autoSaveInterval = 30.0f;
    int maxBackups = 5;
    bool showGridInViewport = true;
    float gridSize = 1.0f;
    
    // Project
    std::string projectName;
    std::string projectPath;
    
    void save();
    void load();

private:
    bool showSettingsWindow = false;
};
