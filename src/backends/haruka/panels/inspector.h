#pragma once

#include "Haruka/Core/Scene/SceneManager.h"
#include "commands/command_history.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <functional>
#include <memory>

namespace Haruka { namespace Renderer { class RenderTarget; } }

class InspectorPanel {
public:
    InspectorPanel();
    ~InspectorPanel();

    void setScene(Haruka::Core::Scene::SceneManager* scene);
    void setSelectedObjectIndex(int index);
    void setCommandHistory(CommandHistory* history);
    void setPlayMode(bool mode) { playMode = mode; }
    void setProjectPath(const std::string& path) { projectPath = path; }
    void onImGuiRender();
    void setOnSceneChanged(std::function<void()> cb) { onSceneChanged = std::move(cb); }

private:
    Haruka::Core::Scene::SceneManager* currentScene = nullptr;
    int selectedObjectIndex = -1;
    CommandHistory* commandHistory = nullptr;
    bool playMode = false;
    std::string projectPath;
    
    bool editingPosition = false, editingRotation = false, editingScale = false;
    glm::dvec3 editStartPosition, editStartRotation, editStartScale;
    std::function<void()> onSceneChanged;

    /** @brief Dibuja la esfera de preview + las miniaturas de cada slot de textura. */
    void renderMaterialPreview(Haruka::Core::Scene::SceneObject& obj);

    // Target de la preview de material. Perezoso y REUTILIZADO entre objetos: la preview se
    // redibuja cada frame (una esfera de 128² no se nota), pero crear el FBO por frame sí.
    std::unique_ptr<Haruka::Renderer::RenderTarget> matPreviewTarget;
};
