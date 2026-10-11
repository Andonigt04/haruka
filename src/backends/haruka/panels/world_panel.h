#pragma once
/**
 * @file panels/world_panel.h
 * @brief ANFITRIÓN del pincel del mundo en el editor.
 *
 * El panel vive en el MOTOR (`Tools/WorldEditPanel.h`), como el de prefabricados y por la misma
 * razón: el juego no enlaza el editor y Andoni quiere tocar el mundo también desde el juego. Aquí
 * está lo que sólo el editor tiene:
 *
 *   · LA ESCENA: una cueva o isla nueva es una entrada del `.scene` (`"type": "Cave"/"Island"`,
 *     ver `PLAN_PINCEL_MUNDO.md` §2). El panel del motor la pone en el `VoxWorld` (se ve al
 *     instante) y avisa; aquí se escribe la entrada con `AddObjectCommand` (deshacible).
 *   · EL PROYECTO: dónde van los PNG de las cuevas/islas nuevas (`assets/vox/`).
 *   · EL VIEWPORT: el rayo del ratón y el dibujo del indicador (`ViewportPanel::handleWorldTool`).
 */
#include "Haruka/Tools/WorldEditPanel.h"

#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace Haruka::Core::Scene { class SceneManager; }
class CommandHistory;

class WorldPanel {
public:
    void setScene(Haruka::Core::Scene::SceneManager* scene) { m_scene = scene; }
    void setCommandHistory(CommandHistory* h) { m_history = h; }
    void setProjectPath(const std::string& root);
    void setOnSceneChanged(std::function<void()> fn) { m_onChanged = std::move(fn); }

    /** @brief Dibuja el panel; antes engancha el `VoxWorld` del planeta activo (puede cambiar al
     *  abrir otra escena). */
    void onImGuiRender();
    /** @brief El panel del motor, para que el viewport le pase el gesto. */
    Haruka::Tools::WorldEditPanel& panel() { return m_panel; }
    /** @brief Al cerrar el panel: sin herramienta activa en el viewport. */
    void deactivate() { m_panel.setTool(Haruka::Tools::WorldEditPanel::Tool::None); }

private:
    /** @brief Vista de edición (clima/agua/cota sobre el mar) y coordenadas de cámara y cursor. */
    void drawEditView();
public:

    /** @brief Guarda los trazos del mundo (lo llama el guardado de la escena). */
    void saveWorldEdits();

private:
    void attachWorld();
    /// LA ESCENA MANDA: cada frame se comparan las entradas Cave/Island con las definiciones del
    /// `VoxWorld` y se aplican las diferencias (añadida, quitada, movida con el gizmo, editada en
    /// el inspector, deshecha con Ctrl+Z). Así ningún camino tiene que avisar a nadie.
    void syncFromScene();
    void addSceneEntry(const std::string& type, const std::string& name, const glm::dvec3& dir,
                       const nlohmann::json& props);
    void removeSceneEntry(const std::string& name);

    Haruka::Tools::WorldEditPanel m_panel;
    Haruka::Core::Scene::SceneManager*  m_scene = nullptr;
    CommandHistory*        m_history = nullptr;
    std::string            m_projectPath;
    bool                   m_wired = false;
    std::function<void()>  m_onChanged;
};
