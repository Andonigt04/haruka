#include "export_panel.h"
#include "editor_app.h"
#include "engine_sdk.h"   // runtime del motor activo, para copiarlo al export

#include <imgui.h>
#include <nfd.h>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <system_error>
#include <algorithm>

namespace fs = std::filesystem;

namespace {
struct FileCopyPlan {
    fs::path sourceRoot;
    fs::path destinationRoot;
    std::vector<fs::path> files;
    size_t cursor = 0;
    std::string label;
};
}

struct ExportPanel::ExportBatchState {
    Haruka::Project* project = nullptr;
    fs::path projectPath;
    fs::path exportFolder;
    fs::path shadersSource;
    std::string engineBinary;
    std::string logicLibSrc;
    std::string logicLibDst;
    std::string runScriptPath;
    std::vector<FileCopyPlan> plans;
    size_t planCursor = 0;
    std::string status;
    bool ready = false;
};

ExportPanel::ExportTask::ExportTask(ExportPanel* panel)
    : EditorTaskBase("Export Game"), owner(panel) {}

bool ExportPanel::ExportTask::onStart(std::string& error) {
    if (!owner) {
        error = "Export panel unavailable.";
        return false;
    }
    if (!owner->prepareExport(owner->pendingEditorApp, error)) {
        return false;
    }
    return true;
}

void ExportPanel::ExportTask::onUpdate() {
    if (!owner) {
        fail("Export panel unavailable.");
        return;
    }
    owner->updateExportTask();
}

void ExportPanel::ExportTask::onCancel() {
    if (owner) {
        owner->setExportError("Export cancelled by user.");
        owner->finishExportTask();
    }
}

ExportPanel::ExportPanel() {
    strcpy(version, "1.0.0");
}

ExportPanel::~ExportPanel() = default;

void ExportPanel::update() {
    if (activeTask && activeTask->isRunning()) {
        activeTask->update();
    }
    if (activeTask && activeTask->isDone()) {
        activeTask.reset();
    }
}

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
                startExportTask(editorApp);
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
        if (activeTask && activeTask->isRunning()) {
            ImGui::Text("Export running...");
            ImGui::ProgressBar(activeTask->getProgress(), ImVec2(-1.0f, 0.0f));
            ImGui::TextWrapped("%s", activeTask->getStatus().c_str());
            if (ImGui::Button("Cancel Export")) {
                activeTask->requestCancel();
            }
            ImGui::Separator();
        }
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

bool ExportPanel::prepareExport(EditorApplication* editorApp, std::string& error) {
    if (!editorApp) {
        error = "Editor application unavailable.";
        return false;
    }
    Haruka::Project* project = editorApp->getProject();
    if (!project) {
        error = "No project loaded.";
        return false;
    }

    auto& config = project->getConfig();
    config.name = std::string(gameName);
    config.version = std::string(version);
    config.exportSettings.author = std::string(author);
    config.exportSettings.description = std::string(description);

    if (config.engineBinary.empty()) {
        // Runtime que se copia junto al juego exportado. Primero el build del propio proyecto
        // y si no, el que declare el motor activo (EngineSdk); antes había aquí una ruta
        // absoluta de la máquina de desarrollo, que en cualquier otra apunta a la nada.
        std::string defaultEngineBin = project->getPath() + "/../../build/HarukaEngine";
        if (std::filesystem::exists(defaultEngineBin)) {
            config.engineBinary = std::filesystem::absolute(defaultEngineBin).string();
        } else if (const EngineSdk& engine = EngineSdk::current(); !engine.runtimeBinary.empty()) {
            config.engineBinary = engine.runtimeBinary;
        } else {
            config.engineBinary = "HarukaEngine";   // se busca en el PATH
        }
    }
    project->save();

    batchState = std::make_unique<ExportBatchState>();
    batchState->project = project;
    batchState->projectPath = project->getPath();
    batchState->exportFolder = config.outputPath.empty() ? fs::path(batchState->projectPath) / "export" : fs::path(config.outputPath);
    batchState->shadersSource = shadersPath;
    batchState->engineBinary = config.engineBinary;
    batchState->logicLibSrc = (batchState->projectPath / "build" / ("lib" + config.name + ".so")).string();
    batchState->logicLibDst = (batchState->exportFolder / ("lib" + config.name + ".so")).string();
    batchState->runScriptPath = (batchState->exportFolder / config.name).string();

    try {
        fs::create_directories(batchState->exportFolder);
        batchState->plans.clear();

        auto collectFiles = [](const fs::path& root) {
            std::vector<fs::path> result;
            if (!fs::exists(root)) return result;
            for (const auto& entry : fs::recursive_directory_iterator(root)) {
                if (entry.is_regular_file()) result.push_back(entry.path());
            }
            return result;
        };

        batchState->plans.push_back({batchState->projectPath / "scenes", batchState->exportFolder / "scenes", collectFiles(batchState->projectPath / "scenes"), 0, "Scenes"});
        batchState->plans.push_back({batchState->projectPath / "assets", batchState->exportFolder / "assets", collectFiles(batchState->projectPath / "assets"), 0, "Assets"});
        if (!batchState->shadersSource.empty() && fs::exists(batchState->shadersSource)) {
            batchState->plans.push_back({batchState->shadersSource, batchState->exportFolder / "shaders", collectFiles(batchState->shadersSource), 0, "Shaders"});
        }

        exportStatus = "Export prepared.";
        exportStatusError = false;
        batchState->ready = true;
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        batchState.reset();
        return false;
    }
}

