#include "backends/haruka/haruka_backend.h"
#include "editor_util.h"
#include "commands/scene_commands.h"   // AddObjectCommand: lo usa `addSceneObject`
#include "commands/command_history.h"
#include "panels/panel_registry.h"
#include "Haruka/Core/AssetPaths.h"
#include "Haruka/Core/Components/MeshRendererComponent.h"
#include "Haruka/Renderer/PrimitiveShapes.h"
#include "Haruka/Renderer/Shader.h"   // Shader::setBaseDir → raíz de assets del motor
#include "Haruka/Renderer/MotorInstance.h"
#include "Haruka/Core/Application.h"   // el test del pincel pide el planeta a la Application
#include "Haruka/World/Planet/PlanetarySystem.h"
#include "Haruka/World/Planet/Planet.h"
#include "Haruka/Tools/ErrorReporter.h"

#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <dlfcn.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

// ── Entrada del .so (ver backend/engine_backend.h) ─────────────────────────────────────────────

HARUKA_EDITOR_BACKEND_DECLARE_API();

/// Comprueba ANTES de construir nada que el ImGui del editor es el mismo con el que se compiló
/// este backend (y el motor): el editor exporta el suyo y es el que usan todos, así que con otra
/// versión cada `ImGui::` del motor leería estructuras con otro layout.
HARUKA_EDITOR_PLUGIN_EXPORT IEngineBackend* haruka_editor_backend_create(const HarukaImGuiAbi* imgui,
                                                                       char* error, size_t errorLen) {
    if (!imgui || !harukaBindImGui(*imgui)) {
        harukaImGuiAbiDescribeMismatch(imgui, error, errorLen);
        return nullptr;
    }
    return new HarukaBackend();
}

namespace {
/// Lo que reciben los paneles en `onSceneChanged` es la escena de ESTE backend.
Haruka::Core::Scene::SceneManager* sceneOf(void* h) { return static_cast<Haruka::Core::Scene::SceneManager*>(h); }
}

HarukaBackend::HarukaBackend() = default;
HarukaBackend::~HarukaBackend() = default;

