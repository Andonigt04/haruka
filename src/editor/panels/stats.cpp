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
    ImGui::Begin("Performance Stats");
    
    // Stats principales
    ImGui::Text("FPS: %.1f", fps);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%.2f ms)", frameTime);
    
    ImGui::Separator();
    
    // Gráfico de FPS
    ImGui::Text("FPS History:");
    std::vector<float> fpsVec(fpsHistory.begin(), fpsHistory.end());
    ImGui::PlotLines("##fps", fpsVec.data(), (int)fpsVec.size(), 0, nullptr, 0.0f, 120.0f, ImVec2(0, 80));
    
    // Gráfico de Frame Time
    ImGui::Text("Frame Time (ms):");
    std::vector<float> ftVec(frameTimeHistory.begin(), frameTimeHistory.end());
    ImGui::PlotLines("##frametime", ftVec.data(), (int)ftVec.size(), 0, nullptr, 0.0f, 33.0f, ImVec2(0, 80));
    
    ImGui::Separator();
    
    // Rendering stats
    ImGui::Text("Rendering:");
    ImGui::BulletText("Vertices Rendered: %d", renderedVertexCount);
    ImGui::BulletText("Vertices Total: %d", totalVertexCount);
    ImGui::BulletText("Triangles Rendered: %d", renderedTriangleCount);
    ImGui::BulletText("Triangles Total: %d", totalTriangleCount);
    ImGui::BulletText("Draw Calls Rendered: %d", renderedDrawCalls);
    ImGui::BulletText("Draw Calls Total: %d", totalDrawCalls);
    
    ImGui::Separator();
    
    // Performance hints
    if (fps < 30.0f) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "⚠ Low FPS detected");
    } else if (fps < 60.0f) {
        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "⚠ FPS below 60");
    } else {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ Good performance");
    }
    
    ImGui::End();
}