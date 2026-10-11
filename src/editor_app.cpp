#define IMGUI_IMPL_OPENGL_LOADER_GLAD

#include "editor_app.h"
#include "engine_sdk.h"       // motor activo: template, build y runtime (resuelto en runtime)

#include <glad/glad.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
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

EditorApplication::EditorApplication(std::unique_ptr<IEngineBackend> b) : backend(std::move(b)) {}

EditorApplication::~EditorApplication() {
    shutdown();
}

void EditorApplication::init() {
    // ===== SDL3 & GLAD Setup =====
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
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

    // Resolver el motor AQUÍ y no en el primer uso: es lo que decide si "Nuevo proyecto" y
    // "Compilar" van a funcionar, y enterarse al arrancar (con la lista de rutas miradas si
    // no hay ninguno) ahorra el viaje de crear un proyecto para descubrir que no hay motor.
    EngineSdk::current();

    // ===== ImGui Setup =====
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    // Layout de paneles en el directorio de config del USUARIO, no en el cwd. ImGui escribe
    // imgui.ini relativo al cwd, y el editor no hace `cd`: instalado en /usr, arrancado desde
    // el menú (cwd = $HOME) o desde una carpeta sin permiso de escritura, el layout se perdía
    // entre sesiones o se dejaba un imgui.ini suelto donde tocara. La cadena es static porque
    // ImGui guarda el puntero, no copia.
    static std::string iniPath;
    {
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        const char* home = std::getenv("HOME");
        std::filesystem::path dir;
        if (xdg && *xdg)        dir = std::filesystem::path(xdg) / "haruka-editor";
        else if (home && *home) dir = std::filesystem::path(home) / ".config" / "haruka-editor";
        if (!dir.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            if (!ec) {
                iniPath = (dir / "imgui.ini").string();
                io.IniFilename = iniPath.c_str();
            }
        }
    }

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

    // ===== Consola: desde aquí todo lo que se imprima sale en el panel =====
    coutCapture = std::make_unique<StreamCapture>(std::cout, &consolePanel, LogLevel::Info);
    cerrCapture = std::make_unique<StreamCapture>(std::cerr, &consolePanel, LogLevel::Error);

    projectBrowserPanel.setOnFileLoad([this](const std::string& path) { openDocument(path); });

    // ===== Paneles: núcleo, backend y plugins, en este orden =====
    // ⚠️ EL ORDEN DE REGISTRO ES EL DE DIBUJO (panel_registry.h). El backend mete los suyos en
    // medio para conservar el orden que tenían cuando todo vivía aquí.
    registerCorePanelsBefore();
    // El backend se carga AQUÍ y no en main(): comprueba al cargarse que su ImGui es este, y para
    // eso el contexto tiene que existir ya.
    if (!backend) backend = backendLoader.load(EngineSdk::current().editorBackend);
    if (backend) {
        EditorServices services;
        services.panels = &panels;
        services.history = &commandHistory;
        services.window = window;
        services.markDirty = [this]() { sceneDirty = true; };
        services.requestOpenDocument = [this](const std::string& path) { openDocument(path); };
        if (!backend->init(services)) {
            std::cerr << "[Editor] el backend '" << backend->name() << "' no arranco: sigo sin motor" << std::endl;
            backend->shutdown();
            backend.reset();
        } else {
            std::cout << "[Editor] backend: " << backend->name() << std::endl;
            registerGenericScenePanels();
        }
    }
    registerCorePanelsAfter();

    // Plugins DESPUÉS de los paneles del editor (sus ids ganan si chocan) y con ImGui ya creado:
    // el plugin recibe este contexto. Si ya hay proyecto o escena, se los da `registerPanel`.
    plugins.setEngineName(EngineSdk::current().name);
    plugins.setContextQueries(
        [this]() { return projectRoot(); },
        [this]() -> void* { return backend ? backend->sceneHandle() : nullptr; });
    plugins.scan(PluginManager::defaultSearchDirs(EngineSdk::current().editorPluginsDir));
    syncPanelsToProject();

    if (backend) currentFile.path = backend->untitledDocument();
    currentFile.name = "Untitled";

    menuBar = std::make_unique<MenuBar>(this);

    std::cout << "✓ Haruka Editor initialized" << std::endl;

    // ── Arranque sin manos (para probar sin clicar) ────────────────────────────────────────────
    //   HARUKA_EDITOR_PROJECT=<carpeta>  abre ese proyecto y su documento por defecto
    //   HARUKA_EDITOR_OPEN=world,prefab   abre esos paneles (ids del registro)
    //   HARUKA_EDITOR_QUIT_AFTER=<s>      cierra solo a los s segundos
    if (const char* pr = std::getenv("HARUKA_EDITOR_PROJECT"); pr && *pr && backend) {
        const std::string root(pr);
        openProject(root);
        if (const std::string doc = backend->defaultDocument(root); !doc.empty()) loadFile(doc);
        std::cout << "[Editor] proyecto abierto sin manos: " << root << std::endl;
    }
    if (const char* op = std::getenv("HARUKA_EDITOR_OPEN"); op && *op) panels.openFromList(op);
    if (const char* q = std::getenv("HARUKA_EDITOR_QUIT_AFTER")) autoQuitAt = std::atof(q);
}

