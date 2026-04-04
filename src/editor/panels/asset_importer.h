#pragma once

#include <string>
#include <vector>
#include <functional>

/** @brief Description of one imported asset result. */
struct ImportedAsset {
    std::string name;
    std::string path;
    std::string type; // model, texture, audio, etc
    bool imported = false;
};

/**
 * @brief Asset import panel for models and textures.
 */
class AssetImporter {
public:
    /** @brief Constructs an importer panel. */
    AssetImporter() = default;
    
    /** @brief Draws the asset importer UI. */
    void onImGuiRender();
    
    /** @brief Imports models from one folder. */
    std::vector<ImportedAsset> importModels(const std::string& folderPath);
    /** @brief Imports textures from one folder. */
    std::vector<ImportedAsset> importTextures(const std::string& folderPath);
    
    /** @brief Registers a callback for completed imports. */
    void setOnImportComplete(std::function<void(const ImportedAsset&)> cb) {
        onImportComplete = std::move(cb);
    }

private:
    std::vector<ImportedAsset> pendingImports;
    bool showImportDialog = false;
    std::string selectedPath;
    int importType = 0; // 0=model, 1=texture
    
    std::function<void(const ImportedAsset&)> onImportComplete;
};
