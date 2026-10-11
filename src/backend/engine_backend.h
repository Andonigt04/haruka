/**
 * @file engine_backend.h
 * @brief Contrato entre el editor genérico y un motor: lo único que el núcleo sabe de un motor.
 *
 * El NÚCLEO del editor (ventana, ImGui, menús, registro de paneles, plugins, consola, navegador de
 * ficheros, deshacer, autoguardado, copias de seguridad, crear/compilar proyecto con EngineSdk) no
 * incluye ninguna cabecera del motor: lo comprueba el build (cmake/check_core_engine_free.cmake).
 * Todo lo que depende de un motor concreto —su escena, su viewport, sus paneles, el play mode—
 * vive detrás de esta interfaz. `backends/haruka/` es la de haruka-cpp.
 *
 * Qué es un "documento": lo que se abre, se guarda y se ensucia (para Haruka, una `.scene` o
 * `.prefab`). El núcleo lleva la ruta, el "sin guardar", el autoguardado y las copias; el backend
 * solo sabe leerlo y escribirlo.
 *
 * Tipos que SÍ cruzan: SDL_Window (la ventana es del editor) y lo que dibuja ImGui. Un backend de un
 * motor que no se pueda enlazar en el proceso (Unreal, O3DE) implementa esto como un adaptador que
 * habla con el motor por IPC y pinta en un panel la imagen que le llega.
 */
#pragma once

#include "plugins/haruka_editor_plugin.h"   // HarukaImGuiAbi: el mismo control de ImGui que los plugins
#include "backend/scene_model.h"

#include <cstddef>
#include <functional>
#include <string>

struct SDL_Window;
class PanelRegistry;
class CommandHistory;

/// Lo que el editor le presta al backend. Vive mientras vive el backend.
struct EditorServices {
    PanelRegistry* panels = nullptr;
    CommandHistory* history = nullptr;
    SDL_Window* window = nullptr;
    /// El documento abierto tiene cambios sin guardar (enciende el "*" y el autoguardado).
    std::function<void()> markDirty;
    /// Pedir al editor que abra un documento (doble clic en un fichero, etc.): pasa por el aviso
    /// de "cambios sin guardar" del núcleo, no directamente al backend.
    std::function<void(const std::string&)> requestOpenDocument;
};

class IEngineBackend {
public:
    virtual ~IEngineBackend() = default;

    /// Nombre para logs y título ("haruka-cpp").
    virtual const char* name() const = 0;

    // ── Ciclo de vida ────────────────────────────────────────────────────────────────────────────
    /// Con la ventana, el contexto gráfico e ImGui ya creados. Registra aquí sus paneles en
    /// `services.panels`. false = no se pudo; el editor sigue sin backend.
    virtual bool init(const EditorServices& services) = 0;
    /// Antes de destruir ImGui y el contexto gráfico.
    virtual void shutdown() = 0;
    /// Cada frame, ANTES de ImGui::NewFrame (lógica, cámaras, tareas largas).
    virtual void update(float dt) = 0;
    /// Cada frame, DENTRO del frame de ImGui y antes de los paneles (p. ej. el juego en play mode,
    /// que dibuja su propio HUD con ImGui).
    virtual void beginFrame(float dt) { (void)dt; }

    // ── Menús ────────────────────────────────────────────────────────────────────────────────────
    /// Entradas propias dentro de un menú del editor ("File", "Edit"), al final de ese menú.
    virtual void drawMenuItems(const std::string& menu) { (void)menu; }
    /// Menús propios en la barra, entre "Edit" y "View" (Haruka: "Objects", "Material").
    virtual void drawMainMenus() {}
    /// Ventanas modales o sueltas que no son paneles del registro (diálogos del backend).
    virtual void drawPopups() {}

