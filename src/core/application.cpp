#include "application.h"

#include <iostream>
#include <cmath>
#include <unordered_set>
#include <glm/gtc/matrix_transform.hpp>

#include "renderer/mesh.h"
#include "renderer/motor_instance.h"
#include "world_system.h"
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
#include "project.h"
#include "error_reporter.h"
#include "renderer/gpu_instancing.h"
#include "physics/raycast_simple.h"
#include "object_types.h"

// Mouse input state
struct MouseState {
    float lastX = 640.0f;
    float lastY = 360.0f;
    bool firstMouse = true;
};

static MouseState g_mouseState;

namespace {
struct SceneRenderItem {
    glm::mat4 modelMatrix{1.0f};
    glm::vec3 color{1.0f};
    Haruka::MaterialComponent* material = nullptr;
    std::shared_ptr<MeshRendererComponent> meshRenderer;
    std::string modelPath;
};

std::vector<SceneRenderItem> g_sceneRenderQueue;
}

Application::Application() : _window(nullptr) {}

Application::~Application() { cleanup(); }

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->getCamera()->ProcessMouseScroll(static_cast<float>(yoffset));
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (g_mouseState.firstMouse) {
        g_mouseState.lastX = xpos;
        g_mouseState.lastY = ypos;
        g_mouseState.firstMouse = false;
        return;
    }

    float xoffset = xpos - g_mouseState.lastX;
    float yoffset = g_mouseState.lastY - ypos;

    g_mouseState.lastX = xpos;
    g_mouseState.lastY = ypos;

    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app && app->getCamera()) {
        app->getCamera()->rotate(xoffset, -yoffset);
    }
}

void Application::setupQuad() {
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
    };

    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void Application::loadScene(const std::string& scenePath) {
    _currentScene = std::make_unique<Haruka::Scene>();
    
    if (_currentScene->load(scenePath)) {
        std::cout << "Scene loaded: " << _currentScene->getName() << std::endl;
    } else {
        HARUKA_MOTOR_ERROR(ErrorCode::SCENE_PARSE_ERROR, std::string("Failed to load scene from: ") + scenePath);
        // Crear escena vacía por defecto
        _currentScene = std::make_unique<Haruka::Scene>("DefaultScene");
    }
}

