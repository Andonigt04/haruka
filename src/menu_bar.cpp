#include "menu_bar.h"
#include "editor_app.h"
#include <imgui.h>
#include <nfd.h>
#include <SDL3/SDL.h>
#include <iostream>
#include <cstdio>

void MenuBar::render() {
    if (!ImGui::BeginMainMenuBar()) return;
    
    renderFileMenu();
    renderEditMenu();
    // Menús del motor (Haruka: "Objects", "Material"), entre Edit y View.
    if (editorApp->backend) editorApp->backend->drawMainMenus();
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
        if (result == NFD_OKAY) {
            editorApp->openProject(outPath);
            free(outPath);
        } else if (result == NFD_CANCEL) {
            std::cout << "User cancelled folder selection" << std::endl;
        }
    }

    ImGui::Separator();

    if (ImGui::MenuItem("Save", "Ctrl+S")) {
        if (!editorApp->currentFile.path.empty()) {
            editorApp->saveFile(editorApp->currentFile.path);
        } else {
            const std::string untitled = editorApp->backend ? editorApp->backend->untitledDocument() : std::string{};
            std::snprintf(editorApp->saveAsBuffer, sizeof(editorApp->saveAsBuffer), "%s", untitled.c_str());
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
        const std::string filter = editorApp->backend ? editorApp->backend->documentFilter() : std::string{};
        nfdresult_t result = NFD_OpenDialog(filter.empty() ? nullptr : filter.c_str(), nullptr, &outPath);

        if (result == NFD_OKAY) {
            editorApp->openDocument(outPath);
            free(outPath);
        }
    }

    ImGui::Separator();

    if (editorApp->backend) {
        editorApp->backend->drawMenuItems("File");   // Haruka: "Export Game"
        ImGui::Separator();
    }

    if (ImGui::MenuItem("Exit", "Alt+F4")) {
        SDL_Event quitEvent;
        quitEvent.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&quitEvent);
    }

    ImGui::EndMenu();
}

void MenuBar::renderEditMenu() {
    if (!ImGui::BeginMenu("Edit")) return;

    if (ImGui::MenuItem("Undo", "Ctrl+Z", false, editorApp->commandHistory.canUndo())) {
        editorApp->commandHistory.undo();
    }
    if (ImGui::MenuItem("Redo", "Ctrl+Y", false, editorApp->commandHistory.canRedo())) {
        editorApp->commandHistory.redo();
    }
    if (editorApp->backend) {
        ImGui::Separator();
        editorApp->backend->drawMenuItems("Edit");   // Haruka: modo del gizmo
    }

    ImGui::EndMenu();
}

void MenuBar::renderViewMenu() {
    if (!ImGui::BeginMenu("View")) return;

    // Una entrada por panel registrado (EditorApplication::registerPanels): un panel nuevo sale
    // aquí sin tocar este fichero.
    editorApp->panels.drawViewMenuItems();

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

    IEngineBackend* be = editorApp->backend.get();
    const bool playing = be && be->isPlaying();
    if (ImGui::MenuItem(playing ? "Stop" : "Start", "F5", playing, be && (playing || be->canPlay()))) {
        if (!playing) be->enterPlay();
        else          be->exitPlay();
    }
}

void MenuBar::renderHelpMenu() {
    if (!ImGui::BeginMenu("Help")) return;
    
    if (ImGui::MenuItem("About")) {
        std::cout << "Haruka Engine Editor v0.1" << std::endl;
    }
    
    ImGui::EndMenu();
}
