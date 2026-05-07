#define IMGUI_IMPL_OPENGL_LOADER_GLAD

#include "editor_app.h"
#include "core/camera.h"
#include "tools/error_reporter.h"
#include "core/components/mesh_renderer_component.h"
#include "core/components/material_component.h"
#include "tools/object_types.h"
#include "core/scene/scene_loader.h"
#include "commands/scene_commands.h"
#include "renderer/primitive_shapes.h"
#include "renderer/shader.h"

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
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
#include <vector>

EditorApplication::EditorApplication() : window(nullptr) {}

EditorApplication::~EditorApplication() {
    shutdown();
}

void EditorApplication::init() {
    // ===== SDL3 & GLAD Setup =====
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "Failed to initialize SDL3 in Editor");
        throw std::runtime_error("Failed to initialize SDL3");
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    window = SDL_CreateWindow("Haruka Editor", width, height,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        throw std::runtime_error("Failed to create SDL3 window");
    }

    glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        throw std::runtime_error("Failed to create OpenGL context");
    }
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // Configure shader base directory so Shader loads .spv relative to the
    // application base path (installed shaders live under ../bin/shaders/).
    Shader::setBaseDir(SDL_GetBasePath());

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

    ImGui_ImplSDL3_InitForOpenGL(window, glContext);
    ImGui_ImplOpenGL3_Init("#version 460");

    // ===== Scene & Project Setup =====
    currentScene = std::make_unique<Haruka::SceneManager>();
    currentProject = std::make_unique<Haruka::Project>();
    currentFile.path = "scenes/Untitled.scene";
    currentFile.name = "Untitled";

    // ===== EventManager — wire to panels =====
    // Note: SceneManager doesn't own EventManager; that's the editor's responsibility
    sceneHierarchyPanel.setEventManager(&eventManager);
    viewportPanel.setEventManager(&eventManager);

    // ===== Panels Setup =====
    sceneHierarchyPanel.setScene(currentScene.get());
    sceneHierarchyPanel.setCommandHistory(&commandHistory);
    inspectorPanel.setScene(currentScene.get());
    inspectorPanel.setCommandHistory(&commandHistory);
    inspectorPanel.setOnSceneChanged([this]() { sceneDirty = true; });
    projectBrowserPanel.setProject(currentProject.get());
    projectBrowserPanel.setScene(currentScene.get());
    planetTerrainEditorPanel.setScene(currentScene.get());
    planetarySystem = std::make_unique<Haruka::PlanetarySystem>();
    planetTerrainEditorPanel.setPlanetarySystem(planetarySystem.get());
    
    // ===== Camera Setup =====
    viewportCamera = std::make_unique<Camera>(Haruka::WorldPos(0.0f, 5.0f, 15.0f));
    glm::quat initialOrientation = glm::angleAxis(glm::radians(0.0f), glm::vec3(0, 1, 0));
    viewportCamera->orientation = initialOrientation;

    viewportPanel.setScene(currentScene.get());
    viewportPanel.setCamera(viewportCamera.get());
    viewportPanel.setSDLWindow(window);
    viewportPanel.setStatsPanel(&statsPanel);

    editorCamPos = viewportCamera->position;
    editorCamRot = viewportCamera->orientation;

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
        if (currentScene && index >= 0 && index < (int)currentScene->getAllObjects().size()) {
            inspectorPanel.setSelectedObjectIndex(index);
        }
    });

    sceneHierarchyPanel.setOnObjectSelectedByName([this](const std::string& name) {
        if (!name.empty() && currentScene) {
            auto obj = currentScene->getObjectByName(name);
            if (obj) {
                materialEditorPanel.setSelectedObject(obj.get());
            }
        }
    });

    // ===== Stream Capture Setup =====
    coutCapture = std::make_unique<StreamCapture>(std::cout, &consolePanel, LogLevel::Info);
    cerrCapture = std::make_unique<StreamCapture>(std::cerr, &consolePanel, LogLevel::Error);

    // Initialize panels
    settingsPanel.load();
    
    // Inicializar MenuBar
    menuBar = std::make_unique<MenuBar>(this);
    
}

