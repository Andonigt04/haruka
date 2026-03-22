#include "scripting_editor.h"
#include <imgui.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <cstdlib>
#include <algorithm>

namespace fs = std::filesystem;

ScriptFile* ScriptingEditor::openScript(const std::string& path) {
    // Check if already open
    for (auto& script : openScripts) {
        if (script.path == path) {
            script.isOpen = true;
            return &script;
        }
    }
    
    // Load new script
    if (fs::exists(path)) {
        std::ifstream file(path);
        if (file.is_open()) {
            ScriptFile script;
            script.name = fs::path(path).stem().string();
            script.path = path;
            script.content = std::string((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
            script.isOpen = true;
            file.close();
            
            openScripts.push_back(script);
            std::cout << "✓ Script opened: " << script.name << std::endl;
            return &openScripts.back();
        }
    }
    
    return nullptr;
}

void ScriptingEditor::closeScript(const std::string& path) {
    auto it = std::find_if(openScripts.begin(), openScripts.end(),
        [&path](const ScriptFile& s) { return s.path == path; });
    
    if (it != openScripts.end()) {
        openScripts.erase(it);
        std::cout << "✓ Script closed" << std::endl;
    }
}

void ScriptingEditor::saveScript(const std::string& path) {
    auto it = std::find_if(openScripts.begin(), openScripts.end(),
        [&path](const ScriptFile& s) { return s.path == path; });
    
    if (it != openScripts.end()) {
        std::ofstream file(path);
        if (file.is_open()) {
            file << it->content;
            file.close();
            it->modified = false;
            std::cout << "✓ Script saved: " << it->name << std::endl;
        }
    }
}

void ScriptingEditor::compileScript(const std::string& path) {
    auto it = std::find_if(openScripts.begin(), openScripts.end(),
        [&path](const ScriptFile& s) { return s.path == path; });
    
    if (it != openScripts.end()) {
        // Save before compiling
        saveScript(path);
        
        // Run compiler (g++ example)
        std::string cmd = "cd " + fs::path(path).parent_path().string() + 
                         " && g++ -c " + it->name + ".cpp -o " + it->name + ".o 2>&1";
        
        FILE* pipe = popen(cmd.c_str(), "r");
        compileOutput.clear();
        
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                compileOutput += buffer;
            }
            pclose(pipe);
            
            bool success = compileOutput.empty();
            if (success) {
                std::cout << "✓ Script compiled successfully" << std::endl;
            } else {
                std::cout << "✗ Compilation errors" << std::endl;
            }
            
            if (onScriptCompiled) onScriptCompiled(it->name, success);
        }
    }
}

void ScriptingEditor::onImGuiRender() {
    if (ImGui::Begin("Script Editor")) {
        if (openScripts.empty()) {
            ImGui::Text("No scripts open");
        } else {
            // Tabs
            ImGui::BeginTabBar("ScriptTabs");
            for (size_t i = 0; i < openScripts.size(); ++i) {
                auto& script = openScripts[i];
                bool open = true;
                std::string label = script.name + (script.modified ? " *" : "");
                
                if (ImGui::BeginTabItem(label.c_str(), &open)) {
                    selectedScriptIndex = i;
                    
                    // Simple text editor
                    ImGui::InputTextMultiline("##editor", script.content.data(),
                        script.content.capacity(), ImVec2(-1, -50), ImGuiInputTextFlags_AllowTabInput);
                    
                    script.modified = true;
                    
                    ImGui::Separator();
                    
                    if (ImGui::Button("Save", ImVec2(80, 0))) {
                        saveScript(script.path);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Compile", ImVec2(80, 0))) {
                        compileScript(script.path);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Close", ImVec2(80, 0))) {
                        closeScript(script.path);
                    }
                    
                    ImGui::EndTabItem();
                }
                
                if (!open) {
                    closeScript(script.path);
                }
            }
            ImGui::EndTabBar();
            
            // Compiler output
            if (!compileOutput.empty()) {
                ImGui::TextDisabled("Compiler Output:");
                ImGui::TextWrapped("%s", compileOutput.c_str());
            }
        }
    }
    ImGui::End();
}
