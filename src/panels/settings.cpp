#include "settings.h"
#include "core/application.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>

using json = nlohmann::json;

void SettingsPanel::onImGuiRender() {
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("V-Sync", &vsync);
            ImGui::SliderInt("Max FPS##graphics", &maxFPS, 30, 240);
        }
        
        if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Master Volume##audio", &masterVolume, 0.0f, 1.0f);
        }
        
        if (ImGui::CollapsingHeader("Editor", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Auto Save##editor", &autoSave);
            ImGui::SliderFloat("Auto Save Interval (s)##editor", &autoSaveInterval, 5.0f, 300.0f);
            ImGui::SliderInt("Max Backups##editor", &maxBackups, 1, 20);
            ImGui::Checkbox("Show Grid in Viewport", &showGridInViewport);
            ImGui::SliderFloat("Grid Size##editor", &gridSize, 0.1f, 10.0f);
        }

        if (ImGui::CollapsingHeader("Rendering Quality", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* qualityNames[] = {"Low", "Medium", "High", "Ultra"};
            ImGui::Combo("Quality Preset", &renderQualityPreset, qualityNames, IM_ARRAYSIZE(qualityNames));

            ImGui::SliderFloat("Layer 2 Max Distance", &layer2MaxDistance, 100.0f, 10000.0f, "%.0f");
            ImGui::SliderFloat("Layer 3 Max Distance", &layer3MaxDistance, 200.0f, 25000.0f, "%.0f");
            ImGui::SliderFloat("Layer 4 Max Distance", &layer4MaxDistance, 50.0f, 5000.0f, "%.0f");
            ImGui::SliderFloat("Layer 5 Max Distance", &layer5MaxDistance, 20.0f, 2000.0f, "%.0f");
            ImGui::TextDisabled("Layer 1 is always rendered (critical objects).");

            Application::setRenderQualityPreset(renderQualityPreset);
            Application::setLayerMaxDistance(2, layer2MaxDistance);
            Application::setLayerMaxDistance(3, layer3MaxDistance);
            Application::setLayerMaxDistance(4, layer4MaxDistance);
            Application::setLayerMaxDistance(5, layer5MaxDistance);
        }
        
        if (ImGui::CollapsingHeader("Project", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::TextDisabled("Project Name: %s", projectName.c_str());
            ImGui::TextDisabled("Project Path: %s", projectPath.c_str());
        }
        
        ImGui::Separator();
        
        if (ImGui::Button("Save Settings", ImVec2(120, 0))) {
            save();
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Settings", ImVec2(120, 0))) {
            load();
        }
    }
    ImGui::End();
}

void SettingsPanel::save() {
    json settings;
    settings["graphics"]["vsync"] = vsync;
    settings["graphics"]["maxFPS"] = maxFPS;
    settings["audio"]["masterVolume"] = masterVolume;
    settings["editor"]["autoSave"] = autoSave;
    settings["editor"]["autoSaveInterval"] = autoSaveInterval;
    settings["editor"]["maxBackups"] = maxBackups;
    settings["editor"]["showGridInViewport"] = showGridInViewport;
    settings["editor"]["gridSize"] = gridSize;
    settings["rendering"]["qualityPreset"] = renderQualityPreset;
    settings["rendering"]["layer2MaxDistance"] = layer2MaxDistance;
    settings["rendering"]["layer3MaxDistance"] = layer3MaxDistance;
    settings["rendering"]["layer4MaxDistance"] = layer4MaxDistance;
    settings["rendering"]["layer5MaxDistance"] = layer5MaxDistance;
    
    std::ofstream file("editor_settings.json");
    if (file.is_open()) {
        file << settings.dump(2);
        file.close();
        std::cout << "✓ Settings saved" << std::endl;
    }
}

void SettingsPanel::load() {
    if (std::filesystem::exists("editor_settings.json")) {
        std::ifstream file("editor_settings.json");
        if (file.is_open()) {
            json settings;
            file >> settings;
            file.close();
            
            if (settings.contains("graphics")) {
                auto g = settings["graphics"];
                vsync = g.value("vsync", true);
                maxFPS = g.value("maxFPS", 60);
            }
            if (settings.contains("audio")) {
                auto a = settings["audio"];
                masterVolume = a.value("masterVolume", 1.0f);
            }
            if (settings.contains("editor")) {
                auto e = settings["editor"];
                autoSave = e.value("autoSave", true);
                autoSaveInterval = e.value("autoSaveInterval", 30.0f);
                maxBackups = e.value("maxBackups", 5);
                showGridInViewport = e.value("showGridInViewport", true);
                gridSize = e.value("gridSize", 1.0f);
            }
            if (settings.contains("rendering")) {
                auto r = settings["rendering"];
                renderQualityPreset = r.value("qualityPreset", 2);
                layer2MaxDistance = r.value("layer2MaxDistance", 1200.0f);
                layer3MaxDistance = r.value("layer3MaxDistance", 3500.0f);
                layer4MaxDistance = r.value("layer4MaxDistance", 900.0f);
                layer5MaxDistance = r.value("layer5MaxDistance", 300.0f);
            }

            Application::setRenderQualityPreset(renderQualityPreset);
            Application::setLayerMaxDistance(2, layer2MaxDistance);
            Application::setLayerMaxDistance(3, layer3MaxDistance);
            Application::setLayerMaxDistance(4, layer4MaxDistance);
            Application::setLayerMaxDistance(5, layer5MaxDistance);
            
            std::cout << "✓ Settings loaded" << std::endl;
        }
    }
}
