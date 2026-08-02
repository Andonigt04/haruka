#pragma once

#include "core/scene/scene_manager.h"
#include "commands/command_history.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <functional>
#include <string>
#include <vector>

/**
 * @brief Panel de objetos del IDE organizado por CAPAS.
 *
 * Muestra todos los objetos de la escena como una REJILLA VISUAL de tarjetas
 * (swatch de color, tipo y nombre) agrupadas por capa (Props, Monsters,
 * Spawns, Characters, Lights, Default…), con búsqueda y filtro por capa.
 * Cada capa es solo una etiqueta persistente (`properties["layer"]`) que se
 * serializa con la escena.
 *
 * El panel permite crear objetos típicos y seleccionarlos (sincronizado con
 * hierarchy / inspector / viewport / material editor) para ver y editar sus
 * texturas y materiales.
 */
class ObjectsPanel {
public:
    ObjectsPanel() = default;

    /** @brief Fija la escena que el panel inspecciona. */
    void setScene(Haruka::SceneManager* scene);
    /** @brief Fija el historial de comandos para undo/redo. */
    void setCommandHistory(CommandHistory* history);
    /** @brief Fija el índice de selección (desde otro panel). */
    void setSelectedObjectIndex(int index);
    /** @brief Callback de cambio de escena (marca dirty). */
    void setOnSceneChanged(std::function<void()> cb) { onSceneChanged = std::move(cb); }
    /** @brief Callback de selección por índice. */
    void setOnObjectSelectedByIndex(std::function<void(int)> cb);
    /** @brief Callback de selección por nombre. */
    void setOnObjectSelectedByName(std::function<void(const std::string&)> cb);
    /** @brief Callback para abrir el editor de grafo del material de un objeto. */
    void setOnOpenNodeGraph(std::function<void(const std::string&)> cb) { onOpenNodeGraph = std::move(cb); }
    /** @brief Callback de DOBLE clic: encuadrar el objeto en el viewport (no es selección). */
    void setOnObjectFocused(std::function<void(int)> cb) { onObjectFocused = std::move(cb); }
    /** @brief Renderiza el panel. */
    void onImGuiRender();

    int getSelectedObjectIndex() const { return selectedObjectIndex; }

    // --- Sistema de capas ---
    /** @brief Capas por defecto del editor. */
    static const std::vector<std::string>& defaultLayers();
    /** @brief Capa del objeto (`properties["layer"]`), "Default" si no tiene. */
    static std::string objectLayer(const Haruka::SceneObject& obj);
    /** @brief Asigna la capa del objeto. */
    static void setObjectLayer(Haruka::SceneObject& obj, const std::string& layer);

    // --- Creación de objetos (cada uno etiquetado con su capa) ---
    void createProp();
    void createMonster();
    void createSpawnPoint();
    void createCharacter();
    void createMesh();
    void createLight();

    // --- Modo de colocación (props sobre el planeta / herramientas de malla) ---
    /** @brief Callback que lanza el modo de colocación en el viewport
     *  (`modelPath`, `label`, `radius`, `circle`, `action`, `layer`; action: 0=prop,
     *  1=levantar, 2=excavar, 3=allanar; layer: capa del objeto colocado, p.ej. "Trees"). */
    void setOnBeginPlacement(std::function<void(const std::string&, const std::string&,
                                                float, bool, int, const std::string&)> cb) {
        onBeginPlacement = std::move(cb);
    }

private:
    Haruka::SceneManager* currentScene = nullptr;
    CommandHistory* commandHistory = nullptr;
    int selectedObjectIndex = -1;
    char searchBuffer[256] = {0};
    int activeLayerFilter = -1;   // -1 = All

    std::function<void(int)> onObjectSelectedByIndex;
    std::function<void(const std::string&)> onObjectSelectedByName;
    std::function<void(const std::string&)> onOpenNodeGraph;
    std::function<void(int)> onObjectFocused;
    std::function<void()> onSceneChanged;
    std::function<void(const std::string&, const std::string&, float, bool, int, const std::string&)> onBeginPlacement;
    float placementRadius = 50.0f;   // radio de afectación de la colocación (m)

    std::string nextName(const std::string& prefix);
    void addObject(std::shared_ptr<Haruka::SceneObject> obj);
    void selectObject(int index, const std::string& name);
    void renderLayerSection(const std::string& layer, const std::vector<int>& indices);
    void renderObjectTile(int index);
    void showContextMenu(int index);
    void duplicateObject(int index);

    std::shared_ptr<Haruka::SceneObject> makeMeshObject(const std::string& name,
                                                        const std::string& meshType,
                                                        const glm::vec3& albedo);
};
