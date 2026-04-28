#include "HarukaVulkanEngine.h"

#include "core/application.h"
#include "renderer/motor_instance.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <iostream>

HarukaVulkanEngine::HarukaVulkanEngine() = default;

HarukaVulkanEngine::~HarukaVulkanEngine() {
    shutdown();
}

// ─── init ────────────────────────────────────────────────────────────────────
EngineInitResult HarukaVulkanEngine::init(SDL_Window* sdlWindow) {
    window = sdlWindow;

    // 1. Crear o reutilizar Application
    motorApp = MotorInstance::getInstance().getApplication();
    if (!motorApp) {
        ownedApp = std::make_unique<Application>();
        motorApp = ownedApp.get();
        MotorInstance::getInstance().setApplication(motorApp);
    }

    // 2. El motor inicializa TODO Vulkan internamente
    //    (VkInstance, VkPhysicalDevice, VkDevice, swapchain, render pass…)
    //    Le pasamos la ventana para que cree la surface.
    motorApp->set_external_window(window);
    motorApp->create_vulkan_context();

    // 3. Verificar que el motor dejó handles válidos
    if (!motorApp->getVkDevice() || !motorApp->getVkRenderPass()) {
        return { false, "El motor no inicializó Vulkan correctamente" };
    }

    // 4. Cargar funciones KHR desde el device del motor
    VkDevice dev = motorApp->getVkDevice();
    pfnAcquireNextImage = (PFN_vkAcquireNextImageKHR)
        vkGetDeviceProcAddr(dev, "vkAcquireNextImageKHR");
    pfnQueuePresent = (PFN_vkQueuePresentKHR)
        vkGetDeviceProcAddr(dev, "vkQueuePresentKHR");

    if (!pfnAcquireNextImage || !pfnQueuePresent) {
        return { false, "No se pudieron cargar las funciones KHR del swapchain" };
    }

    // 5. Descriptor pool para ImGui
    if (!createImGuiDescriptorPool()) {
        return { false, "Failed to create ImGui descriptor pool" };
    }

    // 6. Semáforos y fence
    if (!createSyncObjects()) {
        return { false, "Failed to create sync objects" };
    }

    ready = true;
    return { true, nullptr };
}

// ─── initImGui ───────────────────────────────────────────────────────────────
bool HarukaVulkanEngine::initImGui() {
    if (!ready) return false;

    motorApp->setImGuiRenderCallback([](VkCommandBuffer cmd, uint32_t) {
        ImDrawData* dd = ImGui::GetDrawData();
        if (dd) ImGui_ImplVulkan_RenderDrawData(dd, cmd);
    });

    // Obtener imageCount del swapchain del motor
    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(motorApp->getVkDevice(),
                            motorApp->getVkSwapchain(),
                            &imageCount, nullptr);

    ImGui_ImplSDL3_InitForVulkan(window);

    ImGui_ImplVulkan_InitInfo info = {};
    info.Instance       = motorApp->getVkInstance();
    info.PhysicalDevice = motorApp->getVkPhysicalDevice();
    info.Device         = motorApp->getVkDevice();
    info.QueueFamily    = motorApp->getGraphicsQueueFamily();
    info.Queue          = motorApp->getVkQueue();
    info.DescriptorPool = imguiDescriptorPool;
    info.MinImageCount  = imageCount;
    info.ImageCount     = imageCount;
    info.PipelineInfoMain.RenderPass  = motorApp->getVkRenderPass();
    info.PipelineInfoMain.Subpass     = 0;
    info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    return ImGui_ImplVulkan_Init(&info);
}

// ─── shutdown ────────────────────────────────────────────────────────────────
void HarukaVulkanEngine::shutdown() {
    if (!motorApp) return;

    VkDevice dev = motorApp->getVkDevice();
    if (dev) vkDeviceWaitIdle(dev);

    if (ImGui::GetCurrentContext()) {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }

    destroySyncObjects();

    if (imguiDescriptorPool != VK_NULL_HANDLE && dev) {
        vkDestroyDescriptorPool(dev, imguiDescriptorPool, nullptr);
        imguiDescriptorPool = VK_NULL_HANDLE;
    }

    ready = false;
    motorApp = nullptr;
}

