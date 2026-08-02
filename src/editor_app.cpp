#define IMGUI_IMPL_OPENGL_LOADER_GLAD

#include "editor_app.h"
#include "core/camera.h"
#include "tools/error_reporter.h"
#include "core/components/mesh_renderer_component.h"
#include "renderer/primitive_shapes.h"
#include "renderer/shader.h"   // Shader::setBaseDir → raíz de assets del motor

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <nfd.h>
#include <dlfcn.h>

#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <cerrno>
#include <vector>

// Paralelismo de compilación ACOTADO POR RAM, igual que el build.sh del juego: cada g++ de
// este proyecto pide ~0.4-0.5 GB de pico, así que `-j$(nproc)` lanza todos los trabajos a la
// vez y el build tira de swap. Se calcula desde MemAvailable (lo que el kernel puede reclamar
// de verdad), dejando 1.5 GB al sistema. Override con el mismo criterio en build.sh: JOBS=N.
static int ramBoundedJobs() {
    long availMb = 0;
    if (FILE* f = std::fopen("/proc/meminfo", "r")) {
        char line[256];
        while (std::fgets(line, (int)sizeof line, f)) {
            long v = 0;
            if (std::sscanf(line, "MemAvailable: %ld kB", &v) == 1) { availMb = v / 1024; break; }
        }
        std::fclose(f);
    }
    const long cores = std::max<long>(::sysconf(_SC_NPROCESSORS_ONLN), 1);
    if (availMb <= 0) return (int)cores;
    long usable = availMb - 1536;               // reserva para el sistema
    if (usable < 600) usable = 600;
    int jobs = (int)(usable / 600);             // ~0.6 GB por trabajo (margen sobre los 0.5 medidos)
    jobs = std::max(jobs, 1);
    if (jobs > (int)cores) jobs = (int)cores;
    return jobs;
}

EditorApplication::EditorApplication() : window(nullptr) {}

EditorApplication::~EditorApplication() {
    shutdown();
}

