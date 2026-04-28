#include "editor_app.h"

#include "core/application.h"
#include "core/camera.h"
#include "core/error_reporter.h"
#include "core/components/mesh_renderer_component.h"
#include "renderer/primitive_shapes.h"
#include "renderer/motor_instance.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <nfd.h>
#include <dlfcn.h>

#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/wait.h>
#include <signal.h>

EditorApplication::EditorApplication(IEngine* eng)
    : engine(eng), window(nullptr) {}

EditorApplication::~EditorApplication() {
    shutdown();
}

// ─── init ─────────────────────────────────────────────────────────────────────
void EditorApplication::init() {
    // 1. SDL — solo la ventana, sin contexto gráfico
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }

    window = SDL_CreateWindow(
        "Haruka Editor",
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    if (!window) {
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
    }

    // 2. Motor inicializa TODO lo gráfico (Vulkan, swapchain, render pass…)
    auto result = engine->init(window);
    if (!result.success) {
        throw std::runtime_error(std::string("Engine init failed: ") +
                                 (result.errorMsg ? result.errorMsg : "unknown"));
    }

    // 3. ImGui — contexto y estilo (sin tocar Vulkan)
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

    // 4. El motor conecta ImGui a Vulkan (ImGui_ImplVulkan_Init internamente)
    if (!engine->initImGui()) {
        throw std::runtime_error("engine->initImGui() failed");
    }
    imguiInitialized = true;

    // 5. Escena y proyecto
    currentScene   = std::make_unique<Haruka::Scene>("Untitled");
    currentProject = std::make_unique<Haruka::Project>();
    currentFile.path    = "scenes/Untitled.scene";
    currentFile.name    = "Untitled";
    currentFile.isPrefab = false;

    // 6. Paneles
    sceneHierarchyPanel.setScene(currentScene.get());
    sceneHierarchyPanel.setCommandHistory(&commandHistory);
    inspectorPanel.setScene(currentScene.get());
    inspectorPanel.setCommandHistory(&commandHistory);
    inspectorPanel.setOnSceneChanged([this]() { sceneDirty = true; });
    projectBrowserPanel.setProject(currentProject.get());
    projectBrowserPanel.setScene(currentScene.get());
    planetTerrainEditorPanel.setScene(currentScene.get());

    if (MotorInstance::getInstance().getApplication()) {
        planetTerrainEditorPanel.setPlanetarySystem(
            MotorInstance::getInstance().getApplication()->getPlanetarySystem());
    }

    // 7. Cámara
    viewportCamera = std::make_unique<Camera>(Haruka::WorldPos(0.0f, 5.0f, 15.0f));
    viewportCamera->orientation = glm::angleAxis(glm::radians(0.0f), glm::vec3(0, 1, 0));

    viewportPanel.setScene(currentScene.get());
    viewportPanel.setCamera(viewportCamera.get());
    viewportPanel.setStatsPanel(&statsPanel);
    viewportPanel.setPlayMode(false);

    editorCamPos = viewportCamera->position;
    editorCamRot = viewportCamera->orientation;

    // 8. Callbacks
    projectBrowserPanel.setOnFileLoad([this](const std::string& path) {
        if (sceneDirty) {
            pendingSceneToLoad = path;
            showUnsavedChangesPopup = true;
        } else {
            loadFile(path);
        }
    });

    sceneHierarchyPanel.setOnObjectSelectedByIndex([this](int index) {
        if (currentScene && index >= 0 &&
            index < (int)currentScene->getObjects().size()) {
            inspectorPanel.setSelectedObjectIndex(index);
            viewportPanel.setSelectedObjectIndex(index);
        }
    });

    sceneHierarchyPanel.setOnObjectSelectedByName([this](const std::string& name) {
        if (!name.empty() && currentScene) {
            auto obj = currentScene->getObject(name);
            if (obj) materialEditorPanel.setSelectedObject(obj);
        }
    });

    // 9. Stream capture
    coutCapture = std::make_unique<StreamCapture>(std::cout, &consolePanel, LogLevel::Info);
    cerrCapture = std::make_unique<StreamCapture>(std::cerr, &consolePanel, LogLevel::Error);

    settingsPanel.load();
    menuBar = std::make_unique<MenuBar>(this);

    std::cout << "✓ Haruka Editor initialized" << std::endl;
}