// ─── beginFrame ──────────────────────────────────────────────────────────────
bool HarukaVulkanEngine::beginFrame() {
    if (!ready) return false;
    // Solo arrancar los backends de ImGui — el motor gestiona el swapchain
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    return true;
}

// ─── renderImGui ─────────────────────────────────────────────────────────────
void HarukaVulkanEngine::renderImGui(ImDrawData*) {
    // No hacer nada — el callback lo maneja
    //DEPRECATED: el motor ahora gestiona la renderización de ImGui internamente, así que no necesitamos hacer nada aquí.
}

// ─── endFrame ────────────────────────────────────────────────────────────────
void HarukaVulkanEngine::endFrame() {
    // El motor presenta el frame — nosotros no hacemos nada aquí
    //DEPRECATED: el motor ahora gestiona la presentación internamente, así que no necesitamos llamar a pfnQueuePresent aquí.
}

// ─── renderScene ─────────────────────────────────────────────────────────────
void HarukaVulkanEngine::renderScene(Haruka::Scene* scene, Camera* camera) {
    if (!ready || !motorApp) return;
    // El motor renderiza la escena en su render target interno
    if (scene)  MotorInstance::getInstance().setScene(scene);
    if (camera) MotorInstance::getInstance().setCamera(camera);
    motorApp->renderFrame();
}

// ─── getViewportTextureID ────────────────────────────────────────────────────
EngineTextureID HarukaVulkanEngine::getViewportTextureID() const {
    // TODO: devolver el VkDescriptorSet del render target del motor
    // Por ahora devuelve nullptr — el viewport mostrará negro hasta implementarlo
    return nullptr;
}

// ─── getStats ────────────────────────────────────────────────────────────────
EngineStats HarukaVulkanEngine::getStats() const {
    return {
        Application::getLastRenderedVertices(),
        Application::getLastRenderedTriangles(),
        Application::getLastRenderedDrawCalls(),
        Application::getLastVisibleChunks(),
        0.0f,
        0.0f
    };
}

// ─── onResize ────────────────────────────────────────────────────────────────
void HarukaVulkanEngine::onResize(int /*width*/, int /*height*/) {
    if (motorApp) motorApp->recreateSwapchainAndResources();
}

// ─── helpers privados ────────────────────────────────────────────────────────
bool HarukaVulkanEngine::createImGuiDescriptorPool() {
    VkDescriptorPoolSize sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
    };
    VkDescriptorPoolCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    info.maxSets       = 1000;
    info.poolSizeCount = 2;
    info.pPoolSizes    = sizes;
    return vkCreateDescriptorPool(motorApp->getVkDevice(), &info,
                                  nullptr, &imguiDescriptorPool) == VK_SUCCESS;
}

bool HarukaVulkanEngine::createSyncObjects() {
    VkDevice dev = motorApp->getVkDevice();

    VkSemaphoreCreateInfo semInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    return vkCreateSemaphore(dev, &semInfo, nullptr, &editorImageAvailable) == VK_SUCCESS
        && vkCreateSemaphore(dev, &semInfo, nullptr, &editorRenderFinished) == VK_SUCCESS
        && vkCreateFence(dev, &fenceInfo, nullptr, &editorInFlightFence)    == VK_SUCCESS;
}

void HarukaVulkanEngine::destroySyncObjects() {
    if (!motorApp) return;
    VkDevice dev = motorApp->getVkDevice();
    if (!dev) return;
    if (editorImageAvailable) vkDestroySemaphore(dev, editorImageAvailable, nullptr);
    if (editorRenderFinished) vkDestroySemaphore(dev, editorRenderFinished, nullptr);
    if (editorInFlightFence)  vkDestroyFence(dev, editorInFlightFence, nullptr);
    editorImageAvailable = VK_NULL_HANDLE;
    editorRenderFinished = VK_NULL_HANDLE;
    editorInFlightFence  = VK_NULL_HANDLE;
}
