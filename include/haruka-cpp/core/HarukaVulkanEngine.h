#pragma once

#include "IEngine.h"
#include "VulkanDevice.h"
#include "RenderGraph.h"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <memory>

class Application;

/**
 * @brief Implementación de IEngine con SDL3 + Vulkan.
 *
 * Es el único punto del proyecto que conoce Vulkan, VulkanDevice y RenderGraph.
 * El editor solo ve IEngine*.
 *
 * Ciclo de vida:
 *   init()       → VulkanDevice::init() + RenderGraph::init()
 *   initImGui()  → ImGui_ImplVulkan_Init + crear offscreen descriptor set
 *   beginFrame() → ImGui_ImplVulkan_NewFrame + ImGui_ImplSDL3_NewFrame
 *   [editor: ImGui::NewFrame → renderUI → ImGui::Render]
 *   renderScene()→ Application::renderFrame() (graba + presenta via RenderGraph)
 *   shutdown()   → cleanup en orden inverso
 */
class HarukaVulkanEngine final : public IEngine {
public:
    HarukaVulkanEngine();
    ~HarukaVulkanEngine() override;

    // ── IEngine ──────────────────────────────────────────────────────────────
    EngineInitResult init(SDL_Window* window) override;
    bool             initImGui()             override;
    void             shutdown()              override;

    bool             beginFrame()                      override;
    void             renderImGui(ImDrawData* drawData) override;
    void             endFrame()                        override;

    void             renderScene(Haruka::Scene* scene, Camera* camera) override;
    EngineTextureID  getViewportTextureID() const override;
    EngineStats      getStats()             const override;
    bool             isReady()              const override { return ready_; }
    void             onResize(int width, int height)   override;

    // ── Acceso directo (solo para código que lo necesite explícitamente) ─────
    Application*   getApplication() const { return motorApp_; }
    VulkanDevice*  getDevice()      const { return device_.get(); }
    RenderGraph*   getRenderGraph() const { return renderGraph_.get(); }

private:
    std::unique_ptr<VulkanDevice> device_;
    std::unique_ptr<RenderGraph>  renderGraph_;

    Application*                  motorApp_  = nullptr;
    std::unique_ptr<Application>  ownedApp_;

    SDL_Window* window_ = nullptr;
    bool        ready_  = false;

    VkDescriptorPool imguiPool_ = VK_NULL_HANDLE;

    bool createImGuiDescriptorPool();
};