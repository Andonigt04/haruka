#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <nfd.h>

#include "editor_app.h"
#include "core/camera.h"
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <cstdlib>
#include <filesystem>

// Definición débil para evitar linker error
Camera* g_gameCamera = nullptr;

EditorApplication::EditorApplication() : window(nullptr) {}

EditorApplication::~EditorApplication() {
    shutdown();
}

void EditorApplication::init() {
    if (!glfwInit()) {
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

    currentScene = std::make_unique<Haruka::Scene>("Untitled");
    currentProject = std::make_unique<Haruka::Project>();
    currentScenePath = "scenes/Untitled.scene";
    sceneHierarchyPanel.setScene(currentScene.get());
    sceneHierarchyPanel.setCommandHistory(&commandHistory);
    inspectorPanel.setScene(currentScene.get());
    inspectorPanel.setCommandHistory(&commandHistory);
    inspectorPanel.setOnSceneChanged([this]() {
        sceneDirty = true;
    });
    projectBrowserPanel.setProject(currentProject.get());
    projectBrowserPanel.setScene(currentScene.get());
    materialEditorPanel.setScene(currentScene.get());
    
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

    projectBrowserPanel.setOnSceneLoad([this](const std::string& sceneName) {
        if (!currentProject) return;
        const std::string path = currentProject->getPath() + "/scenes/" + sceneName + ".scene";

        if (sceneDirty) {
            pendingSceneToLoad = path;
            showUnsavedChangesPopup = true;
            return;
        }
        loadScene(path);
    });

    projectBrowserPanel.setOnSceneSave([this](const std::string& sceneName) {
        if (!currentProject) return;
        currentScenePath = currentProject->getPath() + "/scenes/" + sceneName + ".scene";
    });

    projectBrowserPanel.setOnSceneNew([this](const std::string& sceneName) {
        if (!currentProject) return;
        currentScenePath = currentProject->getPath() + "/scenes/" + sceneName + ".scene";
    });

    sceneHierarchyPanel.setOnObjectSelectedByIndex([this](int index) {
        inspectorPanel.setSelectedObjectIndex(index);
    });

    sceneHierarchyPanel.setOnObjectSelectedByName([this](const std::string& name) {
        materialEditorPanel.setSelectedObject(name);
    });

    // Capture cout & cerr
    coutCapture = std::make_unique<StreamCapture>(std::cout, &consolePanel, LogLevel::Info);
    cerrCapture = std::make_unique<StreamCapture>(std::cerr, &consolePanel, LogLevel::Error);

    std::cout << "Haruka Editor initialized successfully" << std::endl;

}

void EditorApplication::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window) {
        glfwDestroyWindow(window);
    }
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
    
    viewportPanel.onUpdate(deltaTime);
}

void EditorApplication::updatePlayMode(float deltaTime) {
    if (!isPlayMode) return;
    
    playModeTime += deltaTime;
}