void EditorApplication::shutdown() {
    // Restaurar streams primero (evita escritura concurrente a consola durante teardown)
    coutCapture.reset();
    cerrCapture.reset();

    // Descargar módulo de juego de forma explícita al cerrar aplicación
    if (gameInterface && gameInterface->onShutdown) {
        gameInterface->onShutdown();
    }
    gameInterface = nullptr;
    if (gameLibHandle) {
        dlclose(gameLibHandle);
        gameLibHandle = nullptr;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (glContext) SDL_GL_DestroyContext(glContext);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void EditorApplication::run() {
    init();
    while (!_shouldClose) {
        update();
        render();
    }
}

void EditorApplication::update() {
    float currentFrame = SDL_GetTicks() / 1000.0f;
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) {
            _shouldClose = true;
        } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                   event.window.windowID == SDL_GetWindowID(window)) {
            _shouldClose = true;
        }
    }

    updatePlayMode(deltaTime);
    // planetTerrainEditorPanel.update(); // TODO: Implement when panel is refactored
    exportPanel.update();
    
    // Auto-save system
    if (autoSaveEnabled && sceneDirty && !currentFile.path.empty()) {
        timeSinceLastSave += deltaTime;
        if (timeSinceLastSave >= autoSaveInterval) {
            saveFile(currentFile.path);
            timeSinceLastSave = 0.0f;
        }
    }
    
    processEditorEvents();
    viewportPanel.update(deltaTime);

    statsPanel.update(deltaTime);
}

void EditorApplication::updatePlayMode(float deltaTime) {
    if (!isPlayMode) return;
    
    playModeTime += deltaTime;

    // Ejecutar gameplay update
    if (gameInterface && gameInterface->onUpdate) {
        gameInterface->onUpdate(window, deltaTime);
    }

    // Sincronizar cámara de juego al viewport SIN compartir ownership/puntero
    if (gameInterface && gameInterface->getCamera && viewportCamera) {
        Camera* gameCam = gameInterface->getCamera();
        if (gameCam) {
            viewportCamera->position = gameCam->position;
            viewportCamera->orientation = gameCam->orientation;
            viewportCamera->zoom = gameCam->zoom;
            viewportCamera->speed = gameCam->speed;
            viewportCamera->sensitivity = gameCam->sensitivity;
            viewportPanel.setCamera(viewportCamera.get());
        }
    }
}

void EditorApplication::render() {
    // Update window title with dirty flag
    std::string title = "Haruka Editor";
    if (!currentFile.path.empty()) {
        title += " - " + currentFile.name + " (Scene)";
    }
    if (sceneDirty) title += " *";
    SDL_SetWindowTitle(window, title.c_str());

    // ImGui frame setup
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    renderUI();

    // Render
    ImGui::Render();
    int display_w, display_h;
    SDL_GetWindowSizeInPixels(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Handle multi-viewport
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        SDL_Window*   backup_window  = SDL_GL_GetCurrentWindow();
        SDL_GLContext backup_context = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backup_window, backup_context);
    }

    SDL_GL_SwapWindow(window);
}