bool HarukaBackend::init(const EditorServices& s) {
    services = s;

    // RAÍZ DE ASSETS DEL MOTOR. Sin esto `Shader::baseDir()` queda vacía y el motor pide
    // "shaders/simple.vert" RELATIVO al cwd, mientras los ficheros viven en "assets/shaders/" junto
    // al ejecutable: no compila NI UN pipeline (cielo, escena, terreno…) y el viewport sale negro sin
    // más pista que un HARUKA_LOGW por shader. Va aquí, con el contexto GL ya creado por el núcleo y
    // ANTES de crear la Application, porque los pipelines se hornean en el primer frame.
    // Se deriva de SDL_GetBasePath (el directorio del EJECUTABLE, no el cwd): el editor se lanza
    // tanto desde su carpeta como desde el IDE o un script, y el cwd cambia con quien lo arranca.
    if (const char* exeDir = SDL_GetBasePath()) {
        const std::string assetsRoot = std::string(exeDir) + "assets/";
        Haruka::Renderer::Shader::setBaseDir(assetsRoot.c_str());   // fija también Haruka::Core::AssetPaths
        std::cout << "[Haruka] assets root: " << assetsRoot << std::endl;
    }

    currentScene = std::make_unique<Haruka::Core::Scene::SceneManager>();
    currentScene->setName("Untitled");
    currentProject = std::make_unique<Haruka::Core::Project>();

    sceneHierarchyPanel.setScene(currentScene.get());
    sceneHierarchyPanel.setCommandHistory(services.history);
    inspectorPanel.setScene(currentScene.get());
    inspectorPanel.setCommandHistory(services.history);
    inspectorPanel.setOnSceneChanged([this]() { markDirty(); });
    materialEditorPanel.setOnSceneChanged([this]() { markDirty(); });
    nodeGraphEditorPanel.setScene(currentScene.get());
    nodeGraphEditorPanel.setOnSceneChanged([this]() { markDirty(); });
    // El panel de prefabricados necesita el VIEWPORT, que es quien tiene el gizmo con el que se
    // mueve la pieza seleccionada. La ruta del proyecto le llega por el registro, con todos.
    prefabPanel.setViewport(&viewportPanel);
    // El clic en el VIEWPORT elige objeto por su cuenta y no avisa a nadie: sin esto, el panel le
    // seguía cediendo su pieza al gizmo y el objeto picado se quedaba sin él.
    viewportPanel.setOnObjectPicked([this]() { prefabPanel.dropPieceSelection(); });
    // El pincel del mundo: el viewport le pasa el gesto (rayo, clic, arrastre) y pinta su indicador.
    viewportPanel.setWorldTool(&worldPanel.panel());
    worldPanel.setCommandHistory(services.history);

    viewportCamera = std::make_unique<Haruka::Core::Camera>(Haruka::Tools::WorldPos(0.0f, 5.0f, 15.0f));
    viewportCamera->orientation = glm::angleAxis(glm::radians(0.0f), glm::vec3(0, 1, 0));

    viewportPanel.setScene(currentScene.get());
    viewportPanel.setCamera(viewportCamera.get());
    viewportPanel.setSDLWindow(services.window);
    viewportPanel.setStatsPanel(&statsPanel);
    viewportPanel.setPlayMode(false);

    sceneHierarchyPanel.setOnObjectSelectedByIndex([this](int index) {
        if (currentScene && index >= 0 && index < (int)currentScene->getObjects().size()) {
            // ⚠️ EL GIZMO ES UNO SOLO. Si queda viva una pieza de prefabricado seleccionada, el
            // viewport se la sigue cediendo a ella y el objeto que acabas de clicar se queda SIN
            // gizmo — que desde fuera se ve como "el gizmo ha desaparecido". Seleccionar un objeto
            // de la escena es, por definición, dejar de editar la pieza.
            prefabPanel.dropPieceSelection();
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
    // DOBLE clic en jerarquía = llevar la cámara al objeto y encuadrarlo.
    sceneHierarchyPanel.setOnObjectFocused([this](int index) { viewportPanel.focusOnObject(index); });

    settingsPanel.load();
    registerPanels();
    return true;
}

void HarukaBackend::shutdown() {
    // Descargar módulo de juego de forma explícita al cerrar aplicación
    if (gameInterface && gameInterface->onShutdown) gameInterface->onShutdown();
    gameInterface = nullptr;
    if (gameLibHandle) {
        dlclose(gameLibHandle);
        gameLibHandle = nullptr;
    }
    // Liberar recursos GL del motor ANTES de que el núcleo destruya el contexto.
    viewportPanel.shutdownGLResources();
}

void HarukaBackend::update(float dt) {
    statsPanel.update(dt);
    updatePlayMode(dt);
    runWorldTestOnce();
    exportPanel.update();
    viewportPanel.onUpdate(dt);
}

void HarukaBackend::beginFrame(float dt) {
    viewportPanel.setGizmoMode(gizmoMode);
    // Play Mode: el onUpdate del juego (gameplay + HUD/consola con ImGui::Begin) DEBE correr
    // dentro del scope de frame, como en el bucle standalone (application.cpp: NewFrame ->
    // onUpdate -> Render). Si se llamara en update() antes de NewFrame, ImGui abortaría con
    // "WithinFrameScope".
    if (isPlayMode && gameInterface && gameInterface->onUpdate) {
        gameInterface->onUpdate(services.window, dt);
    }
}

void HarukaBackend::updatePlayMode(float dt) {
    if (!isPlayMode) return;
    playModeTime += dt;

    // Sincronizar cámara de juego al viewport SIN compartir ownership/puntero
    if (gameInterface && gameInterface->getCamera && viewportCamera) {
        if (Haruka::Core::Camera* gameCam = gameInterface->getCamera()) {
            viewportCamera->position = gameCam->position;
            viewportCamera->orientation = gameCam->orientation;
            viewportCamera->zoom = gameCam->zoom;
            viewportCamera->speed = gameCam->speed;
            viewportCamera->sensitivity = gameCam->sensitivity;
            viewportPanel.setCamera(viewportCamera.get());
        }
    }
}

void HarukaBackend::runWorldTestOnce() {
    // HARUKA_EDITOR_WORLD_TEST=1: a los 25 s, el gesto del pincel SIN ratón — una cueva nueva y un
    // trazo junto a la boca de la primera cueva de la escena — y se imprime lo que quedó en la
    // escena. Es lo único de esta herramienta que no se puede medir en la suite (el resto, en
    // `test_vox_brush`), y lo que separa "el panel llama a la escena" de "el panel se ve".
    {
        static bool s_done = false;
        static const bool s_on = [] { const char* e = std::getenv("HARUKA_EDITOR_WORLD_TEST"); return e && e[0] == '1'; }();
        if (s_on && !s_done && SDL_GetTicks() / 1000.0 >= 25.0) {
            s_done = true;
            auto& wp = worldPanel.panel();
            if (Haruka::World::Vox::VoxWorld* w = wp.world(); w && currentScene) {
                Haruka::Core::Application* app = Haruka::Renderer::MotorInstance::getInstance().getApplication();
                auto* ps = app ? app->getPlanetarySystem() : nullptr;
                auto* tp = ps ? ps->activeTerrestrialMut() : nullptr;
                const auto mouths = w->placedCaveMouths();
                if (tp && !mouths.empty()) {
                    const glm::dvec3 up = mouths[0];
                    const glm::dvec3 ref = std::abs(up.y) < 0.99 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0);
                    const glm::dvec3 e1 = glm::normalize(glm::cross(ref, up));
                    const size_t antes = currentScene->getObjects().size();
                    wp.setAssetDir("");   // el test no deja PNG en el proyecto
                    wp.setTool(Haruka::Tools::WorldEditPanel::Tool::PlaceCave);
                    wp.press(tp->position() + glm::normalize(up + e1 * (3000.0 / tp->radius())) * tp->radius());
                    wp.setTool(Haruka::Tools::WorldEditPanel::Tool::Brush);
                    const glm::dvec3 suelo = tp->position() + up * (tp->radius() + (double)w->elevAt(up));
                    const float antesCampo = w->density(suelo - tp->position() - up * 3.0);
                    wp.press(suelo + e1 * 60.0);
                    wp.release();
                    const float despues = w->density(suelo + e1 * 60.0 - tp->position() - up * 3.0);
                    wp.setTool(Haruka::Tools::WorldEditPanel::Tool::None);
                    std::string tipos;
                    for (const auto& o : currentScene->getObjects()) if (o.type == "Cave" || o.type == "Island") tipos += o.type + ":" + o.name + " ";
                    std::printf("[WorldTest] objetos %zu -> %zu · %s· campo a 3 m bajo el suelo: lejos %+.2f, en el trazo %+.2f · deshacer %zu · sucia=%d\n",
                                antes, currentScene->getObjects().size(), tipos.c_str(), antesCampo, despues, w->undoCount(), markedDirty ? 1 : 0);
                    services.history->undo();
                    worldPanel.onImGuiRender();   // un frame del panel: la escena manda y el mundo se sincroniza
                    std::printf("[WorldTest] tras deshacer el comando de escena: %zu objetos · cuevas en el mundo %zu\n",
                                currentScene->getObjects().size(), w->caveDefs().size());
                }
            }
        }
    }
}

// ── Menús ────────────────────────────────────────────────────────────────────────────────────

void HarukaBackend::drawMenuItems(const std::string& menu) {
    if (menu == "File") {
        if (ImGui::MenuItem("Export Game")) {
            if (currentProject) exportPanel.show();
            else std::cerr << "✗ No project loaded" << std::endl;
        }
    } else if (menu == "Edit") {
        if (ImGui::MenuItem("Move Gizmo", "W", gizmoMode == 0)) gizmoMode = 0;
        if (ImGui::MenuItem("Rotate Gizmo", "E", gizmoMode == 1)) gizmoMode = 1;
        if (ImGui::MenuItem("Scale Gizmo", "R", gizmoMode == 2)) gizmoMode = 2;
    }
}

void HarukaBackend::drawMainMenus() {
    if (ImGui::BeginMenu("Objects")) {
        if (currentScene) {
            if (ImGui::MenuItem("Cube"))    createSceneObject("Cube");
            if (ImGui::MenuItem("Sphere"))  createSceneObject("Sphere");
            if (ImGui::MenuItem("Plane"))   createSceneObject("Plane");
            if (ImGui::MenuItem("Capsule")) createSceneObject("Capsule");
        }
        ImGui::Separator();
        // Entidades dinámicas (capas del panel de objetos)
        if (ImGui::MenuItem("Prop"))        createProp();
        if (ImGui::MenuItem("Monster"))     createMonster();
        if (ImGui::MenuItem("Spawn Point")) createSpawnPoint();
        if (ImGui::MenuItem("Character"))   createCharacter();
        ImGui::Separator();
        if (currentScene) {
            if (ImGui::MenuItem("Light"))             createSceneObject("Light");
            if (ImGui::MenuItem("Point Light"))       createSceneObject("PointLight");
            if (ImGui::MenuItem("Directional Light")) createSceneObject("DirectionalLight");
            ImGui::Separator();
            if (ImGui::MenuItem("Sun"))               createSceneObject("SUN");
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Material")) {
        // Atajos a dos ventanas del registro; si alguna dejara de existir, simplemente no sale.
        if (bool* f = services.panels->openFlag("nodegraph")) ImGui::MenuItem("Node Graph Editor", nullptr, f);
        if (bool* f = services.panels->openFlag("material"))  ImGui::MenuItem("Material Editor", nullptr, f);
        ImGui::Separator();
        if (ImGui::MenuItem("Bake Textures for Selected")) nodeGraphEditorPanel.bakeSelectedTextures();
        ImGui::EndMenu();
    }
}

void HarukaBackend::drawPopups() {
    exportPanel.render(currentProject.get());
}

// ── Proyecto y documentos ─────────────────────────────────────────────────────────────────────

bool HarukaBackend::openProject(const std::string& root) {
    if (!currentProject) currentProject = std::make_unique<Haruka::Core::Project>();
    const bool ok = currentProject->load(root);
    // ⚠️ Y LA RAÍZ DE ASSETS, que sólo se fijaba al CREAR un proyecto: sin esto el motor
    // resuelve las rutas de `AssetPaths` contra el proyecto anterior (o contra ninguno), y
    // lo que se abre no es del todo el proyecto que has elegido.
    Haruka::Core::AssetPaths::setProjectRoot(root + "/");
    return ok;
}

std::string HarukaBackend::initializeNewProject(const std::string& projectPath, const std::string& name) {
    // project.hrk con el nombre del proyecto nuevo
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream dateStream;
    dateStream << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");

    nlohmann::json projectConfig;
    projectConfig["name"] = name;
    projectConfig["version"] = "0.1.0";
    projectConfig["created"] = dateStream.str();
    projectConfig["startScene"] = "scenes/main.scene";
    if (std::ofstream configFile(projectPath + "/project.hrk"); configFile.is_open()) {
        configFile << projectConfig.dump(2);
    }

    // Escena inicial: la que haya abierta en el editor.
    const std::string scenePath = projectPath + "/scenes/main.scene";
    std::filesystem::create_directories(projectPath + "/scenes");
    currentScene->save(scenePath);

    openProject(projectPath);
    bindPanelsToScene();
    return scenePath;
}

std::string HarukaBackend::projectRoot() const {
    return currentProject ? currentProject->getPath() : std::string{};
}

std::string HarukaBackend::projectName() const {
    return currentProject ? currentProject->getConfig().name : std::string{};
}

std::string HarukaBackend::defaultDocument(const std::string& root) const {
    const std::string scene = root + "/scenes/main.scene";
    return std::filesystem::exists(scene) ? scene : std::string{};
}

bool HarukaBackend::saveDocument(const std::string& path) {
    if (!currentScene) return false;
    const bool ok = currentScene->save(path);
    // Los trazos del MUNDO van con la escena: a la carpeta `voxEdits` del planeta.
    worldPanel.saveWorldEdits();
    return ok;
}

bool HarukaBackend::loadDocument(const std::string& path) {
    // RAÍZ DE ASSETS DEL PROYECTO: el path llega como <proyecto>/scenes/x.scene. El motor
    // resuelve sus assets contra la carpeta del EDITOR (fijada en init()); las texturas de
    // escena ("assets/textures/...") y las biomas por defecto de SimplePlanet viven en la del
    // PROYECTO. Se sube hasta hallar project.hrk y se fija esa raíz ANTES de que el viewport
    // hornee el motor (setScene → Application::init). Sin esto, cada textura del proyecto
    // falla bajo el editor y las biomas caen al fallback procedural.
    namespace fs = std::filesystem;
    fs::path projDir = fs::path(path).parent_path();
    while (!projDir.empty() && projDir != projDir.root_path() && !fs::exists(projDir / "project.hrk")) {
        projDir = projDir.parent_path();
    }
    if (!projDir.empty() && fs::exists(projDir / "project.hrk"))
        Haruka::Core::AssetPaths::setProjectRoot(projDir.string() + "/");

    // La escena NUEVA se construye antes de soltar la vieja, y los paneles se repuntan enseguida
    // (bindPanelsToScene): en cuanto este unique_ptr suelta la anterior, cualquier panel que siga
    // apuntándola trabaja sobre memoria liberada.
    currentScene = std::make_unique<Haruka::Core::Scene::SceneManager>();
    const bool loaded = currentScene->load(path);

    // INCONDICIONAL, y antes de decidir si fue bien: la escena anterior ya está destruida en la
    // línea de arriba. Repuntar solo en el camino de éxito dejaba a los paneles apuntando a memoria
    // liberada cuando el fichero no cargaba — el mismo use-after-free, en la rama que menos se prueba.
    bindPanelsToScene();
    markedDirty = false;
    return loaded;
}

std::string HarukaBackend::documentKind(const std::string& path) const {
    return (path.find(".prefab") != std::string::npos) ? "Prefab" : "Scene";
}

void HarukaBackend::bindPanelsToScene() {
    // Primero las SELECCIONES de todos y después la escena: los paneles guardan `SceneObject*`
    // crudos y un índice que apuntan a la escena vieja. Lo que recibe cada uno está en su entrada
    // de `registerPanels`; el viewport es el último con escena porque reinicia la Application.
    services.panels->sceneChanged(currentScene.get());
}

void HarukaBackend::registerPanels() {
    // ⚠️ EL ORDEN DE ESTA LISTA ES EL DE DIBUJO (ver panel_registry.h). Prefabricados y mundo van
    // antes que el viewport: el primero le cede su pieza al gizmo y el segundo su pincel, y el
    // viewport los usa en su propio dibujo. Y el viewport va el último de los que reciben escena.
    //
    // Grupos del menú View: 0 = vistas de la escena, 1 = herramientas, 2 = depuración.
    auto onScene = [this]() { markDirty(); };

    {
        PanelEntry e{"hierarchy", "Scene Hierarchy", 0, true};
        e.draw = [this] { sceneHierarchyPanel.onImGuiRender(); };
        e.clearSelection = [this] { sceneHierarchyPanel.setSelectedObjectIndex(-1); };
        e.onSceneChanged = [this](void* h) { sceneHierarchyPanel.setScene(sceneOf(h)); };
        services.panels->add(std::move(e));
    }
    {
        // Prefabricados del JUEGO (`assets/data/prefabs/*.json`), no las escenas `.prefab`: ver la
        // cabecera de `prefab_panel.cpp`, que explica por qué son dos cosas con el mismo nombre.
        PanelEntry e{"prefab", "Prefabricados", 1, false};
        e.draw = [this] { prefabPanel.onImGuiRender(); };
        // ⚠️ CON EL PANEL CERRADO NADIE SUELTA LA PIEZA. `onImGuiRender` es quien reengancha (y
        // desengancha) el gizmo, así que cerrar el panel con una pieza seleccionada dejaba al
        // viewport manipulándola PARA SIEMPRE, y con un puntero a un vector que el panel realoja.
        e.whileClosed = [this] { prefabPanel.dropPieceSelection(); };
        e.onProjectChanged = [this](const std::string& root) { prefabPanel.setProjectPath(root); };
        // Mover una pieza de un montaje colocado es un cambio de la ESCENA (va en `prefabEdits`).
        e.onSceneChanged = [this, onScene](void* h) {
            auto* s = sceneOf(h);
            prefabPanel.setScene(s);
            prefabPanel.setOnSceneChanged(onScene);
        };
        services.panels->add(std::move(e));
    }
    {
        // El pincel del mundo: trazos sobre el campo, cuevas e islas (objetos de la escena).
        PanelEntry e{"world", "Mundo (pincel, cuevas, islas)", 1, false};
        e.draw = [this] { worldPanel.onImGuiRender(); };
        e.whileClosed = [this] { worldPanel.deactivate(); };   // panel cerrado = sin pincel
        e.onProjectChanged = [this](const std::string& root) { worldPanel.setProjectPath(root); };
        e.onSceneChanged = [this, onScene](void* h) {
            auto* s = sceneOf(h);
            worldPanel.setScene(s);
            worldPanel.setOnSceneChanged(onScene);
        };
        services.panels->add(std::move(e));
    }
    {
        PanelEntry e{"inspector", "Inspector", 0, true};
        e.draw = [this] { inspectorPanel.onImGuiRender(); };
        e.onProjectChanged = [this](const std::string& root) { inspectorPanel.setProjectPath(root); };
        e.clearSelection = [this] { inspectorPanel.setSelectedObjectIndex(-1); };
        e.onSceneChanged = [this](void* h) { inspectorPanel.setScene(sceneOf(h)); };
        services.panels->add(std::move(e));
    }
    {
        PanelEntry e{"stats", "Performance Stats", 0, true};
        e.draw = [this] { statsPanel.onImGuiRender(); };
        services.panels->add(std::move(e));
    }
    {
        PanelEntry e{"material", "Material Editor", 0, true};
        e.draw = [this] { materialEditorPanel.onImGuiRender(); };
        e.onProjectChanged = [this](const std::string& root) { materialEditorPanel.setProjectPath(root); };
        e.clearSelection = [this] { materialEditorPanel.setSelectedObject(nullptr); };
        services.panels->add(std::move(e));
    }
    {
        // Texturas y materiales procedurales.
        PanelEntry e{"nodegraph", "Node Graph Editor", 0, false};
        e.draw = [this] { nodeGraphEditorPanel.onImGuiRender(); };
        e.onProjectChanged = [this](const std::string& root) { nodeGraphEditorPanel.setProjectPath(root); };
        e.clearSelection = [this] { nodeGraphEditorPanel.setSelectedObject(nullptr); };
        e.onSceneChanged = [this](void* h) { nodeGraphEditorPanel.setScene(sceneOf(h)); };
        services.panels->add(std::move(e));
    }
    {
        // En play mode NO se deshabilita: es donde se ve el juego.
        PanelEntry e{"viewport", "Viewport", 0, true, false};
        e.draw = [this] { viewportPanel.onImGuiRender(); };
        e.clearSelection = [this] { viewportPanel.setSelectedObjectIndex(-1); };
        e.onSceneChanged = [this](void* h) { viewportPanel.setScene(sceneOf(h)); };
        services.panels->add(std::move(e));
    }
    {
        PanelEntry e{"settings", "Settings", 1, false, false};
        e.draw = [this] { settingsPanel.onImGuiRender(); };
        services.panels->add(std::move(e));
    }
}

// ── Play mode y creación de objetos (movidos tal cual desde EditorApplication) ─────────────

void HarukaBackend::enterPlay() {
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
    Haruka::Core::AssetPaths::setProjectRoot(projectPath + "/");
    
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
        typedef Haruka::Core::GameInterface* (*GetGameInterfaceFunc)();
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
        HARUKA_EDITOR_ERROR(Haruka::Tools::ErrorCode::MOTOR_LIBRARY, "Could not load project library: " + std::string(dlerror()));
    }

    std::cout << "▶ Play Mode started" << std::endl;
}

void HarukaBackend::exitPlay() {
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

std::string HarukaBackend::nextObjectName(const std::string& prefix) {
    int n = currentScene ? (int)currentScene->getAllObjects().size() : 0;
    std::string base = prefix + "_" + std::to_string(n);
    int i = n;
    while (currentScene && currentScene->getObject(base)) base = prefix + "_" + std::to_string(++i);
    return base;
}

std::shared_ptr<Haruka::Core::Scene::SceneObject> HarukaBackend::makeMeshObject(const std::string& name,
                                                                  const std::string& meshType,
                                                                  const glm::vec3& albedo) {
    auto obj = std::make_shared<Haruka::Core::Scene::SceneObject>();
    obj->name = name;
    obj->type = "Mesh";
    obj->objectType = Haruka::Tools::classifyObjectType("Mesh");
    obj->position = glm::dvec3(0.0);
    obj->rotation = EditorUtil::eulerToRotation(glm::dvec3(0.0));
    obj->scale = glm::dvec3(1.0);

    obj->meshRenderer = std::make_shared<Haruka::Core::Components::MeshRendererComponent>();
    std::vector<glm::vec3> verts, norms;
    std::vector<unsigned int> indices;

    if (meshType == "capsule") {
        Haruka::Renderer::PrimitiveShapes::createCapsule(0.5f, 2.0f, 24, 16, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "capsule";
        obj->properties["meshRenderer"]["radius"] = 0.5f;
        obj->properties["meshRenderer"]["height"] = 2.0f;
    } else if (meshType == "sphere") {
        Haruka::Renderer::PrimitiveShapes::createSphere(1.0f, 32, 32, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "sphere";
        obj->properties["meshRenderer"]["radius"] = 1.0f;
        obj->properties["meshRenderer"]["segments"] = 32;
    } else {
        Haruka::Renderer::PrimitiveShapes::createCube(1.0f, verts, norms, indices);
        obj->properties["meshRenderer"]["meshType"] = "cube";
        obj->properties["meshRenderer"]["size"] = 1.0f;
    }

    obj->meshRenderer->setMesh(verts, norms, std::vector<glm::vec3>(), indices);

    obj->material = std::make_shared<Haruka::Core::Components::MaterialComponent>();
    obj->material->name = obj->name + "_Material";
    obj->material->albedo = albedo;
    obj->color = glm::dvec3(albedo);
    return obj;
}

void HarukaBackend::addSceneObject(std::shared_ptr<Haruka::Core::Scene::SceneObject> obj) {
    if (!obj || !currentScene) return;

    // El historial es del NÚCLEO (lo comparte con el resto del editor): se usa el que presta.
    services.history->execute(std::make_unique<AddObjectCommand>(currentScene.get(), *obj));
    markDirty();

    // Seleccionar el objeto recién creado.
    int idx = (int)currentScene->getAllObjects().size() - 1;
    if (idx >= 0) {
        sceneHierarchyPanel.setSelectedObjectIndex(idx);
        inspectorPanel.setSelectedObjectIndex(idx);
        viewportPanel.setSelectedObjectIndex(idx);
    }
}

void HarukaBackend::createProp() {
    auto obj = makeMeshObject(nextObjectName("Prop"), "cube", glm::vec3(0.6f, 0.6f, 0.7f));
    obj->type = "Mesh";
    EditorUtil::setObjectLayer(*obj, "Props");
    addSceneObject(obj);
}

void HarukaBackend::createMonster() {
    auto obj = makeMeshObject(nextObjectName("Monster"), "capsule", glm::vec3(0.7f, 0.3f, 0.3f));
    // Entidad dinámica: tipo libre (CUSTOM) con atributos editables por proyecto.
    obj->type = "Monster";
    obj->objectType = Haruka::Tools::classifyObjectType("Monster");
    EditorUtil::setObjectLayer(*obj, "Monsters");
    obj->properties["monster"] = {
        {"health", 100}, {"speed", 3.0}, {"damage", 10},
        {"attackRange", 2.0}, {"aggroRange", 15.0}, {"radius", 0.5},
        {"spawnTable", ""}
    };
    addSceneObject(obj);
}

void HarukaBackend::createSpawnPoint() {
    auto obj = makeMeshObject(nextObjectName("SpawnPoint"), "cube", glm::vec3(1.0f, 0.85f, 0.2f));
    obj->type = "SpawnPoint";
    obj->scale = glm::dvec3(0.5);
    EditorUtil::setObjectLayer(*obj, "Spawns");
    obj->properties["spawn"] = {
        {"type", "monster"}, {"radius", 5.0}, {"maxEntities", 5},
        {"cooldown", 30.0}, {"active", true}, {"zoneId", ""}
    };
    addSceneObject(obj);
}

void HarukaBackend::createCharacter() {
    auto obj = makeMeshObject(nextObjectName("Character"), "capsule", glm::vec3(0.3f, 0.7f, 0.9f));
    obj->type = "Character";
    obj->objectType = Haruka::Tools::classifyObjectType("Character");
    EditorUtil::setObjectLayer(*obj, "Characters");
    addSceneObject(obj);
}

void HarukaBackend::createMesh() {
    auto obj = makeMeshObject(nextObjectName("Mesh"), "cube", glm::vec3(0.8f, 0.8f, 0.8f));
    EditorUtil::setObjectLayer(*obj, "Default");
    addSceneObject(obj);
}

void HarukaBackend::createLight() {
    auto obj = makeMeshObject(nextObjectName("Light"), "sphere", glm::vec3(1.0f, 1.0f, 0.8f));
    obj->type = "Light";
    obj->objectType = Haruka::Tools::classifyObjectType("Light");
    obj->intensity = 2.0;
    obj->color = glm::dvec3(1.0f, 1.0f, 0.8f);
    EditorUtil::setObjectLayer(*obj, "Lights");
    addSceneObject(obj);
}

void HarukaBackend::createSceneObject(const std::string& type) {
    if (!currentScene) return;
    std::string meshType = type;
    std::transform(meshType.begin(), meshType.end(), meshType.begin(), ::tolower);
    sceneHierarchyPanel.createPrimitive(type, meshType);
    markDirty();
}
