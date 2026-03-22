#pragma once

#include <string>
#include <vector>
#include <functional>

struct SearchResult {
    std::string name;
    std::string path;
    std::string type; // file, scene, prefab, object
    float relevance; // 0-1
};

class SearchPanel {
public:
    SearchPanel() = default;
    
    void onImGuiRender();
    void search(const std::string& query);
    
    const std::vector<SearchResult>& getResults() const { return results; }

private:
    std::string searchQuery;
    std::vector<SearchResult> results;
    int selectedResultIndex = -1;
    char searchBuffer[256] = "";
    
    std::vector<SearchResult> searchFiles(const std::string& path, const std::string& query);
    std::vector<SearchResult> searchInScene(const std::string& query);
    float calculateRelevance(const std::string& haystack, const std::string& needle);
};