void EditorApplication::renderUI() {
    try {
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

        menuBar->render();
        ImGui::End();

        if (isPlayMode) ImGui::BeginDisabled();

        // Project Browser
        if (showProjectBrowser) {
            try {
                projectBrowserPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "ProjectBrowser crash: " + std::string(e.what()));
            }
        }
            
        // Scene Hierarchy
        if (showSceneHierarchy) {
            try {
                sceneHierarchyPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "SceneHierarchy crash: " + std::string(e.what()));
            }
        }
        
        // Inspector
        if (showInspector) {
            try {
                inspectorPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "Inspector crash: " + std::string(e.what()));
            }
        }
        
        // Console
        if (showConsole) {
            try {
                consolePanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "Console crash: " + std::string(e.what()));
            }
        }
        
        // Stats
        if (showStats) {
            try {
                statsPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "Stats crash: " + std::string(e.what()));
            }
        }
        
        // Material Editor
        if (showMaterialEditor) {
            try {
                materialEditorPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "MaterialEditor crash: " + std::string(e.what()));
            }
        }

        if (showPlanetTerrainEditor) {
            try {
                planetTerrainEditorPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "PlanetTerrainEditor crash: " + std::string(e.what()));
            }
        }

        if (isPlayMode) ImGui::EndDisabled();

        // Viewport (siempre visible)
        if (showViewport) {
            try {
                viewportPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "Viewport crash: " + std::string(e.what()));
            }
        }
        
        if (showDemoWindow) {
            ImGui::ShowDemoWindow(&showDemoWindow);
        }
        
        if (showSettings) {
            try {
                settingsPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "Settings crash: " + std::string(e.what()));
            }
        }
        
        if (showAssetImporter) {
            try {
                assetImporter.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "AssetImporter crash: " + std::string(e.what()));
            }
        }
        
        if (showUIBuilder) {
            try {
                uiBuilder.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "UIBuilder crash: " + std::string(e.what()));
            }
        }

        // Save As popup (fuera del menú)
        if (showSaveAsPopup) ImGui::OpenPopup("Save File As");
        if (ImGui::BeginPopupModal("Save File As", &showSaveAsPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Path##save", saveAsBuffer, sizeof(saveAsBuffer));
            
            if (ImGui::Button("Save")) {
                saveFile(saveAsBuffer);
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
        
        // Export Panel
        exportPanel.render(this);

        // Unsaved changes popup
        if (showUnsavedChangesPopup) ImGui::OpenPopup("Unsaved Changes");
        if (ImGui::BeginPopupModal("Unsaved Changes", &showUnsavedChangesPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Hay cambios sin guardar.");
            if (ImGui::Button("Guardar y continuar")) {
                if (!currentFile.path.empty()) saveFile(currentFile.path);
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
    } catch (const std::exception& e) {
        HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "RenderUI general crash: " + std::string(e.what()));
    }
}


void EditorApplication::enterPlayMode() {
    if (isPlayMode) return;

    isPlayMode = true;
    playModeTime = 0.0f;

    inspectorPanel.setPlayMode(true);

    // Sin proyecto: entrar en play con la escena actual (útil para testing)
    if (!currentProject) return;

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
            planetTerrainEditorPanel.setScene(currentScene.get());

            // Apply Camera scene object at play start (game library can override afterwards)
            // Searches top-level objects first, then prefab children
            const Haruka::SceneObject* camObj = nullptr;
            for (const auto& objPtr : currentScene->getAllObjects()) {
                if (!objPtr) continue;
                const auto& obj = *objPtr;
                if (obj.type == "Camera") { camObj = &obj; break; }
                // Note: New SceneObject uses childrenIndices instead of direct children
                if (camObj) break;
            }
            if (camObj) {
                viewportCamera->position = camObj->position;
                glm::dquat qYaw   = glm::angleAxis(glm::radians(camObj->rotation.y), glm::dvec3(0, 1, 0));
                glm::dquat qPitch = glm::angleAxis(glm::radians(camObj->rotation.x), glm::dvec3(1, 0, 0));
                glm::dquat qRoll  = glm::angleAxis(glm::radians(camObj->rotation.z), glm::dvec3(0, 0, 1));
                viewportCamera->orientation = qYaw * qPitch * qRoll;
                if (camObj->properties.is_object()) {
                    if (camObj->properties.contains("speed"))
                        viewportCamera->speed = camObj->properties["speed"].get<float>();
                    if (camObj->properties.contains("sensitivity"))
                        viewportCamera->sensitivity = camObj->properties["sensitivity"].get<float>();
                }
                viewportPanel.setCamera(viewportCamera.get());
                std::cout << "✓ Camera from scene: " << camObj->name << std::endl;
            }

            // Resetear selección
            sceneHierarchyPanel.setSelectedObjectIndex(-1);
            inspectorPanel.setSelectedObjectIndex(-1);

            std::cout << "Scene loaded: " << startScenePath << std::endl;
        }
    }

    std::string logicLib = "lib" + currentProject->getConfig().name + ".so";
    std::filesystem::path pProject(projectPath);

    std::vector<std::filesystem::path> candidates;
    candidates.push_back(pProject / logicLib);            // proyecto raíz
    candidates.push_back(pProject / "build" / logicLib); // build del proyecto

    // Ruta de salida configurable por proyecto
    if (!currentProject->getConfig().outputPath.empty()) {
        std::filesystem::path outPath(currentProject->getConfig().outputPath);
        if (outPath.is_relative()) {
            candidates.push_back(pProject / outPath / logicLib);
        } else {
            candidates.push_back(outPath / logicLib);
        }
    }

    // Derivar build del engine desde engineBinary en project.hrk
    if (!currentProject->getConfig().engineBinary.empty()) {
        std::filesystem::path engineBin(currentProject->getConfig().engineBinary);
        candidates.push_back(engineBin.parent_path() / logicLib);
    }

    std::string libPath;
    std::filesystem::file_time_type newestTime{};
    bool foundLib = false;
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c) && std::filesystem::is_regular_file(c)) {
            auto t = std::filesystem::last_write_time(c);
            if (!foundLib || t > newestTime) {
                newestTime = t;
                libPath = c.string();
                foundLib = true;
            }
        }
    }
    if (!foundLib) {
        libPath = (pProject / logicLib).string();
    }

    if (!gameLibHandle) {
        gameLibHandle = dlopen(libPath.c_str(), RTLD_LAZY);
    }
    
    if (gameLibHandle) {
        typedef Haruka::GameInterface* (*GetGameInterfaceFunc)();
        GetGameInterfaceFunc getGameInterface = (GetGameInterfaceFunc)dlsym(gameLibHandle, "getGameInterface");
        
        if (getGameInterface) {
            gameInterface = getGameInterface();
            
            if (gameInterface) {
                std::cout << "✓ Game interface loaded: " << (gameInterface->name ? gameInterface->name : "Unknown") << std::endl;

                // Ejecutar inicialización de juego en Play Mode
                if (gameInterface->onInit) {
                    gameInterface->onInit(currentScene.get());
                    std::cout << "✓ Game initialized" << std::endl;
                }
                
                // Establecer cámara
                if (gameInterface->getCamera) {
                    Camera* gameCamera = gameInterface->getCamera();
                    if (gameCamera) {
                        viewportCamera->position = gameCamera->position;
                        viewportCamera->orientation = gameCamera->orientation;
                        viewportCamera->zoom = gameCamera->zoom;
                        viewportCamera->speed = gameCamera->speed;
                        viewportCamera->sensitivity = gameCamera->sensitivity;
                        viewportPanel.setCamera(viewportCamera.get());
                        std::cout << "✓ Game camera synced to viewport" << std::endl;
                    }
                }
            }
        } else {
            std::cout << "⚠ getGameInterface not found, project may not implement it" << std::endl;
        }
    } else {
        std::cout << "⚠ No game library found (" << logicLib << "), running without game logic\n";
    }

    std::cout << "▶ Play Mode started" << std::endl;
}