void EditorApplication::init() {
    // ===== SDL3 & GLAD Setup =====
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "Failed to initialize SDL3 in Editor");
        throw std::runtime_error("Failed to initialize SDL3");
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

    window = SDL_CreateWindow("Haruka Editor", width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        throw std::runtime_error("Failed to create SDL window");
    }

    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    if (!glCtx) {
        throw std::runtime_error("Failed to create OpenGL context");
    }
    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // RAÍZ DE ASSETS DEL MOTOR. Sin esto `Shader::baseDir()` queda vacía y el motor pide
    // "shaders/simple.vert" RELATIVO al cwd, mientras los ficheros viven en "assets/shaders/" junto
    // al ejecutable: no compila NI UN pipeline (cielo, escena, terreno…) y el viewport sale negro sin
    // más pista que un HARUKA_LOGW por shader. Va aquí, tras el contexto GL y ANTES de crear la
    // Application, porque los pipelines se hornean en el primer frame.
    // Se deriva de SDL_GetBasePath (el directorio del EJECUTABLE, no el cwd): el editor se lanza
    // tanto desde su carpeta como desde el IDE o un script, y el cwd cambia con quien lo arranca.
    if (const char* exeDir = SDL_GetBasePath()) {
        const std::string assetsRoot = std::string(exeDir) + "assets/";
        Shader::setBaseDir(assetsRoot.c_str());   // fija también Haruka::AssetPaths
        std::cout << "[Editor] assets root: " << assetsRoot << std::endl;
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

    ImGui_ImplSDL3_InitForOpenGL(window, glCtx);
    ImGui_ImplOpenGL3_Init("#version 460");

    // ===== Scene & Project Setup =====
    currentScene = std::make_unique<Haruka::SceneManager>();
    currentScene->setName("Untitled");
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
    inspectorPanel.setProjectPath(currentProject->getPath());
    objectsPanel.setScene(currentScene.get());
    objectsPanel.setCommandHistory(&commandHistory);
    objectsPanel.setOnSceneChanged([this]() { sceneDirty = true; });
    // Colocación de props/herramientas de malla: el panel pide, el viewport ejecuta.
    objectsPanel.setOnBeginPlacement([this](const std::string& model, const std::string& label,
                                            float radius, bool circle, int action,
                                            const std::string& layer) {
        viewportPanel.beginPlacement(model, label, radius, circle, action, layer);
    });
    materialEditorPanel.setProjectPath(currentProject->getPath());
    materialEditorPanel.setOnSceneChanged([this]() { sceneDirty = true; });
    nodeGraphEditorPanel.setScene(currentScene.get());
    nodeGraphEditorPanel.setProjectPath(currentProject->getPath());
    nodeGraphEditorPanel.setOnSceneChanged([this]() { sceneDirty = true; });
    projectBrowserPanel.setProject(currentProject.get());
    projectBrowserPanel.setScene(currentScene.get());
    
    // ===== Camera Setup =====
    viewportCamera = std::make_unique<Haruka::Core::Camera>(Haruka::WorldPos(0.0f, 5.0f, 15.0f));
    glm::quat initialOrientation = glm::angleAxis(glm::radians(0.0f), glm::vec3(0, 1, 0));
    viewportCamera->orientation = initialOrientation;

    viewportPanel.setScene(currentScene.get());
    viewportPanel.setCamera(viewportCamera.get());
    viewportPanel.setSDLWindow(window);
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
        if (currentScene && index >= 0 && index < (int)currentScene->getObjects().size()) {
            inspectorPanel.setSelectedObjectIndex(index);
            viewportPanel.setSelectedObjectIndex(index);
            const auto& all = currentScene->getAllObjects();
            if (index < (int)all.size()) nodeGraphEditorPanel.setSelectedObject(all[index].get());
        }
    });

    sceneHierarchyPanel.setOnObjectSelectedByName([this](const std::string& name) {
        if (!name.empty() && currentScene) {
            auto obj = currentScene->getObject(name);
            if (obj) {
                materialEditorPanel.setSelectedObject(obj.get());
                nodeGraphEditorPanel.setSelectedObject(obj.get());
            }
        }
    });

    objectsPanel.setOnObjectSelectedByIndex([this](int index) {
        if (currentScene && index >= 0 && index < (int)currentScene->getObjects().size()) {
            sceneHierarchyPanel.setSelectedObjectIndex(index);
            inspectorPanel.setSelectedObjectIndex(index);
            viewportPanel.setSelectedObjectIndex(index);
            const auto& all = currentScene->getAllObjects();
            if (index < (int)all.size()) nodeGraphEditorPanel.setSelectedObject(all[index].get());
        }
    });

    objectsPanel.setOnObjectSelectedByName([this](const std::string& name) {
        if (!name.empty() && currentScene) {
            auto obj = currentScene->getObject(name);
            if (obj) {
                materialEditorPanel.setSelectedObject(obj.get());
                nodeGraphEditorPanel.setSelectedObject(obj.get());
            }
        }
    });

    // DOBLE clic en jerarquía u Objects = llevar la cámara al objeto y encuadrarlo.
    sceneHierarchyPanel.setOnObjectFocused([this](int index) { viewportPanel.focusOnObject(index); });
    objectsPanel.setOnObjectFocused([this](int index) { viewportPanel.focusOnObject(index); });

    objectsPanel.setOnOpenNodeGraph([this](const std::string& name) {
        showNodeGraphEditor = true;
        if (currentScene) {
            auto obj = currentScene->getObject(name);
            if (obj) nodeGraphEditorPanel.setSelectedObject(obj.get());
        }
    });

    // ===== Stream Capture Setup =====
    coutCapture = std::make_unique<StreamCapture>(std::cout, &consolePanel, LogLevel::Info);
    cerrCapture = std::make_unique<StreamCapture>(std::cerr, &consolePanel, LogLevel::Error);

    // Initialize panels
    settingsPanel.load();
    
    // Inicializar MenuBar
    menuBar = std::make_unique<MenuBar>(this);
    
    std::cout << "✓ Haruka Editor initialized" << std::endl;
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

    // Liberar recursos GL del motor ANTES de destruir el contexto (RenderTargets, Application).
    viewportPanel.shutdownGLResources();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    if (window) { SDL_GL_DestroyContext(SDL_GL_GetCurrentContext()); SDL_DestroyWindow(window); }
    SDL_Quit();
}

void EditorApplication::run() {
    init();
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
        }
        update();
        render();
    }
}

