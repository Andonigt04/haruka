#ifndef HARUKA_VULKAN_ENGINE_H
#define HARUKA_VULKAN_ENGINE_H

#include "IEngine.h"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <memory>

class Application;

/**
 * @brief Implementación de IEngine que usa el motor Haruka con SDL3 + Vulkan.
 *
 * Esta clase es la única en todo el proyecto que toca handles Vulkan directamente.
 * El IDE solo ve IEngine*.
 *
 * Internamente:
 *   - Delega la creación de VkInstance/VkDevice/swapchain a Application::create_vulkan_context()
 *   - Expone getViewportTextureID() como VkDescriptorSet para ImGui
 *   - Gestiona el descriptor pool propio de ImGui
 */
class HarukaVulkanEngine final : public IEngine {
public:
    HarukaVulkanEngine();
    ~HarukaVulkanEngine() override;

    // ── IEngine ──────────────────────────────────────────────────────────
    EngineInitResult init(SDL_Window* window) override;
    bool             initImGui() override;
    void             shutdown() override;

    bool             beginFrame() override;
    void             renderImGui(ImDrawData* drawData) override;
    void             endFrame() override;

    void             renderScene(Haruka::Scene* scene, Camera* camera) override;
    EngineTextureID  getViewportTextureID() const override;

    EngineStats      getStats() const override;
    bool             isReady() const override { return ready; }
    void             onResize(int width, int height) override;

    // ── Acceso directo al motor (solo para código que realmente lo necesite) ──
    Application*     getApplication() const { return motorApp; }

private:
    // El motor (Application) gestiona todos los handles Vulkan
    Application*    motorApp            = nullptr;
    std::unique_ptr<Application> ownedApp; // si lo creamos aquí

    SDL_Window*     window              = nullptr;
    bool            ready               = false;

    // Descriptor pool exclusivo para ImGui
    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

    // Command buffer del frame actual (tomado del motor)
    VkCommandBuffer  currentCmd          = VK_NULL_HANDLE;
    uint32_t         currentImageIndex   = 0;

    // Punteros de función KHR (cargados del device del motor)
    PFN_vkAcquireNextImageKHR pfnAcquireNextImage = nullptr;
    PFN_vkQueuePresentKHR     pfnQueuePresent     = nullptr;

    // Semáforos y fence propios del editor
    VkSemaphore editorImageAvailable = VK_NULL_HANDLE;
    VkSemaphore editorRenderFinished = VK_NULL_HANDLE;
    VkFence     editorInFlightFence  = VK_NULL_HANDLE;

    bool createImGuiDescriptorPool();
    bool createSyncObjects();
    void destroySyncObjects();
};

#endif