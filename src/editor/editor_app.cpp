#define IMGUI_IMPL_OPENGL_LOADER_GLAD

#include "editor_app.h"
#include "core/camera.h"
#include "core/error_reporter.h"
#include "core/components/mesh_renderer_component.h"
#include "renderer/primitive_shapes.h"

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
#include <vector>

EditorApplication::EditorApplication() : window(nullptr) {}

EditorApplication::~EditorApplication() {
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
        if (currentScene && index >= 0 && index < (int)currentScene->getObjects().size()) {
            inspectorPanel.setSelectedObjectIndex(index);
        }
    });

    sceneHierarchyPanel.setOnObjectSelectedByName([this](const std::string& name) {
        if (!name.empty() && currentScene) {
            auto obj = currentScene->getObject(name);
            if (obj) {
                materialEditorPanel.setSelectedObject(obj);
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
        currentFile.isPrefab = false;
        sceneDirty = false;

        // Sincronizar paneles
        projectBrowserPanel.setProject(currentProject.get());
        projectBrowserPanel.setScene(currentScene.get());
        sceneHierarchyPanel.setScene(currentScene.get());
        inspectorPanel.setScene(currentScene.get());
        viewportPanel.setScene(currentScene.get());

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
        HARUKA_EDITOR_ERROR(ErrorCode::FAILED_TO_SAVE_FILE, "Failed to save: " + path);
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
        
        // Resetear selección a ningún objeto
        sceneHierarchyPanel.setSelectedObjectIndex(-1);
        inspectorPanel.setSelectedObjectIndex(-1);
        
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
    
    sceneHierarchyPanel.createPrimitive(type, type);
    sceneDirty = true;
}