void EditorApplication::exitPlayMode() {
    if (!isPlayMode) return;
    
    isPlayMode = false;

    // Modo seguro: NO ejecutar shutdown/dlclose al salir de Play.
    // Evita corrupción de heap por destrucción cruzada de runtime dinámico.
    gameInterface = nullptr;

    if (std::filesystem::exists(playModeBackupPath)) {
        currentScene->load(playModeBackupPath);
        planetTerrainEditorPanel.setScene(currentScene.get());
    }
    
    viewportPanel.setCamera(viewportCamera.get());
    
    inspectorPanel.setPlayMode(false);
    
    std::cout << "⏹ Play Mode stopped" << std::endl;
}

void EditorApplication::createNewProject(const std::string& name, const std::string& basePath) {
    if (name.empty() || basePath.empty()) {
        std::cerr << "Project name and path cannot be empty" << std::endl;
        return;
    }

    try {
        // Usar template como base
        std::string projectPath = basePath + name;
        // Construir ruta al template de forma relativa
        std::filesystem::path editorPath = std::filesystem::current_path();
        std::string templatePath = editorPath.parent_path().string() + "/template";

        // Copiar template recursivamente si existe
        if (std::filesystem::exists(templatePath)) {
            std::filesystem::copy(templatePath, projectPath, std::filesystem::copy_options::recursive);
        } else {
            // Fallback: crear estructura básica si no existe template
            std::filesystem::create_directories(projectPath + "/scripts");
            std::filesystem::create_directories(projectPath + "/scenes");
            std::filesystem::create_directories(projectPath + "/assets");
        }

        // Actualizar CMakeLists.txt con el nombre del proyecto
        std::string cmakeFilePath = projectPath + "/CMakeLists.txt";
        if (std::filesystem::exists(cmakeFilePath)) {
            std::ifstream cmakeIn(cmakeFilePath);
            std::string cmakeContent((std::istreambuf_iterator<char>(cmakeIn)),
                                      std::istreambuf_iterator<char>());
            cmakeIn.close();

            // Reemplazar placeholder PROJECT_NAME_PLACEHOLDER con el nombre real
            size_t pos = 0;
            while ((pos = cmakeContent.find("PROJECT_NAME_PLACEHOLDER", pos)) != std::string::npos) {
                cmakeContent.replace(pos, 24, name); // 24 = length("PROJECT_NAME_PLACEHOLDER")
                pos += name.length();
            }

            std::ofstream cmakeOut(cmakeFilePath);
            if (cmakeOut.is_open()) {
                cmakeOut << cmakeContent;
                cmakeOut.close();
            }
        }

        // Actualizar project.hrk con nombre del nuevo proyecto
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
        sceneDirty = false;

        // Sincronizar paneles
        projectBrowserPanel.setProject(currentProject.get());
        projectBrowserPanel.setScene(currentScene.get());
        sceneHierarchyPanel.setScene(currentScene.get());
        inspectorPanel.setScene(currentScene.get());
        viewportPanel.setScene(currentScene.get());
        planetTerrainEditorPanel.setScene(currentScene.get());

        std::cout << "✓ Project created from template: " << projectPath << std::endl;
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
        HARUKA_EDITOR_ERROR(ErrorCode::PROJECT_COMPILATION_FAIL, "Project compilation failed: ");
    }
    
    isProjectCompiling = false;
}

