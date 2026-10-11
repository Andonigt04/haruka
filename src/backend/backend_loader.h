#pragma once

#include "backend/engine_backend.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @brief Busca y carga el backend de motor (`.so` con haruka_editor_backend_create).
 *
 * Orden de búsqueda (el primero que CARGA gana; los que fallan se cuentan en `attempts`):
 *   1. $HARUKA_EDITOR_BACKEND                 — ruta a un .so (pruebas, otro motor)
 *   2. "editor": {"backend": …} del motor     — haruka-engine.json del SDK activo
 *   3. <dir del exe>/backends/*.so            — lo que se instaló junto al editor
 * HARUKA_EDITOR_NO_BACKEND=1 se salta todo: editor sin motor.
 *
 * La librería NO se descarga nunca (se queda hasta que acaba el proceso): arrastra al motor, y
 * descargar un motor con estado global vivo (hilos, cachés, el juego en play mode) no compensa
 * lo que ahorra.
 */
class BackendLoader {
public:
    struct Attempt {
        std::string path;
        std::string error;   ///< vacío = este es el que se cargó
    };

    /// Necesita el contexto ImGui ya creado: se le pasa al backend para que compruebe que es el suyo.
    std::unique_ptr<IEngineBackend> load(const std::string& enginePluginBackend);
    /// Carga UNA ruta concreta. nullptr + `error` si no vale.
    static std::unique_ptr<IEngineBackend> loadFile(const std::string& path, std::string& error);

    const std::vector<Attempt>& attempts() const { return tried; }
    static std::vector<std::string> candidates(const std::string& engineBackend);

private:
    std::vector<Attempt> tried;
};