void EditorApplication::render() {
    // Actualizar título de ventana con dirty flag
    std::string title = "Haruka Editor";
    if (currentScene) {
        title += " - " + currentScene->getName();
    }
    if (sceneDirty) {
        title += " *";
    }
    glfwSetWindowTitle(window, title.c_str());

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    renderUI();

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

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

    // Save As popup (fuera del menú)
    if (showSaveAsPopup) ImGui::OpenPopup("Save Scene As");
    if (ImGui::BeginPopupModal("Save Scene As", &showSaveAsPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Path", saveAsBuffer, sizeof(saveAsBuffer));
        if (ImGui::Button("Save")) {
            currentScenePath = saveAsBuffer;
            saveScene(currentScenePath);
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
            if (!currentScenePath.empty()) saveScene(currentScenePath);
            if (!pendingSceneToLoad.empty()) loadScene(pendingSceneToLoad);
            pendingSceneToLoad.clear();
            showUnsavedChangesPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Descartar")) {
            if (!pendingSceneToLoad.empty()) loadScene(pendingSceneToLoad);
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
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project")) {
                std::snprintf(newProjectNameBuffer, sizeof(newProjectNameBuffer), "NewProject");
                std::snprintf(newProjectPathBuffer, sizeof(newProjectPathBuffer), 
                             "/mnt/sdb1/haruka/projects/");
                showNewProjectDialog = true;
            }

            if (ImGui::MenuItem("Open Project", "Ctrl+O")) {
                nfdchar_t* outPath = nullptr;
                nfdresult_t result = NFD_PickFolder(nullptr, &outPath);

                if (result == NFD_OKAY) {
                    if (currentProject) {
                        const std::string selectedPath(outPath);
                        currentProject->load(selectedPath);
                        projectBrowserPanel.setProject(currentProject.get());
                        projectBrowserPanel.setScene(currentScene.get());
                        std::cout << "Project loaded: " << selectedPath << std::endl;
                    }
                    free(outPath);
                } else if (result == NFD_CANCEL) {
                    std::cout << "User cancelled folder selection" << std::endl;
                } else {
                    std::cerr << "Error selecting folder: " << NFD_GetError() << std::endl;
                }
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (currentScenePath.empty()) {
                    if (currentProject && !currentProject->getPath().empty() && currentScene) {
                        currentScenePath = currentProject->getPath() + "/scenes/" + currentScene->getName() + ".scene";
                    } else {
                        currentScenePath = "scenes/current.scene";
                    }
                }
                saveScene(currentScenePath);
            }

            if (ImGui::MenuItem("Save Scene As...")) {
                std::snprintf(saveAsBuffer, sizeof(saveAsBuffer), "%s", currentScenePath.c_str());
                showSaveAsPopup = true;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                glfwSetWindowShouldClose(window, true);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Move Gizmo", "W", gizmoMode == 0)) {
                gizmoMode = 0;
            }
            if (ImGui::MenuItem("Rotate Gizmo", "E", gizmoMode == 1)) {
                gizmoMode = 1;
            }
            if (ImGui::MenuItem("Scale Gizmo", "R", gizmoMode == 2)) {
                gizmoMode = 2;
            }
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
}

void EditorApplication::saveScene(const std::string& path) {
    if (currentScene) {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        bool ok = currentScene->save(path);
        if (ok) {
            currentScenePath = path;
            sceneDirty = false;
            std::cout << "Scene saved to: " << path << std::endl;
        } else {
            std::cerr << "Failed to save scene" << std::endl;
        }
    }
}

void EditorApplication::loadScene(const std::string& path) {
    if (!currentScene) {
        currentScene = std::make_unique<Haruka::Scene>();
    }

    if (currentScene->load(path)) {
        currentScenePath = path;
        sceneDirty = false;
        std::cout << "Scene loaded: " << path << std::endl;
        sceneHierarchyPanel.setScene(currentScene.get());
        inspectorPanel.setScene(currentScene.get());
        viewportPanel.setScene(currentScene.get());
        projectBrowserPanel.setScene(currentScene.get());
    } else {
        std::cerr << "Failed to load scene: " << path << std::endl;
    }
}

void EditorApplication::enterPlayMode() {
    if (isPlayMode || !currentProject || !currentScene) return;
    
    editorScene = currentScene.get();
    isPlayMode = true;
    playModeTime = 0.0f;
    
    viewportPanel.setPlayMode(true);
    inspectorPanel.setPlayMode(true);

    if (currentScene) {
        currentScene->save(playModeBackupPath);
    }

    extern Camera* g_gameCamera;
    if (g_gameCamera) {
        viewportPanel.setCamera(g_gameCamera);
        std::cout << "Game camera set to viewport" << std::endl;
    }

    std::cout << "▶ Play Mode started" << std::endl;
}

void EditorApplication::exitPlayMode() {
    if (!isPlayMode || !editorScene) return;
    
    isPlayMode = false;

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
        std::filesystem::create_directories(projectPath);
        std::filesystem::create_directories(projectPath + "/scenes");
        std::filesystem::create_directories(projectPath + "/assets");
        std::filesystem::create_directories(projectPath + "/prefabs");

        // Crear archivo project.hrk (config JSON)
        nlohmann::json projectConfig;
        projectConfig["name"] = name;
        projectConfig["version"] = "1.0.0";
        projectConfig["created"] = "2026-03-11";

        std::ofstream configFile(projectPath + "/project.hrk");
        if (configFile.is_open()) {
            configFile << projectConfig.dump(2);
            configFile.close();
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

        currentScenePath = scenePath;
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