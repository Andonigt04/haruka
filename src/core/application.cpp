#include "application.h"

#include <iostream>
#include <cmath>
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

// Mouse input state
struct MouseState {
    float lastX = 640.0f;
    float lastY = 360.0f;
    bool firstMouse = true;
};

static MouseState g_mouseState;

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
        
        // Cargar modelos de la escena
        _sceneModels.clear();
        for (const auto& obj : _currentScene->getObjects()) {
            if (obj.type == "Model" && !obj.modelPath.empty()) {
                try {
                    auto model = std::make_unique<Model>(obj.modelPath);
                    _sceneModels.push_back(std::move(model));
                    std::cout << "  Loaded model: " << obj.name << " from " << obj.modelPath << std::endl;
                } catch (const std::exception& e) {
                    HARUKA_MOTOR_ERROR(ErrorCode::MODEL_LOAD_FAILED, std::string("Failed to load model ") + obj.name + ": " + e.what());
                }
            }
        }
    } else {
        HARUKA_MOTOR_ERROR(ErrorCode::SCENE_PARSE_ERROR, std::string("Failed to load scene from: ") + scenePath);
        // Crear escena vacía por defecto
        _currentScene = std::make_unique<Haruka::Scene>("DefaultScene");
    }
}

void Application::init_window() {
    if (!glfwInit()) {
        HARUKA_MOTOR_ERROR(ErrorCode::MOTOR_INIT_FAILED, "Failed to initialize GLFW");
        throw std::runtime_error("Fallo al inicializar GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    _window = glfwCreateWindow(_width, _height, "Haruka Engine | Runtime", nullptr, nullptr);
    if (!_window) {
        HARUKA_MOTOR_ERROR(ErrorCode::WINDOW_CREATION_FAILED, "Failed to create GLFW window");
        throw std::runtime_error("Fallo al crear la ventana");
    }

    glfwMakeContextCurrent(_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Fallo al inicializar GLAD");
    }

    _camera = std::make_unique<Camera>(Haruka::WorldPos(0.0f, 0.0f, 15.0f));

    glfwSetWindowUserPointer(_window, this);
    glfwSetScrollCallback(_window, scroll_callback);
    glfwSetCursorPosCallback(_window, mouse_callback);
    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glEnable(GL_DEPTH_TEST);

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

    // Registrar RenderTarget para que el Editor pueda acceder
    MotorInstance::getInstance().setRenderTarget(_lightingTarget.get());

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
    
    std::vector<glm::vec3> cubeVerts, cubeNorms;
    std::vector<unsigned int> cubeIndices;
    PrimitiveShapes::createCube(1.0f, cubeVerts, cubeNorms, cubeIndices);
    cubeMesh = std::make_unique<SimpleMesh>(cubeVerts, cubeNorms, cubeIndices);

    std::vector<glm::vec3> planeVerts, planeNorms;
    std::vector<unsigned int> planeIndices;
    PrimitiveShapes::createPlane(2.0f, 2.0f, 10, planeVerts, planeNorms, planeIndices);
    planeMesh = std::make_unique<SimpleMesh>(planeVerts, planeNorms, planeIndices);

    _worldSystem->initComputeShaders();

    _currentScene = std::make_unique<Haruka::Scene>("EmptyScene");
}

void Application::renderScene(Shader* shader) {
    if (!_currentScene) return;
    
    Shader* useShader = shader ? shader : _mainShader.get();
    
    size_t modelIndex = 0;
    for (const auto& obj : _currentScene->getObjects()) {
        if (obj.type == "Model" && modelIndex < _sceneModels.size()) {
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, glm::vec3(obj.position));
            modelMatrix = glm::rotate(modelMatrix, glm::radians((float)obj.rotation.x), glm::vec3(1, 0, 0));
            modelMatrix = glm::rotate(modelMatrix, glm::radians((float)obj.rotation.y), glm::vec3(0, 1, 0));
            modelMatrix = glm::rotate(modelMatrix, glm::radians((float)obj.rotation.z), glm::vec3(0, 0, 1));
            modelMatrix = glm::scale(modelMatrix, glm::vec3(obj.scale));
            
            useShader->use();
            useShader->setMat4("model", modelMatrix);
            _sceneModels[modelIndex]->Draw(*useShader);
            
            modelIndex++;
        }
        
        // Renderizar primitivos (Mesh sin modelPath)
        if (obj.type == "Mesh") {
            if (!cubeMesh) {
                HARUKA_MOTOR_ERROR(ErrorCode::RENDER_TARGET_FAILED, "cubeMesh not initialized for object: " + obj.name);
                continue;
            }
            
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, glm::vec3(obj.position));
            modelMatrix = glm::rotate(modelMatrix, glm::radians((float)obj.rotation.x), glm::vec3(1, 0, 0));
            modelMatrix = glm::rotate(modelMatrix, glm::radians((float)obj.rotation.y), glm::vec3(0, 1, 0));
            modelMatrix = glm::rotate(modelMatrix, glm::radians((float)obj.rotation.z), glm::vec3(0, 0, 1));
            modelMatrix = glm::scale(modelMatrix, glm::vec3(obj.scale));
            
            useShader->use();
            useShader->setMat4("model", modelMatrix);
            useShader->setVec3("color", obj.color);
            
            // Renderizar cubo primitivo
            cubeMesh->draw();
        }
    }
}

