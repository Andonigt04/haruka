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

private:
    float fps = 0.0f;
    float frameTime = 0.0f;
    int renderedVertexCount = 0;
    int renderedDrawCalls = 0;
    int renderedTriangleCount = 0;
    int totalVertexCount = 0;
    int totalDrawCalls = 0;
    int totalTriangleCount = 0;
    
    std::deque<float> fpsHistory;
    std::deque<float> frameTimeHistory;
    
    const size_t historySize = 100;
    
    float fpsAccumulator = 0.0f;
    int fpsFrameCount = 0;
    float fpsUpdateInterval = 0.5f;
};