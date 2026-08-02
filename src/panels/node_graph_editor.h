#pragma once

#include "core/scene/scene_manager.h"
#include "rhi/rhi_types.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Haruka {
namespace Tools { namespace ProcGraph {
    class Graph;
    struct RGBAImage;
}}
namespace Renderer { class RenderTarget; }
}

/**
 * @brief Editor visual de nodos para texturas/materiales (ProcGraph).
 *
 * Permite construir un grafo procedural que genera texturas (albedo, normal,
 * metallic, roughness, ao) y aplicarlas a los slots de textura del material del
 * objeto seleccionado. El grafo se serializa como JSON en
 * `obj->properties["materialGraph"]` y se evalúa con el ProcGraph del motor
 * (Perlin, Voronoi, FBM, math, ramps…) para preview y bake a PNG.
 */
class NodeGraphEditorPanel {
public:
    NodeGraphEditorPanel();
    // Definido en el .cpp: `materialPreviewTarget` es un tipo incompleto aquí y ~unique_ptr
    // necesita la definición.
    ~NodeGraphEditorPanel();

    /** @brief Fija la escena que el panel inspecciona. */
    void setScene(Haruka::SceneManager* scene);
    /** @brief Raíz del proyecto (para guardar las texturas horneadas). */
    void setProjectPath(const std::string& path) { projectPath = path; }
    /** @brief Callback de cambio de escena (marca dirty). */
    void setOnSceneChanged(std::function<void()> cb) { onSceneChanged = std::move(cb); }
    /** @brief Fija el objeto cuyo grafo de material se edita. */
    void setSelectedObject(Haruka::SceneObject* obj);
    /** @brief Renderiza el panel. */
    void onImGuiRender();

    /** @brief Genera el grafo por defecto (output + perlin) para la selección. */
    void createDefaultGraph();
    /**
     * @brief Genera el grafo de árbol estilo anime con una semilla.
     * @param foliage true -> copa (follaje), false -> corteza (nudos + ramas).
     * La semilla se guarda en `properties["treeSeed"]` del objeto: cada árbol
     * tiene su propia semilla que genera las texturas de su material.
     */
    void createTreeGraph(bool foliage);
    /** @brief Hornea las texturas conectadas del objeto seleccionado. */
    void bakeSelectedTextures() { bakeTextures(); }

private:
    struct GNode {
        int id = 0;
        std::string type;
        float x = 0, y = 0;                 // coordenadas de mundo (canvas)
        nlohmann::json params = nlohmann::json::object();
    };
    struct GConnection {
        int from = -1, fromOut = 0, to = -1, toIn = 0;
    };
    struct NodeLayout {
        ImVec2 min, max;
        ImVec2 headerMin, headerMax;
        float bodyTop = 0;
        std::vector<ImVec2> inPts, outPts;
    };
    struct SocketHit {
        int nodeId = -1;
        bool input = false;
        int index = -1;
    };
    struct SlotLink {
        int fromEngine = -1;
        int fromOut = 0;
        int toIn = 0;
        int kind = 0;                       // 0 = float, 1 = vec3
    };

    // --- Modelo del grafo ---
    GNode* findNode(int id);
    const GNode* findNode(int id) const;
    int outputNodeId() const;
    int addNodeRaw(const std::string& type, float x, float y);
    void removeNode(int id);
    void duplicateNode(int id);
    void markDirty();
    void syncToObject();
    nlohmann::json serializeGraph() const;
    void deserializeGraph(const nlohmann::json& j);

    // --- Canvas ---
    ImVec2 screenOfWorld(float wx, float wy) const;
    ImVec2 worldOfScreen(float sx, float sy) const;
    NodeLayout computeLayout(const GNode& n) const;
    int hitTestNode(const ImVec2& pos) const;
    /** @brief Mueve el nodo al final de `nodes` = se dibuja el ÚLTIMO, encima de los demás. */
    void bringNodeToFront(int nodeId);
    void zoomAt(const ImVec2& screen, float factor);
    void fitView();

    void renderToolbar();
    void renderCanvas();
    void renderPreview();
    void renderPopups();
    void renderAddMenu();
    void renderNode(ImDrawList* dl, GNode& n);
    void drawConnections(ImDrawList* dl);
    void drawWire(ImDrawList* dl, const ImVec2& a, const ImVec2& b, ImU32 col, float th);
    void handleCanvasInput();

    // --- Evaluación / bake ---
    bool buildEngineGraph(Haruka::Tools::ProcGraph::Graph& g,
                          std::map<int, int>& nodeToEngine,
                          std::vector<SlotLink>& links);
    void updatePreview();
    void uploadPreview(const Haruka::Tools::ProcGraph::RGBAImage& img, int w, int h);
    void bakeTextures();

    Haruka::SceneManager* currentScene = nullptr;
    Haruka::SceneObject* selectedObject = nullptr;
    std::string selectedObjectName;
    std::string projectPath;
    std::function<void()> onSceneChanged;

    std::vector<GNode> nodes;
    std::vector<GConnection> connections;
    int nextNodeId = 1;
    int selectedNodeId = -1;

    // Estado del canvas
    ImVec2 canvasOrigin = {48, 48};         // pantalla del origen (0,0) de mundo
    float zoom = 1.0f;
    ImVec2 canvasRectMin = {0, 0};
    ImVec2 canvasRectMax = {0, 0};
    ImVec2 addPosWorld = {0, 0};

    int draggingNodeId = -1;
    ImVec2 dragGrabWorld = {0, 0};
    bool draggingWire = false;
    int dragWireFrom = -1;
    int dragWireOut = 0;
    SocketHit hoveredSocket;
    SocketHit clickedSocket;        // socket sobre el que se pulsó el botón este frame
    int clickedBodyNode = -1;       // nodo cuyo cuerpo (InvisibleButton) se pulsó este frame
    int activeBodyNode = -1;        // nodo cuyo cuerpo está siendo arrastrado (botón activo)
    int ctxNode = -1;

    // Preview / bake
    std::string previewSlot = "albedo";
    int bakeSize = 256;
    bool normalFromHeight = true;
    bool previewDirty = true;
    int treeSeed = 1;               // semilla del árbol (crea sus texturas)
    int previewWidth = 0, previewHeight = 0;
    std::string previewStatus;
    Haruka::RHI::TextureHandle previewTex;
    // Target de la ESFERA de material (el motor pinta en él). Perezoso: solo existe cuando hay un
    // objeto con material seleccionado.
    std::unique_ptr<Haruka::Renderer::RenderTarget> materialPreviewTarget;
    uint32_t previewGL = 0;
};