void EditorApplication::registerCorePanelsBefore() {
    PanelEntry e{"browser", "Project Browser", 0, true};
    e.draw = [this] { projectBrowserPanel.onImGuiRender(); };
    e.onProjectChanged = [this](const std::string& root) { projectBrowserPanel.setProjectRoot(root); };
    panels.add(std::move(e));
}

void EditorApplication::registerGenericScenePanels() {
    // Un motor que el núcleo no conoce (remoto, simulado) describe su escena con ISceneModel y el
    // núcleo pone la jerarquía y el inspector. Haruka trae los suyos y devuelve nullptr.
    ISceneModel* model = backend ? backend->genericScene() : nullptr;
    if (!model) return;
    genericHierarchy.setModel(model);
    genericInspector.setModel(model);
    genericInspector.setHistory(&commandHistory);
    genericInspector.setOnChanged([this]() { sceneDirty = true; });
    {
        PanelEntry e{"hierarchy", "Scene Hierarchy", 0, true};
        e.draw = [this] { genericHierarchy.onImGuiRender(); };
        panels.add(std::move(e));
    }
    {
        PanelEntry e{"inspector", "Inspector", 0, true};
        e.draw = [this] { genericInspector.onImGuiRender(); };
        panels.add(std::move(e));
    }
}

void EditorApplication::registerCorePanelsAfter() {
    {
        PanelEntry e{"console", "Console", 0, true};
        e.draw = [this] { consolePanel.onImGuiRender(); };
        panels.add(std::move(e));
    }
    {
        PanelEntry e{"imgui-demo", "ImGui Demo", 2, false, false};
        e.draw = [this] { ImGui::ShowDemoWindow(panels.openFlag("imgui-demo")); };
        panels.add(std::move(e));
    }
    {
        PanelEntry e{"importer", "Asset Importer", 1, false, false};
        e.draw = [this] { assetImporter.onImGuiRender(); };
        panels.add(std::move(e));
    }
    {
        PanelEntry e{"search", "Search Panel", 1, false, false};
        e.draw = [this] { searchPanel.onImGuiRender(); };
        panels.add(std::move(e));
    }
    {
        PanelEntry e{"uibuilder", "UI Builder", 1, false, false};
        e.draw = [this] { uiBuilder.onImGuiRender(); };
        panels.add(std::move(e));
    }
    {
        // Qué plugins hay, cuáles fallaron y por qué; cargar los nuevos sin reiniciar.
        PanelEntry e{"plugins", "Plugins", 2, false, false};
        e.draw = [this] { plugins.drawPanel(); };
        panels.add(std::move(e));
    }
}

void EditorApplication::shutdown() {
    // Restaurar streams primero (evita escritura concurrente a consola durante teardown)
    coutCapture.reset();
    cerrCapture.reset();

    // Plugins y backend antes que ImGui: su shutdown puede tocar el contexto, y sus paneles salen
    // del registro. El backend libera además sus recursos GL antes de que caiga el contexto.
    plugins.unloadAll();
    if (backend) backend->shutdown();

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
        if (autoQuitAt > 0.0 && SDL_GetTicks() / 1000.0 >= autoQuitAt) running = false;
    }
}

void EditorApplication::update() {
    float currentFrame = static_cast<float>(SDL_GetTicks() / 1000.0f);
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    if (backend) backend->update(deltaTime);

    // Auto-save system
    if (autoSaveEnabled && sceneDirty && !currentFile.path.empty() && backend) {
        timeSinceLastSave += deltaTime;
        if (timeSinceLastSave >= autoSaveInterval) {
            saveFile(currentFile.path);
            timeSinceLastSave = 0.0f;
        }
    }
}

