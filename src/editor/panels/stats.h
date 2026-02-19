#pragma once

#include <imgui.h>
#include <vector>
#include <deque>

class StatsPanel {
public:
    StatsPanel();
    
    void onImGuiRender();
    void update(float deltaTime);
    
    void setVertexCount(int count) { vertexCount = count; }
    void setDrawCalls(int count) { drawCalls = count; }
    void setTriangleCount(int count) { triangleCount = count; }

private:
    float fps = 0.0f;
    float frameTime = 0.0f;
    int vertexCount = 0;
    int drawCalls = 0;
    int triangleCount = 0;
    
    std::deque<float> fpsHistory;
    std::deque<float> frameTimeHistory;
    
    const size_t historySize = 100;
    
    float fpsAccumulator = 0.0f;
    int fpsFrameCount = 0;
    float fpsUpdateInterval = 0.5f;
};