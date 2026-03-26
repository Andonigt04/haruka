#ifndef APPLICATION_H
#define APPLICATION_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <vector>

#include "math_types.h"
#include "world_system.h"
#include "camera.h"
#include "scene.h"
#include "project.h"
#include "renderer/mesh.h"
#include "renderer/shader.h"
#include "renderer/model.h"
#include "renderer/shadow.h"
#include "renderer/hdr.h"
#include "renderer/bloom.h"
#include "renderer/gbuffer.h"
#include "renderer/ssao.h"
#include "renderer/ibl.h"
#include "renderer/point_shadow.h"
#include "renderer/render_target.h"
#include "renderer/simple_mesh.h"
#include "renderer/primitive_shapes.h"
#include "renderer/light_culler.h"
#include "renderer/gpu_instancing.h"
#include "renderer/compute_postprocess.h"
#include "renderer/cascaded_shadow.h"
#include "renderer/virtual_texturing.h"
#include "error_reporter.h"
#include "io/asset_streamer.h"
#include "debug_overlay.h"
#include "physics/raycast_simple.h"

class Application {
public:
    Application();
    ~Application();
    
    Camera* getCamera() { return _camera.get(); }
    Haruka::Scene* getCurrentScene() { return _currentScene.get(); }
    RaycastSimple* getRaycastSystem() { return _raycastSystem.get(); }
    void run();

private:
    // Constants
    static constexpr int WINDOW_WIDTH = 1280;
    static constexpr int WINDOW_HEIGHT = 720;
    static constexpr int MAX_LIGHTS = 256; 

    void init_window();
    void main_loop();
    void cleanup();
    
    void loadScene(const std::string& scenePath);
    void renderScene(Shader* shader = nullptr);

    // Window
    GLFWwindow* _window;
    const int _width = 1280;
    const int _height = 720;

    // Scene & Project
    std::unique_ptr<Haruka::Scene> _currentScene;
    std::unique_ptr<Haruka::Project> _currentProject;
    std::vector<std::unique_ptr<Model>> _sceneModels;
    
    // Rendering systems
    std::unique_ptr<Shader> _mainShader;
    std::unique_ptr<Shader> _lampShader;
    std::unique_ptr<Shadow> _shadowSystem;
    std::unique_ptr<HDR> _hdrSystem;
    std::unique_ptr<Bloom> _bloomSystem;
    std::unique_ptr<GBuffer> _gBuffer;
    std::unique_ptr<SSAO> _ssaoSystem;
    std::unique_ptr<IBL> _iblSystem;
    std::unique_ptr<PointShadow> _pointShadowSystem;
    std::unique_ptr<WorldSystem> _worldSystem;
    std::unique_ptr<LightCuller> _lightCuller;
    std::unique_ptr<GPUInstancing> _instancing;
    std::unique_ptr<ComputePostProcess> _computePostProcess;
    std::unique_ptr<CascadedShadowMap> _cascadedShadow;
    std::unique_ptr<VirtualTexturing> _virtualTexturing;
    std::unique_ptr<RaycastSimple> _raycastSystem;
    
    // Debug overlay para profiling (singleton reference)
    // Nota: DebugOverlay es singleton
    
    // Asset streaming para reducir RAM
    // Nota: AssetStreamer es singleton, pero mantener referencia aquí
    
    std::unique_ptr<RenderTarget> _lightingTarget;
    std::unique_ptr<RenderTarget> _bloomExtractTarget;
    
    // Primitives for celestial bodies
    std::unique_ptr<SimpleMesh> sphereLOD[4];
    std::unique_ptr<SimpleMesh> cubeMesh;
    std::unique_ptr<SimpleMesh> planeMesh;
    
    // Camera & timing
    std::unique_ptr<Camera> _camera;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    
    // Screen quad
    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    void setupQuad();
};

#endif