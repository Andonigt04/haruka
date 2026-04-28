#pragma once

/**
 * @file IEngine.h
 * @brief Contrato entre el IDE de Haruka y cualquier motor de renderizado.
 *
 * El IDE no sabe nada de Vulkan, OpenGL, DirectX ni Metal.
 * El motor implementa esta interfaz. Si en el futuro se cambia de motor,
 * solo hay que reimplementar IEngine — el IDE no cambia.
 *
 * Flujo de un frame:
 *   1. IDE llama a beginFrame()
 *   2. IDE llama a renderImGui(drawData)
 *   3. IDE llama a endFrame()
 *
 * El motor renderiza la escena internamente y expone
 * getViewportTextureID() para que el IDE la muestre con ImGui::Image().
 */

#include <cstdint>

struct SDL_Window;
struct ImDrawData;

namespace Haruka { class Scene; }
class Camera;

// ── Tipos opacos que el IDE usa sin saber qué hay dentro ──────────────────
using EngineTextureID = void*;  ///< ImTextureID — opaco, ImGui::Image() lo usa directamente

/** @brief Resultado de la inicialización del motor. */
struct EngineInitResult {
    bool        success  = false;
    const char* errorMsg = nullptr;
};

/** @brief Estadísticas de renderizado expuestas al IDE. */
struct EngineStats {
    int   renderedVertices  = 0;
    int   renderedTriangles = 0;
    int   drawCalls         = 0;
    int   visibleChunks     = 0;
    float gpuTimeMs         = 0.0f;
    float fps               = 0.0f;
};

// ─────────────────────────────────────────────────────────────────────────
/**
 * @brief Interfaz abstracta pura que todo motor debe implementar
 *        para ser compatible con el IDE de Haruka.
 *
 * Implementaciones disponibles:
 *   - HarukaVulkanEngine  →  usa Application + SDL3 + Vulkan
 *   - (futuro) HarukaGLEngine  →  usa GLFW + OpenGL
 */
class IEngine {
public:
    virtual ~IEngine() = default;

    // ── Ciclo de vida ────────────────────────────────────────────────────

    /**
     * @brief Inicializa el motor con la ventana que el IDE ya creó.
     * El motor es responsable de crear el contexto gráfico (Vulkan/GL/etc.)
     * sobre esa ventana. El IDE NO crea VkInstance, VkDevice, ni nada similar.
     */
    virtual EngineInitResult init(SDL_Window* window) = 0;

    /**
     * @brief Inicializa el backend de ImGui para este motor.
     * Llamar DESPUÉS de ImGui::CreateContext() y antes del primer frame.
     * El motor llama internamente a ImGui_ImplVulkan_Init (o equivalente).
     */
    virtual bool initImGui() = 0;

    /** @brief Libera todos los recursos del motor. */
    virtual void shutdown() = 0;

    // ── Frame loop ───────────────────────────────────────────────────────

    /**
     * @brief Inicia un nuevo frame de renderizado.
     * El motor adquiere la imagen del swapchain y llama a
     * ImGui_ImplVulkan_NewFrame() + ImGui_ImplSDL3_NewFrame().
     * El IDE solo llama a ImGui::NewFrame() después de esto.
     */
    virtual bool beginFrame() = 0;

    /**
     * @brief Renderiza los draw data de ImGui sobre el frame actual.
     * Llamar DESPUÉS de ImGui::Render() y ANTES de endFrame().
     */
    virtual void renderImGui(ImDrawData* drawData) = 0;

    /**
     * @brief Presenta el frame en pantalla (vkQueuePresentKHR o glSwapBuffers).
     * El motor gestiona semáforos, fences y sincronización internamente.
     */
    virtual void endFrame() = 0;

    // ── Viewport / escena ────────────────────────────────────────────────

    /**
     * @brief Renderiza la escena 3D en una textura interna.
     * El IDE llama a esto antes de beginFrame() o dentro del frame,
     * según la implementación.
     */
    virtual void renderScene(Haruka::Scene* scene, Camera* camera) = 0;

    /**
     * @brief Devuelve el ID de textura del viewport para ImGui::Image().
     * El tipo es void* para ser agnóstico de la API gráfica.
     * En Vulkan será un VkDescriptorSet; en OpenGL un GLuint.
     */
    virtual EngineTextureID getViewportTextureID() const = 0;

    // ── Info y estadísticas ──────────────────────────────────────────────

    /** @brief Devuelve estadísticas del último frame renderizado. */
    virtual EngineStats getStats() const = 0;

    /** @brief Devuelve true si el motor está inicializado y operativo. */
    virtual bool isReady() const = 0;

    /**
     * @brief Notifica al motor que la ventana cambió de tamaño.
     * El motor recrea el swapchain internamente.
     */
    virtual void onResize(int width, int height) = 0;
};
