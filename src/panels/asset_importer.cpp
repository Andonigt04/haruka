#include "asset_importer.h"
#include <imgui.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

std::vector<ImportedAsset> AssetImporter::importModels(const std::string& folderPath) {
    std::vector<ImportedAsset> assets;
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
            if (!fs::is_regular_file(entry)) continue;
            
            std::string ext = entry.path().extension().string();
            if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx") {
                ImportedAsset asset;
                asset.name = entry.path().stem().string();
                asset.path = entry.path().string();
                asset.type = "model";
                asset.imported = true;
                assets.push_back(asset);
                std::cout << "✓ Imported model: " << asset.name << std::endl;
                if (onImportComplete) onImportComplete(asset);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error importing models: " << e.what() << std::endl;
    }
    
    return assets;
}

std::vector<ImportedAsset> AssetImporter::importTextures(const std::string& folderPath) {
    std::vector<ImportedAsset> assets;
    
    try {
        for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
            if (!fs::is_regular_file(entry)) continue;
            
            std::string ext = entry.path().extension().string();
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".hdr" || ext == ".tga") {
                ImportedAsset asset;
                asset.name = entry.path().stem().string();
                asset.path = entry.path().string();
                asset.type = "texture";
                asset.imported = true;
                assets.push_back(asset);
                std::cout << "✓ Imported texture: " << asset.name << std::endl;
                if (onImportComplete) onImportComplete(asset);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error importing textures: " << e.what() << std::endl;
    }
    
    return assets;
}

void AssetImporter::onImGuiRender() {
    if (ImGui::Begin("Asset Importer")) {
        ImGui::Text("Import Assets");
        ImGui::Separator();
        
        if (ImGui::Button("Import Models", ImVec2(150, 0))) {
            showImportDialog = true;
            importType = 0;
        }
        
        if (ImGui::Button("Import Textures", ImVec2(150, 0))) {
            showImportDialog = true;
            importType = 1;
        }
        
        ImGui::Separator();
        
        ImGui::Text("Pending Imports: %zu", pendingImports.size());
        for (const auto& asset : pendingImports) {
            ImGui::Text("  📦 %s (%s)", asset.name.c_str(), asset.type.c_str());
        }
        
        if (showImportDialog) {
            ImGui::OpenPopup("Select Import Folder");
            showImportDialog = false;
        }
        
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        
        if (ImGui::BeginPopupModal("Select Import Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char pathBuffer[512] = "assets/";
            ImGui::InputText("Folder Path##import", pathBuffer, sizeof(pathBuffer));
            
            if (ImGui::Button("Import", ImVec2(120, 0))) {
                if (importType == 0) {
                    pendingImports = importModels(pathBuffer);
                } else {
                    pendingImports = importTextures(pathBuffer);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}
