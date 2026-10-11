#pragma once

#include <imgui.h>   // ANTES que engine_backend.h: activa harukaBindImGui (ver haruka_editor_plugin.h)
#include "backend/engine_backend.h"
#include "Haruka/Core/Project.h"
#include "Haruka/Core/Scene/SceneManager.h"
#include "Haruka/Core/Camera.h"
#include "Haruka/Core/GameInterface.h"
#include "panels/scene_hierarchy.h"
#include "panels/inspector.h"
#include "panels/material_editor.h"
#include "panels/node_graph_editor.h"
#include "panels/viewport.h"
#include "panels/stats.h"
#include "panels/settings.h"
#include "panels/export_panel.h"
#include "panels/prefab_panel.h"
#include "panels/world_panel.h"

#include <memory>
#include <string>

/**
 * @brief El editor para haruka-cpp: todo lo que antes vivía en EditorApplication y es del motor.
 *
 * Posee el proyecto (`project.hrk`), la escena, la cámara del viewport, los paneles que entienden
 * de SceneObject, y el play mode (la librería del juego con `GameInterface`). El núcleo solo le
 * habla por IEngineBackend.
 */
class HarukaBackend final : public IEngineBackend {
public:
    HarukaBackend();
    ~HarukaBackend() override;

    const char* name() const override { return "haruka-cpp"; }

    bool init(const EditorServices& services) override;
    void shutdown() override;
    void update(float dt) override;
    void beginFrame(float dt) override;

    void drawMenuItems(const std::string& menu) override;
    void drawMainMenus() override;
    void drawPopups() override;

    bool openProject(const std::string& root) override;
    std::string initializeNewProject(const std::string& root, const std::string& name) override;
    std::string projectRoot() const override;
    std::string projectName() const override;
    void* sceneHandle() const override { return currentScene.get(); }
    std::string defaultDocument(const std::string& root) const override;

    std::string documentFilter() const override { return "scene,prefab"; }
    std::string untitledDocument() const override { return "scenes/Untitled.scene"; }
    bool saveDocument(const std::string& path) override;
    bool loadDocument(const std::string& path) override;
    std::string documentKind(const std::string& path) const override;

    bool canPlay() const override { return currentProject != nullptr; }
    bool isPlaying() const override { return isPlayMode; }
    void enterPlay() override;
    void exitPlay() override;

private:
    void registerPanels();
    /// Apunta TODOS los paneles a `currentScene` y borra las selecciones (ver PanelRegistry).
    void bindPanelsToScene();
    void markDirty() { markedDirty = true; if (services.markDirty) services.markDirty(); }
    bool markedDirty = false;   // desde la última carga (lo imprime el test del pincel)

    // ⚠️ CREAR UN OBJETO NO ERA COSA DE UN PANEL, y estaba dentro de uno. Al quitar `ObjectsPanel`
    // se habría ido con él la única forma de crear un Prop, un Monster, un Spawn Point o un
    // Character — que se piden desde el MENÚ, no desde el panel. Viven aquí, que es quien tiene la
    // escena y el historial de deshacer.
    void createSceneObject(const std::string& type);
    void createProp();
    void createMonster();
    void createSpawnPoint();
    void createCharacter();
    void createMesh();
    void createLight();
    /// Nombre libre con un prefijo ("Prop_3"): no repite ninguno de la escena.
    std::string nextObjectName(const std::string& prefix);
    /// Primitiva con su malla en CPU y su material. `meshType`: "cube" | "sphere" | "capsule".
    std::shared_ptr<Haruka::Core::Scene::SceneObject> makeMeshObject(const std::string& name,
                                                        const std::string& meshType,
                                                        const glm::vec3& albedo);
    /// La añade a la escena (por el historial si lo hay) y la deja seleccionada.
    void addSceneObject(std::shared_ptr<Haruka::Core::Scene::SceneObject> obj);

    /// HARUKA_EDITOR_WORLD_TEST=1: el gesto del pincel sin ratón a los 25 s (ver update()).
    void runWorldTestOnce();

    EditorServices services;

    std::unique_ptr<Haruka::Core::Project> currentProject;
    std::unique_ptr<Haruka::Core::Scene::SceneManager> currentScene;
    std::unique_ptr<Haruka::Core::Camera> viewportCamera;

    SceneHierarchyPanel sceneHierarchyPanel;
    InspectorPanel inspectorPanel;
    ViewportPanel viewportPanel;
    StatsPanel statsPanel;
    MaterialEditorPanel materialEditorPanel;
    NodeGraphEditorPanel nodeGraphEditorPanel;
    SettingsPanel settingsPanel;
    ExportPanel exportPanel;
    PrefabPanel prefabPanel;
    WorldPanel worldPanel;   // el pincel del mundo (cuevas, islas, trazos): panel del motor

    int gizmoMode = 0;   // 0 mover, 1 rotar, 2 escalar

    // Play mode
    bool isPlayMode = false;
    float playModeTime = 0.0f;
    std::string playModeBackupPath = "/tmp/haruka_playmode_backup.scene";
    void* gameLibHandle = nullptr;
    Haruka::Core::GameInterface* gameInterface = nullptr;
    void updatePlayMode(float dt);
};
