#include "menu_bar.h"
#include "editor_app.h"
#include <imgui.h>
#include <nfd.h>
#include <iostream>
#include <cstdio>

void MenuBar::render() {
    if (!ImGui::BeginMainMenuBar()) return;
    
    renderFileMenu();
    renderEditMenu();
    renderObjectsMenu();
    renderViewMenu();
    renderPlayControls();
    renderHelpMenu();
    
    ImGui::EndMainMenuBar();
}

void MenuBar::renderFileMenu() {
    if (!ImGui::BeginMenu("File")) return;
    
    if (ImGui::MenuItem("New Project")) {
        std::snprintf(editorApp->newProjectNameBuffer, sizeof(editorApp->newProjectNameBuffer), "NewProject");
        std::snprintf(editorApp->newProjectPathBuffer, sizeof(editorApp->newProjectPathBuffer), "/mnt/sdb1/haruka/projects/");
        editorApp->showNewProjectDialog = true;
    }

    if (ImGui::MenuItem("Open Project", "Ctrl+O")) {
        nfdchar_t* outPath = nullptr;
        nfdresult_t result = NFD_PickFolder(nullptr, &outPath);

        if (result == NFD_OKAY && editorApp->currentProject) {
            const std::string selectedPath(outPath);
            editorApp->currentProject->load(selectedPath);
            editorApp->projectBrowserPanel.setProject(editorApp->currentProject.get());
            editorApp->projectBrowserPanel.setScene(editorApp->currentScene.get());
            std::cout << "Project loaded: " << selectedPath << std::endl;
            free(outPath);
        } else if (result == NFD_CANCEL) {
            std::cout << "User cancelled folder selection" << std::endl;
        }
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Save", "Ctrl+S")) {
        if (!editorApp->currentFile.path.empty()) {
            editorApp->saveFile(editorApp->currentFile.path, editorApp->currentFile.isPrefab);
        } else {
            std::snprintf(editorApp->saveAsBuffer, sizeof(editorApp->saveAsBuffer), "scenes/Untitled.scene");
            editorApp->showSaveAsPopup = true;
        }
    }

    if (ImGui::MenuItem("Save As...")) {
        std::snprintf(editorApp->saveAsBuffer, sizeof(editorApp->saveAsBuffer), "%s", editorApp->currentFile.path.c_str());
        editorApp->showSaveAsPopup = true;
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Open File", "Ctrl+O")) {
        nfdchar_t* outPath = nullptr;
        nfdresult_t result = NFD_OpenDialog("scene,prefab", nullptr, &outPath);
        
        if (result == NFD_OKAY) {
            editorApp->loadFile(outPath);
            free(outPath);
        }
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Export Game")) {
        editorApp->exportGame();
    }

    if (ImGui::MenuItem("Exit", "Alt+F4")) {
        editorApp->_shouldClose = true;
    }

    ImGui::EndMenu();
}

void MenuBar::renderEditMenu() {
    if (!ImGui::BeginMenu("Edit")) return;
    
    if (ImGui::MenuItem("Move Gizmo", "W", editorApp->gizmoMode == 0)) editorApp->gizmoMode = 0;
    if (ImGui::MenuItem("Rotate Gizmo", "E", editorApp->gizmoMode == 1)) editorApp->gizmoMode = 1;
    if (ImGui::MenuItem("Scale Gizmo", "R", editorApp->gizmoMode == 2)) editorApp->gizmoMode = 2;
    
    ImGui::Separator();
    
    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editorApp->commandHistory.canUndo())) {
        editorApp->commandHistory.undo();
    }
    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, editorApp->commandHistory.canRedo())) {
        editorApp->commandHistory.redo();
    }
    
    ImGui::EndMenu();
}

void MenuBar::renderObjectsMenu() {
    if (!ImGui::BeginMenu("Objects")) return;

    if (ImGui::MenuItem("Cube")) {
        if (editorApp->currentScene) {
            editorApp->createSceneObject("Cube");
        }
    }
    if (ImGui::MenuItem("Sphere")) {
        if (editorApp->currentScene) {
            editorApp->createSceneObject("Sphere");
        }
    }
    if (ImGui::MenuItem("Plane")) {
        if (editorApp->currentScene) {
            editorApp->createSceneObject("Plane");
        }
    }
    if (ImGui::MenuItem("Capsule")) {
        if (editorApp->currentScene) {
            editorApp->createSceneObject("Capsule");
        }
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Light")) {
        if (editorApp->currentScene) {
            editorApp->createSceneObject("Light");
        }
    }
    if (ImGui::MenuItem("Point Light")) {
        if (editorApp->currentScene) {
            editorApp->createSceneObject("PointLight");
        }
    }
    if (ImGui::MenuItem("Directional Light")) {
        if (editorApp->currentScene) {
            editorApp->createSceneObject("DirectionalLight");
        }
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Sun (Sistema de unidades)")) {
        if (editorApp->currentScene) {
            editorApp->createSceneObject("SUN");
        }
    }
    if (ImGui::MenuItem("Planet (Sistema de unidades)")) {
        if (editorApp->currentScene) {
            editorApp->createSceneObject("PLANET");
        }
    }

    ImGui::EndMenu();
}

void MenuBar::renderViewMenu() {
    if (!ImGui::BeginMenu("View")) return;
    
    ImGui::MenuItem("Scene Hierarchy", nullptr, &editorApp->showSceneHierarchy);
    ImGui::MenuItem("Inspector", nullptr, &editorApp->showInspector);
    ImGui::MenuItem("Project Browser", nullptr, &editorApp->showProjectBrowser);
    ImGui::MenuItem("Viewport", nullptr, &editorApp->showViewport);
    ImGui::MenuItem("Console", nullptr, &editorApp->showConsole);
    ImGui::MenuItem("Performance Stats", nullptr, &editorApp->showStats);
    ImGui::MenuItem("Material Editor", nullptr, &editorApp->showMaterialEditor);
    
    ImGui::Separator();
    
    ImGui::MenuItem("Settings", nullptr, &editorApp->showSettings);
    ImGui::MenuItem("Asset Importer", nullptr, &editorApp->showAssetImporter);
    ImGui::MenuItem("Search Panel", nullptr, &editorApp->showSearchPanel);
    ImGui::MenuItem("UI Builder", nullptr, &editorApp->showUIBuilder);
    
    ImGui::Separator();
    ImGui::MenuItem("ImGui Demo", nullptr, &editorApp->showDemoWindow);
    
    ImGui::EndMenu();
}

void MenuBar::renderPlayControls() {
    if (ImGui::Button("Compile Project", ImVec2(150, 0))) {
        editorApp->compileProject();
    }

    if (editorApp->isProjectCompiling) {
        ImGui::SameLine();
        ImGui::Text("Compiling...");
    }

    if (ImGui::MenuItem((!editorApp->isPlayMode) ? "Start" : "Stop", "F5", editorApp->isPlayMode)) {
        if (!editorApp->isPlayMode) {
            editorApp->enterPlayMode();
        } else {
            editorApp->exitPlayMode();
        }
    }
}

void MenuBar::renderHelpMenu() {
    if (!ImGui::BeginMenu("Help")) return;
    
    if (ImGui::MenuItem("About")) {
        std::cout << "Haruka Engine Editor v0.1" << std::endl;
    }
    
    ImGui::EndMenu();
}
