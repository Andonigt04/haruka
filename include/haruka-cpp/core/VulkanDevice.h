#pragma once

/**
 * @file VulkanDevice.h
 * @brief Infraestructura Vulkan pura: instance, device, swapchain, render passes.
 *
 * Responsabilidades:
 *   - Crear y destruir VkInstance, VkPhysicalDevice, VkDevice, queues
 *   - Gestionar el swapchain y su recreación en resize
 *   - Crear render passes (escena y ImGui)
 *   - Command pool y command buffers
 *   - Descriptor pool y UBO de matrices globales
 *   - Semáforos y fences de sincronización
 *
 * NO responsable de:
 *   - Qué se dibuja (escena, ImGui, postprocess)
 *   - Pipelines de geometría/lighting/postprocess
 *   - Sistemas del motor (shaders, shadow, bloom, etc.)
 */

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>
#include <stdexcept>
#include <functional>
#include <glm/glm.hpp>

class VulkanDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice();

    // ── Ciclo de vida ────────────────────────────────────────────────────────

    /** @brief Inicializa instance, surface, device, swapchain, render passes, sync. */
    void init(SDL_Window* window);

    /** @brief Destruye todos los recursos Vulkan en orden inverso. */
    void destroy();

    /** @brief Recrea el swapchain y recursos dependientes (framebuffers, cmd buffers).
     *  Llámalo cuando vkAcquireNextImageKHR o vkQueuePresentKHR devuelvan
     *  VK_ERROR_OUT_OF_DATE_KHR, o cuando detectes un cambio de tamaño en Wayland. */
    void recreateSwapchain();

    // ── Getters principales ──────────────────────────────────────────────────

    VkInstance       instance()        const { return vkInstance_; }
    VkPhysicalDevice physicalDevice()  const { return vkPhysicalDevice_; }
    VkDevice         device()          const { return vkDevice_; }
    VkQueue          graphicsQueue()   const { return vkGraphicsQueue_; }
    VkQueue          presentQueue()    const { return vkPresentQueue_; }
    uint32_t         graphicsFamily()  const { return graphicsFamily_; }

    VkSwapchainKHR   swapchain()       const { return vkSwapchain_; }
    VkExtent2D       swapchainExtent() const { return swapchainExtent_; }
    int              width()           const { return width_; }
    int              height()          const { return height_; }

    VkRenderPass     sceneRenderPass() const { return vkSceneRenderPass_; }
    VkRenderPass     imguiRenderPass() const { return vkImGuiRenderPass_; }

    VkFramebuffer    sceneFramebuffer(uint32_t i) const {
        return i < vkSceneFramebuffers_.size() ? vkSceneFramebuffers_[i] : VK_NULL_HANDLE;
    }
    VkFramebuffer    imguiFramebuffer(uint32_t i) const {
        return i < vkImGuiFramebuffers_.size() ? vkImGuiFramebuffers_[i] : VK_NULL_HANDLE;
    }

    const std::vector<VkImage>&     swapchainImages()     const { return vkSwapchainImages_; }
    const std::vector<VkImageView>& swapchainImageViews() const { return vkSwapchainImageViews_; }

    VkCommandPool    commandPool()     const { return vkCommandPool_; }
    VkCommandBuffer  commandBuffer(uint32_t i) const {
        return i < vkCommandBuffers_.size() ? vkCommandBuffers_[i] : VK_NULL_HANDLE;
    }
    size_t           commandBufferCount() const { return vkCommandBuffers_.size(); }

    VkDescriptorPool descriptorPool()  const { return vkDescriptorPool_; }

    // UBO de matrices globales (view + proj)
    VkBuffer              uboBuffer()      const { return vkUniformBuffer_; }
    VkDeviceMemory        uboMemory()      const { return vkUniformBufferMemory_; }
    VkDescriptorSetLayout uboLayout()      const { return vkUniformDescriptorSetLayout_; }
    VkDescriptorSet       uboDescSet()     const { return vkUniformDescriptorSet_; }

    // Semáforos y fence del frame
    VkSemaphore imageAvailableSemaphore() const { return vkImageAvailableSemaphore_; }
    VkSemaphore renderFinishedSemaphore() const { return vkRenderFinishedSemaphore_; }
    VkFence     inFlightFence()           const { return vkInFlightFence_; }

    // ── Helpers ──────────────────────────────────────────────────────────────

    /** @brief Encuentra el índice de tipo de memoria compatible con los flags dados. */
    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;

    /** @brief Detecta si el swapchain necesita recrearse comparando con surface caps.
     *  Útil en Wayland donde VK_SUBOPTIMAL_KHR no se emite al redimensionar. */
    bool swapchainNeedsResize() const;

    // ── Estado ───────────────────────────────────────────────────────────────
    bool isReady() const { return ready_; }

private:
    // Orden de creación == orden inverso de destrucción
    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createSceneRenderPass();
    void createImGuiRenderPass();
    void createSceneFramebuffers();
    void createImGuiFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createDescriptorPool();
    void createUniformBuffer();

    void destroySwapchainDependents();

    bool isDeviceSuitable(VkPhysicalDevice dev) const;
    bool checkExtensionSupport(VkPhysicalDevice dev) const;

    SDL_Window* window_ = nullptr;
    bool        ready_  = false;

    int width_  = 1600;
    int height_ = 900;

    // Core
    VkInstance       vkInstance_       = VK_NULL_HANDLE;
    VkPhysicalDevice vkPhysicalDevice_ = VK_NULL_HANDLE;
    VkDevice         vkDevice_         = VK_NULL_HANDLE;
    VkSurfaceKHR     vkSurface_        = VK_NULL_HANDLE;
    uint32_t         graphicsFamily_   = 0;

    // Queues
    VkQueue vkGraphicsQueue_ = VK_NULL_HANDLE;
    VkQueue vkPresentQueue_  = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR            vkSwapchain_      = VK_NULL_HANDLE;
    VkExtent2D                swapchainExtent_  = {0, 0};
    std::vector<VkImage>      vkSwapchainImages_;
    std::vector<VkImageView>  vkSwapchainImageViews_;

    // Render passes
    VkRenderPass vkSceneRenderPass_ = VK_NULL_HANDLE; // CLEAR — para offscreen/escena
    VkRenderPass vkImGuiRenderPass_ = VK_NULL_HANDLE; // LOAD  — para ImGui sobre swapchain

    // Framebuffers
    std::vector<VkFramebuffer> vkSceneFramebuffers_; // swapchain, usado como fallback
    std::vector<VkFramebuffer> vkImGuiFramebuffers_; // swapchain, para ImGui

    // Commands
    VkCommandPool                vkCommandPool_    = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> vkCommandBuffers_;

    // Sync
    VkSemaphore vkImageAvailableSemaphore_ = VK_NULL_HANDLE;
    VkSemaphore vkRenderFinishedSemaphore_ = VK_NULL_HANDLE;
    VkFence     vkInFlightFence_           = VK_NULL_HANDLE;

    // Descriptors
    VkDescriptorPool          vkDescriptorPool_             = VK_NULL_HANDLE;
    VkDescriptorSetLayout     vkMaterialDescriptorSetLayout_= VK_NULL_HANDLE;
    VkBuffer                  vkUniformBuffer_              = VK_NULL_HANDLE;
    VkDeviceMemory            vkUniformBufferMemory_        = VK_NULL_HANDLE;
    VkDescriptorSetLayout     vkUniformDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSet           vkUniformDescriptorSet_       = VK_NULL_HANDLE;
};