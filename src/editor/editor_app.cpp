#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "editor_app.h"
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <cstdlib>

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
    sceneHierarchyPanel.setScene(currentScene.get());
    sceneHierarchyPanel.setCommandHistory(&commandHistory);
    inspectorPanel.setScene(currentScene.get());
    inspectorPanel.setCommandHistory(&commandHistory);
    projectBrowserPanel.setProject(currentProject.get());
    sceneManagerPanel.setScene(currentScene.get());
    sceneManagerPanel.setProject(currentProject.get());

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

    // Capture cout & cerr
    coutCapture = std::make_unique<StreamCapture>(std::cout, &consolePanel, LogLevel::Info);
    cerrCapture = std::make_unique<StreamCapture>(std::cerr, &consolePanel, LogLevel::Error);

    std::cout << "Haruka Editor initialized successfully" << std::endl;

    auto& net = Haruka::NetworkManager::getInstance();
    net.initClient("127.0.0.1", 8082);

    const char* nameEnv = std::getenv("HARUKA_CHAT_NAME");
    const char* localEnv = std::getenv("HARUKA_CHAT_LOCAL");
    const char* remoteEnv = std::getenv("HARUKA_CHAT_REMOTE");

    std::string chatName = nameEnv ? nameEnv : "Player";
    int localPort = localEnv ? std::atoi(localEnv) : 5000;
    int remotePort = remoteEnv ? std::atoi(remoteEnv) : 5001;

    inGameChat = std::make_unique<Haruka::InGameChat>(chatName, localPort, remotePort);
    inGameChat->setNetworkClient(net.getClient());
    inGameChat->setUseNetwork(true);
    inGameChat->init();
}

void EditorApplication::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window) {
        glfwDestroyWindow(window);
    }
    glfwTerminate();

    if (inGameChat) {
        inGameChat->shutdown();
    }
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

    // Toggle chat con T
    if (ImGui::IsKeyPressed(ImGuiKey_T) && !ImGui::GetIO().WantCaptureKeyboard) {
        if (inGameChat) {
            inGameChat->toggleChat();
        }
    }
    
    if (inGameChat) {
        inGameChat->update(deltaTime);
    }
}

void EditorApplication::render() {
    viewportPanel.onUpdate(deltaTime);

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

    sceneHierarchyPanel.onImGuiRender();
    inspectorPanel.setSelectedObjectIndex(sceneHierarchyPanel.getSelectedObjectIndex());
    inspectorPanel.onImGuiRender();
    projectBrowserPanel.onImGuiRender();

    if (projectBrowserPanel.isSceneSelected()) {
        loadScene(projectBrowserPanel.getSelectedScenePath());
        projectBrowserPanel.resetSceneSelection();
    }
    consolePanel.onImGuiRender();
    statsPanel.onImGuiRender();

    if (isPlayMode) ImGui::EndDisabled();

    viewportPanel.onImGuiRender();
    viewportPanel.setGizmoMode(gizmoMode);

    if (showDemoWindow) {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }

    // Chat
    if (inGameChat) {
        inGameChat->render();
    }
}

void EditorApplication::showMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project", "Ctrl+N")) {
                std::cout << "New Project" << std::endl;
            }
            if (ImGui::MenuItem("Open Project", "Ctrl+O")) {
                if (currentProject) {
                    currentProject->load("/mnt/sdb1/haruka/projects/MyProject");
                    std::cout << "Project loaded" << std::endl;
                }
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                if (currentProject && !currentProject->getPath().empty()) {
                    std::string scenePath = currentProject->getPath() + "/scenes/main.scene";
                    saveScene(scenePath);
                } else {
                    saveScene("scenes/current.scene");
                }
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

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                std::cout << "Haruka Engine Editor v0.1" << std::endl;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Play")) {
            if (!isPlayMode) {
                if (ImGui::MenuItem("Play")) {
                    isPlayMode = true;

                    // Guardar escena antes de play
                    if (currentProject && !currentProject->getPath().empty()) {
                        std::string scenePath = currentProject->getPath() + "/scenes/main.scene";
                        saveScene(scenePath);
                    }

                    // Guardar estado de cámara
                    editorCamPos = viewportCamera->position;
                    editorCamRot = viewportCamera->orientation;

                    viewportPanel.setPlayMode(true);
                }
            } else {
                if (ImGui::MenuItem("Stop")) {
                    isPlayMode = false;

                    // Restaurar cámara
                    viewportCamera->position = editorCamPos;
                    viewportCamera->orientation = editorCamRot;

                    viewportPanel.setPlayMode(false);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void EditorApplication::saveScene(const std::string& path) {
    if (currentScene) {
        bool ok = currentScene->save(path);
        if (ok) {
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
        std::cout << "Scene loaded: " << path << std::endl;
        sceneHierarchyPanel.setScene(currentScene.get());
        inspectorPanel.setScene(currentScene.get());
        viewportPanel.setScene(currentScene.get());
    } else {
        std::cerr << "Failed to load scene: " << path << std::endl;
    }
}