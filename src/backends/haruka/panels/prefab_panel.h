#pragma once
/**
 * @file panels/prefab_panel.h
 * @brief ANFITRIÓN del panel de prefabricados en el editor.
 *
 * El panel en sí vive en el MOTOR (`Tools/PrefabEditPanel.h`) y no aquí, por la misma razón que
 * `preview_render`: **el juego no enlaza el editor**, y un editor de montajes que sólo existe en el
 * IDE obliga a salir del juego para tocar un prefabricado y condena a escribir un segundo editor el
 * día que el juego quiera montar algo. Lo reportó Andoni con una pregunta de una línea: "¿y si
 * quiero desde el juego, sin tener que editarlo?".
 *
 * Lo que queda aquí es lo que de verdad es del editor y el juego no tiene:
 *
 *   · DÓNDE están los montajes — en el proyecto abierto, no en los assets del juego.
 *   · CÓMO se ve un item — el editor no tiene `ItemRegistry` poblado (eso lo hace el juego al
 *     arrancar), así que lee lo mínimo del JSON de items del proyecto: su `.glb` si lo declara y,
 *     si no, su tamaño para dibujar una caja. Sin esto todas las piezas serían cubos de 1 m.
 *   · EL GIZMO — ImGuizmo sobre el viewport. El juego no tiene ninguna de las dos cosas, así que el
 *     panel del motor no lo conoce: expone qué pieza está seleccionada y aquí se conecta.
 */
#include "Haruka/Tools/PrefabEditPanel.h"

#include <functional>
#include <string>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Haruka::Core::Scene { class SceneManager; struct SceneObject; }
class ViewportPanel;

/** @brief El panel del motor, alimentado con lo que sabe el editor. */
class PrefabPanel {
public:
    /** @brief Viewport al que ceder la pieza seleccionada para que su gizmo la manipule. */
    void setViewport(ViewportPanel* vp) { viewport = vp; }

    /** @brief Raíz del proyecto abierto: de ahí cuelgan `assets/data/prefabs` y `assets/data/items`. */
    void setProjectPath(const std::string& path);

    /**
     * @brief Escena en la que COLOCAR montajes, y a la que se le instala el expansor.
     *
     * Colocar un prefabricado mete en la escena UNA entrada (`prefabName` + pose); las piezas las
     * pone el expansor y van marcadas `fromPrefab`, así que al guardar la escena escribe la entrada
     * y no las 870 tablas. Esa es toda la diferencia entre un prefabricado y copiar objetos.
     */
    void setScene(Haruka::Core::Scene::SceneManager* scene);

    /** @brief Dibuja el panel y reengancha el gizmo a la pieza seleccionada. */
    void onImGuiRender();

    /** @brief Suelta la pieza que tenga el viewport. Al cerrar el panel o cambiar de proyecto. */
    void releaseGizmo();

    /** @brief Deselecciona la pieza Y se la quita al gizmo: lo llama quien selecciona OTRA cosa. */
    void dropPieceSelection() { panel.clearSelection(); releaseGizmo(); }

    /** @brief Avisar de que la escena ha cambiado (para que el editor marque "sin guardar"). */
    void setOnSceneChanged(std::function<void()> fn) { m_onChanged = std::move(fn); }

private:
    /// Lo mínimo que el editor necesita saber de un item para dibujarlo.
    struct ItemLook {
        std::string model;
        glm::vec3   size{0.5f};
    };

    void loadItemIndex();
    Haruka::Tools::PreviewGeo geoOf(const std::string& itemId) const;

    /// Pone en la escena las piezas de UNA entrada de prefabricado. Es lo que el editor entiende
    /// por "expandir": mallas que se ven y se mueven. El juego hace otra cosa (física, colisión).
    void expandEntry(Haruka::Core::Scene::SceneManager& scene, const Haruka::Core::Scene::SceneObject& entry);

    /**
     * @brief Recoge en las ENTRADAS lo que se le haya hecho a sus piezas en la escena.
     *
     * Es la mitad que faltaba: mover una pieza con el gizmo cambiaba el objeto y la entrada no se
     * enteraba, así que al guardar se perdía. Aquí se compara cada pieza con la pose que le tocaría
     * según el montaje y, si se desvía, se anota en `prefabEdits` — que es lo único que se guarda.
     *
     * Va por comparación y no por "avísame cuando muevas" a propósito: así recoge el gizmo, el
     * inspector, un comando de deshacer o cualquier cosa futura que toque el objeto, sin que ninguna
     * tenga que acordarse de llamar a nadie.
     */
    void syncEditsFromScene();

    /// El montaje SIN cambios, cacheado por nombre: es la referencia contra la que se mide la
    /// desviación. Con los cambios ya aplicados no se podría distinguir "movida" de "así nació".
    const Haruka::Game::Prefab::Prefab* basePrefab(const std::string& name);

    Haruka::Tools::PrefabEditPanel panel;
    ViewportPanel* viewport = nullptr;
    Haruka::Core::Scene::SceneManager* m_scene = nullptr;

    std::string m_projectPath;
    std::unordered_map<std::string, ItemLook> m_items;
    bool m_itemsLoaded = false;
    std::unordered_map<std::string, Haruka::Game::Prefab::Prefab> m_baseCache;

    /// Última pose vista de cada ENTRADA. Sirve para distinguir "han movido el montaje" (hay que
    /// arrastrar sus piezas) de "han movido una pieza" (hay que anotarlo en `prefabEdits`). Sin
    /// esta memoria las dos cosas son idénticas vistas desde un solo frame.
    struct EntryPose { glm::dvec3 pos{0.0}; glm::dquat rot{1.0, 0.0, 0.0, 0.0}; bool valid = false; };
    std::unordered_map<uint64_t, EntryPose> m_lastEntryPose;
    std::function<void()> m_onChanged;
};