void EditorApplication::render() {
    // Update window title with dirty flag
    std::string title = "Haruka Editor";
    if (!currentFile.path.empty()) {
        title += " - " + currentFile.name;
        if (backend) {
            const std::string kind = backend->documentKind(currentFile.path);
            if (!kind.empty()) title += " (" + kind + ")";
        }
    }
    if (sceneDirty) title += " *";
    SDL_SetWindowTitle(window, title.c_str());

    // ImGui frame setup
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (backend) backend->beginFrame(deltaTime);

    renderUI();

    // Render
    ImGui::Render();
    int display_w, display_h;
    SDL_GetWindowSizeInPixels(window, &display_w, &display_h);
    // La PANTALLA, explícitamente. El backend ha podido dibujar su viewport sobre el FBO de un
    // RenderTarget; en GL el fin de un pase no desata nada, así que sin este bind la UI entera se
    // pintaría DENTRO del viewport y el backbuffer se quedaría sin escribir (la ventana alterna
    // entre dos buffers viejos = parpadea entera). Quien dibuja a la pantalla, la ata.
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

        // Todas las ventanas, en orden de registro. En play mode las de edición salen
        // deshabilitadas; el viewport y las herramientas sueltas, no.
        panels.drawAll(isPlaying());
        // Cargar/descargar pedido desde el panel "Plugins": fuera de drawAll, que recorre el registro.
        plugins.processPending();

        if (backend) backend->drawPopups();

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
        std::cerr << "[Editor] renderUI lanzo: " << e.what() << std::endl;
    }
}

// ── Proyecto ──────────────────────────────────────────────────────────────────────────────────

void EditorApplication::syncPanelsToProject() {
    panels.projectChanged(projectRoot());
}

void EditorApplication::openProject(const std::string& root) {
    if (!backend) {
        std::cerr << "✗ Sin backend de motor: no se puede abrir un proyecto" << std::endl;
        return;
    }
    const bool ok = backend->openProject(root);
    // Una sola llamada para TODOS los paneles: la lista copiada a mano dejaba fuera al
    // último que se añadiera (le pasó al de prefabricados).
    syncPanelsToProject();
    if (ok) std::cout << "Project loaded: " << root << std::endl;
    else    std::cerr << "⚠ El motor no reconoce " << root << " como proyecto (se abre la carpeta igualmente)" << std::endl;
}

void EditorApplication::createNewProject(const std::string& name, const std::string& basePath) {
    if (name.empty() || basePath.empty()) {
        std::cerr << "Project name and path cannot be empty" << std::endl;
        return;
    }
    if (!backend) {
        std::cerr << "✗ Sin backend de motor: no se puede crear un proyecto" << std::endl;
        return;
    }

    try {
        // Usar el template del MOTOR como base (fuente única de verdad). Qué motor y dónde
        // está lo decide EngineSdk en runtime, no la ruta con la que se compiló el editor.
        std::string projectPath = basePath + name;
        const EngineSdk& engine = EngineSdk::current();
        const std::string templatePath = engine.templateDir;

        if (!templatePath.empty() && std::filesystem::exists(templatePath)) {
            std::filesystem::copy(templatePath, projectPath, std::filesystem::copy_options::recursive);
        } else {
            // Sin motor (o motor sin template) el proyecto sale SIN CMakeLists ni build.sh, o
            // sea que no compila: se avisa aquí, con la lista de rutas miradas, en vez de dejar
            // que el usuario lo descubra al pulsar "Compilar".
            std::cerr << "⚠ Sin template de motor: el proyecto se crea vacio y no compilara.\n"
                      << "  Motores buscados en:\n" << EngineSdk::searchPathsDescription();
            std::filesystem::create_directories(projectPath + "/scripts");
            std::filesystem::create_directories(projectPath + "/scenes");
            std::filesystem::create_directories(projectPath + "/assets");
        }

        // Placeholders del template: nombre del proyecto y raíz del motor activo.
        std::string cmakeFilePath = projectPath + "/CMakeLists.txt";
        if (std::filesystem::exists(cmakeFilePath)) {
            std::ifstream cmakeIn(cmakeFilePath);
            std::string cmakeContent((std::istreambuf_iterator<char>(cmakeIn)),
                                      std::istreambuf_iterator<char>());
            cmakeIn.close();

            auto replaceAll = [&cmakeContent](const std::string& from, const std::string& to) {
                size_t pos = 0;
                while ((pos = cmakeContent.find(from, pos)) != std::string::npos) {
                    cmakeContent.replace(pos, from.length(), to);
                    pos += to.length();
                }
            };
            replaceAll("PROJECT_NAME_PLACEHOLDER", name);
            replaceAll("HARUKA_ENGINE_LOCAL_PLACEHOLDER", engine.root);

            if (std::ofstream cmakeOut(cmakeFilePath); cmakeOut.is_open()) cmakeOut << cmakeContent;
        }

        // Lo que el MOTOR pone en un proyecto nuevo (Haruka: project.hrk y la escena inicial).
        const std::string doc = backend->initializeNewProject(projectPath, name);
        currentFile.path = doc;
        currentFile.name = doc.empty() ? std::string{} : std::filesystem::path(doc).stem().string();
        sceneDirty = false;
        syncPanelsToProject();

        std::cout << "✓ Project created from template: " << projectPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "✗ Failed to create project: " << e.what() << std::endl;
    }
}

