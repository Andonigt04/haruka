#pragma once

#include <imgui.h>
#include <vector>
#include <deque>

class StatsPanel {
public:
    StatsPanel();
    
    void onImGuiRender();
    void update(float deltaTime);
    
    void setVertexCount(int count) { renderedVertexCount = count; }
    void setDrawCalls(int count) { renderedDrawCalls = count; }
    void setTriangleCount(int count) { renderedTriangleCount = count; }

    void setTotalVertexCount(int count) { totalVertexCount = count; }
    void setTotalDrawCalls(int count) { totalDrawCalls = count; }
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