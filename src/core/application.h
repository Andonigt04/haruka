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
#include "renderer/shader.h"
#include "renderer/shadow.h"
#include "renderer/hdr.h"
#include "renderer/bloom.h"
#include "renderer/gbuffer.h"
#include "renderer/ssao.h"
#include "renderer/ibl.h"
#include "renderer/point_shadow.h"
#include "renderer/render_target.h"
#include "renderer/simple_mesh.h"
#include "renderer/light_culler.h"
#include "renderer/gpu_instancing.h"
#include "renderer/compute_postprocess.h"
#include "renderer/cascaded_shadow.h"
#include "renderer/virtual_texturing.h"
#include "error_reporter.h"
#include "io/asset_streamer.h"
#include "debug_overlay.h"
#include "physics/raycast_simple.h"

class MotorInstance;

/**
 * Application - Motor de Haruka Engine
 * 
 * Responsabilidades:
 * - Inicializar sistemas de renderizado
 * - Renderizar un frame de la escena actual
 * - Mantener cámara y estado global
 * 
 * NO es responsable de:
 * - Crear/gestionar ventana (viewport del editor es la ventana)
 * - Procesar input (viewport maneja input)
 * - UI (viewport maneja ImGui)
 */
class Application {
public:
    Application();
    ~Application();
    
    // Getters
    Camera* getCamera() { return _camera.get(); }
    Haruka::Scene* getCurrentScene() { return _currentScene.get(); }
    RaycastSimple* getRaycastSystem() { return _raycastSystem.get(); }
    
    // Callbacks desde MotorInstance (cuando viewport cambia escena/cámara)
    void onSceneChanged(Haruka::Scene* scene) { 
        _currentScene.reset();
        _currentScene = std::unique_ptr<Haruka::Scene>(scene);
    }
    void onCameraChanged(Camera* cam) { 
        _camera.reset();
        _camera = std::unique_ptr<Camera>(cam);
    }

    // inicia el producto final
    void run(const std::string& startScenePath);
    // inicia todo lo esencial - sobrecargado para recibir escena
    void init(Haruka::Scene& scene);
    // crea la ventana
    void create_window();
    // carga escena
    void loadScene(const std::string& scenePath);
    // renderiza escena
    void renderScene(Shader* shader = nullptr);
    // renderiza
    void main_loop();
    // renderiza UN frame (con deltaTime)
    void renderFrame();
    // renderiza contenido del frame (lógica pura)
    void renderFrameContent();
    // remueve todo para poder cerrar programa
    void cleanup();

private:
    friend class MotorInstance;
    
    static constexpr int MAX_LIGHTS = 256;
    
    // Window (set by viewport via MotorInstance friend access)
    GLFWwindow* _window = nullptr;
    int _width = 1280;
    int _height = 720;

    // Core systems
    std::unique_ptr<Haruka::Scene> _currentScene;
    std::unique_ptr<Camera> _camera;
    
    // Rendering pipeline
    std::unique_ptr<Shader> _mainShader;
    std::unique_ptr<Shader> _lampShader;
    std::unique_ptr<Shadow> _shadowSystem;
    std::unique_ptr<HDR> _hdrSystem;
    std::unique_ptr<Bloom> _bloomSystem;
    std::unique_ptr<GBuffer> _gBuffer;
    std::unique_ptr<SSAO> _ssaoSystem;
    std::unique_ptr<IBL> _iblSystem;
    std::unique_ptr<PointShadow> _pointShadowSystem;
    std::unique_ptr<Haruka::WorldSystem> _worldSystem;
    std::unique_ptr<LightCuller> _lightCuller;
    std::unique_ptr<GPUInstancing> _instancing;
    std::unique_ptr<ComputePostProcess> _computePostProcess;
    std::unique_ptr<CascadedShadowMap> _cascadedShadow;
    std::unique_ptr<VirtualTexturing> _virtualTexturing;
    std::unique_ptr<RaycastSimple> _raycastSystem;
    
    // Render targets
    std::unique_ptr<RenderTarget> _lightingTarget;
    std::unique_ptr<RenderTarget> _bloomExtractTarget;
    
    // Primitives (LOD spheres para cuerpos celestes)
    std::unique_ptr<SimpleMesh> sphereLOD[4];
    
    // Shaders (cached para no recrear cada frame)
    std::unique_ptr<Shader> _geomShader;
    std::unique_ptr<Shader> _ssaoShader;
    std::unique_ptr<Shader> _lightShader;
    std::unique_ptr<Shader> _compositeShader;
    std::unique_ptr<Shader> _flatShader;
    
    // Timing
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;
    
    // Screen quad para post-processing
    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    void setupQuad();
};

#endif