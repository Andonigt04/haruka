#include "search_panel.h"
#include <imgui.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace fs = std::filesystem;

float SearchPanel::calculateRelevance(const std::string& haystack, const std::string& needle) {
    std::string h = haystack;
    std::string n = needle;
    std::transform(h.begin(), h.end(), h.begin(), ::tolower);
    std::transform(n.begin(), n.end(), n.begin(), ::tolower);
    
    // Exact match
    if (h == n) return 1.0f;
    
    // Starts with
    if (h.find(n) == 0) return 0.9f;
    
    // Contains
    size_t pos = h.find(n);
    if (pos != std::string::npos) {
        return 0.7f - (pos * 0.01f); // Penalizar por posición
    }
    
    // Partial match (words)
    int matches = 0;
    for (char c : n) {
        if (h.find(c) != std::string::npos) matches++;
    }
    return (matches / (float)n.length()) * 0.5f;
}

std::vector<SearchResult> SearchPanel::searchFiles(const std::string& path, const std::string& query) {
    std::vector<SearchResult> results;
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(path)) {
            if (!fs::is_regular_file(entry)) continue;
            
            std::string name = entry.path().filename().string();
            float relevance = calculateRelevance(name, query);
            
            if (relevance > 0.3f) {
                std::string ext = entry.path().extension().string();
                std::string type = "file";
                if (ext == ".scene") type = "scene";
                else if (ext == ".prefab") type = "prefab";
                else if (ext == ".obj" || ext == ".gltf") type = "model";
                else if (ext == ".png" || ext == ".jpg") type = "texture";
                
                results.push_back({name, entry.path().string(), type, relevance});
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Search error: " << e.what() << std::endl;
    }
    
    // Sort by relevance
    std::sort(results.begin(), results.end(), 
        [](const SearchResult& a, const SearchResult& b) {
            return a.relevance > b.relevance;
        });
    
    return results;
}

std::vector<SearchResult> SearchPanel::searchInScene(const std::string& query) {
    // Placeholder - buscaría en objetos de la escena actual
    return {};
}

void SearchPanel::search(const std::string& query) {
    if (query.empty()) {
        results.clear();
        return;
    }
    
    results.clear();
    
    // Search in project files
    auto fileResults = searchFiles(".", query);
    results.insert(results.end(), fileResults.begin(), fileResults.end());
    
    // Search in scene objects
    auto sceneResults = searchInScene(query);
    results.insert(results.end(), sceneResults.begin(), sceneResults.end());
}

void SearchPanel::onImGuiRender() {
    ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Search")) {
        ImGui::End();
        return;
    }
    
    if (ImGui::InputTextWithHint("##search", "Search files, scenes, objects...", searchBuffer, sizeof(searchBuffer))) {
        search(searchBuffer);
    }
    
    ImGui::Separator();
    ImGui::Text("Results: %zu", results.size());
    ImGui::Separator();
    
    // Simple list without child window
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        bool selected = (i == selectedResultIndex);
        
        if (ImGui::Selectable(result.name.c_str(), selected)) {
            selectedResultIndex = i;
        }
        
        ImGui::SameLine();
        ImGui::TextDisabled("(%s) %.1f%%", result.type.c_str(), result.relevance * 100);
    }
    
    ImGui::End();
}
