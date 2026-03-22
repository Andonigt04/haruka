#include "settings.h"
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
            
            std::cout << "✓ Settings loaded" << std::endl;
        }
    }
}
