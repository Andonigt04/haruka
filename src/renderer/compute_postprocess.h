#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <memory>
#include <string>

/**
 * ComputePostProcess - Post-processing basado en compute shaders
 * 
 * Ventajas:
 * - +20% más rápido que fragment shaders
 * - Mejor paralelización en GPU
 * - Cache coherence mejorada
 * - SSBO para datos
 * 
 * Operaciones soportadas:
 * - Bloom extraction + blur
 * - Tone mapping (ACES, Reinhard)
 * - Color grading
 */

class ComputePostProcess {
public:
    enum ToneMapMode {
        TONE_LINEAR,
        TONE_REINHARD,
        TONE_ACES,
        TONE_FILMIC
    };

    ComputePostProcess();
    ~ComputePostProcess();

    /**
     * Inicializar post-processing
     * @param width Screen width
     * @param height Screen height
     */
    void init(int width, int height);

    /**
     * Aplicar bloom usando compute shader
     * @param inputTexture Texture HDR de entrada
     * @param outputTexture Texture para bloom
     * @param threshold Threshold para bloom
     * @param strength Intensidad del bloom
     */
    void bloomCompute(
        GLuint inputTexture,
        GLuint outputTexture,
        float threshold = 1.0f,
        float strength = 1.0f
    );

    /**
     * Aplicar tone mapping
     * @param inputTexture Texture HDR
     * @param outputTexture Texture LDR (final)
     * @param exposure Exposición
     * @param mode Modo de tone mapping
     */
    void toneMappingCompute(
        GLuint inputTexture,
        GLuint outputTexture,
        float exposure = 1.0f,
        ToneMapMode mode = TONE_ACES
    );

    /**
     * Aplicar color grading
     * @param inputTexture Texture de entrada
     * @param outputTexture Texture de salida
     * @param saturation Saturación (0.0-2.0)
     * @param contrast Contraste (0.0-2.0)
     * @param brightness Brillo (-1.0-1.0)
     */
    void colorGradingCompute(
        GLuint inputTexture,
        GLuint outputTexture,
        float saturation = 1.0f,
        float contrast = 1.0f,
        float brightness = 0.0f
    );

    /**
     * Aplicar todos los post-effects
     */
    void processAll(
        GLuint inputTexture,
        GLuint outputTexture,
        float exposure = 1.0f,
        float bloomThreshold = 1.0f,
        float bloomStrength = 1.0f,
        ToneMapMode toneMode = TONE_ACES,
        float saturation = 1.0f,
        float contrast = 1.0f,
        float brightness = 0.0f
    );

    /**
     * Obtener estadísticas
     */
    struct ComputeStats {
        int dispatchWidth;
        int dispatchHeight;
        int localGroupSize;
        float estimatedSpeedup;  // vs fragment shader
    };

    ComputeStats getStats() const { return stats; }

private:
    GLuint bloomShader = 0;
    GLuint toneMappingShader = 0;
    GLuint colorGradingShader = 0;

    int screenWidth = 0;
    int screenHeight = 0;
    int localGroupSize = 8;  // 8x8 compute groups

    ComputeStats stats;

    GLuint compileComputeShader(const std::string& source);
    void dispatchCompute(GLuint shader, int width, int height);
};
