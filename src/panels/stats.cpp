#include "stats.h"
#include <algorithm>

StatsPanel::StatsPanel() {
    fpsHistory.resize(historySize, 0.0f);
    frameTimeHistory.resize(historySize, 0.0f);
}

void StatsPanel::update(float deltaTime) {
    frameTime = deltaTime * 1000.0f; // ms
    
    fpsAccumulator += deltaTime;
    fpsFrameCount++;
    
    if (fpsAccumulator >= fpsUpdateInterval) {
        fps = fpsFrameCount / fpsAccumulator;
        fpsAccumulator = 0.0f;
        fpsFrameCount = 0;
    }
    
    // Actualizar historial
    fpsHistory.pop_front();
    fpsHistory.push_back(fps);
    
    frameTimeHistory.pop_front();
    frameTimeHistory.push_back(frameTime);
}

void StatsPanel::onImGuiRender() {
    ImGui::Begin("Stats");

    ImGui::Text("%.1f FPS  (%.2f ms)", fps, frameTime);
    std::vector<float> fpsVec(fpsHistory.begin(), fpsHistory.end());
    ImGui::PlotLines("##fps", fpsVec.data(), (int)fpsVec.size(), 0, nullptr, 0.0f, 120.0f, ImVec2(-1, 50));

    ImGui::Separator();
    ImGui::Text("Rendering");
    ImGui::BulletText("Verts:  %d", renderedVertexCount);
    ImGui::BulletText("Tris:   %d", renderedTriangleCount);
    ImGui::BulletText("Draws:  %d", renderedDrawCalls);

    ImGui::Separator();
    ImGui::Text("Terrain");
    ImGui::BulletText("Visible:  %d", visibleChunkCount);
    ImGui::BulletText("Resident: %d", residentChunkCount);
    ImGui::BulletText("Memory:   %d MB", residentMemoryMB);

    ImGui::End();
}