void ExportPanel::setExportError(const std::string& message) {
    exportStatus = message;
    exportStatusError = true;
}

void ExportPanel::startExportTask(EditorApplication* editorApp) {
    if (activeTask && activeTask->isRunning()) {
        exportStatus = "Export already running.";
        exportStatusError = true;
        return;
    }

    pendingEditorApp = editorApp;
    activeTask = std::make_unique<ExportTask>(this);
    if (!activeTask->start()) {
        exportStatus = activeTask->getStatus();
        exportStatusError = true;
        pendingEditorApp = nullptr;
    }
}

void ExportPanel::updateExportTask() {
    if (!batchState || !batchState->ready) {
        return;
    }

    constexpr size_t kFilesPerFrame = 80;
    size_t totalFiles = 0;
    size_t doneFiles = 0;
    for (const auto& plan : batchState->plans) {
        totalFiles += plan.files.size();
        doneFiles += std::min(plan.cursor, plan.files.size());
    }

    if (batchState->planCursor < batchState->plans.size()) {
        auto& plan = batchState->plans[batchState->planCursor];
        fs::create_directories(plan.destinationRoot);
        size_t processed = 0;
        while (plan.cursor < plan.files.size() && processed < kFilesPerFrame) {
            const auto& sourceFile = plan.files[plan.cursor++];
            auto relative = fs::relative(sourceFile, plan.sourceRoot);
            auto destinationFile = plan.destinationRoot / relative;
            fs::create_directories(destinationFile.parent_path());
            std::error_code ec;
            fs::copy_file(sourceFile, destinationFile, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                setExportError(std::string("Failed to copy ") + sourceFile.string() + ": " + ec.message());
                if (activeTask) activeTask->reportFail(exportStatus);
                finishExportTask();
                return;
            }
            processed++;
            doneFiles++;
        }

        if (plan.cursor >= plan.files.size()) {
            batchState->planCursor++;
        }

        float stageRatio = batchState->plans.empty() ? 1.0f : static_cast<float>(batchState->planCursor) / static_cast<float>(batchState->plans.size());
        float fileRatio = totalFiles == 0 ? 1.0f : static_cast<float>(doneFiles) / static_cast<float>(totalFiles);
        if (batchState->planCursor < batchState->plans.size()) {
            exportStatus = "Copying " + batchState->plans[batchState->planCursor].label + "...";
        } else {
            exportStatus = "Finalizing export...";
        }
        if (activeTask) {
            activeTask->reportProgress(std::clamp(0.15f + fileRatio * 0.55f + stageRatio * 0.20f, 0.0f, 0.95f), exportStatus);
        }
        return;
    }

    try {
        if (!batchState->logicLibSrc.empty() && fs::exists(batchState->logicLibSrc)) {
            fs::create_directories(fs::path(batchState->logicLibDst).parent_path());
            fs::copy_file(batchState->logicLibSrc, batchState->logicLibDst, fs::copy_options::overwrite_existing);
        }

        if (!batchState->engineBinary.empty() && fs::exists(batchState->engineBinary)) {
            fs::copy_file(batchState->engineBinary, batchState->exportFolder / "HarukaEngine", fs::copy_options::overwrite_existing);
        }

        fs::copy_file(batchState->projectPath / "project.hrk", batchState->exportFolder / "project.hrk", fs::copy_options::overwrite_existing);

        std::ofstream runFile(batchState->runScriptPath);
        runFile << "#!/bin/bash\n";
        runFile << "DIR=\"$(dirname \"$0\")\"\n";
        runFile << "$DIR/HarukaEngine $DIR/project.hrk\n";
        runFile.close();
        fs::permissions(batchState->runScriptPath, fs::perms::owner_exec | fs::perms::owner_write | fs::perms::owner_read);

        exportStatus = "✓ Game exported successfully to " + batchState->exportFolder.string();
        exportStatusError = false;
        if (activeTask) activeTask->reportComplete(exportStatus);
    } catch (const std::exception& e) {
        setExportError(std::string("Export failed: ") + e.what());
        if (activeTask) activeTask->reportFail(exportStatus);
    }

    finishExportTask();
}

void ExportPanel::finishExportTask() {
    pendingEditorApp = nullptr;
    batchState.reset();
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
    startExportTask(editorApp);
}