void EditorApplication::update() {
    float currentFrame = static_cast<float>(SDL_GetTicks() / 1000.0f);
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    statsPanel.update(deltaTime);
    updatePlayMode(deltaTime);
    exportPanel.update();
    
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

    // El onUpdate del juego NO se ejecuta aquí: el HUD/consola del juego dibujan con
    // ImGui::Begin, y update() corre ANTES de ImGui::NewFrame() -> assertion WithinFrameScope.
    // Se invoca en render(), justo después de NewFrame, igual que el bucle standalone del motor.

    // Sincronizar cámara de juego al viewport SIN compartir ownership/puntero
    if (gameInterface && gameInterface->getCamera && viewportCamera) {
        Haruka::Core::Camera* gameCam = gameInterface->getCamera();
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
        title += " - " + currentFile.name + " (" + getFileType(currentFile.path) + ")";
    }
    if (sceneDirty) title += " *";
    SDL_SetWindowTitle(window, title.c_str());

    // ImGui frame setup
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Play Mode: el onUpdate del juego (gameplay + HUD/consola con ImGui::Begin) DEBE correr
    // dentro del scope de frame, como en el bucle standalone (application.cpp: NewFrame ->
    // onUpdate -> Render). Si se llamara en update() antes de NewFrame, ImGui abortaría con
    // "WithinFrameScope".
    if (isPlayMode && gameInterface && gameInterface->onUpdate) {
        gameInterface->onUpdate(window, deltaTime);
    }

    renderUI();

    // Render
    ImGui::Render();
    int display_w, display_h;
    SDL_GetWindowSizeInPixels(window, &display_w, &display_h);
    // La PANTALLA, explícitamente. renderUI() ha llamado al motor para pintar el viewport, y el
    // motor dibuja sobre el FBO de ese RenderTarget; en GL el fin de un pase no desata nada, así
    // que sin este bind la UI entera se pintaría DENTRO del viewport y el backbuffer se quedaría
    // sin escribir (la ventana alterna entre dos buffers viejos = parpadea entera). El motor
    // también lo restaura por su lado, pero el editor no debe depender del estado GL que le deje
    // otro: quien dibuja a la pantalla, la ata.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, display_w, display_h);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    // Handle multi-viewport
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        SDL_Window* backup_current_context = SDL_GL_GetCurrentWindow();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        SDL_GL_MakeCurrent(backup_current_context, SDL_GL_GetCurrentContext());
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
        
        // Objects (por capas)
        if (showObjectsPanel) {
            try {
                objectsPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "ObjectsPanel crash: " + std::string(e.what()));
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

        // Node Graph Editor (texturas/materiales procedurales)
        if (showNodeGraphEditor) {
            try {
                nodeGraphEditorPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "NodeGraphEditor crash: " + std::string(e.what()));
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
        viewportPanel.setGizmoMode(gizmoMode);

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
        
        if (showSearchPanel) {
            try {
                searchPanel.onImGuiRender();
            } catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "SearchPanel crash: " + std::string(e.what()));
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
        
        // Export Panel
        exportPanel.render(this);

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
    } catch (const std::exception& e) {
        HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED, "RenderUI general crash: " + std::string(e.what()));
    }
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

    // Asegurar la raíz de assets del proyecto también en el camino de play (el motor la usa
    // para resolver texturas de escena y biomas de SimplePlanet).
    Haruka::AssetPaths::setProjectRoot(projectPath + "/");
    
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
            
            // Resetear selección
            sceneHierarchyPanel.setSelectedObjectIndex(-1);
            inspectorPanel.setSelectedObjectIndex(-1);
            viewportPanel.setSelectedObjectIndex(-1);
            
            std::cout << "Scene loaded: " << startScenePath << std::endl;
        }
    }

    std::string logicLib = "lib" + currentProject->getConfig().name + ".so";
    std::filesystem::path pProject(projectPath);

    std::vector<std::filesystem::path> candidates;
    candidates.push_back(pProject / logicLib);            // proyecto raíz
    candidates.push_back(pProject / "build" / logicLib); // build del proyecto
    candidates.push_back(pProject / "build" / "bin" / logicLib); // layout moderno (template FetchContent)

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
                    Haruka::Core::Camera* gameCamera = gameInterface->getCamera();
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
        HARUKA_EDITOR_ERROR(ErrorCode::MOTOR_LIBRARY, "Could not load project library: " + std::string(dlerror()));
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
        // Usar el template del MOTOR como base (fuente única de verdad). El editor se construye
        // con FetchContent y sabe la ruta del motor en HARUKA_ENGINE_ROOT.
        std::string projectPath = basePath + name;
        std::string templatePath;
#ifdef HARUKA_ENGINE_ROOT
        templatePath = std::string(HARUKA_ENGINE_ROOT) + "/template";
