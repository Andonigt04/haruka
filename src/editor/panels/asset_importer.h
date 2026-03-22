#pragma once

#include <string>
#include <vector>
#include <functional>

struct ImportedAsset {
    std::string name;
    std::string path;
    std::string type; // model, texture, audio, etc
    bool imported = false;
};

class AssetImporter {
public:
    AssetImporter() = default;
    
    void onImGuiRender();
    
    std::vector<ImportedAsset> importModels(const std::string& folderPath);
    std::vector<ImportedAsset> importTextures(const std::string& folderPath);
    
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
