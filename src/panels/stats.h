#pragma once

#include <imgui.h>
#include <vector>
#include <deque>
#include "tools/profiler.h"   // Haruka::Profiler::Node — árbol CPU por etapa

/** @brief Real-time rendering and frame statistics panel. */
class StatsPanel {
public:
    /** @brief Constructs a statistics panel. */
    StatsPanel();
    
    /** @brief Draws the stats UI. */
    void onImGuiRender();
    /** @brief Updates frame timing state. */
    void update(float deltaTime);
    
    /** @brief Sets last rendered vertex count. */
    void setVertexCount(int count) { renderedVertexCount = count; }
    /** @brief Sets last rendered draw call count. */
    void setDrawCalls(int count) { renderedDrawCalls = count; }
    /** @brief Sets last rendered triangle count. */
    void setTriangleCount(int count) { renderedTriangleCount = count; }

    /** @brief Sets total vertex count. */
    void setTotalVertexCount(int count) { totalVertexCount = count; }
    /** @brief Sets total draw call count. */
    void setTotalDrawCalls(int count) { totalDrawCalls = count; }
    /** @brief Sets total triangle count. */
    void setTotalTriangleCount(int count) { totalTriangleCount = count; }
    /** @brief Sets terrain geometry breakdown (base mesh, clipmap) + draw calls.
     *  El desglose de AGUA se retiró: el mar dejó de ser una malla propia del planeta y sus
     *  contadores (`RenderStats::waterVertices/waterTriangles`) ya no existen en el motor. */
    void setTerrainStats(int baseVerts, int baseTris, int clipVerts, int clipTris, int drawCalls) {
        baseVertexCount = baseVerts;   baseTriangleCount = baseTris;
        clipVertexCount = clipVerts;   clipTriangleCount = clipTris;
        terrainDrawCalls = drawCalls;
    }

    /** @brief Árbol CPU del último frame (renderFrameContent, water.draw, …) para el desglose
     *  "qué etapa tarda más". Se copia: el snapshot del profiler es estable durante el frame. */
    void setProfilerNodes(const std::vector<Haruka::Profiler::Node>& nodes) { profilerNodes = nodes; }

private:
    /** @brief Dibuja el árbol del profiler recursivamente (subárbol de `parent`). */
    void drawProfilerNode(const std::vector<Haruka::Profiler::Node>& nodes, int parent,
                          float frameMs, int depth);

    float fps = 0.0f;
    float frameTime = 0.0f;
    int renderedVertexCount = 0;
    int renderedDrawCalls = 0;
    int renderedTriangleCount = 0;
    int totalVertexCount = 0;
    int totalDrawCalls = 0;
    int totalTriangleCount = 0;
    // Terreno del último frame (el "Chunk Streaming" legacy era de la arquitectura de chunks,
    // que ya no existe: el planeta es malla base + clipmap + agua).
    int baseVertexCount = 0,  baseTriangleCount = 0;
    int clipVertexCount = 0,  clipTriangleCount = 0;
    int terrainDrawCalls = 0;
    // Árbol CPU del último frame del hilo de render (ms inclusivos por etapa).
    std::vector<Haruka::Profiler::Node> profilerNodes;
    
    std::deque<float> fpsHistory;
    std::deque<float> frameTimeHistory;
    
    const size_t historySize = 100;
    
    float fpsAccumulator = 0.0f;
    int fpsFrameCount = 0;
    float fpsUpdateInterval = 0.5f;
};