void EditorApplication::exportGame() {
    if (!currentProject) {
        std::cerr << "✗ No project loaded" << std::endl;
        return;
    }
    
    // Mostrar panel de export
    exportPanel.show();
}

void EditorApplication::showExportDialog() {
    // El panel de export se renderiza desde renderUI()
    exportPanel.render(this);
}

void EditorApplication::saveFile(const std::string& path) {
    if (!currentScene || path.empty()) return;
    
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
        currentFile.lastSaveTime = SDL_GetTicks() / 1000.0f;
        sceneDirty = false;
        timeSinceLastSave = 0.0f;
        std::cout << "✓ Scene saved: " << path << std::endl;
    } else {
        HARUKA_EDITOR_ERROR(ErrorCode::FAILED_TO_SAVE_FILE, "Failed to save: " + path);
    }
}

void EditorApplication::loadFile(const std::string& path) {
    // Clear panel scene pointers before destroying the old scene to avoid dangling refs
    sceneHierarchyPanel.setScene(nullptr);
    inspectorPanel.setScene(nullptr);
    viewportPanel.setScene(nullptr);
    planetTerrainEditorPanel.setScene(nullptr);
    projectBrowserPanel.setScene(nullptr);

    currentScene = std::make_unique<Haruka::Scene>();
    currentScene->setEventManager(&eventManager); // notify engine of loaded objects

    if (loadedScene->load(path)) {
        currentScene = std::move(loadedScene);

        // Actualizar estado del archivo
        currentFile.path = path;
        currentFile.name = std::filesystem::path(path).stem().string();
        currentFile.lastSaveTime = SDL_GetTicks() / 1000.0f;

        // Resetear estado de cambios
        sceneDirty = false;
        timeSinceLastSave = 0.0f;
        
        // Sincronizar panels
        sceneHierarchyPanel.setScene(currentScene.get());
        inspectorPanel.setScene(currentScene.get());
        viewportPanel.setScene(currentScene.get());
        planetTerrainEditorPanel.setScene(currentScene.get());
        
        // Sync editor camera to the scene Camera object if present
        for (const auto& objPtr : currentScene->getAllObjects()) {
            if (!objPtr) continue;
            const auto& obj = *objPtr;
            if (obj.type == "Camera") {
                viewportCamera->position = obj.position;
                viewportCamera->orientation = obj.rotation;
                if (obj.properties.is_object()) {
                    if (obj.properties.contains("speed"))
                        viewportCamera->speed = obj.properties["speed"].get<float>();
                    if (obj.properties.contains("sensitivity"))
                        viewportCamera->sensitivity = obj.properties["sensitivity"].get<float>();
                }
                viewportPanel.setCamera(viewportCamera.get());
                std::cout << "✓ Editor camera synced to: " << obj.name << std::endl;
                break;
            }
        }

        // Resetear selección a ningún objeto
        sceneHierarchyPanel.setSelectedObjectIndex(-1);
        inspectorPanel.setSelectedObjectIndex(-1);

        std::cout << "✓ Scene loaded: " << path << std::endl;
    } else {
        HARUKA_EDITOR_ERROR(ErrorCode::FAILED_TO_LOAD_FILE, "Failed to load file: " + path);
    }
}

