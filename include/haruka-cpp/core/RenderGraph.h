#pragma once

/**
 * @file RenderGraph.h
 * @brief Gestiona los render passes concretos de Haruka: escena 3D, offscreen, ImGui.
 *
 * Responsabilidades:
 *   - Offscreen render target (imagen + framebuffer + sampler + descriptor set ImGui)
 *   - Grabar el command buffer de cada frame (geometry → blit → ImGui)
 *   - Callback de ImGui inyectado por HarukaVulkanEngine
 *   - Build de la render queue (frustum culling, LOD)
 *   - Actualizar el UBO de matrices (view + proj)
 *
 * NO responsable de:
 *   - Crear VkInstance, VkDevice, swapchain, render passes (→ VulkanDevice)
 *   - Sistemas del motor de alto nivel (shaders, shadow, bloom)
 */

#include <vulkan/vulkan.h>
#include <functional>
#include <vector>

class VulkanDevice;
namespace Haruka { class Scene; }
class Camera;

class RenderGraph {
public:
    RenderGraph() = default;
    ~RenderGraph();

    // ── Ciclo de vida ────────────────────────────────────────────────────────

    /** @brief Inicializa recursos dependientes del device (NO del swapchain). */
    void init(VulkanDevice* device);

    /** @brief Destruye todos los recursos propios. */
    void destroy();

    /** @brief Recrea el offscreen target tras un resize del swapchain. */
    void recreateOffscreen();

    // ── Frame ────────────────────────────────────────────────────────────────

    /** @brief Graba todos los passes en el command buffer del frame actual.
     *
     *  Flujo:
     *    1. Offscreen pass  — escena 3D (CLEAR, sin ImGui)
     *    2. Blit            — offscreen → swapchain image
     *    3. ImGui pass      — ImGui sobre swapchain (LOAD, no toca offscreen)
     */
    void recordFrame(VkCommandBuffer cmd, uint32_t imageIndex,
                     Haruka::Scene* scene, Camera* camera);

    // ── Offscreen ────────────────────────────────────────────────────────────

    VkDescriptorSet offscreenDescriptorSet() const { return offscreenDescSet_; }
    VkSampler       offscreenSampler()       const { return offscreenSampler_; }
    VkImageView     offscreenImageView()     const { return offscreenImageView_; }
    void            setOffscreenDescriptorSet(VkDescriptorSet ds) { offscreenDescSet_ = ds; }

    // ── Callback ImGui ───────────────────────────────────────────────────────

    /** @brief El engine registra aquí la función que graba ImGui en Vulkan. */
    void setImGuiCallback(std::function<void(VkCommandBuffer, uint32_t)> cb) {
        imguiCallback_ = std::move(cb);
    }

private:
    void createOffscreenImage();
    void createOffscreenFramebuffer();
    void createOffscreenSampler();
    void destroyOffscreen();

    VulkanDevice* device_ = nullptr;

    // Offscreen render target
    VkImage         offscreenImage_       = VK_NULL_HANDLE;
    VkDeviceMemory  offscreenMemory_      = VK_NULL_HANDLE;
    VkImageView     offscreenImageView_   = VK_NULL_HANDLE;
    VkFramebuffer   offscreenFramebuffer_ = VK_NULL_HANDLE;
    VkSampler       offscreenSampler_     = VK_NULL_HANDLE;
    VkDescriptorSet offscreenDescSet_     = VK_NULL_HANDLE;

    // Callback del editor para grabar ImGui
    std::function<void(VkCommandBuffer, uint32_t)> imguiCallback_;
};