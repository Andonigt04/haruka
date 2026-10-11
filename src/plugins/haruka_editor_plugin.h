/**
 * @file haruka_editor_plugin.h
 * @brief Contrato IDE <-> plugin. Lo ÚNICO que un plugin necesita del editor (más `imgui.h`).
 *
 * Un plugin es un `.so` que exporta estos símbolos C:
 *
 *     HARUKA_EDITOR_PLUGIN_DECLARE_API();      // haruka_editor_plugin_api = versión del contrato
 *     HARUKA_EDITOR_PLUGIN_EXPORT int  haruka_editor_plugin_init(const HarukaEditorHost* host); // 0 = rechazo
 *     HARUKA_EDITOR_PLUGIN_EXPORT void haruka_editor_plugin_shutdown(void);                     // opcional
 *
 * ⚠️ USA LA MACRO PARA LA VERSIÓN. En C++ un `const int` de ámbito de fichero tiene enlace INTERNO
 * aunque esté dentro de `extern "C" { ... }`: el símbolo no se exporta y el editor rechaza el
 * plugin con "falta haruka_editor_plugin_api". Pasó al escribir los plugins de la sonda.
 *
 * y en `init` registra sus paneles con `host->registerPanel`. Ver plugins/example_panel/.
 *
 * ⚠️ NO HAY TIPOS DEL MOTOR AQUÍ, a propósito: el mismo editor tiene que poder cargar plugins de
 * motores distintos. Lo que depende del motor (la escena) viaja como `void*` opaco y
 * `host->engineName` dice qué es; un plugin que no conoce ese motor lo ignora.
 *
 * IMGUI: el plugin NO enlaza ImGui. Compila contra `imgui.h` y sus llamadas se resuelven al cargar
 * contra el ImGui que ya está en el proceso del editor, así que comparte su contexto: sus ventanas
 * se acoplan con las demás. Eso exige la MISMA versión y configuración de ImGui (mismo tamaño de
 * ImGuiIO, ImGuiStyle, ImDrawVert); `harukaPluginBindImGui` lo comprueba y el plugin debe devolver
 * 0 si falla. Un plugin que compile su propia copia de ImGui (estática, oculta) también vale: la
 * misma función le fija el contexto y el asignador del editor.
 */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Sube cuando cambia cualquier struct de este fichero. El editor no llama a `init` de un plugin
/// con otra versión: leería campos en sitios que no son.
#define HARUKA_EDITOR_PLUGIN_API_VERSION 1

#ifdef __cplusplus
#define HARUKA_EDITOR_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
/* La declaración `extern "C"` previa es lo que da enlace EXTERNO a la definición `const` de debajo. */
#define HARUKA_EDITOR_PLUGIN_DECLARE_API()                                     \
    HARUKA_EDITOR_PLUGIN_EXPORT const int haruka_editor_plugin_api;            \
    const int haruka_editor_plugin_api = HARUKA_EDITOR_PLUGIN_API_VERSION
#else
#define HARUKA_EDITOR_PLUGIN_EXPORT __attribute__((visibility("default")))
#define HARUKA_EDITOR_PLUGIN_DECLARE_API()                                     \
    HARUKA_EDITOR_PLUGIN_EXPORT const int haruka_editor_plugin_api = HARUKA_EDITOR_PLUGIN_API_VERSION
#endif

#define HARUKA_EDITOR_PLUGIN_API_SYMBOL      "haruka_editor_plugin_api"
#define HARUKA_EDITOR_PLUGIN_INIT_SYMBOL     "haruka_editor_plugin_init"
#define HARUKA_EDITOR_PLUGIN_SHUTDOWN_SYMBOL "haruka_editor_plugin_shutdown"

/// Una ventana del plugin. Las cadenas se COPIAN al registrar; `user` se devuelve tal cual.
typedef struct HarukaPanelDesc {
    const char* id;            ///< único en todo el editor; si choca con otro, se rechaza
    const char* label;         ///< texto del menú View
    int menuGroup;             ///< 0 vistas, 1 herramientas, 2 depuración (o el que se quiera)
    int openByDefault;
    int lockedInPlay;          ///< deshabilitado en play mode
    void* user;
    void (*draw)(void* user);                                   ///< obligatorio
    void (*whileClosed)(void* user);                            ///< opcional
    void (*onProjectChanged)(void* user, const char* projectRoot); ///< opcional
    void (*clearSelection)(void* user);                         ///< opcional
    void (*onSceneChanged)(void* user, void* engineScene);      ///< opcional; opaco, ver engineName
} HarukaPanelDesc;

/// Lo que hace falta para que el plugin dibuje en el contexto ImGui del editor.
typedef struct HarukaImGuiAbi {
    const char* version;       ///< IMGUI_VERSION del editor
    int sizeofIO;
    int sizeofStyle;
    int sizeofDrawVert;
    int sizeofDrawIdx;
    void* context;             ///< ImGuiContext*
    void* (*alloc)(size_t size, void* user);
    void  (*free)(void* ptr, void* user);
    void* allocUser;
} HarukaImGuiAbi;

typedef struct HarukaEditorHost {
    int apiVersion;
    const char* engineName;    ///< nombre del motor activo ("haruka-cpp"); qué es `engineScene`
    HarukaImGuiAbi imgui;
    void* self;                ///< se pasa de vuelta a las funciones de abajo
    /// 1 = registrado. Si ya hay proyecto o escena, el panel recibe sus avisos en el acto.
    int (*registerPanel)(void* self, const HarukaPanelDesc* desc);
    /// level: 0 info, 1 aviso, 2 error. Sale en la consola del editor.
    void (*log)(void* self, int level, const char* message);
} HarukaEditorHost;

typedef int  (*HarukaEditorPluginInitFn)(const HarukaEditorHost* host);
typedef void (*HarukaEditorPluginShutdownFn)(void);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus) && defined(IMGUI_VERSION)
#include <cstring>
/// ¿El ImGui con el que se compiló QUIEN INCLUYE esto es el del editor? (versión y tamaños)
inline bool harukaImGuiAbiMatches(const HarukaImGuiAbi& a) {
    return std::strcmp(a.version, IMGUI_VERSION) == 0 &&
           a.sizeofIO == (int)sizeof(ImGuiIO) && a.sizeofStyle == (int)sizeof(ImGuiStyle) &&
           a.sizeofDrawVert == (int)sizeof(ImDrawVert) && a.sizeofDrawIdx == (int)sizeof(ImDrawIdx);
}
/// Comprueba y, si casa, fija el contexto y el asignador del editor. Sirve a plugins y backends.
inline bool harukaBindImGui(const HarukaImGuiAbi& a) {
    if (!harukaImGuiAbiMatches(a)) return false;
    ImGui::SetAllocatorFunctions(a.alloc, a.free, a.allocUser);
    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(a.context));
    return true;
}
/**
 * @brief Comprueba que el ImGui del plugin es el del editor y le fija contexto y asignador.
 * Incluir `imgui.h` ANTES que esta cabecera para tenerla. false = el plugin debe rechazarse.
 */
inline bool harukaPluginBindImGui(const HarukaEditorHost* host) { return harukaBindImGui(host->imgui); }
#endif