    // ── Proyecto ─────────────────────────────────────────────────────────────────────────────────
    /// Abrir un proyecto existente. El núcleo luego avisa a los paneles (`projectChanged`).
    virtual bool openProject(const std::string& root) = 0;
    /// Tras copiar el template del motor a `root` (eso lo hace el núcleo con EngineSdk): escribir lo
    /// que el motor necesite y dejarlo abierto. Devuelve el documento inicial ("" = ninguno).
    virtual std::string initializeNewProject(const std::string& root, const std::string& name) = 0;
    /// Raíz del proyecto abierto ("" = ninguno).
    virtual std::string projectRoot() const = 0;
    /// Nombre con el que se compila (lib<Nombre>.so). "" = usar el de la carpeta.
    virtual std::string projectName() const = 0;
    /// La escena abierta, OPACA (lo que reciben los paneles en `onSceneChanged`). nullptr = ninguna.
    virtual void* sceneHandle() const { return nullptr; }
    /// La escena descrita de forma genérica (ver scene_model.h). Si no es nullptr, el NÚCLEO pone
    /// su jerarquía y su inspector genéricos sobre ella; un backend con paneles propios devuelve
    /// nullptr. Debe vivir lo que vive el backend.
    virtual ISceneModel* genericScene() { return nullptr; }
    /// Documento que se abre al arrancar con un proyecto ("" = ninguno).
    virtual std::string defaultDocument(const std::string& root) const { (void)root; return {}; }

    // ── Documentos ───────────────────────────────────────────────────────────────────────────────
    /// Filtro del diálogo de abrir, al estilo de NFD ("scene,prefab").
    virtual std::string documentFilter() const { return {}; }
    /// Ruta propuesta en "Guardar como" sin documento ("scenes/Untitled.scene").
    virtual std::string untitledDocument() const { return {}; }
    virtual bool saveDocument(const std::string& path) = 0;
    /// Al volver, los paneles ya apuntan al documento nuevo AUNQUE la carga falle (el backend no
    /// puede dejarlos sobre lo que acaba de destruir).
    virtual bool loadDocument(const std::string& path) = 0;
    /// Etiqueta para el título ("Scene", "Prefab").
    virtual std::string documentKind(const std::string& path) const { (void)path; return {}; }

    // ── Play mode ────────────────────────────────────────────────────────────────────────────────
    virtual bool canPlay() const { return false; }
    virtual bool isPlaying() const { return false; }
    virtual void enterPlay() {}
    virtual void exitPlay() {}
};

// ── Backend como librería (.so) ──────────────────────────────────────────────────────────────
//
// El editor NO enlaza ningún backend: carga uno al arrancar (ver backend/backend_loader.h). La
// librería exporta, con enlace C:
//
//     HARUKA_EDITOR_BACKEND_DECLARE_API();
//     HARUKA_EDITOR_PLUGIN_EXPORT IEngineBackend* haruka_editor_backend_create(
//         const HarukaImGuiAbi* imgui, char* error, size_t errorLen);   // nullptr = rechazo
//
// ⚠️ A DIFERENCIA DE LOS PLUGINS, ESTO ES ABI DE C++ (clase con vtable, std::string,
// std::function): backend y editor tienen que compilarse con el mismo compilador y la misma
// libstdc++. Subir la versión con CUALQUIER cambio de IEngineBackend o EditorServices: un backend
// viejo llamaría a métodos virtuales por posiciones de la vtable que ya no son las suyas.
#define HARUKA_EDITOR_BACKEND_API_VERSION 2   // 2: genericScene()
#define HARUKA_EDITOR_BACKEND_API_SYMBOL    "haruka_editor_backend_api"
#define HARUKA_EDITOR_BACKEND_CREATE_SYMBOL "haruka_editor_backend_create"

#define HARUKA_EDITOR_BACKEND_DECLARE_API()                                    \
    HARUKA_EDITOR_PLUGIN_EXPORT const int haruka_editor_backend_api;           \
    const int haruka_editor_backend_api = HARUKA_EDITOR_BACKEND_API_VERSION

using HarukaEditorBackendCreateFn = IEngineBackend* (*)(const HarukaImGuiAbi* imgui, char* error, size_t errorLen);
