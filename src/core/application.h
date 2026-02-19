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

class Application {
public:
    Application();
    ~Application();
    
    Camera* getCamera() { return _camera.get(); }
    void run();

private:
    void init_window();
    void main_loop();
    void cleanup();
    
    void loadScene(const std::string& scenePath);
    void renderScene();

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