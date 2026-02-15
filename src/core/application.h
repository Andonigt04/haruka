#ifndef APPLICATION_H
#define APPLICATION_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>

#include "math_types.h"
#include "camera.h"
#include "renderer/mesh.h"
#include "renderer/buffer_objects.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/model.h"
#include "renderer/shadow.h"
#include "renderer/hdr.h"
#include "renderer/bloom.h"
#include "renderer/gbuffer.h"
#include "renderer/ssao.h"
#include "renderer/ibl.h"
#include "renderer/point_shadow.h"
#include "renderer/render_target.h"

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

    GLFWwindow* _window;
    const int _width = 1280;
    const int _height = 720;

    std::unique_ptr<Shader> _mainShader;
    std::unique_ptr<Texture> _texture1;
    std::unique_ptr<Texture> _specularMap;
    
    std::unique_ptr<Shader> _lampShader;
    std::unique_ptr<Model> _model;
    std::unique_ptr<Model> _model2;
    std::unique_ptr<Mesh> _planeMesh; 
    std::unique_ptr<Shadow> _shadowSystem;
    std::unique_ptr<HDR> _hdrSystem;
    std::unique_ptr<Bloom> _bloomSystem;
    std::unique_ptr<GBuffer> _gBuffer;
    std::unique_ptr<SSAO> _ssaoSystem;
    std::unique_ptr<IBL> _iblSystem;
    std::unique_ptr<PointShadow> _pointShadowSystem;

    unsigned int quadVAO = 0;
    unsigned int quadVBO = 0;
    void setupQuad();

    std::unique_ptr<VertexArray> _vao;
    std::unique_ptr<VertexBuffer> _vbo;
    std::unique_ptr<IndexBuffer> _ebo;
    
    std::unique_ptr<Camera> _camera;

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    std::unique_ptr<RenderTarget> _lightingTarget;
    std::unique_ptr<RenderTarget> _bloomExtractTarget;
};

#endif