void EditorApplication::compileProject() {
    if (isProjectCompiling) return;

    std::string projectPath = projectRoot();
    if (projectPath.empty()) {
        std::cerr << "✗ No project loaded. Open a project first." << std::endl;
        return;
    }

    isProjectCompiling = true;
    std::cout << "Compiling project: " << projectPath << std::endl;

    // Compilar el proyecto. Se pasa el nombre (lib<Nombre>.so) y la ruta del motor ACTIVO
    // para que el proyecto compile contra el MISMO motor sin redescargarlo.
    const std::string projectName = backend ? backend->projectName() : std::string{};
    const EngineSdk& engine = EngineSdk::current();
    std::string cmakeArgs;
    if (!projectName.empty()) {
        cmakeArgs += " -DPROJECT_NAME=" + projectName;
    }
    if (engine.valid()) {
        // El nombre de la variable lo declara el motor: otro motor puede llamarla de otra forma.
        cmakeArgs += " -D" + engine.cmakeEngineVar + "=\"" + engine.root + "\"";
    }

    std::string compileCmd;
    if (std::filesystem::exists(std::filesystem::path(projectPath) / engine.buildScript)) {
        // Template moderno: el script de build del motor configura y compila el layout del juego.
        compileCmd = "cd \"" + projectPath + "\" && ./" + engine.buildScript + cmakeArgs;
    } else {
        // Fallback legacy (proyecto sin build.sh): build inline, pero con el paralelismo
        // ACOTADO POR RAM igual que el build.sh moderno — `-j$(nproc)` tiraba de swap.
        compileCmd = "cd \"" + projectPath + "\" && mkdir -p build && cd build && cmake .." +
                     cmakeArgs + " && make -j" + std::to_string(ramBoundedJobs());
    }
    const int result = system(compileCmd.c_str());

    if (result == 0) std::cout << "✓ Project compiled successfully" << std::endl;
    else             std::cerr << "✗ Project compilation failed (codigo " << result << ")" << std::endl;

    isProjectCompiling = false;
}

// ── Documentos ────────────────────────────────────────────────────────────────────────────────

void EditorApplication::openDocument(const std::string& path) {
    if (sceneDirty) {
        pendingSceneToLoad = path;
        showUnsavedChangesPopup = true;
    } else {
        loadFile(path);
    }
}

void EditorApplication::saveFile(const std::string& path) {
    if (!backend || path.empty()) return;

    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }

    // Crear backup si existe
    if (std::filesystem::exists(path)) {
        createFileBackup(path);
    }

    if (backend->saveDocument(path)) {
        currentFile.path = path;
        currentFile.name = p.stem().string();
        currentFile.lastSaveTime = SDL_GetTicks() / 1000.0f;
        sceneDirty = false;
        timeSinceLastSave = 0.0f;
        std::cout << "✓ Saved: " << path << std::endl;
    } else {
        std::cerr << "✗ Failed to save: " << path << std::endl;
    }
}

void EditorApplication::loadFile(const std::string& path) {
    if (!backend) return;
    // El backend deja los paneles apuntando al documento nuevo AUNQUE la carga falle (ver
    // IEngineBackend::loadDocument): aquí solo queda decidir qué se muestra como abierto.
    if (backend->loadDocument(path)) {
        currentFile.path = path;
        currentFile.name = std::filesystem::path(path).stem().string();
        currentFile.lastSaveTime = SDL_GetTicks() / 1000.0f;
        sceneDirty = false;
        timeSinceLastSave = 0.0f;
        std::cout << "✓ Loaded: " << path << std::endl;
    } else {
        std::cerr << "✗ Failed to load file: " << path << std::endl;
    }
}

// Las copias de seguridad son por EXTENSIÓN y no por tipo de documento: un `.prefab` guarda solo
// la última (se borran las anteriores), el resto rota hasta `maxBackups`.
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
