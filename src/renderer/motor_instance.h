#pragma once

class RenderTarget;
class Application;

/**
 * MotorInstance - Singleton que permite al Editor/Scripts acceder al Motor
 * 
 * El Motor registra su RenderTarget y Application cuando está corriendo.
 * El Editor/Scripts lo consultan para acceder a sistemas como Raycast.
 */
class MotorInstance {
public:
    static MotorInstance& getInstance() {
        static MotorInstance instance;
        return instance;
    }

    // El Motor registra su RenderTarget cuando está corriendo
    void setRenderTarget(RenderTarget* target) {
        motorRenderTarget = target;
    }

    // El Motor registra su Application para acceso desde scripts
    void setApplication(Application* app) {
        motorApplication = app;
    }

    // El Editor consulta si el Motor está activo
    RenderTarget* getRenderTarget() const {
        return motorRenderTarget;
    }

    // Scripts acceden a sistemas del motor (raycast, etc)
    Application* getApplication() const {
        return motorApplication;
    }

    // Limpiar cuando el Motor se detiene
    void clear() {
        motorRenderTarget = nullptr;
        motorApplication = nullptr;
    }

    bool isMotorActive() const {
        return motorRenderTarget != nullptr;
    }

private:
    MotorInstance() = default;
    ~MotorInstance() = default;

    // Prevenir copia
    MotorInstance(const MotorInstance&) = delete;
    MotorInstance& operator=(const MotorInstance&) = delete;

    RenderTarget* motorRenderTarget = nullptr;
    Application* motorApplication = nullptr;
};