void Application::main_loop() {
    
    std::vector<glm::vec3> lights;
    std::vector<glm::vec3> lightColors;
    
    // Actualizar posición de la cámara en AssetStreamer (para priorización)
    AssetStreamer::getInstance().updateCameraPosition(_camera->position);
    
    // Actualizar cascadas de sombra según dirección de luz
    // Si hay luces direccionales en la escena, usar la primera
    // Si no hay, usar default pero NO aplicar sombras
    glm::vec3 lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));  // Default (sin efecto si no hay luces)
    
    _cascadedShadow->updateCascades(
        lightDir,
        _camera->position,
        glm::vec3(0.0f, 0.0f, -1.0f),  // Forward direction
        0.1f,                          // zNear
        1000.0f,                        // zFar
        45.0f                           // FOV
    );
    
    // Procesar virtual texturing feedback
    if (_virtualTexturing) {
        _virtualTexturing->processFeedback();
    }
    
    // Get lights from scene (motor es agnóstico - IDE decide qué hay en la escena)
    for (const auto& obj : _currentScene->getObjects()) {
        if (obj.type == "Light") {
            lights.push_back(obj.position);
            lightColors.push_back(obj.color * obj.intensity);
        }
    }
    
    // Sin fallback - si IDE no agrega luces, no hay luces
    // El motor NO debe tomar decisiones sobre la escena

    while (!glfwWindowShouldClose(_window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        _camera->processInput(_window, deltaTime);

        glm::mat4 proj = glm::perspective(glm::radians(_camera->zoom), (float)_width / (float)_height, 0.1f, 10000000000.0f);
        glm::mat4 view = _camera->getViewMatrix();

        // Update world system
        _worldSystem->updateLocalPositions(_camera->position);
        _worldSystem->frustumCull(_camera->position, view * proj, 10000.0f * Haruka::Units::MEGAMETER);

        // ========== GEOMETRY PASS ==========
        _gBuffer->bindForWriting();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Shader geomShader("shaders/deferred_geom.vert", "shaders/deferred_geom.frag");
        geomShader.use();
        geomShader.setMat4("projection", proj);
        geomShader.setMat4("view", view);

        // Render scene objects con shader de geometría
        renderScene(&geomShader);

        // Render celestial bodies with LOD
        for (const auto& body : _worldSystem->getBodies()) {
            if (!body.visible) continue;

            glm::vec3 camPos(_camera->position.x, _camera->position.y, _camera->position.z);
            glm::vec3 bodyPos(body.localPos.x, body.localPos.y, body.localPos.z);
            float distance = glm::length(camPos - bodyPos);
            
            int lod = distance < 50.0f ? 0 : distance < 200.0f ? 1 : distance < 1000.0f ? 2 : 3;

            glm::mat4 bodyModel = glm::translate(glm::mat4(1.0f), glm::vec3(body.localPos));
            bodyModel = glm::scale(bodyModel, glm::vec3(body.radius));
            
            geomShader.setMat4("model", bodyModel);
            sphereLOD[lod]->draw();
        }
            
        _gBuffer->unbind();

        // ========== SSAO PASS ==========
        _ssaoSystem->bindForWriting();
        glClear(GL_COLOR_BUFFER_BIT);

        Shader ssaoShader("shaders/screenquad.vert", "shaders/ssao.frag");
        ssaoShader.use();
        _gBuffer->bindForReading(0, 0);
        _gBuffer->bindForReading(1, 1);
        
        ssaoShader.setInt("gPosition", 0);
        ssaoShader.setInt("gNormal", 1);
        ssaoShader.setInt("texNoise", 2);
        ssaoShader.setInt("kernelSize", 64);
        ssaoShader.setFloat("radius", 0.5f);
        ssaoShader.setFloat("bias", 0.025f);
        ssaoShader.setMat4("projection", proj);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        _ssaoSystem->unbind();

        // ========== LIGHTING PASS ==========
        _lightingTarget->bindForWriting();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Shader lightShader("shaders/screenquad.vert", "shaders/deferred_light.frag");
        lightShader.use();

        _gBuffer->bindForReading(0, 0);
        _gBuffer->bindForReading(1, 1);
        _gBuffer->bindForReading(2, 2);
        _gBuffer->bindForReading(3, 3);
        _ssaoSystem->bindForReading(4);
        _iblSystem->bindPrefilterMap(5);
        _iblSystem->bindBRDFLUT(6);

        lightShader.setInt("gPosition", 0);
        lightShader.setInt("gNormal", 1);
        lightShader.setInt("gAlbedoSpec", 2);
        lightShader.setInt("gEmissive", 3);
        lightShader.setInt("ssao", 4);
        lightShader.setInt("prefilterMap", 5);
        lightShader.setInt("brdfLUT", 6);
        lightShader.setVec3("viewPos", _camera->position);

        // ===== LIGHT CULLING (Soporta ilimitadas luces) =====
        auto culledLights = _lightCuller->cullLights(_currentScene.get(), view, proj, MAX_LIGHTS);
        
        lightShader.setInt("numLights", culledLights.size());

        for (size_t i = 0; i < culledLights.size(); i++) {
            lightShader.setVec3("lights[" + std::to_string(i) + "].position", culledLights[i].position);
            lightShader.setVec3("lights[" + std::to_string(i) + "].color", culledLights[i].color);
        }

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        _lightingTarget->unbind();

        // ========== FINAL COMPOSITE ==========
        glViewport(0, 0, _width, _height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Shader compositeShader("shaders/screenquad.vert", "shaders/bloom_composite.frag");
        compositeShader.use();
        _lightingTarget->bindForReading(0);
        _bloomExtractTarget->bindForReading(1);
        compositeShader.setInt("scene", 0);
        compositeShader.setInt("bloom", 1);
        compositeShader.setFloat("bloomStrength", 0.0f); // Disable bloom for now

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        // Actualizar métricas de debug overlay
        FrameMetrics metrics;
        metrics.fps = 60.0f;  // Aproximado
        metrics.frameTimeMs = 16.67f;  // ~60 FPS
        metrics.drawCalls = 100;  // Ejemplo
        metrics.totalLights = _currentScene ? _currentScene->getObjects().size() : 0;
        metrics.culledLights = _lightCuller ? _lightCuller->getCulledLights() : 0;
        
        auto streamStats = AssetStreamer::getInstance().getStats();
        metrics.loadedAssets = streamStats.loadedAssets;
        metrics.pendingAssets = streamStats.pendingAssets;
        metrics.cacheUtilization = streamStats.cacheUtilization;
        
        // Cascaded shadow maps info
        metrics.numCascades = 4;
        metrics.activeCascade = 0;  // Simulado (en producción, basado en distancia)
        
        DebugOverlay::getInstance().updateMetrics(metrics);
        
        // Renderizar debug overlay (ImGui)
        DebugOverlay::getInstance().render();

        glfwSwapBuffers(_window);
        glfwPollEvents();
    }
}

void Application::cleanup() {
    // Limpiar registro en MotorInstance
    MotorInstance::getInstance().clear();
    glfwTerminate();
}

void Application::run() {
    init_window();
    main_loop();
}