void EditorApplication::createFileBackup(const std::string& filePath) {
    std::filesystem::path p(filePath);
    std::string backupDir = p.parent_path().string() + "/backups";
    std::filesystem::create_directories(backupDir);
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", std::localtime(&time));
    
    std::string ext = p.extension().string();
    std::string backupPath = backupDir + "/" + p.stem().string() + "_" + timestamp + ext;
    
    try {
        std::filesystem::copy_file(filePath, backupPath, 
            std::filesystem::copy_options::overwrite_existing);
        cleanOldBackups(filePath);
        
        std::cout << "✓ Backup created: " << backupPath << std::endl;
    } catch (const std::exception& e) {
        HARUKA_EDITOR_ERROR(ErrorCode::FAILED_TO_SAVE_FILE, "Backup failed: " + std::string(e.what()));
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

// ---------------------------------------------------------------------------
// buildSceneObjectForType — engine applies default props per primitive type
// ---------------------------------------------------------------------------
static Haruka::SceneObject buildSceneObjectForType(const std::string& name, Haruka::PrimitiveType& type, const nlohmann::json& data)
{
    Haruka::SceneObject obj;
    obj.name        = name;
    obj.type        = Haruka::primitiveTypeToString(type);
    obj.position    = Haruka::WorldPos();
    obj.rotation    = Haruka::Rotation();
    obj.scale       = glm::dvec3(1.0);
    obj.parentIndex = data.value("parentIndex", -1);
    obj.modelPath   = data.value("modelPath", "");
    if (!data.value("fromLoad", false)) obj.properties = data;

    // Populate default meshRenderer properties based on type.
    std::vector<glm::vec3> verts, norms;
    std::vector<unsigned int> indices;

    if (type == Haruka::PrimitiveType::CUBE) {
        PrimitiveShapes::createCube(1.0f, verts, norms, indices);
        obj.properties["meshRenderer"]["meshType"] = Haruka::PrimitiveType::CUBE;
        obj.properties["meshRenderer"]["size"]     = 1.0f;
    } else if (type == Haruka::PrimitiveType::SPHERE) {
        PrimitiveShapes::createSphere(1.0f, 32, 32, verts, norms, indices);
        obj.properties["meshRenderer"]["meshType"]  = Haruka::PrimitiveType::SPHERE;
        obj.properties["meshRenderer"]["radius"]    = 1.0f;
        obj.properties["meshRenderer"]["segments"]  = 32;
    } else if (type == Haruka::PrimitiveType::CAPSULE) {
        PrimitiveShapes::createCapsule(0.5f, 2.0f, 24, 16, verts, norms, indices);
        obj.properties["meshRenderer"]["meshType"]  = Haruka::PrimitiveType::CAPSULE;
        obj.properties["meshRenderer"]["radius"]    = 0.5f;
        obj.properties["meshRenderer"]["height"]    = 2.0f;
        obj.properties["meshRenderer"]["segments"]  = 24;
        obj.properties["meshRenderer"]["stacks"]    = 16;
    } else if (type == Haruka::PrimitiveType::PLANE) {
        PrimitiveShapes::createPlane(2.0f, 2.0f, 10, verts, norms, indices);
        obj.properties["meshRenderer"]["meshType"]     = Haruka::PrimitiveType::PLANE;
        obj.properties["meshRenderer"]["width"]        = 2.0f;
        obj.properties["meshRenderer"]["height"]       = 2.0f;
        obj.properties["meshRenderer"]["subdivisions"] = 10;
    }

    if (!verts.empty()) {
        std::vector<glm::vec3> colors(verts.size(), glm::vec3(obj.color));
        obj.meshRenderer = std::make_shared<MeshRendererComponent>();
        obj.meshRenderer->setMesh(verts, norms, colors, indices);
        obj.material = std::make_shared<Haruka::MaterialComponent>();
        obj.material->albedo = glm::vec3(obj.color);
    }
    return obj;
}

// ---------------------------------------------------------------------------
// processEditorEvents — drains EventManager queue each frame
// ---------------------------------------------------------------------------
void EditorApplication::processEditorEvents() {
    while (auto optEvt = eventManager.poll()) {
        auto& evt = *optEvt;

        if (auto* objEvt = dynamic_cast<Haruka::ObjectEvent*>(evt.get())) {
            using AT = Haruka::ObjectEvent::ActionType;

            if (objEvt->action == AT::Created) {
                bool fromLoad = objEvt->data.value("fromLoad", false);
                if (!fromLoad && currentScene) {
                    // Engine builds the SceneObject with correct default props.
                    auto primitiveType = Haruka::stringToPrimitiveType(objEvt->objectType);
                    Haruka::SceneObject obj = buildSceneObjectForType(
                        objEvt->objectName, primitiveType, objEvt->data);
                    commandHistory.execute(
                        std::make_unique<AddObjectCommand>(currentScene.get(), obj));
                    sceneDirty = true;
                }
                // fromLoad=true objects are already in the scene (Scene::load() added them).

            } else if (objEvt->action == AT::Deleted) {
                if (currentScene) {
                    commandHistory.execute(
                        std::make_unique<DeleteObjectCommand>(
                            currentScene.get(), objEvt->objectName));
                    sceneDirty = true;
                }

            } else if (objEvt->action == AT::Duplicated) {
                if (currentScene) {
                    auto src = currentScene->getObjectByName(objEvt->objectName);
                    if (src) {
                        Haruka::SceneObject dup = *src;
                        dup.name      = src->name + "_copy";
                        dup.position += glm::dvec3(1.0, 0.0, 0.0);
                        commandHistory.execute(
                            std::make_unique<AddObjectCommand>(currentScene.get(), dup));
                        sceneDirty = true;
                    }
                }

            } else if (objEvt->action == AT::Reparented) {
                if (currentScene) {
                    int newParent = objEvt->data.value("newParentIndex", -1);
                    // Note: SceneManager stores objects as shared_ptr; direct mutation requires refactoring
                    // For now, mark scene as dirty and reload
                    sceneDirty = true;
                }

            } else if (objEvt->action == AT::Selected) {
                // Selection handled by viewport/hierarchy via callbacks; nothing extra needed.
            }

        } else if (auto* logEvt = dynamic_cast<Haruka::LogEvent*>(evt.get())) {
            // Forward to console output (std::cout captured by ConsolePanel).
            if (logEvt->level == Haruka::LogEvent::Level::Error)
                std::cerr << "[Engine] " << logEvt->message << "\n";
            else
                std::cout << "[Engine] " << logEvt->message << "\n";
        }
    }
}