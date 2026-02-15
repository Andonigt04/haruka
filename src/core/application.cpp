#include "application.h"

#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

#include "renderer/mesh.h"
#include "world_system.h"

Application::Application() : _window(nullptr) {}

Application::~Application() { cleanup(); }

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app) {
        app->getCamera()->ProcessMouseScroll(static_cast<float>(yoffset));
    }
}

float lastX = 640.0f, lastY = 360.0f;
bool firstMouse = true;

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app && app->getCamera()) {
        app->getCamera()->rotate(xoffset, -yoffset);
    }
}

static unsigned int CreateSolidTexture(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    unsigned char data[4] = { r, g, b, a };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return tex;
}

void Application::setupQuad()
{
    float quadVertices[] = {
        // Positions   // TexCoords
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

void Application::init_window() {
    if (!glfwInit()) throw std::runtime_error("Fallo al inicializar GLFW");

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    _window = glfwCreateWindow(_width, _height, "Haruka Engine | 64-bit Core", nullptr, nullptr);
    if (!_window) throw std::runtime_error("Fallo al crear la ventana");

    glfwMakeContextCurrent(_window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Fallo al inicializar GLAD");
    }

    std::vector<Vertex> planeVertices = {
        // Posición              Normal              TexCoords        Tangent           Bitangent
        {{-10.0f, -1.0f,  10.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f},   {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 10.0f, -1.0f,  10.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f},   {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 10.0f, -1.0f, -10.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f},   {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-10.0f, -1.0f, -10.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f},   {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}
    };

    std::vector<unsigned int> planeIndices = {
        0, 1, 2,
        2, 3, 0
    };

    // Crear texturas neutras (BLANCAS para no afectar color)
    unsigned int defaultDiffuse = CreateSolidTexture(255, 255, 255, 255);  // Blanco = sin afectar
    unsigned int defaultNormal = CreateSolidTexture(128, 128, 255, 255);   // Azul púrpura = normal neutral
    unsigned int defaultAO = CreateSolidTexture(255, 255, 255, 255);       // Blanco = sin oclusión
    unsigned int defaultEmissive = CreateSolidTexture(0, 0, 0, 255);       // Negro = no emite

    std::vector<MeshTexture> planeTextures;  // Vector VACÍO

    _planeMesh = std::make_unique<Mesh>(planeVertices, planeIndices, planeTextures);

    _camera = std::make_unique<Camera>(Haruka::WorldPos(0.0f, 0.0f, 15.0f));

    glfwSetWindowUserPointer(_window, this);
    glfwSetScrollCallback(_window, scroll_callback);
    glfwSetCursorPosCallback(_window, mouse_callback);

    glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glEnable(GL_DEPTH_TEST);

    // 1. Shaders y Modelos
    _mainShader = std::make_unique<Shader>("shaders/simple.vert", "shaders/pbr.frag");
    _lampShader = std::make_unique<Shader>("shaders/simple.vert", "shaders/light_cube.frag");
    _shadowSystem = std::make_unique<Shadow>(1024, 1024);
    _hdrSystem = std::make_unique<HDR>(_width, _height);
    _bloomSystem = std::make_unique<Bloom>(_width, _height);
    _gBuffer = std::make_unique<GBuffer>(_width, _height);
    _ssaoSystem = std::make_unique<SSAO>(_width, _height);
    _iblSystem = std::make_unique<IBL>();
    _pointShadowSystem = std::make_unique<PointShadow>(1024);
    _lightingTarget = std::make_unique<RenderTarget>(_width, _height);
    _bloomExtractTarget = std::make_unique<RenderTarget>(_width, _height);

    _model = std::make_unique<Model>("assets/models/DamagedHelmet.glb");
    _model2 = std::make_unique<Model>("assets/models/backpack.obj");

    setupQuad();
}

void Application::main_loop() {
    glm::vec3 pointLightPositions[] = {
        glm::vec3( 5.7f,  1.0f,  2.0f),
        glm::vec3( 2.3f, -3.3f, -4.0f),
        glm::vec3(-4.0f,  2.0f, -12.0f),
        glm::vec3( 0.0f,  0.0f, -3.0f)
    };

    while (!glfwWindowShouldClose(_window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        _camera->processInput(_window, deltaTime);

        glm::mat4 proj = glm::perspective(glm::radians(_camera->zoom), (float)_width / (float)_height, 0.1f, 100.0f);
        glm::mat4 view = _camera->getViewMatrix();

        // ========== POINT SHADOW PASS ==========
        _pointShadowSystem->bindForWriting();

        Shader pointShadowShader("shaders/point_shadow.vert", "shaders/point_shadow.frag", "shaders/point_shadow.geom");
        pointShadowShader.use();

        float farPlane = 150.0f;  // antes 50.0f
        glm::vec3 lightPos = pointLightPositions[0];
        glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, farPlane);
        std::vector<glm::mat4> shadowMatrices(6);

        shadowMatrices[0] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 1.0,  0.0,  0.0), glm::vec3(0.0, -1.0,  0.0));
        shadowMatrices[1] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3(-1.0,  0.0,  0.0), glm::vec3(0.0, -1.0,  0.0));
        shadowMatrices[2] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0,  1.0,  0.0), glm::vec3(0.0,  0.0,  1.0));
        shadowMatrices[3] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0, -1.0,  0.0), glm::vec3(0.0,  0.0, -1.0));
        shadowMatrices[4] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0,  0.0,  1.0), glm::vec3(0.0, -1.0,  0.0));
        shadowMatrices[5] = shadowProj * glm::lookAt(lightPos, lightPos + glm::vec3( 0.0,  0.0, -1.0), glm::vec3(0.0, -1.0,  0.0));

        for (unsigned int i = 0; i < 6; ++i) {
            pointShadowShader.setMat4("shadowMatrices[" + std::to_string(i) + "]", shadowMatrices[i]);
        }

        pointShadowShader.setVec3("lightPos", lightPos);
        pointShadowShader.setFloat("farPlane", farPlane);

        glm::mat4 modelMatrix = glm::mat4(1.0f);
        pointShadowShader.setMat4("model", modelMatrix);
        _model->Draw(pointShadowShader);

        modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f));
        pointShadowShader.setMat4("model", modelMatrix);
        _model2->Draw(pointShadowShader);

        glm::mat4 planeModel = glm::mat4(1.0f);
        pointShadowShader.setMat4("model", planeModel);
        _planeMesh->Draw(pointShadowShader);

        _pointShadowSystem->unbind();

        // ========== GEOMETRY PASS ==========
        _gBuffer->bindForWriting();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Shader geomShader("shaders/deferred_geom.vert", "shaders/deferred_geom.frag");
        geomShader.use();
        geomShader.setMat4("projection", proj);
        geomShader.setMat4("view", view);

        modelMatrix = glm::mat4(1.0f);
        geomShader.setMat4("model", modelMatrix);
        _model->Draw(geomShader);

        modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 0.0f, 0.0f));
        geomShader.setMat4("model", modelMatrix);
        _model2->Draw(geomShader);

        planeModel = glm::mat4(1.0f);
        geomShader.setMat4("model", planeModel);
        _planeMesh->Draw(geomShader);

        _gBuffer->unbind();

        // ========== SSAO PASS ==========
        _ssaoSystem->bindForWriting();
        glClear(GL_COLOR_BUFFER_BIT);

        Shader ssaoShader("shaders/screenquad.vert", "shaders/ssao.frag");
        ssaoShader.use();

        _gBuffer->bindForReading(0, 0); // gPosition
        _gBuffer->bindForReading(1, 1); // gNormal
        
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
        _pointShadowSystem->bindForReading(5);
        _iblSystem->bindPrefilterMap(6);
        _iblSystem->bindBRDFLUT(7);

        lightShader.setInt("gPosition", 0);
        lightShader.setInt("gNormal", 1);
        lightShader.setInt("gAlbedoSpec", 2);
        lightShader.setInt("gEmissive", 3);
        lightShader.setInt("ssao", 4);
        lightShader.setInt("pointShadowMap", 5);
        lightShader.setInt("prefilterMap", 6);
        lightShader.setInt("brdfLUT", 7);
        lightShader.setVec3("lightPos", pointLightPositions[0]);
        lightShader.setFloat("farPlane", farPlane);

        lightShader.setVec3("viewPos", _camera->position);
        lightShader.setInt("numLights", 2);

        lightShader.setVec3("lights[0].position", pointLightPositions[0]);
        lightShader.setVec3("lights[0].color", glm::vec3(100.0f, 100.0f, 100.0f));

        lightShader.setVec3("lights[1].position", pointLightPositions[1]);
        lightShader.setVec3("lights[1].color", glm::vec3(50.0f, 50.0f, 50.0f));


        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        _lightingTarget->unbind();

        // ========== BLOOM EXTRACT PASS ==========
        _bloomExtractTarget->bindForWriting();
        glClear(GL_COLOR_BUFFER_BIT);

        Shader bloomExtractShader("shaders/screenquad.vert", "shaders/bloom_extract.frag");
        bloomExtractShader.use();
        _lightingTarget->bindForReading(0);  // Leer del lighting output
        bloomExtractShader.setInt("scene", 0);
        bloomExtractShader.setFloat("threshold", 1.0f);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        _bloomExtractTarget->unbind();

        // ========== FINAL COMPOSITE ==========
        glViewport(0, 0, _width, _height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Shader compositeShader("shaders/screenquad.vert", "shaders/bloom_composite.frag");
        compositeShader.use();
        _lightingTarget->bindForReading(0);
        _bloomExtractTarget->bindForReading(1);
        compositeShader.setInt("scene", 0);
        compositeShader.setInt("bloom", 1);
        compositeShader.setFloat("bloomStrength", 0.5f);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        glfwSwapBuffers(_window);
        glfwPollEvents();
    }
}

void Application::cleanup() {
    glfwTerminate();
}

void Application::run() {
    init_window();
    main_loop();
}