// ─── shutdown ─────────────────────────────────────────────────────────────────
void EditorApplication::shutdown() {
    coutCapture.reset();
    cerrCapture.reset();

    if (gameInterface && gameInterface->onShutdown)
        gameInterface->onShutdown();
    gameInterface = nullptr;
    if (gameLibHandle) {
        dlclose(gameLibHandle);
        gameLibHandle = nullptr;
    }

    if (imguiInitialized) {
        // El motor hace shutdown de ImGui_ImplVulkan internamente
        engine->shutdown();
        ImGui::DestroyContext();
        imguiInitialized = false;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}

// ─── run ──────────────────────────────────────────────────────────────────────
void EditorApplication::run() {
    init();

    bool running = true;
    while (running) {
        // Procesar eventos SDL
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                int w, h;
                SDL_GetWindowSize(window, &w, &h);
                engine->onResize(w, h);
            }
        }

        update();
        render();
    }
}

// ─── update ──────────────────────────────────────────────────────────────────
void EditorApplication::update() {
    float now = (float)SDL_GetTicks() / 1000.0f;
    deltaTime = now - lastFrame;
    lastFrame = now;

    statsPanel.update(deltaTime);
    updatePlayMode(deltaTime);
    planetTerrainEditorPanel.update();
    exportPanel.update();
    viewportPanel.onUpdate(deltaTime);

    if (autoSaveEnabled && sceneDirty && !currentFile.path.empty()) {
        timeSinceLastSave += deltaTime;
        if (timeSinceLastSave >= autoSaveInterval) {
            saveFile(currentFile.path, currentFile.isPrefab);
            timeSinceLastSave = 0.0f;
        }
    }

    // Estadísticas del motor → panel de stats
    auto stats = engine->getStats();
    statsPanel.setVertexCount(stats.renderedVertices);
    statsPanel.setDrawCalls(stats.drawCalls);
    statsPanel.setTriangleCount(stats.renderedTriangles);
    statsPanel.setVisibleChunkCount(stats.visibleChunks);
}

// ─── render ──────────────────────────────────────────────────────────────────
void EditorApplication::render() {
    // El motor adquiere imagen del swapchain y arranca ImGui_ImplVulkan_NewFrame
    if (!engine->beginFrame()) return;

    // El IDE solo toca ImGui::NewFrame en adelante
    ImGui::NewFrame();
    renderUI();
    ImGui::Render();

    // El motor dibuja los draw data de ImGui en Vulkan y presenta
    engine->renderImGui(ImGui::GetDrawData());
    engine->endFrame();

    // Multi-viewport de ImGui (ventanas flotantes fuera de la ventana principal)
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}

