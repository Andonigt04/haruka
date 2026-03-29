#include "export_panel.h"
#include "editor/editor_app.h"

#include <imgui.h>
#include <nfd.h>
#include <filesystem>
#include <iostream>
#include <cstring>

namespace fs = std::filesystem;

ExportPanel::ExportPanel() {
    strcpy(version, "1.0.0");
}

ExportPanel::~ExportPanel() = default;

void ExportPanel::render(EditorApplication* editorApp) {
    if (!visible) return;
    if (!editorApp) return;

    static std::string lastProjectPath;
    std::string currentProjectPath;
    if (editorApp->getProject()) {
        currentProjectPath = editorApp->getProject()->getPath();
    }
    // Si el proyecto cambió, recarga los datos
    if (currentProjectPath != lastProjectPath) {
        loadProjectSettings(editorApp);
        lastProjectPath = currentProjectPath;
    }

    ImGui::SetNextWindowSize(ImVec2(700, 800), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Game Export", &visible, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Export Configuration");
        ImGui::Separator();
        
        // ===== METADATOS =====
        if (ImGui::CollapsingHeader("Game Information", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("Game Name##export", gameName, sizeof(gameName));
            ImGui::InputText("Version##export", version, sizeof(version));
            ImGui::InputText("Author##export", author, sizeof(author));
            ImGui::InputTextMultiline("Description##export", description, sizeof(description),
                                     ImVec2(-1, 80));
            ImGui::InputText("Shaders Folder##export", shadersPath, sizeof(shadersPath));
            ImGui::SameLine();
            ImGui::TextDisabled("(Ruta a copiar en ../shaders)");
        }
        
        // ===== CONFIGURACIÓN DE BUILD =====
        if (ImGui::CollapsingHeader("Build Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            const char* buildTypes[] = { "Debug", "Release", "Shipping" };
            ImGui::Combo("Build Type##export", &buildTypeSelected, buildTypes, IM_ARRAYSIZE(buildTypes));
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Debug: Full symbols\nRelease: Optimized\nShipping: Maximum optimization");
            }
            
            const char* platforms[] = { "Linux 64-bit", "Windows 64-bit", "macOS 64-bit" };
            ImGui::Combo("Platform##export", &platformSelected, platforms, IM_ARRAYSIZE(platforms));
        }
        
        // ===== OPCIONES DE COMPILACIÓN =====
        if (ImGui::CollapsingHeader("Compilation Options")) {
            ImGui::Checkbox("Include Debug Symbols##export", &includeDebugSymbols);
            ImGui::Checkbox("Optimize Assets##export", &optimizeAssets);
            ImGui::Checkbox("Strip Unused Content##export", &stripUnusedContent);
            ImGui::Checkbox("Compress Assets##export", &compressAssets);
        }
        
        ImGui::Separator();
        
        // Sincronizar exportPath con config.outputPath antes de mostrar
        if (editorApp && editorApp->getProject()) {
            auto& config = editorApp->getProject()->getConfig();
            if (strcmp(exportPath, config.outputPath.c_str()) != 0) {
                strncpy(exportPath, config.outputPath.c_str(), sizeof(exportPath));
                exportPath[sizeof(exportPath)-1] = '\0';
            }
        }
        if (ImGui::InputText("Export Folder##export", exportPath, sizeof(exportPath))) {
            // Al editar, guardar en config.outputPath
            if (editorApp && editorApp->getProject()) {
                auto& config = editorApp->getProject()->getConfig();
                config.outputPath = exportPath;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            nfdchar_t* outPath = nullptr;
            nfdresult_t result = NFD_PickFolder(nullptr, &outPath);
            if (result == NFD_OKAY && outPath) {
                strncpy(exportPath, outPath, sizeof(exportPath));
                exportPath[sizeof(exportPath)-1] = '\0';
                if (editorApp && editorApp->getProject()) {
                    auto& config = editorApp->getProject()->getConfig();
                    config.outputPath = exportPath;
                }
                free(outPath);
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Carpeta destino del export)");
        
        ImGui::Separator();
        
        // ===== VALIDACIÓN =====
        bool isValid = validateSettings();
        if (!isValid) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "⚠ Game Name, Version, and Author are required");
        }
        
        ImGui::Separator();
        
        // ===== BOTONES =====
        if (ImGui::Button("Export Game", ImVec2(150, 0))) {
            if (isValid) {
                saveProjectSettings(editorApp);
                performExport(editorApp);
            } else {
                exportStatus = "Fill all required fields";
                exportStatusError = true;
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Load Project Settings", ImVec2(180, 0))) {
            loadProjectSettings(editorApp);
            exportStatus = "Settings loaded from project";
            exportStatusError = false;
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(100, 0))) {
            visible = false;
        }
        
        // ===== STATUS =====
        ImGui::Separator();
        if (!exportStatus.empty()) {
            if (exportStatusError) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", exportStatus.c_str());
            } else {
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", exportStatus.c_str());
            }
        }
        
        ImGui::End();
    }
}

