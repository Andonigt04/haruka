#define IMGUI_IMPL_OPENGL_LOADER_GLAD

#include "editor_app.h"
#include "core/camera.h"
#include "core/error_reporter.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nfd.h>
#include <dlfcn.h>

#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>

EditorApplication::EditorApplication() : window(nullptr) {}

EditorApplication::~EditorApplication() {
    stopMotorProcess();
    shutdown();
}

void EditorApplication::init() {
    // ===== GLFW & GLAD Setup =====
    if (!glfwInit()) {
        HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "Failed to initialize GLFW in Editor");
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, "Haruka Editor", nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // ===== ImGui Setup =====
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    // ===== Scene & Project Setup =====
    currentScene = std::make_unique<Haruka::Scene>("Untitled");
    currentProject = std::make_unique<Haruka::Project>();
    currentFile.path = "scenes/Untitled.scene";
    currentFile.name = "Untitled";
    currentFile.isPrefab = false;

    // ===== Panels Setup =====
    sceneHierarchyPanel.setScene(currentScene.get());
    sceneHierarchyPanel.setCommandHistory(&commandHistory);
    inspectorPanel.setScene(currentScene.get());
    inspectorPanel.setCommandHistory(&commandHistory);
    inspectorPanel.setOnSceneChanged([this]() { sceneDirty = true; });
    projectBrowserPanel.setProject(currentProject.get());
    projectBrowserPanel.setScene(currentScene.get());
    materialEditorPanel.setScene(currentScene.get());
    
    // ===== Camera Setup =====
    viewportCamera = std::make_unique<Camera>(Haruka::WorldPos(0.0f, 5.0f, 15.0f));
    glm::quat initialOrientation = glm::angleAxis(glm::radians(0.0f), glm::vec3(0, 1, 0));
    viewportCamera->orientation = initialOrientation;

    viewportPanel.setScene(currentScene.get());
    viewportPanel.setCamera(viewportCamera.get());
    viewportPanel.setGLFWWindow(window);
    viewportPanel.setStatsPanel(&statsPanel);

    editorCamPos = viewportCamera->position;
    editorCamRot = viewportCamera->orientation;
    viewportPanel.setPlayMode(false);

    // ===== Callbacks Setup =====
    projectBrowserPanel.setOnFileLoad([this](const std::string& path) {
        if (sceneDirty) {
            pendingSceneToLoad = path;
            showUnsavedChangesPopup = true;
        } else {
            loadFile(path);
        }
    });

    sceneHierarchyPanel.setOnObjectSelectedByIndex([this](int index) {
        inspectorPanel.setSelectedObjectIndex(index);
    });

    sceneHierarchyPanel.setOnObjectSelectedByName([this](const std::string& name) {
        materialEditorPanel.setSelectedObject(name);
    });

    // ===== Stream Capture Setup =====
    coutCapture = std::make_unique<StreamCapture>(std::cout, &consolePanel, LogLevel::Info);
    cerrCapture = std::make_unique<StreamCapture>(std::cerr, &consolePanel, LogLevel::Error);

    // Initialize panels
    settingsPanel.load();
    
    // Setup default scene object
    if (currentScene) {
        Haruka::SceneObject cube;
        cube.name = "Cube_Default";
        cube.type = "Cube";
        cube.position = glm::dvec3(0, 0, 0);
        cube.rotation = glm::dvec3(0, 0, 0);
        cube.scale = glm::dvec3(1.0, 1.0, 1.0);
        currentScene->addObject(cube);
        std::cout << "✓ Default cube added to scene" << std::endl;
    }
    
    // Iniciar motor sub-proceso
    startMotorProcess();
    
    std::cout << "✓ Haruka Editor initialized" << std::endl;
}

void EditorApplication::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}

void EditorApplication::run() {
    init();
    while (!glfwWindowShouldClose(window)) {
        update();
        render();
    }
}

void EditorApplication::update() {
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    statsPanel.update(deltaTime);
    glfwPollEvents();
    updatePlayMode(deltaTime);
    
    // Auto-save system
    if (autoSaveEnabled && sceneDirty && !currentFile.path.empty()) {
        timeSinceLastSave += deltaTime;
        if (timeSinceLastSave >= autoSaveInterval) {
            saveFile(currentFile.path, currentFile.isPrefab);
            timeSinceLastSave = 0.0f;
        }
    }
    
    viewportPanel.onUpdate(deltaTime);
}

