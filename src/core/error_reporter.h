#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <chrono>

/**
 * ErrorReporter - Sistema de reporte de errores estructurado
 * 
 * Identifica:
 * - Componente que falló (Motor, Editor, Gameplay, Network, Renderer)
 * - Código de error específico
 * - Descripción detallada
 * - Stack trace opcional
 */

enum class ErrorComponent {
    MOTOR = 0,           // Core engine
    EDITOR = 1,          // Editor IDE
    GAMEPLAY = 2,        // Game logic
    RENDERER = 3,        // Graphics rendering
    NETWORK = 4,         // Networking
    PHYSICS = 5,         // Physics engine
    AUDIO = 6,           // Audio system
    IO = 7,              // Input/Output
    SCENE = 8,           // Scene management
    ASSET_LOADER = 9,    // Asset loading
    UNKNOWN = 255
};

enum class ErrorCode {
    // Motor/Core (0-99)
    MOTOR_INIT_FAILED = 0,
    WINDOW_CREATION_FAILED = 1,
    OPENGL_INIT_FAILED = 2,
    SHADER_COMPILATION_FAILED = 3,
    
    // Editor (100-199)
    EDITOR_INIT_FAILED = 100,
    PROJECT_LOAD_FAILED = 101,
    SCENE_SAVE_FAILED = 102,
    INVALID_PROJECT = 103,
    
    // Gameplay (200-299)
    GAME_LOGIC_ERROR = 200,
    INVALID_SCENE = 201,
    OBJECT_CREATION_FAILED = 202,
    COMPONENT_INIT_FAILED = 203,
    
    // Renderer (300-399)
    RENDER_TARGET_FAILED = 300,
    TEXTURE_LOAD_FAILED = 301,
    MODEL_LOAD_FAILED = 302,
    SHADER_UNIFORM_MISSING = 303,
    
    // Network (400-499)
    SOCKET_CREATION_FAILED = 400,
    CONNECTION_TIMEOUT = 401,
    INVALID_MESSAGE = 402,
    SEND_FAILED = 403,
    
    // Physics (500-599)
    PHYSICS_INIT_FAILED = 500,
    COLLISION_CALC_FAILED = 501,
    
    // Audio (600-699)
    AUDIO_INIT_FAILED = 600,
    AUDIO_LOAD_FAILED = 601,
    
    // IO (700-799)
    FILE_NOT_FOUND = 700,
    FILE_READ_ERROR = 701,
    FILE_WRITE_ERROR = 702,
    PERMISSION_DENIED = 703,
    
    // Scene (800-899)
    SCENE_PARSE_ERROR = 800,
    INVALID_OBJECT = 801,
    
    // Asset Loader (900-999)
    ASSET_NOT_FOUND = 900,
    ASSET_FORMAT_INVALID = 901,
    ASSET_CORRUPTED = 902,
    
    // Unknown
    UNKNOWN_ERROR = 9999
};

struct ErrorInfo {
    ErrorComponent component;
    ErrorCode code;
    std::string message;
    std::string file;
    int line;
    std::string function;
    std::string timestamp;
    std::string stackTrace;

    std::string toString() const;
    std::string getComponentName() const;
    std::string getErrorName() const;
};

class ErrorReporter {
public:
    static ErrorReporter& getInstance() {
        static ErrorReporter instance;
        return instance;
    }

    /**
     * Reportar un error
     */
    static void report(
        ErrorComponent component,
        ErrorCode code,
        const std::string& message,
        const std::string& file = "",
        int line = 0,
        const std::string& function = ""
    ) {
        ErrorInfo error;
        error.component = component;
        error.code = code;
        error.message = message;
        error.file = file;
        error.line = line;
        error.function = function;
        error.timestamp = getCurrentTimestamp();
        
        getInstance().logError(error);
    }

    /**
     * Obtener último error
     */
    ErrorInfo getLastError() const { return lastError; }

    /**
     * Limpiar estado de error
     */
    void clearError() {
        lastError = {};
    }

    /**
     * ¿Hay error pendiente?
     */
    bool hasError() const { return lastError.code != ErrorCode::UNKNOWN_ERROR; }

private:
    ErrorReporter() = default;

    ErrorInfo lastError;

    void logError(const ErrorInfo& error) {
        lastError = error;

        // Log a stderr en formato estructurado
        std::cerr << "\n" << std::string(70, '=') << "\n";
        std::cerr << "❌ ERROR REPORT\n";
        std::cerr << std::string(70, '=') << "\n";
        std::cerr << error.toString();
        std::cerr << std::string(70, '=') << "\n\n";
    }

    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::ctime(&time);
        return ss.str();
    }
};

// Macros para fácil uso
#define HARUKA_ERROR(component, code, message) \
    ErrorReporter::report(component, code, message, __FILE__, __LINE__, __FUNCTION__)

#define HARUKA_MOTOR_ERROR(code, message) \
    ErrorReporter::report(ErrorComponent::MOTOR, code, message, __FILE__, __LINE__, __FUNCTION__)

#define HARUKA_EDITOR_ERROR(code, message) \
    ErrorReporter::report(ErrorComponent::EDITOR, code, message, __FILE__, __LINE__, __FUNCTION__)

#define HARUKA_GAMEPLAY_ERROR(code, message) \
    ErrorReporter::report(ErrorComponent::GAMEPLAY, code, message, __FILE__, __LINE__, __FUNCTION__)

#define HARUKA_RENDERER_ERROR(code, message) \
    ErrorReporter::report(ErrorComponent::RENDERER, code, message, __FILE__, __LINE__, __FUNCTION__)

#define HARUKA_NETWORK_ERROR(code, message) \
    ErrorReporter::report(ErrorComponent::NETWORK, code, message, __FILE__, __LINE__, __FUNCTION__)
