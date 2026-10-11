#pragma once

#include "backend/scene_model.h"
#include "commands/command.h"

#include <functional>
#include <string>

class CommandHistory;

/// Cambiar una propiedad de una escena genérica, deshacible. `execute` y `undo` solo PIDEN el
/// cambio al modelo: con un motor remoto se aplica cuando el motor contesta.
class SetPropertyCommand final : public ICommand {
public:
    SetPropertyCommand(ISceneModel* model, std::string node, SceneProperty before, SceneProperty after)
        : model(model), node(std::move(node)), before(std::move(before)), after(std::move(after)) {}
    void execute() override { model->setProperty(node, after); }
    void undo() override { model->setProperty(node, before); }

private:
    ISceneModel* model;
    std::string node;
    SceneProperty before, after;
};

/// Árbol de nodos de un ISceneModel. Clic = seleccionar (en el motor: la selección es suya).
class GenericHierarchyPanel {
public:
    void setModel(ISceneModel* m) { model = m; }
    void onImGuiRender();

private:
    ISceneModel* model = nullptr;
};

/**
 * @brief Propiedades del nodo seleccionado, con un widget por tipo.
 *
 * Mientras se arrastra un valor se manda cada cambio al motor (se ve en vivo en su viewport), pero
 * al historial entra UN solo comando al soltar, con el valor de antes de empezar: deshacer un
 * arrastre lo deshace entero, no el último píxel.
 */
class GenericInspectorPanel {
public:
    void setModel(ISceneModel* m) { model = m; }
    void setHistory(CommandHistory* h) { history = h; }
    /// Cada cambio enviado (para el "sin guardar" del editor).
    void setOnChanged(std::function<void()> fn) { onChanged = std::move(fn); }
    void onImGuiRender();

private:
    void editProperty(const SceneNode& node, const SceneProperty& prop);

    ISceneModel* model = nullptr;
    CommandHistory* history = nullptr;
    std::function<void()> onChanged;

    // El widget que se está editando: su valor local manda sobre el del modelo hasta soltarlo,
    // porque el del modelo llega con retraso (ida y vuelta al motor).
    std::string activeKey;
    SceneProperty activeBefore, activeCurrent;
};