void Application::create_window() {
    if (!glfwInit()) {
        HARUKA_MOTOR_ERROR(ErrorCode::MOTOR_INIT_FAILED, "Failed to initialize GLFW");
        throw std::runtime_error("Fallo al inicializar GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    _window = glfwCreateWindow(_width, _height, "Haruka Engine", nullptr, nullptr);
    if (!_window) {
        HARUKA_MOTOR_ERROR(ErrorCode::WINDOW_CREATION_FAILED, "Failed to create GLFW window");
        throw std::runtime_error("Fallo al crear la ventana");
    }

    glfwMakeContextCurrent(_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Fallo al inicializar GLAD");
    }

    glfwSetWindowUserPointer(_window, this);
    glfwSetScrollCallback(_window, scroll_callback);
    glfwSetCursorPosCallback(_window, mouse_callback);
    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glEnable(GL_DEPTH_TEST);
}

void Application::init(Haruka::Scene& scene) {
    
    _currentScene = std::make_unique<Haruka::Scene>(scene);

    // Buscar cámara en la escena
    Camera* sceneCamera = nullptr;
    const auto& objects = _currentScene->getObjects();
    for (const auto& obj : objects) {
        if (obj.type == "Camera") {
            sceneCamera = new Camera(obj.position);
            std::cout << "✓ Camera found in scene at position: " << obj.position.x << ", " << obj.position.y << ", " << obj.position.z << std::endl;
            break;
        }
    }
    
    // Si no hay cámara en la escena, usar default
    if (!sceneCamera) {
        sceneCamera = new Camera(Haruka::WorldPos(0.0f, 0.0f, 15.0f));
        std::cout << "⚠ No camera found in scene, using default position" << std::endl;
    }
    
    _camera = std::unique_ptr<Camera>(sceneCamera);

    // Initialize rendering systems
    _mainShader = std::make_unique<Shader>("shaders/simple.vert", "shaders/pbr.frag");
    _lampShader = std::make_unique<Shader>("shaders/simple.vert", "shaders/light_cube.frag");
    
    _shadowSystem = std::make_unique<Shadow>(1024, 1024);
    _hdrSystem = std::make_unique<HDR>(_width, _height);
    _bloomSystem = std::make_unique<Bloom>(_width, _height);
    _gBuffer = std::make_unique<GBuffer>(_width, _height);
    _ssaoSystem = std::make_unique<SSAO>(_width, _height);
    _iblSystem = std::make_unique<IBL>();
    _worldSystem = std::make_unique<WorldSystem>();
    _pointShadowSystem = std::make_unique<PointShadow>(1024);

    _lightingTarget = std::make_unique<RenderTarget>(_width, _height);
    _bloomExtractTarget = std::make_unique<RenderTarget>(_width, _height);

    // Registrar RenderTarget por defecto (standalone), sin pisar uno externo (viewport)
    if (!MotorInstance::getInstance().getRenderTarget()) {
        MotorInstance::getInstance().setRenderTarget(_lightingTarget.get());
    }

    // Registrar Application para que scripts accedan a sistemas (raycast, etc)
    MotorInstance::getInstance().setApplication(this);

    // Inicializar light culler para soportar ilimitadas luces
    _lightCuller = std::make_unique<LightCuller>();

    // Inicializar GPU instancing para renderizado eficiente
    _instancing = std::make_unique<GPUInstancing>();
    _instancing->init(10000);  // Máximo 10k instancias por batch

    // Inicializar Asset Streaming para cargar assets bajo demanda
    AssetStreamer::getInstance().init(512, 2);  // 512 MB cache, 2 worker threads

    // Inicializar Debug Overlay para profiling
    DebugOverlay::getInstance().init();

    // Inicializar Compute Post-Processing
    _computePostProcess = std::make_unique<ComputePostProcess>();
    _computePostProcess->init(_width, _height);

    // Inicializar Cascaded Shadow Maps
    _cascadedShadow = std::make_unique<CascadedShadowMap>();
    _cascadedShadow->init(0.1f, 100.0f, 0.5f);  // zNear, zFar, lambda

    // Inicializar Virtual Texturing
    _virtualTexturing = std::make_unique<VirtualTexturing>();
    VTConfig vtConfig;
    vtConfig.pageSize = 128;
    vtConfig.maxResidentPages = 1024;
    vtConfig.maxCacheMemory = 256LL * 1024 * 1024;
    _virtualTexturing->init(vtConfig);

    // Inicializar Raycast System
    _raycastSystem = std::make_unique<RaycastSimple>();

    setupQuad();

    // Create LOD primitives for celestial bodies
    int lodConfigs[4][2] = {
        {64, 32},  // LOD 0: High quality
        {32, 16},  // LOD 1: Medium
        {16, 8},   // LOD 2: Low
        {8, 4}     // LOD 3: Very low
    };

    for (int i = 0; i < 4; i++) {
        std::vector<glm::vec3> verts, norms;
        std::vector<unsigned int> indices;
        PrimitiveShapes::createSphere(1.0f, lodConfigs[i][0], lodConfigs[i][1], verts, norms, indices);
        sphereLOD[i] = std::make_unique<SimpleMesh>(verts, norms, indices);
    }
    
    _worldSystem->initComputeShaders();
    
    // Initialize cached shaders (avoid recreating each frame)
    _geomShader = std::make_unique<Shader>("shaders/deferred_geom.vert", "shaders/deferred_geom.frag");
    _ssaoShader = std::make_unique<Shader>("shaders/screenquad.vert", "shaders/ssao.frag");
    _lightShader = std::make_unique<Shader>("shaders/screenquad.vert", "shaders/deferred_light.frag");
    _compositeShader = std::make_unique<Shader>("shaders/screenquad.vert", "shaders/bloom_composite.frag");
    _flatShader = std::make_unique<Shader>("shaders/simple.vert", "shaders/light_cube.frag");
}

void Application::renderScene(Shader* shader) {
    (void)shader; // Esta función solo construye la cola de render

    Haruka::Scene* scene = MotorInstance::getInstance().getScene();
    if (!scene) {
        scene = _currentScene.get();
    }
    g_sceneRenderQueue.clear();
    if (!scene) return;
    
    for (const auto& obj : scene->getObjects()) {
        Haruka::ObjectType objType = Haruka::stringToObjectType(obj.type);

        if (!Haruka::isRenderableObjectType(objType)) {
            continue;
        }

        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3(obj.position));
        modelMatrix = glm::rotate(modelMatrix, glm::radians((float)obj.rotation.x), glm::vec3(1, 0, 0));
        modelMatrix = glm::rotate(modelMatrix, glm::radians((float)obj.rotation.y), glm::vec3(0, 1, 0));
        modelMatrix = glm::rotate(modelMatrix, glm::radians((float)obj.rotation.z), glm::vec3(0, 0, 1));
        modelMatrix = glm::scale(modelMatrix, glm::vec3(obj.scale));

        SceneRenderItem item;
        item.modelMatrix = modelMatrix;
        item.color = glm::vec3(obj.color);
        item.material = obj.material.get();

        // MeshRenderer (componente de geometría almacenada)
        if (obj.meshRenderer) {
            item.meshRenderer = obj.meshRenderer;
            g_sceneRenderQueue.push_back(std::move(item));
            continue;
        }

        // Modelo externo
        if (objType == Haruka::ObjectType::MODEL && !obj.modelPath.empty()) {
            item.modelPath = obj.modelPath;
            g_sceneRenderQueue.push_back(std::move(item));
            continue;
        }
    }
}

void Application::main_loop() {
    while (!glfwWindowShouldClose(_window)) {
        renderFrame();
        glfwSwapBuffers(_window);
        glfwPollEvents();
    }
}

void Application::renderFrame() {
    Haruka::Scene* scene = MotorInstance::getInstance().getScene();
    if (!scene && !_currentScene) return;
    if (!_camera) return;

    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    
    renderFrameContent();
}

void Application::renderFrameContent() {
    // ========== SETUP ==========
    Camera* activeCamera = MotorInstance::getInstance().getCamera();
    if (!activeCamera) {
        activeCamera = _camera.get();
    }
    if (!activeCamera) return;

    glm::mat4 proj = glm::perspective(glm::radians(activeCamera->zoom), (float)_width / (float)_height, 0.1f, 10000000000.0f);
    glm::mat4 view = activeCamera->getViewMatrix();

    // ========== EDITOR MODE (Viewport) ==========
    RenderTarget* editorTarget = MotorInstance::getInstance().getRenderTarget();
    if (editorTarget) {
        renderScene();

        editorTarget->bindForWriting();
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.06f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        _flatShader->use();
        _flatShader->setMat4("projection", proj);
        _flatShader->setMat4("view", view);

        for (const auto& item : g_sceneRenderQueue) {
            _flatShader->setMat4("model", item.modelMatrix);
            glm::vec3 c = item.color;
            if (glm::length(c) < 0.001f) c = glm::vec3(0.8f, 0.8f, 0.8f);
            _flatShader->setVec3("lightColor", c);

            if (item.meshRenderer) {
                item.meshRenderer->render(*_flatShader);
                continue;
            }

            if (!item.modelPath.empty()) {
                try {
                    Model model(item.modelPath);
                    model.Draw(*_flatShader);
                } catch (...) {
                    // Ignorar en fallback
                }
                continue;
            }
        }

        // Fallback: mostrar nada si escena vacía (no renderizar cubo por defecto)
        // Los usuarios deben agregar explícitamente objetos a la escena

        editorTarget->unbind();
        return;
    }

    // ========== RUNTIME MODE (Deferred Pipeline) ==========
    
    // Actualizar posición de cámara
    AssetStreamer::getInstance().updateCameraPosition(activeCamera->position);
    
    // Actualizar cascadas de sombra
    glm::vec3 lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
    _cascadedShadow->updateCascades(
        lightDir,
        activeCamera->position,
        glm::vec3(0.0f, 0.0f, -1.0f),
        0.1f,
        1000.0f,
        75.0f
    );
    
    // Procesar feedback de virtual texturing
    if (_virtualTexturing) {
        _virtualTexturing->processFeedback();
    }

    // Actualizar world system
    _worldSystem->updateLocalPositions(activeCamera->position);
    _worldSystem->frustumCull(activeCamera->position, view * proj, 10000.0f * Haruka::Units::MEGAMETER);

    // ========== GEOMETRY PASS ==========
    _gBuffer->bindForWriting();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    _geomShader->use();
    _geomShader->setMat4("projection", proj);
    _geomShader->setMat4("view", view);

    renderScene();

    for (const auto& item : g_sceneRenderQueue) {
        _geomShader->setMat4("model", item.modelMatrix);
        _geomShader->setVec3("color", item.color);

        if (item.material) {
            _geomShader->setVec3("material.albedo", item.material->albedo);
            _geomShader->setFloat("material.roughness", item.material->roughness);
            _geomShader->setFloat("material.metallic", item.material->metallic);
        }

        if (item.meshRenderer) {
            item.meshRenderer->render(*_geomShader);
            continue;
        }

        if (!item.modelPath.empty()) {
            try {
                Model model(item.modelPath);
                model.Draw(*_geomShader);
            } catch (const std::exception& e) {
                HARUKA_MOTOR_ERROR(
                    ErrorCode::MODEL_LOAD_FAILED,
                    std::string("Failed to draw model ") + item.modelPath + ": " + e.what()
                );
            }
            continue;
        }
    }

    // Render celestial bodies with LOD
    for (const auto& body : _worldSystem->getBodies()) {
        if (!body.visible) continue;

        glm::vec3 camPos(activeCamera->position.x, activeCamera->position.y, activeCamera->position.z);
        glm::vec3 bodyPos(body.localPos.x, body.localPos.y, body.localPos.z);
        float distance = glm::length(camPos - bodyPos);
        
        int lod = distance < 50.0f ? 0 : distance < 200.0f ? 1 : distance < 1000.0f ? 2 : 3;
        
        if (!sphereLOD[lod]) continue;

        glm::mat4 bodyModel = glm::translate(glm::mat4(1.0f), glm::vec3(body.localPos));
        bodyModel = glm::scale(bodyModel, glm::vec3(body.radius));
        
        _geomShader->setMat4("model", bodyModel);
        sphereLOD[lod]->draw();
    }
        
    _gBuffer->unbind();

    // ========== SSAO PASS ==========
    _ssaoSystem->bindForWriting();
    glClear(GL_COLOR_BUFFER_BIT);

    _ssaoShader->use();
    _gBuffer->bindForReading(0, 0);
    _gBuffer->bindForReading(1, 1);
    
    _ssaoShader->setInt("gPosition", 0);
    _ssaoShader->setInt("gNormal", 1);
    _ssaoShader->setInt("texNoise", 2);
    _ssaoShader->setInt("kernelSize", 64);
    _ssaoShader->setFloat("radius", 0.5f);
    _ssaoShader->setFloat("bias", 0.025f);
    _ssaoShader->setMat4("projection", proj);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    _ssaoSystem->unbind();

    // ========== LIGHTING PASS ==========
    _lightingTarget->bindForWriting();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    _lightShader->use();

    _gBuffer->bindForReading(0, 0);
    _gBuffer->bindForReading(1, 1);
    _gBuffer->bindForReading(2, 2);
    _gBuffer->bindForReading(3, 3);
    _ssaoSystem->bindForReading(4);
    _iblSystem->bindPrefilterMap(5);
    _iblSystem->bindBRDFLUT(6);

    _lightShader->setInt("gPosition", 0);
    _lightShader->setInt("gNormal", 1);
    _lightShader->setInt("gAlbedoSpec", 2);
    _lightShader->setInt("gEmissive", 3);
    _lightShader->setInt("ssao", 4);
    _lightShader->setInt("prefilterMap", 5);
    _lightShader->setInt("brdfLUT", 6);
    _lightShader->setVec3("viewPos", activeCamera->position);

    // Light culling
    Haruka::Scene* scene = MotorInstance::getInstance().getScene();
    if (!scene) {
        scene = _currentScene.get();
    }
    auto culledLights = _lightCuller->cullLights(scene, view, proj, MAX_LIGHTS);
    
    _lightShader->setInt("numLights", culledLights.size());

    for (size_t i = 0; i < culledLights.size(); i++) {
        _lightShader->setVec3("lights[" + std::to_string(i) + "].position", culledLights[i].position);
        _lightShader->setVec3("lights[" + std::to_string(i) + "].color", culledLights[i].color);
    }

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    _lightingTarget->unbind();

    // ========== FINAL COMPOSITE ==========
    RenderTarget* externalTarget = MotorInstance::getInstance().getRenderTarget();
    if (externalTarget) {
        externalTarget->bindForWriting();
    }
    glViewport(0, 0, _width, _height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    _compositeShader->use();
    _lightingTarget->bindForReading(0);
    _bloomExtractTarget->bindForReading(1);
    _compositeShader->setInt("scene", 0);
    _compositeShader->setInt("bloom", 1);
    _compositeShader->setFloat("bloomStrength", 0.0f);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);

    if (externalTarget) {
        externalTarget->unbind();
    }

    // ========== DEBUG METRICS ==========
    FrameMetrics metrics;
    metrics.fps = 60.0f;
    metrics.frameTimeMs = 16.67f;
    metrics.drawCalls = 100;
    metrics.totalLights = scene ? scene->getObjects().size() : 0;
    metrics.culledLights = _lightCuller ? _lightCuller->getCulledLights() : 0;
    
    auto streamStats = AssetStreamer::getInstance().getStats();
    metrics.loadedAssets = streamStats.loadedAssets;
    metrics.pendingAssets = streamStats.pendingAssets;
    metrics.cacheUtilization = streamStats.cacheUtilization;
    
    metrics.numCascades = 4;
    metrics.activeCascade = 0;
    
    DebugOverlay::getInstance().updateMetrics(metrics);
}

void Application::cleanup() {
    // Limpiar registro en MotorInstance
    MotorInstance::getInstance().clear();
    glfwTerminate();
}

void Application::run() {
    create_window();
    // TODO: necesario descomentar para produccion 
    //init();
    main_loop();
}