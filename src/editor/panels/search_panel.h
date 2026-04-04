#pragma once

#include <string>
#include <vector>
#include <functional>

/** @brief Search result entry for project/scene lookup. */
struct SearchResult {
    std::string name;
    std::string path;
    std::string type; // file, scene, prefab, object
    float relevance; // 0-1
};

/**
 * @brief Global search panel for files, scenes, prefabs, and objects.
 */
class SearchPanel {
public:
    /** @brief Constructs an empty search panel. */
    SearchPanel() = default;
    
    /** @brief Draws the search UI. */
    void onImGuiRender();
    /** @brief Executes a search query. */
    void search(const std::string& query);
    
    /** @brief Returns current result set. */
    const std::vector<SearchResult>& getResults() const { return results; }

private:
    std::string searchQuery;
    std::vector<SearchResult> results;
    int selectedResultIndex = -1;
    char searchBuffer[256] = "";
    
    /** @brief Searches file system items under a path. */
    std::vector<SearchResult> searchFiles(const std::string& path, const std::string& query);
    /** @brief Searches objects/scenes in memory. */
    std::vector<SearchResult> searchInScene(const std::string& query);
    /** @brief Computes relevance score for a query match. */
    float calculateRelevance(const std::string& haystack, const std::string& needle);
};