#else
        // Fallback legacy: template junto al ejecutable
        std::filesystem::path editorPath = std::filesystem::current_path();
        templatePath = editorPath.parent_path().string() + "/template";
#endif

        // Copiar template recursivamente si existe
        if (std::filesystem::exists(templatePath)) {
            std::filesystem::copy(templatePath, projectPath, std::filesystem::copy_options::recursive);
        } else {
            // Fallback: crear estructura básica si no existe template
            std::filesystem::create_directories(projectPath + "/scripts");
            std::filesystem::create_directories(projectPath + "/scenes");
            std::filesystem::create_directories(projectPath + "/assets");
        }

        // Actualizar CMakeLists.txt con el nombre y el motor del proyecto
        std::string cmakeFilePath = projectPath + "/CMakeLists.txt";
        if (std::filesystem::exists(cmakeFilePath)) {
            std::ifstream cmakeIn(cmakeFilePath);
            std::string cmakeContent((std::istreambuf_iterator<char>(cmakeIn)),
                                      std::istreambuf_iterator<char>());
            cmakeIn.close();

            // Reemplazar placeholder PROJECT_NAME_PLACEHOLDER con el nombre real
            const std::string namePlaceholder = "PROJECT_NAME_PLACEHOLDER";
            size_t pos = 0;
            while ((pos = cmakeContent.find(namePlaceholder, pos)) != std::string::npos) {
                cmakeContent.replace(pos, namePlaceholder.length(), name);
                pos += name.length();
            }

            // Reemplazar HARUKA_ENGINE_LOCAL_PLACEHOLDER con la ruta del motor del editor
            const std::string enginePlaceholder = "HARUKA_ENGINE_LOCAL_PLACEHOLDER";
            const std::string engineRoot =
#ifdef HARUKA_ENGINE_ROOT
                std::string(HARUKA_ENGINE_ROOT);
#else
                std::string();
#endif
            pos = 0;
            while ((pos = cmakeContent.find(enginePlaceholder, pos)) != std::string::npos) {
                cmakeContent.replace(pos, enginePlaceholder.length(), engineRoot);
                pos += engineRoot.length();
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
        Haruka::AssetPaths::setProjectRoot(projectPath + "/");

        currentFile.path = scenePath;
        currentFile.name = "main";
        currentFile.isPrefab = false;
        sceneDirty = false;

        // Sincronizar paneles
        projectBrowserPanel.setProject(currentProject.get());
        inspectorPanel.setProjectPath(currentProject->getPath());
        materialEditorPanel.setProjectPath(currentProject->getPath());
        nodeGraphEditorPanel.setProjectPath(currentProject->getPath());
        bindPanelsToScene();

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
    
    // Compilar el proyecto. Se pasa el nombre (lib<Nombre>.so) y la ruta del motor del editor
    // para que el template compile contra el MISMO motor sin redescargarlo.
    std::string projectName = currentProject->getConfig().name;
#ifdef HARUKA_ENGINE_ROOT
    std::string engineLocal = std::string(HARUKA_ENGINE_ROOT);
#else
    std::string engineLocal;
#endif
    std::string cmakeArgs;
    if (!projectName.empty()) {
        cmakeArgs += " -DPROJECT_NAME=" + projectName;
    }
    if (!engineLocal.empty()) {
        cmakeArgs += " -DHARUKA_ENGINE_LOCAL=\"" + engineLocal + "\"";
    }

    std::string compileCmd;
    if (std::filesystem::exists(std::filesystem::path(projectPath) / "build.sh")) {
        // Template moderno: el build.sh configura y compila el layout del juego.
        compileCmd = "cd \"" + projectPath + "\" && ./build.sh" + cmakeArgs;
    } else {
        // Fallback legacy (proyecto sin build.sh): build inline, pero con el paralelismo
        // ACOTADO POR RAM igual que el build.sh moderno — `-j$(nproc)` tiraba de swap.
        compileCmd = "cd \"" + projectPath + "\" && mkdir -p build && cd build && cmake .." +
                     cmakeArgs + " && make -j" + std::to_string(ramBoundedJobs());
    }
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
        currentFile.lastSaveTime = SDL_GetTicks() / 1000.0f;
        sceneDirty = false;
        timeSinceLastSave = 0.0f;
        
        std::string type = isPrefab ? "Prefab" : "Scene";
        std::cout << "✓ " << type << " saved: " << path << std::endl;
    } else {
        HARUKA_EDITOR_ERROR(ErrorCode::FAILED_TO_SAVE_FILE, "Failed to save: " + path);
    }
}