void ExportPanel::loadProjectSettings(EditorApplication* editorApp) {
    if (!editorApp) return;
    Haruka::Project* project = editorApp->getProject();
    if (!project) return;
    const auto& config = project->getConfig();
    // Copiar los datos del proyecto a los buffers del panel
    strncpy(gameName, config.name.c_str(), sizeof(gameName));
    gameName[sizeof(gameName)-1] = '\0';
    strncpy(version, config.version.c_str(), sizeof(version));
    version[sizeof(version)-1] = '\0';
    strncpy(author, config.exportSettings.author.c_str(), sizeof(author));
    author[sizeof(author)-1] = '\0';
    strncpy(description, config.exportSettings.description.c_str(), sizeof(description));
    description[sizeof(description)-1] = '\0';
    strncpy(shadersPath, config.shadersPath.c_str(), sizeof(shadersPath));
    shadersPath[sizeof(shadersPath)-1] = '\0';
    // Sincronizar exportPath con config.outputPath
    if (!config.outputPath.empty()) {
        strncpy(exportPath, config.outputPath.c_str(), sizeof(exportPath));
        exportPath[sizeof(exportPath)-1] = '\0';
    }
    exportStatus = "Settings loaded successfully";
    exportStatusError = false;
}

void ExportPanel::saveProjectSettings(EditorApplication* editorApp) {
    if (!editorApp) return;
    Haruka::Project* project = editorApp->getProject();
    if (!project) return;
    auto& config = project->getConfig();
    // Copiar los datos del panel a la config del proyecto
    config.name = gameName;
    config.version = version;
    config.exportSettings.author = author;
    config.exportSettings.description = description;
    config.shadersPath = shadersPath;
    // Guardar exportPath en config.outputPath
    config.outputPath = exportPath;
    project->save();
}

bool ExportPanel::validateSettings() const {
    return strlen(gameName) > 0 && strlen(version) > 0 && strlen(author) > 0;
}

void ExportPanel::performExport(EditorApplication* editorApp) {
    if (!editorApp) return;
    Haruka::Project* project = editorApp->getProject();
    if (!project) return;
    
    // Guardar los metadatos en el proyecto
    auto& config = project->getConfig();
    config.name = std::string(gameName);
    config.version = std::string(version);
    config.exportSettings.author = std::string(author);
    config.exportSettings.description = std::string(description);
    // Si engineBinary está vacío, usar ruta por defecto
    if (config.engineBinary.empty()) {
        std::string defaultEngineBin = editorApp->getProject()->getPath() + "/../../build/HarukaEngine";
        if (std::filesystem::exists(defaultEngineBin)) {
            config.engineBinary = std::filesystem::absolute(defaultEngineBin).string();
        } else if (std::filesystem::exists("/mnt/sdb1/haruka/build/HarukaEngine")) {
            config.engineBinary = "/mnt/sdb1/haruka/build/HarukaEngine";
        } else {
            config.engineBinary = "HarukaEngine";
        }
    }
    project->save();

    std::string projectPath = project->getPath();
    std::string exportFolder = config.outputPath;
    if (exportFolder.empty()) exportFolder = projectPath + "/export";
    try {
        // Crear directorios
        std::filesystem::create_directories(exportFolder);
        std::filesystem::create_directories(exportFolder + "/scenes");
        std::filesystem::create_directories(exportFolder + "/assets");

        // Copiar escenas
        std::filesystem::copy(projectPath + "/scenes", exportFolder + "/scenes", 
            std::filesystem::copy_options::overwrite_existing | 
            std::filesystem::copy_options::recursive);

        // Copiar assets
        std::filesystem::copy(projectPath + "/assets", exportFolder + "/assets", 
            std::filesystem::copy_options::overwrite_existing | 
            std::filesystem::copy_options::recursive);

        // Copiar shaders desde la ruta indicada en shadersPath, si existe
        if (strlen(shadersPath) > 0 && std::filesystem::exists(shadersPath)) {
            std::string destShaders = exportFolder + "/shaders";
            std::filesystem::create_directories(destShaders);
            std::filesystem::copy(shadersPath, destShaders,
                std::filesystem::copy_options::overwrite_existing |
                std::filesystem::copy_options::recursive);
        }

        // Copiar librería de lógica en export/ desde build
        std::filesystem::create_directories(exportFolder + "/");
        std::string logicLibSrc = projectPath + "/build/lib" + config.name + ".so";
        std::string logicLibDst = exportFolder + "/lib" + config.name + ".so";
        if (std::filesystem::exists(logicLibSrc)) {
            std::filesystem::copy(logicLibSrc, logicLibDst, std::filesystem::copy_options::overwrite_existing);
        } else {
            std::cerr << "No se encontró la librería lógica para exportar: " << logicLibSrc << std::endl;
        }

        // Copiar configuración
        std::filesystem::copy(projectPath + "/project.hrk", 
            exportFolder + "/project.hrk", 
            std::filesystem::copy_options::overwrite_existing);

        // Copiar ejecutable del motor (HarukaEngine)
        std::string engineBin = config.engineBinary;
        if (!engineBin.empty() && std::filesystem::exists(engineBin)) {
            std::filesystem::copy(engineBin, exportFolder + "/HarukaEngine", std::filesystem::copy_options::overwrite_existing);
        }

        // Crear script de arranque sin extensión (por ejemplo, 'run')
        std::string runScript = exportFolder + "/" + config.name;
        std::ofstream runFile(runScript);
        runFile << "#!/bin/bash\n";
        runFile << "DIR=\"$(dirname \"$0\")\"\n";
        runFile << "$DIR/HarukaEngine $DIR/project.hrk\n";
        runFile.close();
        std::filesystem::permissions(runScript, std::filesystem::perms::owner_exec | std::filesystem::perms::owner_write | std::filesystem::perms::owner_read);

        exportStatus = "✓ Game exported successfully to " + exportFolder;
        exportStatusError = false;
    } catch (const std::exception& e) {
        exportStatus = std::string("Export failed: ") + e.what();
        exportStatusError = true;
    }
}
