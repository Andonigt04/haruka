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

    // Render quality & layer culling
    int renderQualityPreset = 2; // 0=Low,1=Medium,2=High,3=Ultra
    float layer2MaxDistance = 1200.0f;
    float layer3MaxDistance = 3500.0f;
    float layer4MaxDistance = 900.0f;
    float layer5MaxDistance = 300.0f;
    
    // Project
    std::string projectName;
    std::string projectPath;
    
    void save();
    void load();

private:
    bool showSettingsWindow = false;
};
