#pragma once

#include <chrono>
#include <deque>
#include <string>
#include <map>
#include <glm/glm.hpp>

/**
 * DebugOverlay - Información de performance en tiempo real
 * 
 * Muestra:
 * - FPS (frames per second)
 * - Frame time (ms)
 * - Draw calls
 * - Memoria RAM/VRAM
 * - Light count (total + culled)
 * - Asset streaming stats
 * - GPU stats
 * 
 * Impacto: Visibilidad, sin overhead significativo
 */

struct FrameMetrics {
    float fps = 0.0f;
    float frameTimeMs = 0.0f;
    int drawCalls = 0;
    int renderTargetBinds = 0;
    int shaderSwitches = 0;
    
    // Memoria
    size_t ramUsage = 0;
    size_t vramUsage = 0;
    
    // Lighting
    int totalLights = 0;
    int culledLights = 0;
    
    // Assets
    int loadedAssets = 0;
    int pendingAssets = 0;
    float cacheUtilization = 0.0f;
    
    // GPU
    int totalTriangles = 0;
    int totalVertices = 0;
    
    // Shadows
    int activeCascade = 0;  // Cascada activa actual
    int numCascades = 4;    // Total de cascadas
};

class DebugOverlay {
public:
    static DebugOverlay& getInstance() {
        static DebugOverlay instance;
        return instance;
    }

    /**
     * Inicializar overlay
     */
    void init();

    /**
     * Render overlay en ImGui
     * Llamar en el main loop después de todos los renders
     */
    void render();

    /**
     * Actualizar métricas
     */
    void updateMetrics(const FrameMetrics& metrics);

    /**
     * Toggle overlay visibility
     */
    void toggle() { visible = !visible; }
    void show() { visible = true; }
    void hide() { visible = false; }
    bool isVisible() const { return visible; }

    /**
     * Agregar métrica custom
     */
    void addMetric(const std::string& name, float value) {
        customMetrics[name] = value;
    }

    void addMetric(const std::string& name, int value) {
        customMetrics[name] = static_cast<float>(value);
    }

    void addMetric(const std::string& name, const std::string& value) {
        customMetricsStr[name] = value;
    }

    /**
     * Modo de overlay
     */
    enum OverlayMode {
        MINIMAL,      // Solo FPS + frame time
        STANDARD,     // FPS, memoria, luces, assets
        DETAILED,     // Todo + historial de FPS
        GRAPH         // Gráficas de performance
    };

    void setMode(OverlayMode mode) { overlayMode = mode; }
    OverlayMode getMode() const { return overlayMode; }

    /**
     * Obtener últimas métricas
     */
    FrameMetrics getLastMetrics() const { return lastMetrics; }
    
    FrameMetrics getAverageMetrics() const;

    ~DebugOverlay() = default;

private:
    DebugOverlay() = default;

    void renderMinimal();
    void renderStandard();
    void renderDetailed();
    void renderGraphs();

    void updateFPSHistory();

    bool visible = true;
    OverlayMode overlayMode = OverlayMode::STANDARD;

    FrameMetrics lastMetrics;
    std::deque<float> fpsHistory;
    std::deque<float> frameTimeHistory;
    
    std::map<std::string, float> customMetrics;
    std::map<std::string, std::string> customMetricsStr;

    // Timing
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    int frameCount = 0;
    float averageFPS = 0.0f;

    // Colores para overlay
    glm::vec4 colorNormal = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);   // Verde
    glm::vec4 colorWarning = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);  // Amarillo
    glm::vec4 colorCritical = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // Rojo
};
