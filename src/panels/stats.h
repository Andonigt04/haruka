#pragma once

#include <imgui.h>
#include <vector>
#include <deque>

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
    /** @brief Sets currently visible terrain chunk count. */
    void setVisibleChunkCount(int count) { visibleChunkCount = count; }
    /** @brief Sets currently resident terrain chunk count. */
    void setResidentChunkCount(int count) { residentChunkCount = count; }
    /** @brief Sets pending chunk-load queue size. */
    void setPendingChunkLoads(int count) { pendingChunkLoads = count; }
    /** @brief Sets pending chunk-eviction queue size. */
    void setPendingChunkEvictions(int count) { pendingChunkEvictions = count; }
    /** @brief Sets current resident chunk memory in MB. */
    void setResidentMemoryMB(int mb) { residentMemoryMB = mb; }
    /** @brief Sets tracked chunk count in streaming system. */
    void setTrackedChunkCount(int count) { trackedChunkCount = count; }
    /** @brief Sets streaming memory budget (MB). */
    void setMaxMemoryMB(int mb) { maxMemoryMB = mb; }

private:
    // Frame timing
    float fps = 0.0f;
    float frameTime = 0.0f;
    
    // Rendering stats
    int renderedVertexCount = 0;
    int renderedTriangleCount = 0;
    int renderedDrawCalls = 0;

    // Total vertices/triangles/draw calls updated by the application each frame (not just this render pass)
    int totalVertexCount = 0;
    int totalDrawCalls = 0;
    int totalTriangleCount = 0;

    // Terrain streaming stats
    int visibleChunkCount = 0;
    
    int residentChunkCount = 0;
    int pendingChunkLoads = 0;

    int pendingChunkEvictions = 0;
    int trackedChunkCount = 0;

    // Memory stats
    int residentMemoryMB = 0;
    int maxMemoryMB = 0;
    
    std::deque<float> fpsHistory;
    std::deque<float> frameTimeHistory;
    
    const size_t historySize = 100;
    
    float fpsAccumulator = 0.0f;
    int fpsFrameCount = 0;
    float fpsUpdateInterval = 0.5f;
};