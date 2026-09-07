/**
 * @file engine_sdk.h
 * @brief Contrato IDE <-> motor: el único punto donde el editor sabe de un motor.
 *
 * El IDE NO conoce ningún motor concreto. Descubre uno en tiempo de EJECUCIÓN y lee su
 * descriptor (`haruka-engine.json` en la raíz del SDK). Cualquier motor que publique ese
 * fichero se puede usar desde este editor sin recompilarlo.
 *
 * Antes esto era `-DHARUKA_ENGINE_ROOT="<ruta absoluta del build>"`: la ruta del checkout
 * de quien compilaba quedaba dentro del binario. En el propio equipo de desarrollo cuela,
 * pero un editor instalado (rpm) apunta a un directorio que en esa máquina no existe, y el
 * fallo sale tarde y mal: "Nuevo proyecto" copia un template inexistente y crea una carpeta
 * vacía sin CMakeLists, y "Compilar" pasa -DHARUKA_ENGINE_LOCAL=<ruta muerta>, con lo que
 * el proyecto se pone a descargar el motor de GitHub. De ahí que la ruta se resuelva ahora
 * al vuelo y el compile-time solo sea el ÚLTIMO recurso (build de desarrollo).
 *
 * Orden de búsqueda (primero que exista gana), ver EngineSdk::searchPathsDescription():
 *   1. $HARUKA_ENGINE_ROOT                      — override por lanzamiento (CI, varios motores)
 *   2. ~/.config/haruka-editor/engine.json      — elección del usuario, campo "root"
 *   3. HARUKA_EDITOR_ENGINE_DIR                 — SDK instalado por paquete (/usr/share/...)
 *   4. <dir del ejecutable>/engine, ../engine   — layout portable (zip descomprimido)
 *   5. HARUKA_ENGINE_ROOT                       — ruta de compilación (solo build de dev)
 */
#pragma once

#include <string>

/**
 * @brief Descripción de un SDK de motor utilizable por el editor.
 *
 * Los valores por defecto son los de haruka-cpp: un motor SIN descriptor sigue
 * funcionando igual que antes (compatibilidad hacia atrás con los checkouts ya existentes).
 */
struct EngineSdk {
    /// Raíz del SDK, absoluta y sin "/" final. Vacía = no se encontró ningún motor.
    std::string root;
    /// Nombre declarado por el motor (informativo: barra de título, logs, errores).
    std::string name;
    /// Versión declarada por el motor (informativa).
    std::string version;
    /// Template de proyecto nuevo, ABSOLUTO. Vacío = el motor no trae template.
    std::string templateDir;
    /// Script de build del PROYECTO, relativo a la raíz del proyecto generado.
    std::string buildScript = "build.sh";
    /// Variable CMake con la que se le pasa la raíz del motor al proyecto.
    std::string cmakeEngineVar = "HARUKA_ENGINE_LOCAL";
    /// Binario del runtime que se copia al exportar, ABSOLUTO. Vacío o inexistente = sin runtime.
    std::string runtimeBinary;

    /// @brief True si se localizó un motor utilizable.
    bool valid() const { return !root.empty(); }

    /**
     * @brief SDK activo, resuelto una sola vez por proceso.
     *
     * Se cachea a propósito: lo consultan la creación de proyecto, la compilación y el
     * export, y no tendría sentido que una misma sesión usara motores distintos según en
     * qué momento se mire el disco.
     */
    static const EngineSdk& current();

    /// @brief Rutas consultadas, en orden, para el mensaje de error cuando no hay motor.
    static std::string searchPathsDescription();
};
