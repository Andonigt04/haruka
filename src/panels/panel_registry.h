#pragma once

#include <deque>
#include <functional>
#include <string>

/**
 * @brief Una ventana del editor tal como la ve el registro.
 *
 * El registro no conoce la clase del panel: solo sabe dibujarlo, cerrarlo y avisarle de que
 * cambian el proyecto o la escena. Cualquier cosa que rellene esto (un panel del editor hoy, un
 * plugin mañana) sale en el menú View, se abre con HARUKA_EDITOR_OPEN y recibe la escena nueva
 * sin que nadie tenga que acordarse de añadirla en otro sitio.
 */
struct PanelEntry {
    /// Clave estable: la usa HARUKA_EDITOR_OPEN y la buscan los menús ("world", "prefab"…).
    std::string id;
    /// Texto del menú View.
    std::string label;
    /// Bloque del menú View; entre bloques distintos va un separador.
    int menuGroup = 0;
    bool open = false;
    /// En play mode se dibuja deshabilitado (no se edita la escena mientras corre el juego).
    bool lockedInPlay = true;

    /// Dibuja la ventana. Solo se llama con el panel abierto.
    std::function<void()> draw;
    /// Cada frame con el panel CERRADO: soltar lo que el panel tenga enganchado a otro
    /// (la pieza del gizmo, el pincel del viewport).
    std::function<void()> whileClosed;
    /// Ruta del proyecto abierto.
    std::function<void(const std::string&)> onProjectChanged;
    /// Antes de cambiar la escena: borrar selecciones (punteros crudos a la escena vieja).
    std::function<void()> clearSelection;
    /// La escena nueva, OPACA: el registro es del núcleo y no conoce tipos del motor. Quien
    /// registra el panel sabe qué es (el backend de Haruka la convierte a SceneManager*).
    std::function<void(void*)> onSceneChanged;

    /// Quién lo registró: vacío = el editor; si no, el nombre del plugin. Al descargar un plugin
    /// se quitan sus paneles ANTES del dlclose: sus funciones viven en el código que se descarga.
    std::string owner;
};

/**
 * @brief Lista de ventanas del editor: un panel se añade en UN sitio.
 *
 * Antes cada panel vivía en cuatro (su bandera `show…`, su bloque en renderUI, su línea en el
 * menú View y las listas de sincronización de proyecto y escena), y olvidar uno de ellos era la
 * causa de los dos bugs que documentan `syncPanelsToProject` y `bindPanelsToScene`.
 *
 * ⚠️ EL ORDEN DE REGISTRO ES EL DE DIBUJO Y EL DE AVISO. Importa: el panel de prefabricados le
 * cede su pieza al gizmo en su `draw`, así que tiene que ir antes que el viewport; y el viewport
 * reinicia la Application del motor en `onSceneChanged`, así que va detrás de los demás con escena.
 */
class PanelRegistry {
public:
    /// Registra un panel. La referencia devuelta sigue valiendo al añadir más (deque), pero NO
    /// tras `removeOwner`.
    PanelEntry& add(PanelEntry entry);
    /// Quita todos los paneles de un dueño. Devuelve cuántos. No llamar desde dentro de `drawAll`.
    size_t removeOwner(const std::string& owner);
    /// nullptr si no hay ninguno con ese id.
    PanelEntry* find(const std::string& id);
    /// Bandera de abierto para un MenuItem fuera del menú View. nullptr si no existe.
    bool* openFlag(const std::string& id);

    /// Dibuja los abiertos y avisa a los cerrados. Una excepción de un panel no tumba a los demás.
    void drawAll(bool playMode);
    /// Un MenuItem por panel, con separador entre grupos. Va dentro de un BeginMenu.
    void drawViewMenuItems();

    /// Abre los paneles de una lista "world,prefab" (ids separados por comas o espacios).
    void openFromList(const std::string& list);

    void projectChanged(const std::string& root);
    /// Primero TODAS las selecciones y después la escena, en orden de registro.
    void sceneChanged(void* scene);

private:
    std::deque<PanelEntry> entries;
};