// ─── renderUI — igual que antes, sin cambios ─────────────────────────────────
void EditorApplication::renderUI() {
    try {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags wf = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, wf);
        ImGui::PopStyleVar(3);

        ImGuiID dsid = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dsid, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
        menuBar->render();
        ImGui::End();

        if (isPlayMode) ImGui::BeginDisabled();

        auto safePanel = [&](auto& panel, const char* name) {
            try { panel.onImGuiRender(); }
            catch (const std::exception& e) {
                HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED,
                    std::string(name) + " crash: " + e.what());
            }
        };

        if (showProjectBrowser)  safePanel(projectBrowserPanel,  "ProjectBrowser");
        if (showSceneHierarchy)  safePanel(sceneHierarchyPanel,  "SceneHierarchy");
        if (showInspector)       safePanel(inspectorPanel,        "Inspector");
        if (showConsole)         safePanel(consolePanel,          "Console");
        if (showStats)           safePanel(statsPanel,            "Stats");
        if (showMaterialEditor)  safePanel(materialEditorPanel,   "MaterialEditor");
        if (showPlanetTerrainEditor) safePanel(planetTerrainEditorPanel, "PlanetTerrainEditor");

        if (isPlayMode) ImGui::EndDisabled();

        if (showViewport) safePanel(viewportPanel, "Viewport");
        viewportPanel.setGizmoMode(gizmoMode);

        if (showDemoWindow) ImGui::ShowDemoWindow(&showDemoWindow);
        if (showSettings)      safePanel(settingsPanel,   "Settings");
        if (showAssetImporter) safePanel(assetImporter,   "AssetImporter");
        if (showSearchPanel)   safePanel(searchPanel,     "SearchPanel");
        if (showUIBuilder)     safePanel(uiBuilder,       "UIBuilder");

        // Popups
        exportPanel.render(this);

        if (showSaveAsPopup) ImGui::OpenPopup("Save File As");
        if (ImGui::BeginPopupModal("Save File As", &showSaveAsPopup,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::InputText("Path##save", saveAsBuffer, sizeof(saveAsBuffer));
            if (ImGui::Button("Save")) {
                bool isPrefab = std::string(saveAsBuffer).find(".prefab") != std::string::npos;
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

        if (showUnsavedChangesPopup) ImGui::OpenPopup("Unsaved Changes");
        if (ImGui::BeginPopupModal("Unsaved Changes", &showUnsavedChangesPopup,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Hay cambios sin guardar.");
            if (ImGui::Button("Guardar y continuar")) {
                if (!currentFile.path.empty())
                    saveFile(currentFile.path, currentFile.isPrefab);
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

        if (showNewProjectDialog) ImGui::OpenPopup("New Project");
        if (ImGui::BeginPopupModal("New Project", &showNewProjectDialog,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
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
        HARUKA_EDITOR_ERROR(ErrorCode::EDITOR_INIT_FAILED,
            "RenderUI crash: " + std::string(e.what()));
    }
}

// ─── El resto de métodos (enterPlayMode, saveFile, etc.) son idénticos ────────
// a la versión OpenGL — solo cambia glfwGetTime() por SDL_GetTicks()/1000.0f
// y glfwSetWindowTitle por SDL_SetWindowTitle.

void EditorApplication::updatePlayMode(float dt) {
    if (!isPlayMode) return;
    playModeTime += dt;
    if (gameInterface && gameInterface->onUpdate)
        gameInterface->onUpdate(nullptr, dt);
}

void EditorApplication::enterPlayMode() {
    if (isPlayMode || !currentProject) return;
    isPlayMode = true;
    playModeTime = 0.0f;
    viewportPanel.setPlayMode(true);
    inspectorPanel.setPlayMode(true);
    std::cout << "▶ Play Mode started" << std::endl;
}

void EditorApplication::exitPlayMode() {
    if (!isPlayMode) return;
    isPlayMode = false;
    gameInterface = nullptr;
    viewportPanel.setPlayMode(false);
    inspectorPanel.setPlayMode(false);
    std::cout << "⏹ Play Mode stopped" << std::endl;
}

void EditorApplication::createSceneObject(const std::string& type) {
    if (!currentScene) return;
    sceneHierarchyPanel.createPrimitive(type, type);
    sceneDirty = true;
}

void EditorApplication::compileProject() {
    if (!currentProject || isProjectCompiling) return;
    isProjectCompiling = true;
    std::string cmd = "cd " + currentProject->getPath() +
                      " && mkdir -p build && cd build && cmake .. && make -j$(nproc)";
    int r = system(cmd.c_str());
    std::cout << (r == 0 ? "✓ Compiled" : "✗ Compile failed") << std::endl;
    isProjectCompiling = false;
}

void EditorApplication::exportGame()        { exportPanel.show(); }
void EditorApplication::showExportDialog()  { exportPanel.render(this); }

void EditorApplication::saveFile(const std::string& path, bool) {
    if (!currentScene || path.empty()) return;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    if (std::filesystem::exists(path)) createFileBackup(path);
    if (currentScene->save(path)) {
        currentFile.path     = path;
        currentFile.name     = std::filesystem::path(path).stem().string();
        currentFile.isPrefab = path.find(".prefab") != std::string::npos;
        sceneDirty           = false;
        timeSinceLastSave    = 0.0f;
        std::cout << "✓ Saved: " << path << std::endl;
    }
}

void EditorApplication::loadFile(const std::string& path) {
    currentScene = std::make_unique<Haruka::Scene>();
    if (currentScene->load(path)) {
        currentFile.path     = path;
        currentFile.name     = std::filesystem::path(path).stem().string();
        currentFile.isPrefab = path.find(".prefab") != std::string::npos;
        sceneDirty           = false;
        timeSinceLastSave    = 0.0f;
        sceneHierarchyPanel.setScene(currentScene.get());
        inspectorPanel.setScene(currentScene.get());
        viewportPanel.setScene(currentScene.get());
        planetTerrainEditorPanel.setScene(currentScene.get());
        sceneHierarchyPanel.setSelectedObjectIndex(-1);
        inspectorPanel.setSelectedObjectIndex(-1);
        std::cout << "✓ Loaded: " << path << std::endl;
    }
}

void EditorApplication::createNewProject(const std::string& name, const std::string& basePath) {
    if (name.empty() || basePath.empty()) return;
    try {
        std::string projectPath = basePath + name;
        std::filesystem::create_directories(projectPath + "/scenes");
        std::filesystem::create_directories(projectPath + "/assets");
        currentProject = std::make_unique<Haruka::Project>();
        currentProject->load(projectPath);
        projectBrowserPanel.setProject(currentProject.get());
        std::cout << "✓ Project created: " << projectPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ " << e.what() << std::endl;
    }
}

void EditorApplication::createFileBackup(const std::string& filePath) {
    namespace fs = std::filesystem;
    fs::path p(filePath);
    std::string backupDir = p.parent_path().string() + "/backups";
    fs::create_directories(backupDir);
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char ts[20];
    std::strftime(ts, sizeof(ts), "%Y%m%d_%H%M%S", std::localtime(&time));
    std::string backup = backupDir + "/" + p.stem().string() + "_" + ts + p.extension().string();
    try {
        fs::copy_file(filePath, backup, fs::copy_options::overwrite_existing);
        cleanOldBackups(filePath);
    } catch (...) {}
}

void EditorApplication::cleanOldBackups(const std::string& filePath) {
    namespace fs = std::filesystem;
    fs::path p(filePath);
    std::string backupDir = p.parent_path().string() + "/backups";
    std::string stem = p.stem().string();
    std::string ext  = p.extension().string();
    std::vector<fs::path> backups;
    for (const auto& e : fs::directory_iterator(backupDir)) {
        if (e.is_regular_file() &&
            e.path().stem().string().find(stem) != std::string::npos &&
            e.path().extension().string() == ext)
            backups.push_back(e.path());
    }
    std::sort(backups.begin(), backups.end(), [](const auto& a, const auto& b) {
        return fs::last_write_time(a) > fs::last_write_time(b);
    });
    for (size_t i = maxBackups; i < backups.size(); ++i)
        fs::remove(backups[i]);
}

void EditorApplication::deleteAllBackups(const std::string& filePath) {
    namespace fs = std::filesystem;
    fs::path p(filePath);
    std::string backupDir = p.parent_path().string() + "/backups";
    std::string stem = p.stem().string();
    std::string ext  = p.extension().string();
    if (!fs::exists(backupDir)) return;
    for (const auto& e : fs::directory_iterator(backupDir)) {
        if (e.is_regular_file() &&
            e.path().stem().string().find(stem) != std::string::npos &&
            e.path().extension().string() == ext)
            fs::remove(e.path());
    }
}

std::string EditorApplication::getFileType(const std::string& path) {
    return path.find(".prefab") != std::string::npos ? "Prefab" : "Scene";
}