void EditorApplication::bindPanelsToScene() {
    Haruka::SceneManager* scene = currentScene.get();

    // Primero las SELECCIONES, y antes de repuntar: los paneles guardan `SceneObject*` crudos y un
    // índice, y ambos apuntan a la escena vieja. Limpiarlos después dejaría una ventana en la que
    // el panel ya tiene la escena nueva pero sigue con un puntero al objeto muerto.
    sceneHierarchyPanel.setSelectedObjectIndex(-1);
    inspectorPanel.setSelectedObjectIndex(-1);
    objectsPanel.setSelectedObjectIndex(-1);
    viewportPanel.setSelectedObjectIndex(-1);
    nodeGraphEditorPanel.setSelectedObject(nullptr);
    materialEditorPanel.setSelectedObject(nullptr);

    sceneHierarchyPanel.setScene(scene);
    inspectorPanel.setScene(scene);
    objectsPanel.setScene(scene);
    nodeGraphEditorPanel.setScene(scene);
    projectBrowserPanel.setScene(scene);
    viewportPanel.setScene(scene);   // el último: reinicia la Application del motor sobre la escena
}

void EditorApplication::loadFile(const std::string& path) {
    // RAÍZ DE ASSETS DEL PROYECTO: el path llega como <proyecto>/scenes/x.scene. El motor
    // resuelve sus assets contra la carpeta del EDITOR (fijada en init()); las texturas de
    // escena ("assets/textures/...") y las biomas por defecto de SimplePlanet viven en la del
    // PROYECTO. Se sube hasta hallar project.hrk y se fija esa raíz ANTES de que el viewport
    // hornee el motor (setScene → Application::init). Sin esto, cada textura del proyecto
    // falla bajo el editor y las biomas caen al fallback procedural.
    namespace fs = std::filesystem;
    fs::path projDir = fs::path(path).parent_path();
    while (!projDir.empty() && !fs::exists(projDir / "project.hrk")) {
        projDir = projDir.parent_path();
    }
    if (!projDir.empty())
        Haruka::AssetPaths::setProjectRoot(projDir.string() + "/");

    // La escena NUEVA se construye antes de soltar la vieja, y los paneles se repuntan enseguida
    // (bindPanelsToScene): en cuanto este unique_ptr suelta la anterior, cualquier panel que siga
    // apuntándola trabaja sobre memoria liberada.
    currentScene = std::make_unique<Haruka::SceneManager>();
    const bool loaded = currentScene->load(path);

    // INCONDICIONAL, y antes de decidir si fue bien: la escena anterior ya está destruida en la
    // línea de arriba. Repuntar solo en el camino de éxito dejaba a los paneles apuntando a memoria
    // liberada cuando el fichero no cargaba — el mismo use-after-free, en la rama que menos se prueba.
    bindPanelsToScene();

    if (loaded) {
        currentFile.path = path;
        currentFile.name = std::filesystem::path(path).stem().string();
        currentFile.isPrefab = (path.find(".prefab") != std::string::npos);
        currentFile.lastSaveTime = SDL_GetTicks() / 1000.0f;

        sceneDirty = false;
        timeSinceLastSave = 0.0f;

        std::string type = currentFile.isPrefab ? "Prefab" : "Scene";
        std::cout << "✓ " << type << " loaded: " << path << std::endl;
    } else {
        HARUKA_EDITOR_ERROR(ErrorCode::FAILED_TO_LOAD_FILE, "Failed to load file: " + path);
    }
}

void EditorApplication::createFileBackup(const std::string& filePath) {
    std::filesystem::path p(filePath);
    std::string backupDir = p.parent_path().string() + "/backups";
    std::filesystem::create_directories(backupDir);
    
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
        
        if (!isPrefab) {
            cleanOldBackups(filePath);
        }
        
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
        HARUKA_EDITOR_ERROR(ErrorCode::FAILED_TO_DELETE_FILE, "Error deleting backups: " + std::string(e.what()));
    }
}

std::string EditorApplication::getFileType(const std::string& path) {
    return (path.find(".prefab") != std::string::npos) ? "Prefab" : "Scene";
}

void EditorApplication::createSceneObject(const std::string& type) {
    if (!currentScene) return;
    std::string meshType = type;
    std::transform(meshType.begin(), meshType.end(), meshType.begin(), ::tolower);
    sceneHierarchyPanel.createPrimitive(type, meshType);
    sceneDirty = true;
}