void EditorApplication::updatePlayMode(float deltaTime) {
    if (!isPlayMode) return;
    
    playModeTime += deltaTime;
    
    // Llamar al callback de actualización si existe
    if (gameInterface && gameInterface->onUpdate) {
        gameInterface->onUpdate(window, deltaTime);
    }
}

void EditorApplication::render() {
    // Update window title with dirty flag
    std::string title = "Haruka Editor";
    if (!currentFile.path.empty()) {
        title += " - " + currentFile.name + " (" + getFileType(currentFile.path) + ")";
    }
    if (sceneDirty) title += " *";
    glfwSetWindowTitle(window, title.c_str());

    // ImGui frame setup
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    renderUI();

    // Render
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Handle multi-viewport
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }

    glfwSwapBuffers(window);
}

void EditorApplication::renderUI() {
    // Setup DockSpace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse;
    window_flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    showMenuBar();
    ImGui::End();

    if (isPlayMode) ImGui::BeginDisabled();

    // Project Browser como panel principal
    projectBrowserPanel.onImGuiRender();
        
    // Scene hierarchy e inspector (independientes)
    sceneHierarchyPanel.onImGuiRender();
    
    if (sceneHierarchyPanel.getSelectedObjectIndex() >= 0) {
        inspectorPanel.setSelectedObjectIndex(sceneHierarchyPanel.getSelectedObjectIndex());
    }
    
    inspectorPanel.onImGuiRender();
    consolePanel.onImGuiRender();
    statsPanel.onImGuiRender();

    if (isPlayMode) ImGui::EndDisabled();

    viewportPanel.onImGuiRender();
    viewportPanel.setGizmoMode(gizmoMode);

    if (showDemoWindow) {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }
    
    // New panels
    settingsPanel.onImGuiRender();
    assetImporter.onImGuiRender();
    searchPanel.onImGuiRender();
    scriptingEditor.onImGuiRender();
    uiBuilder.onImGuiRender();

    // Save As popup (fuera del menú)
    if (showSaveAsPopup) ImGui::OpenPopup("Save File As");
    if (ImGui::BeginPopupModal("Save File As", &showSaveAsPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Path##save", saveAsBuffer, sizeof(saveAsBuffer));
        bool isPrefab = std::string(saveAsBuffer).find(".prefab") != std::string::npos;
        
        if (ImGui::Button("Save")) {
            saveFile(saveAsBuffer, isPrefab);
            showSaveAsPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            showSaveAsPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Unsaved changes popup
    if (showUnsavedChangesPopup) ImGui::OpenPopup("Unsaved Changes");
    if (ImGui::BeginPopupModal("Unsaved Changes", &showUnsavedChangesPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Hay cambios sin guardar.");
        if (ImGui::Button("Guardar y continuar")) {
            if (!currentFile.path.empty()) saveFile(currentFile.path, currentFile.isPrefab);
            if (!pendingSceneToLoad.empty()) loadFile(pendingSceneToLoad);
            pendingSceneToLoad.clear();
            showUnsavedChangesPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Descartar")) {
            if (!pendingSceneToLoad.empty()) loadFile(pendingSceneToLoad);
            pendingSceneToLoad.clear();
            showUnsavedChangesPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar")) {
            pendingSceneToLoad.clear();
            showUnsavedChangesPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // New Project popup
    if (showNewProjectDialog) ImGui::OpenPopup("New Project");
    if (ImGui::BeginPopupModal("New Project", &showNewProjectDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Project Name", newProjectNameBuffer, sizeof(newProjectNameBuffer));
        ImGui::InputText("Project Path", newProjectPathBuffer, sizeof(newProjectPathBuffer));
        
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            createNewProject(newProjectNameBuffer, newProjectPathBuffer);
            showNewProjectDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            showNewProjectDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorApplication::showMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Project")) {
            std::snprintf(newProjectNameBuffer, sizeof(newProjectNameBuffer), "NewProject");
            std::snprintf(newProjectPathBuffer, sizeof(newProjectPathBuffer), "/mnt/sdb1/haruka/projects/");
            showNewProjectDialog = true;
        }

        if (ImGui::MenuItem("Open Project", "Ctrl+O")) {
            nfdchar_t* outPath = nullptr;
            nfdresult_t result = NFD_PickFolder(nullptr, &outPath);

            if (result == NFD_OKAY && currentProject) {
                const std::string selectedPath(outPath);
                currentProject->load(selectedPath);
                projectBrowserPanel.setProject(currentProject.get());
                projectBrowserPanel.setScene(currentScene.get());
                std::cout << "Project loaded: " << selectedPath << std::endl;
                free(outPath);
            } else if (result == NFD_CANCEL) {
                std::cout << "User cancelled folder selection" << std::endl;
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            if (!currentFile.path.empty()) {
                saveFile(currentFile.path, currentFile.isPrefab);
            } else {
                std::snprintf(saveAsBuffer, sizeof(saveAsBuffer), "scenes/Untitled.scene");
                showSaveAsPopup = true;
            }
        }

        if (ImGui::MenuItem("Save As...")) {
            std::snprintf(saveAsBuffer, sizeof(saveAsBuffer), "%s", currentFile.path.c_str());
            showSaveAsPopup = true;
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Open File", "Ctrl+O")) {
            nfdchar_t* outPath = nullptr;
            nfdresult_t result = NFD_OpenDialog("scene,prefab", nullptr, &outPath);
            
            if (result == NFD_OKAY) {
                loadFile(outPath);
                free(outPath);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Export Game")) {
            exportGame();
        }

        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            glfwSetWindowShouldClose(window, true);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Move Gizmo", "W", gizmoMode == 0)) gizmoMode = 0;
        if (ImGui::MenuItem("Rotate Gizmo", "E", gizmoMode == 1)) gizmoMode = 1;
        if (ImGui::MenuItem("Scale Gizmo", "R", gizmoMode == 2)) gizmoMode = 2;
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("Undo", "Ctrl+Z", false, commandHistory.canUndo())) {
            commandHistory.undo();
        }
        if (ImGui::MenuItem("Redo", "Ctrl+Y", false, commandHistory.canRedo())) {
            commandHistory.redo();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Scene Hierarchy", nullptr, true);
        ImGui::MenuItem("Inspector", nullptr, true);
        ImGui::MenuItem("Project Browser", nullptr, true);
        ImGui::MenuItem("Console", nullptr, true);
        ImGui::MenuItem("Performance Stats", nullptr, true);
        ImGui::Separator();
        ImGui::MenuItem("ImGui Demo", nullptr, &showDemoWindow);
        ImGui::EndMenu();
    }

    if (ImGui::Button("Compile Project", ImVec2(150, 0))) {
        compileProject();
    }

    if (isProjectCompiling) {
        ImGui::SameLine();
        ImGui::Text("Compiling...");
    }

    if (ImGui::MenuItem((!isPlayMode) ? "Start" : "Stop", "F5", isPlayMode)) {
        if (!isPlayMode) {
            enterPlayMode();
        } else {
            exitPlayMode();
        }
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About")) {
            std::cout << "Haruka Engine Editor v0.1" << std::endl;
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void EditorApplication::enterPlayMode() {
    if (isPlayMode || !currentProject) return;
    
    isPlayMode = true;
    playModeTime = 0.0f;
    
    viewportPanel.setPlayMode(true);
    inspectorPanel.setPlayMode(true);

    // Cargar escena de inicio
    std::string projectPath = currentProject->getPath();
    std::string projectConfigPath = projectPath + "/project.hrk";
    
    std::ifstream configFile(projectConfigPath);
    if (configFile.is_open()) {
        nlohmann::json projectConfig;
        configFile >> projectConfig;
        configFile.close();
        
        if (projectConfig.contains("startScene")) {
            std::string startScenePath = projectConfig["startScene"].get<std::string>();
            std::string fullScenePath = projectPath + "/" + startScenePath;
            
            if (currentScene) {
                currentScene->save(playModeBackupPath);
            }
            
            currentScene->load(fullScenePath);
            std::cout << "Scene loaded: " << startScenePath << std::endl;
        }
    }

    // Cargar la librería dinámicamente
    std::string libPath = projectPath + "/build/libTestGameLogic.so";
    gameLibHandle = dlopen(libPath.c_str(), RTLD_LAZY);
    
    if (gameLibHandle) {
        // Obtener el interfaz de juego (COMPLETAMENTE GENÉRICO)
        typedef Haruka::GameInterface* (*GetGameInterfaceFunc)();
        GetGameInterfaceFunc getGameInterface = (GetGameInterfaceFunc)dlsym(gameLibHandle, "getGameInterface");
        
        if (getGameInterface) {
            gameInterface = getGameInterface();
            
            if (gameInterface) {
                std::cout << "✓ Game interface loaded: " << (gameInterface->name ? gameInterface->name : "Unknown") << std::endl;
                
                // Ejecutar inicialización
                if (gameInterface->onInit) {
                    gameInterface->onInit(currentScene.get());
                    std::cout << "✓ Game initialized" << std::endl;
                }
                
                // Establecer cámara
                if (gameInterface->getCamera) {
                    Camera* gameCamera = gameInterface->getCamera();
                    if (gameCamera) {
                        viewportPanel.setCamera(gameCamera);
                        std::cout << "✓ Game camera set" << std::endl;
                    }
                }
            }
        } else {
            std::cout << "⚠ getGameInterface not found, project may not implement it" << std::endl;
        }
    } else {
        std::cerr << "✗ Could not load project library: " << dlerror() << std::endl;
    }

    std::cout << "▶ Play Mode started" << std::endl;
}

void EditorApplication::exitPlayMode() {
    if (!isPlayMode) return;
    
    isPlayMode = false;
    
    // Ejecutar shutdown del juego si existe
    if (gameInterface && gameInterface->onShutdown) {
        gameInterface->onShutdown();
    }
    
    gameInterface = nullptr;
    
    // Cerrar librería si está cargada
    if (gameLibHandle) {
        dlclose(gameLibHandle);
        gameLibHandle = nullptr;
    }

    if (std::filesystem::exists(playModeBackupPath)) {
        currentScene->load(playModeBackupPath);
    }
    
    viewportPanel.setCamera(viewportCamera.get());
    
    viewportPanel.setPlayMode(false);
    inspectorPanel.setPlayMode(false);
    
    std::cout << "⏹ Play Mode stopped" << std::endl;
}

void EditorApplication::createNewProject(const std::string& name, const std::string& basePath) {
    if (name.empty() || basePath.empty()) {
        std::cerr << "Project name and path cannot be empty" << std::endl;
        return;
    }

    try {
        // Crear estructura del proyecto
        std::string projectPath = basePath + name;
        std::filesystem::create_directories(projectPath + "/scripts");
        std::filesystem::create_directories(projectPath + "/scenes");
        std::filesystem::create_directories(projectPath + "/assets");
        std::filesystem::create_directories(projectPath + "/prefabs");

        // Crear archivo project.hrk (config JSON)
        // Obtener fecha actual
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream dateStream;
        dateStream << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        
        nlohmann::json projectConfig;
        projectConfig["name"] = name;
        projectConfig["version"] = "0.1.0";
        projectConfig["created"] = dateStream.str();
        projectConfig["startScene"] = "scenes/main.scene";

        std::ofstream configFile(projectPath + "/project.hrk");
        if (configFile.is_open()) {
            configFile << projectConfig.dump(2);
            configFile.close();
        }

        // Crear CMakeLists.txt
        std::string cmakelists = R"(cmake_minimum_required(VERSION 3.14)
project()" + name + R"()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Rutas
set(ENGINE_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../build")
set(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Buscar librería del engine
find_library(HarukaEngineLib_LIBRARY 
    NAMES HarukaEngineLib
    PATHS 
        ${ENGINE_BUILD_DIR}
        ${ENGINE_BUILD_DIR}/lib
    NO_DEFAULT_PATH
)

if(NOT HarukaEngineLib_LIBRARY)
    message(FATAL_ERROR "HarukaEngineLib not found. Compile the engine first.")
endif()

find_path(HarukaEngineLib_INCLUDE_DIR NAMES core/scene.h PATHS ${ENGINE_ROOT}/src NO_DEFAULT_PATH REQUIRED)

include_directories(
    ${HarukaEngineLib_INCLUDE_DIR}
    ${ENGINE_ROOT}/third_party/glm
    ${ENGINE_ROOT}/third_party/json/include
)

# Compilar scripts del proyecto
file(GLOB_RECURSE PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.cpp"
)

add_library()" + name + R"(Logic SHARED ${PROJECT_SOURCES})
target_link_libraries()" + name + R"(Logic PRIVATE ${HarukaEngineLib_LIBRARY})
)";

        std::ofstream cmakeFile(projectPath + "/CMakeLists.txt");
        if (cmakeFile.is_open()) {
            cmakeFile << cmakelists;
            cmakeFile.close();
        }

        // Crear init.h
        std::string initH = R"(#pragma once

#include "core/scene.h"
#include "core/game_interface.h"
#include <memory>

namespace GameLogic {
    // Definir el interfaz de juego
    extern Haruka::GameInterface gameInterface;
}
)";

        std::ofstream initHFile(projectPath + "/scripts/init.h");
        if (initHFile.is_open()) {
            initHFile << initH;
            initHFile.close();
        }

        // Crear init.cpp base
        std::string initCpp = R"(#include "init.h"
#include <iostream>

// Variables globales
Haruka::Camera* g_gameCamera = nullptr;

// Callbacks del juego
void gameOnInit(Haruka::Scene* scene) {
    if (!scene) return;
    std::cout << "Game initialized" << std::endl;
}

void gameOnUpdate(GLFWwindow* window, float deltaTime) {
    // Implementar lógica del juego aquí
}

Haruka::Camera* gameGetCamera() {
    return g_gameCamera;
}

void gameOnShutdown() {
    std::cout << "Game shutting down..." << std::endl;
}

// Interfaz de juego - EXPORT para que el editor lo cargue
namespace GameLogic {
    Haruka::GameInterface gameInterface = {
        .onInit = gameOnInit,
        .onUpdate = gameOnUpdate,
        .onShutdown = gameOnShutdown,
        .getCamera = gameGetCamera,
        .getScene = nullptr,
        .name = ")" + name + R"(",
        .version = "1.0.0"
    };
}

// Función que el editor carga dinámicamente
extern "C" {
    Haruka::GameInterface* getGameInterface() {
        return &GameLogic::gameInterface;
    }
}
)";

        std::ofstream initCppFile(projectPath + "/scripts/init.cpp");
        if (initCppFile.is_open()) {
            initCppFile << initCpp;
            initCppFile.close();
        }

        // Crear game_globals.h
        std::string gameGlobals = R"(#pragma once

#include "core/camera.h"
#include "game/character.h"

// Globales del juego
extern Haruka::Camera* g_gameCamera;
)";

        std::ofstream globalsFile(projectPath + "/scripts/game_globals.h");
        if (globalsFile.is_open()) {
            globalsFile << gameGlobals;
            globalsFile.close();
        }

        // Crear escena por defecto
        if (!currentScene) {
            currentScene = std::make_unique<Haruka::Scene>("main");
        } else {
            currentScene->setName("main");
        }

        // Guardar escena inicial
        std::string scenePath = projectPath + "/scenes/main.scene";
        currentScene->save(scenePath);

        // Cargar proyecto
        if (!currentProject) {
            currentProject = std::make_unique<Haruka::Project>();
        }
        currentProject->load(projectPath);

        currentFile.path = scenePath;
        currentFile.name = "main";
        currentFile.isPrefab = false;
        sceneDirty = false;

        // Sincronizar paneles
        projectBrowserPanel.setProject(currentProject.get());
        projectBrowserPanel.setScene(currentScene.get());
        sceneHierarchyPanel.setScene(currentScene.get());
        inspectorPanel.setScene(currentScene.get());
        viewportPanel.setScene(currentScene.get());

        std::cout << "✓ Project created: " << projectPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Failed to create project: " << e.what() << std::endl;
    }
}

void EditorApplication::compileProject() {
    if (!currentProject || isProjectCompiling) return;
    
    std::string projectPath = currentProject->getPath();
    
    if (projectPath.empty()) {
        std::cerr << "✗ No project loaded. Open a project first." << std::endl;
        isProjectCompiling = false;
        return;
    }
    
    isProjectCompiling = true;
    
    std::cout << "Compiling project: " << projectPath << std::endl;
    
    // Compilar el proyecto
    std::string compileCmd = "cd " + projectPath + " && mkdir -p build && cd build && cmake .. && make -j$(nproc)";
    int result = system(compileCmd.c_str());
    
    if (result == 0) {
        std::cout << "✓ Project compiled successfully" << std::endl;
    } else {
        std::cerr << "✗ Project compilation failed" << std::endl;
    }
    
    isProjectCompiling = false;
}

void EditorApplication::exportGame() {
    if (!currentProject) {
        std::cerr << "No project loaded" << std::endl;
        return;
    }

    std::string projectPath = currentProject->getPath();
    std::string exportPath = projectPath + "/export";
    
    // Crear directorio export
    std::filesystem::create_directories(exportPath);
    std::filesystem::create_directories(exportPath + "/scenes");
    std::filesystem::create_directories(exportPath + "/assets");
    
    // Copiar escenas
    std::filesystem::copy(projectPath + "/scenes", exportPath + "/scenes", 
        std::filesystem::copy_options::overwrite_existing | 
        std::filesystem::copy_options::recursive);
    
    // Copiar assets
    std::filesystem::copy(projectPath + "/assets", exportPath + "/assets", 
        std::filesystem::copy_options::overwrite_existing | 
        std::filesystem::copy_options::recursive);
    
    // Copiar librería
    std::filesystem::copy(projectPath + "/build/libTestGameLogic.so", 
        exportPath + "/libTestGameLogic.so", 
        std::filesystem::copy_options::overwrite_existing);
    
    // Copiar configuración
    std::filesystem::copy(projectPath + "/project.hrk", 
        exportPath + "/project.hrk", 
        std::filesystem::copy_options::overwrite_existing);
    
    std::cout << "✓ Game exported to: " << exportPath << std::endl;
}

void EditorApplication::saveFile(const std::string& path, bool asPrefab) {
    if (!currentScene || path.empty()) return;
    
    // Detectar tipo por extensión, no por parámetro
    bool isPrefab = (path.find(".prefab") != std::string::npos);
    
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    // Crear backup si existe
    if (std::filesystem::exists(path)) {
        createFileBackup(path);
    }

    bool ok = currentScene->save(path);
    if (ok) {
        currentFile.path = path;
        currentFile.name = p.stem().string();
        currentFile.isPrefab = isPrefab;
        currentFile.lastSaveTime = glfwGetTime();
        sceneDirty = false;
        timeSinceLastSave = 0.0f;
        
        std::string type = isPrefab ? "Prefab" : "Scene";
        std::cout << "✓ " << type << " saved: " << path << std::endl;
    } else {
        std::cerr << "✗ Failed to save file: " << path << std::endl;
    }
}

void EditorApplication::loadFile(const std::string& path) {
    // Crear una nueva escena para cargar el archivo
    currentScene = std::make_unique<Haruka::Scene>();

    if (currentScene->load(path)) {
        // Actualizar estado del archivo
        currentFile.path = path;
        currentFile.name = std::filesystem::path(path).stem().string();
        currentFile.isPrefab = (path.find(".prefab") != std::string::npos);
        currentFile.lastSaveTime = glfwGetTime();
        
        // Resetear estado de cambios
        sceneDirty = false;
        timeSinceLastSave = 0.0f;
        
        // Sincronizar panels
        sceneHierarchyPanel.setScene(currentScene.get());
        inspectorPanel.setScene(currentScene.get());
        viewportPanel.setScene(currentScene.get());
        
        std::string type = currentFile.isPrefab ? "Prefab" : "Scene";
        std::cout << "✓ " << type << " loaded: " << path << std::endl;
    } else {
        std::cerr << "✗ Failed to load file: " << path << std::endl;
    }
}

void EditorApplication::createFileBackup(const std::string& filePath) {
    std::filesystem::path p(filePath);
    std::string backupDir = p.parent_path().string() + "/backups";
    std::filesystem::create_directories(backupDir);
    
    // Para prefabs: eliminar TODOS los backups anteriores
    bool isPrefab = (filePath.find(".prefab") != std::string::npos);
    if (isPrefab) {
        deleteAllBackups(filePath);
    }
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", std::localtime(&time));
    
    std::string ext = p.extension().string();
    std::string backupPath = backupDir + "/" + p.stem().string() + "_" + timestamp + ext;
    
    try {
        std::filesystem::copy_file(filePath, backupPath, 
            std::filesystem::copy_options::overwrite_existing);
        
        // Para escenas: mantener solo los últimos N backups
        if (!isPrefab) {
            cleanOldBackups(filePath);
        }
        
        std::cout << "✓ Backup created: " << backupPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Backup failed: " << e.what() << std::endl;
    }
}

void EditorApplication::cleanOldBackups(const std::string& filePath) {
    std::filesystem::path p(filePath);
    std::string backupDir = p.parent_path().string() + "/backups";
    std::string fileName = p.stem().string();
    std::string ext = p.extension().string();
    
    std::vector<std::filesystem::path> backups;
    for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
        if (entry.is_regular_file()) {
            std::string name = entry.path().stem().string();
            std::string entryExt = entry.path().extension().string();
            if (name.find(fileName) != std::string::npos && entryExt == ext) {
                backups.push_back(entry.path());
            }
        }
    }
    
    std::sort(backups.begin(), backups.end(), 
        [](const auto& a, const auto& b) {
            return std::filesystem::last_write_time(a) > 
                   std::filesystem::last_write_time(b);
        });
    
    if (backups.size() > (size_t)maxBackups) {
        for (size_t i = maxBackups; i < backups.size(); ++i) {
            std::filesystem::remove(backups[i]);
        }
    }
}

void EditorApplication::deleteAllBackups(const std::string& filePath) {
    std::filesystem::path p(filePath);
    std::string backupDir = p.parent_path().string() + "/backups";
    std::string fileName = p.stem().string();
    std::string ext = p.extension().string();
    
    try {
        if (!std::filesystem::exists(backupDir)) return;
        
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.is_regular_file()) {
                std::string name = entry.path().stem().string();
                std::string entryExt = entry.path().extension().string();
                if (name.find(fileName) != std::string::npos && entryExt == ext) {
                    std::filesystem::remove(entry.path());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "✗ Error deleting backups: " << e.what() << std::endl;
    }
}

std::string EditorApplication::getFileType(const std::string& path) {
    return (path.find(".prefab") != std::string::npos) ? "Prefab" : "Scene";
}

void EditorApplication::startMotorProcess() {
    if (motorPID != -1) {
        return;  // Motor ya está corriendo
    }

    if (!currentProject) {
        std::cerr << "⚠ No project loaded, cannot start motor" << std::endl;
        return;
    }

    pid_t pid = fork();

    if (pid == -1) {
        std::cerr << "✗ Failed to fork motor process" << std::endl;
        return;
    }

    if (pid == 0) {
        // Proceso hijo: ejecutar HarukaEngine
        std::string exePath = "../HarukaEngine";  // Ruta relativa desde build/
        
        // Argumentos: ruta del proyecto
        const char* args[] = {
            exePath.c_str(),
            nullptr
        };

        // Ejecutar motor
        execvp(exePath.c_str(), (char* const*)args);

        // Si execvp falla
        std::cerr << "✗ Failed to exec motor: " << strerror(errno) << std::endl;
        exit(1);
    } else {
        // Proceso padre: guardar PID
        motorPID = pid;
        std::cout << "▶ Motor process started (PID: " << motorPID << ")" << std::endl;
    }
}

void EditorApplication::stopMotorProcess() {
    if (motorPID == -1) {
        return;  // Motor no está corriendo
    }

    std::cout << "■ Stopping motor process..." << std::endl;

    // Enviar SIGTERM al proceso motor
    kill(motorPID, SIGTERM);

    // Esperar a que el proceso termine (timeout de 3 segundos)
    int status;
    for (int i = 0; i < 30; i++) {  // 30 * 100ms = 3 segundos
        pid_t result = waitpid(motorPID, &status, WNOHANG);
        if (result == motorPID) {
            std::cout << "✓ Motor process stopped" << std::endl;
            motorPID = -1;
            return;
        }
        usleep(100000);  // 100ms
    }

    // Si aún está corriendo, SIGKILL
    std::cerr << "⚠ Motor did not stop gracefully, forcing..." << std::endl;
    kill(motorPID, SIGKILL);
    waitpid(motorPID, &status, 0);
    motorPID = -1;
    std::cout << "✓ Motor process killed" << std::endl;
}