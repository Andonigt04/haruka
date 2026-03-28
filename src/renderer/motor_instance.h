#pragma once

class Application;
class RenderTarget;
class Camera;

namespace Haruka {
    class Scene;
}

/**
 * MotorInstance - Singleton que permite al Editor/Viewport comunicarse con el Motor
 * 
 * Friend de Application para acceder directamente a _window, _width, _height
 * El Motor registra su RenderTarget, Scene, Camera y Application cuando está corriendo.
 * El Editor/Scripts lo consultan para acceder a sistemas como Raycast.
 */
class MotorInstance {
    friend class Application;
    
public:
    static MotorInstance& getInstance() {
        static MotorInstance instance;
        return instance;
    }
    
    void setRenderTarget(RenderTarget* target) {
        motorRenderTarget = target;
    }
    
    void setScene(Haruka::Scene* scene) {
        motorScene = scene;
    }
    
    void setCamera(Camera* cam) {
        motorCamera = cam;
    }
    
    void setApplication(Application* app) {
        motorApplication = app;
    }
    
    RenderTarget* getRenderTarget() const {
        return motorRenderTarget;
    }
    
    Haruka::Scene* getScene() const {
        return motorScene;
    }
    
    Camera* getCamera() const {
        return motorCamera;
    }
    
    Application* getApplication() const {
        return motorApplication;
    }
    
    bool isMotorActive() const {
        return motorScene != nullptr && motorRenderTarget != nullptr;
    }
    
    void clear() {
        motorScene = nullptr;
        motorCamera = nullptr;
        motorApplication = nullptr;
    }

private:
    MotorInstance() = default;
    ~MotorInstance() = default;

    // Prevenir copia
    MotorInstance(const MotorInstance&) = delete;
    MotorInstance& operator=(const MotorInstance&) = delete;

    RenderTarget* motorRenderTarget = nullptr;
    Haruka::Scene* motorScene = nullptr;
    Camera* motorCamera = nullptr;
    Application* motorApplication = nullptr;
};
