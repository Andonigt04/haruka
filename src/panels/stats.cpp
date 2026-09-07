#include "stats.h"
#include <algorithm>
#include <imgui.h>

// Dibuja un subárbol del profiler (hijos de `parent`), ordenados por ms desc. `frameMs` = total
// del frame de referencia para el %. Indenta por `depth` y colorea según el peso relativo.
void StatsPanel::drawProfilerNode(const std::vector<Haruka::Profiler::Node>& nodes, int parent,
                                  float frameMs, int depth) {
    const Haruka::Profiler::Node& p = nodes[(size_t)parent];
    std::vector<int> kids(p.children);
    std::sort(kids.begin(), kids.end(), [&](int a, int b) { return nodes[(size_t)a].ms > nodes[(size_t)b].ms; });
    for (int kid : kids) {
        const Haruka::Profiler::Node& n = nodes[(size_t)kid];
        const float pct = frameMs > 0.01f ? (float)(n.ms * 100.0 / frameMs) : 0.0f;

        ImGui::Text("%*s%s", depth * 3, "", n.name.c_str());
        ImGui::SameLine();
        ImVec4 col(0.9f, 0.9f, 0.9f, 1.0f);
        if (pct > 30.0f)      col = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
        else if (pct > 15.0f) col = ImVec4(1.0f, 0.7f, 0.2f, 1.0f);
        else if (pct > 5.0f)  col = ImVec4(0.85f, 0.85f, 0.2f, 1.0f);
        ImGui::TextColored(col, "%.2f ms (%.1f%%)", n.ms, pct);
        if (n.count > 1) {
            ImGui::SameLine();
            ImGui::TextDisabled("×%d", n.count);
        }
        drawProfilerNode(nodes, kid, frameMs, depth + 1);
    }
}

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
    ImGui::Text("Terreno (planeta):");
    ImGui::BulletText("Base: %d vértices · %d triángulos", baseVertexCount, baseTriangleCount);
    ImGui::BulletText("Clipmap: %d vértices · %d triángulos", clipVertexCount, clipTriangleCount);
    ImGui::BulletText("Draws (base + clipmap): %d", terrainDrawCalls);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("El terreno es malla fija + clipmap (no hay streaming por chunks).");
    
    ImGui::Separator();
    
    // Perfilado CPU por etapa (árbol del Profiler del último frame del hilo de render).
    ImGui::Text("Perfilado (CPU):");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Tiempo CPU (inclusivo) de cada etapa del frame. Los % son del total de "
                          "renderFrameContent; rojo >30%%, naranja >15%%, amarillo >5%%.");
    if (profilerNodes.empty()) {
        ImGui::TextDisabled("(sin datos — corre un frame para llenar el árbol)");
    } else {
        // Total de referencia: renderFrameContent si está (inclusivo = cubre todo el contenido),
        // si no la mayor etapa de tope-nivel.
        float frameMs = 0.0f;
        bool  hasFrame = false;
        for (const auto& n : profilerNodes) {
            if (n.name == "renderFrameContent") { frameMs = (float)n.ms; hasFrame = true; break; }
        }
        if (!hasFrame)
            for (const auto& n : profilerNodes)
                frameMs = std::max(frameMs, (float)n.ms);
        drawProfilerNode(profilerNodes, 0, frameMs, 0);
        if (frameMs <= 0.0f) ImGui::TextDisabled("(total %.2f ms)", frameMs);
    }

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