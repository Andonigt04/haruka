cmake_minimum_required(VERSION 3.14)
project(HarukaEditor VERSION 0.1.0)

set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sanitizador de diagnóstico (mismo flag que el motor y el juego: si solo lo lleva
# una de las partes, no detecta nada útil). Ver la nota en el CMakeLists del motor.
set(HARUKA_SANITIZE "" CACHE STRING "Sanitizador: vacío, 'address' o 'thread'")
if(HARUKA_SANITIZE AND NOT MSVC)
    message(STATUS "Sanitizador ACTIVO: ${HARUKA_SANITIZE}")
    add_compile_options(-fsanitize=${HARUKA_SANITIZE} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${HARUKA_SANITIZE})
endif()

# =====================================================================================
# --- DESCARGA Y COMPILACIÓN AUTOMÁTICA DEL MOTOR (haruka-cpp) ---
# El motor se trae como dependencia (_deps), igual que hace el juego (Survival):
#   * LOCAL por defecto: si junto al editor hay un checkout del motor (../haruka-cpp)
#     se usa ESE (dev: pruebas tus cambios sin pasar por GitHub).
#   * REMOTO si no existe (clon suelto / CI): descarga la rama BRANCH_NAME de GitHub.
# Override manual:
#   Forzar remoto: cmake -B build -DHARUKA_ENGINE_LOCAL=""
#   Otro local:    cmake -B build -DHARUKA_ENGINE_LOCAL=/ruta/al/motor
#   Otra rama:     cmake -B build -DBRANCH_NAME=otra-rama
# =====================================================================================
# El motor SOLO hace falta para el backend de Haruka (src/backends/haruka): el núcleo del editor
# compila su propio ImGui/glad/nfd (third_party/) y no enlaza ningún motor. Con
# -DHARUKA_EDITOR_BACKEND_HARUKA=OFF ni se descarga: sale un editor sin motor.
# =====================================================================================
option(HARUKA_EDITOR_BACKEND_HARUKA "Compila el backend de haruka-cpp (trae el motor)" ON)
if(HARUKA_EDITOR_BACKEND_HARUKA)
include(FetchContent)

# Engine shader output dir consumed by haruka-cpp CMake. Va a bin/assets/shaders para
# que COINCIDA con donde el runtime los busca (AssetPaths::shaders() = "assets/shaders/").
set(SHADER_OUT_DIR "${CMAKE_BINARY_DIR}/bin/assets/shaders" CACHE PATH "Engine SPIR-V output directory")

set(BRANCH_NAME "main" CACHE STRING "Branch/tag del motor a descargar")

# Bitmask de módulos del motor ('' = todos ON). Ver docs/MODULES.md del motor.
set(MODULES "" CACHE STRING "Per-module on/off bitmask, e.g. 101101111")

set(_default_engine_local "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../haruka-cpp/CMakeLists.txt")
    set(_default_engine_local "${CMAKE_CURRENT_SOURCE_DIR}/../haruka-cpp")
endif()
set(HARUKA_ENGINE_LOCAL "${_default_engine_local}" CACHE PATH "Ruta a un motor LOCAL (vacío = descargar de GitHub)")

if(HARUKA_ENGINE_LOCAL)
    get_filename_component(_eng_abs "${HARUKA_ENGINE_LOCAL}" ABSOLUTE)
    message(STATUS "HarukaEngine: LOCAL @ ${_eng_abs}")
    FetchContent_Declare(haruka_engine SOURCE_DIR "${_eng_abs}")
else()
    message(STATUS "HarukaEngine: REMOTO (GitHub @ ${BRANCH_NAME})")
    FetchContent_Declare(
        haruka_engine
        GIT_REPOSITORY https://github.com/Andonigt04/haruka-cpp.git
        GIT_TAG        ${BRANCH_NAME}
    )
endif()

FetchContent_MakeAvailable(haruka_engine)

set(ENGINE_ROOT "${haruka_engine_SOURCE_DIR}")

# ImGuizmo (gizmos del viewport): submódulo git propio del editor; lo usa solo el backend.
set(IMGUIZMO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/src")
if(NOT EXISTS "${IMGUIZMO_DIR}/ImGuizmo.h")
    message(FATAL_ERROR "ImGuizmo no encontrado en ${IMGUIZMO_DIR}. Recuerda: git submodule update --init --recursive")
endif()
endif() # HARUKA_EDITOR_BACKEND_HARUKA

# =====================================================================================
# --- Layout de instalación ---
# Dos formas de instalar, porque son dos cosas distintas:
#   portable (por defecto): todo junto bajo el prefijo (dist/), se copia y se ejecuta.
#   system:                 FHS, lo que necesita un .rpm/.deb (ver docs/PACKAGING.md).
# Va ANTES del target porque las rutas de GNUInstallDirs se compilan dentro del binario.
# =====================================================================================
set(HARUKA_INSTALL_LAYOUT "portable" CACHE STRING "Layout de instalación: portable | system")
set_property(CACHE HARUKA_INSTALL_LAYOUT PROPERTY STRINGS portable system)

# Un paquete de sistema va a /usr: con el /usr/local por defecto de CMake, las rutas
# compiladas (HARUKA_EDITOR_ENGINE_DIR) apuntarían a un sitio donde el rpm no instala nada.
# Solo se pisa el default de CMake; un -DCMAKE_INSTALL_PREFIX explícito se respeta.
if(HARUKA_INSTALL_LAYOUT STREQUAL "system" AND CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()
include(GNUInstallDirs)
# GNUInstallDirs deriva el docdir del nombre del proyecto CMake (HarukaEditor); el paquete
# se llama haruka-editor y /usr/share/doc debe ir con el nombre DEL PAQUETE.
set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/haruka-editor")

# Directorio privado del editor: el ejecutable NO puede vivir en /usr/bin, porque busca sus
# assets, backends y plugins junto al binario, y el backend arrastra su libHarukaEngine.so.
set(HARUKA_PRIVATE_DIR "${CMAKE_INSTALL_LIBDIR}/haruka-editor")

# =====================================================================================
# --- Editor ---
# =====================================================================================
# El editor son DOS cosas que no se enlazan entre sí (ver src/backend/engine_backend.h):
#   * NÚCLEO (src/ menos src/backends/), el ejecutable: ventana, ImGui, menús, paneles genéricos,
#     plugins. Compila SU ImGui/glad/nfd (third_party/, ver VENDORED.md) y no enlaza ningún motor.
#     `check_core_engine_free` comprueba en cada build que no nombra nada del motor.
#   * BACKEND de haruka-cpp (src/backends/haruka/), un .so en bin/backends que el núcleo CARGA
#     al arrancar. Es lo único que enlaza libHarukaEngine.so.
#
# ⚠️ ENABLE_EXPORTS: el ejecutable exporta sus símbolos, y por eso el backend, el motor y los
# plugins usan ESTE ImGui (y glad y nfd) y no las copias de libHarukaEngine.so: el enlazador
# dinámico resuelve primero contra el ejecutable. Un solo contexto ImGui y una sola tabla de
# funciones GL para todos. Requiere la misma versión de ImGui: el backend lo comprueba al cargarse.
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
list(FILTER EDITOR_SOURCES EXCLUDE REGEX "/src/backends/")
# main.cpp se añade explícitamente abajo (el glob lo recogería y duplicaría el símbolo main).
list(REMOVE_ITEM EDITOR_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

set(EDITOR_THIRD_PARTY "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

find_package(SDL3 REQUIRED)
find_package(nlohmann_json 3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)   # nfd_gtk

option(BUILD_EDITOR "Build Haruka Editor" ON)
if(BUILD_EDITOR)
    add_custom_target(check_core_engine_free
        COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/src
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_core_engine_free.cmake
        COMMENT "Comprobando que el nucleo del editor no depende del motor")

    # OBJECT y no STATIC: de una estática solo se enlazan los objetos que el núcleo usa, y lo que
    # use SOLO el motor (p. ej. imgui_tables) se resolvería contra la copia del motor, con OTRO
    # contexto. Así entra entero y el ejecutable exporta ImGui completo.
    add_library(HarukaEditorThirdParty OBJECT
        ${EDITOR_THIRD_PARTY}/imgui/imgui.cpp
        ${EDITOR_THIRD_PARTY}/imgui/imgui_draw.cpp
        ${EDITOR_THIRD_PARTY}/imgui/imgui_tables.cpp
        ${EDITOR_THIRD_PARTY}/imgui/imgui_widgets.cpp
        ${EDITOR_THIRD_PARTY}/imgui/imgui_demo.cpp
        ${EDITOR_THIRD_PARTY}/imgui/backends/imgui_impl_sdl3.cpp
        ${EDITOR_THIRD_PARTY}/imgui/backends/imgui_impl_opengl3.cpp
        ${EDITOR_THIRD_PARTY}/glad/src/glad.c
        ${EDITOR_THIRD_PARTY}/nativefiledialog/src/nfd_common.c
        ${EDITOR_THIRD_PARTY}/nativefiledialog/src/nfd_gtk.c
    )
    target_include_directories(HarukaEditorThirdParty PUBLIC
        ${EDITOR_THIRD_PARTY}/imgui
        ${EDITOR_THIRD_PARTY}/imgui/backends
        ${EDITOR_THIRD_PARTY}/glad/include
        ${EDITOR_THIRD_PARTY}/nativefiledialog/src/include
    )
    target_link_libraries(HarukaEditorThirdParty PUBLIC SDL3::SDL3 PkgConfig::GTK3)

    add_executable(HarukaEditor
        src/main.cpp
        ${EDITOR_SOURCES}
    )
    add_dependencies(HarukaEditor check_core_engine_free)
    target_include_directories(HarukaEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(HarukaEditor PRIVATE
        HarukaEditorThirdParty SDL3::SDL3 PkgConfig::GTK3 nlohmann_json::nlohmann_json ${CMAKE_DL_LIBS})

    target_compile_definitions(HarukaEditor PRIVATE
        HARUKA_EDITOR
        HARUKA_HAS_NFD=1
        # Dónde busca el editor INSTALADO un SDK de motor (lo puebla otro paquete, un enlace
        # o el propio usuario). Absoluto y fijado al configurar, no derivado del ejecutable:
        # contar "../.." desde el binario se rompe en cuanto cambia el libdir (lib vs lib64).
        HARUKA_EDITOR_ENGINE_DIR="${CMAKE_INSTALL_FULL_DATADIR}/haruka-editor/engine"
    )
    if(HARUKA_EDITOR_BACKEND_HARUKA)
        # Motor con el que se compiló: ÚLTIMO recurso de la búsqueda que hace EngineSdk
        # (src/engine_sdk.h), para que el build de desarrollo siga encontrando ../haruka-cpp
        # sin configurar nada. En un editor instalado esta ruta no existe y se descarta.
        target_compile_definitions(HarukaEditor PRIVATE HARUKA_ENGINE_ROOT="${haruka_engine_SOURCE_DIR}")
    endif()

    set_target_properties(HarukaEditor PROPERTIES
        ENABLE_EXPORTS           ON                 # ver arriba: ImGui/glad/nfd para todos
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    if(HARUKA_EDITOR_BACKEND_HARUKA)
        file(GLOB_RECURSE HARUKA_BACKEND_SOURCES CONFIGURE_DEPENDS "src/backends/haruka/*.cpp")
        add_library(haruka_backend MODULE
            ${HARUKA_BACKEND_SOURCES}
            ${IMGUIZMO_DIR}/ImGuizmo.cpp
        )
        target_include_directories(haruka_backend PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/haruka
            ${IMGUIZMO_DIR}
        )
        # Enlaza el motor y NADA del núcleo: lo que use del editor (PanelRegistry,
        # CommandHistory, ImGui) lo resuelve al cargarse contra el ejecutable.
        target_link_libraries(haruka_backend PRIVATE HarukaEngine)
        target_compile_definitions(haruka_backend PRIVATE HARUKA_EDITOR HARUKA_HAS_NFD=1)
        set_target_properties(haruka_backend PROPERTIES
            PREFIX ""                                          # haruka_backend.so
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/backends"
            # libHarukaEngine.so: en la raíz del build al compilar, en ../lib instalado.
            BUILD_RPATH   "${CMAKE_BINARY_DIR}"
            INSTALL_RPATH "$ORIGIN/../lib")
        add_dependencies(HarukaEditor haruka_backend)
    endif()
endif()

# =====================================================================================
# --- Backend remoto + motor simulado ---
# El modelo de cómo se conecta el editor a un motor en OTRO proceso (Unreal, O3DE): protocolo por
# socket en src/backends/remote/remote_protocol.h. No necesita haruka-cpp. El motor simulado
# (tools/mock_engine) implementa el protocolo para probar el editor sin ningún motor instalado.
# Para usarlo: HARUKA_EDITOR_BACKEND=bin/backends/remote_backend.so (el cargador elige por orden
# alfabético y haruka_backend.so va antes).
# =====================================================================================
option(HARUKA_EDITOR_BACKEND_REMOTE "Compila el backend remoto y el motor simulado" ON)
if(BUILD_EDITOR AND HARUKA_EDITOR_BACKEND_REMOTE)
    find_package(Threads REQUIRED)
    add_executable(haruka_mock_engine tools/mock_engine/mock_engine.cpp)
    target_include_directories(haruka_mock_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/remote)
    target_link_libraries(haruka_mock_engine PRIVATE nlohmann_json::nlohmann_json)
    set_target_properties(haruka_mock_engine PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tools")

    add_library(remote_backend MODULE
        src/backends/remote/remote_backend.cpp
        src/backends/remote/remote_client.cpp)
    target_include_directories(remote_backend PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/backends/remote
        ${EDITOR_THIRD_PARTY}/imgui
        ${EDITOR_THIRD_PARTY}/glad/include)
    # Ni motor, ni ImGui, ni glad: ImGui, las funciones GL y PanelRegistry los pone el editor.
    target_link_libraries(remote_backend PRIVATE nlohmann_json::nlohmann_json Threads::Threads)
    set_target_properties(remote_backend PROPERTIES
        PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/backends")
    add_dependencies(HarukaEditor remote_backend haruka_mock_engine)
endif()

# =====================================================================================
# --- Plugin de ejemplo ---
# Plantilla de plugin (src/plugins/haruka_editor_plugin.h). NO enlaza el motor ni ImGui: solo
# necesita `imgui.h` (el MISMO que el editor) y resuelve ImGui al cargarse dentro del editor.
# Va a bin/plugins, que es uno de los directorios donde el editor busca al arrancar.
# =====================================================================================
option(HARUKA_EDITOR_EXAMPLE_PLUGIN "Compila plugins/example_panel en bin/plugins" ON)
if(BUILD_EDITOR AND HARUKA_EDITOR_EXAMPLE_PLUGIN)
    add_library(example_panel MODULE plugins/example_panel/example_panel.cpp)
    target_include_directories(example_panel PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/plugins"
        "${EDITOR_THIRD_PARTY}/imgui")
    set_target_properties(example_panel PROPERTIES
        PREFIX ""                                   # example_panel.so: el nombre es el del plugin
        CXX_VISIBILITY_PRESET hidden                # solo salen los símbolos del contrato
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins")
    add_dependencies(HarukaEditor example_panel)
endif()

# =====================================================================================
# --- Release layout ---
# =====================================================================================
option(RELEASE "Build the distributable release layout" OFF)

# exe · backends/ · plugins/ · lib/libHarukaEngine.so · assets/shaders viajan SIEMPRE juntos y con
# la misma forma relativa: es lo que esperan el INSTALL_RPATH del backend ($ORIGIN/../lib), la
# búsqueda de backends y plugins (junto al exe) y SDL_GetBasePath. Lo único que cambia entre
# layouts es dónde se planta el conjunto.
if(HARUKA_INSTALL_LAYOUT STREQUAL "system")
    set(_haruka_root "${HARUKA_PRIVATE_DIR}")
else()
    set(_haruka_root ".")
endif()

if(BUILD_EDITOR AND RELEASE)
    install(TARGETS HarukaEditor RUNTIME DESTINATION "${_haruka_root}")
    if(HARUKA_EDITOR_BACKEND_HARUKA)
        install(TARGETS haruka_backend LIBRARY DESTINATION "${_haruka_root}/backends")
        install(TARGETS HarukaEngine LIBRARY DESTINATION "${_haruka_root}/lib")
        install(DIRECTORY "${SHADER_OUT_DIR}/" DESTINATION "${_haruka_root}/assets/shaders" OPTIONAL
            FILES_MATCHING
            PATTERN "*.spv"
            PATTERN "*.vert"
            PATTERN "*.frag"
            PATTERN "*.geom"
            PATTERN "*.comp")
    endif()
    if(HARUKA_EDITOR_EXAMPLE_PLUGIN)
        install(TARGETS example_panel LIBRARY DESTINATION "${_haruka_root}/plugins")
    endif()
    if(HARUKA_EDITOR_BACKEND_REMOTE)
        install(TARGETS remote_backend LIBRARY DESTINATION "${_haruka_root}/backends")
        install(TARGETS haruka_mock_engine RUNTIME DESTINATION "${_haruka_root}/tools")
    endif()
endif()

if(BUILD_EDITOR AND RELEASE AND HARUKA_INSTALL_LAYOUT STREQUAL "system")
    # Lanzador de /usr/bin → binario privado. Hace `exec`, así que /proc/self/exe sigue
    # siendo el binario real y los assets se resuelven solos.
    set(PRIVATE_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/haruka-editor")
    set(EXE_NAME "HarukaEditor")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/packaging/haruka-editor.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/haruka-editor" @ONLY)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/haruka-editor"
            DESTINATION "${CMAKE_INSTALL_BINDIR}")

    # Integración con el escritorio: menú, icono y los .hrk asociados al editor.
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/haruka-editor.desktop"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/haruka-editor.svg"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/haruka-editor.xml"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/mime/packages")

    # Plantilla del descriptor de motor: lo que hay que copiar a la raíz de un motor para
    # que este editor sepa usarlo. El SDK en sí no lo trae el paquete (ver docs/PACKAGING.md).
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/packaging/haruka-engine.json"
            DESTINATION "${CMAKE_INSTALL_DOCDIR}")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/PACKAGING.md"
            DESTINATION "${CMAKE_INSTALL_DOCDIR}" OPTIONAL)
endif()

# =====================================================================================
# ==== CPack ====
# `cpack -G RPM` sobre el árbol ya compilado (no compila desde fuente: el motor se trae con
# FetchContent, que necesita red, y mock/COPR construyen sin ella). Ver docs/PACKAGING.md.
# =====================================================================================
set(CPACK_PACKAGE_NAME "haruka-editor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Editor de escenas y juegos para el motor Haruka")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Andonigt04/haruka-cpp")
set(CPACK_PACKAGE_CONTACT "Haruka <andonigt04@gmail.com>")
set(CPACK_GENERATOR "ZIP;DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Haruka")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")

# --- RPM ---
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")
set(CPACK_RPM_PACKAGE_GROUP "Development/Tools")
set(CPACK_RPM_PACKAGE_URL "${CPACK_PACKAGE_HOMEPAGE_URL}")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
# Las dependencias (SDL3, assimp, openal, gtk3…) las deduce rpm del ELF; declararlas a mano
# solo sirve para que se queden desfasadas. Sí hace falta cmake+gcc: el editor COMPILA los
# proyectos del usuario llamando al build.sh del motor, y sin toolchain "Compilar" falla.
set(CPACK_RPM_PACKAGE_REQUIRES "cmake, gcc-c++, make")

# Directorios que YA son de otros paquetes (filesystem, hicolor-icon-theme, shared-mime-info):
# si el rpm los reclama, desinstalarlo intentaría borrarlos y rpm avisa de conflicto. Las rutas
# van ABSOLUTAS: cpack compara contra el filelist ya generado, que es absoluto.
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
    "${CMAKE_INSTALL_FULL_DATADIR}/applications"
    "${CMAKE_INSTALL_FULL_DATADIR}/icons"
    "${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor"
    "${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable"
    "${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor/scalable/apps"
    "${CMAKE_INSTALL_FULL_DATADIR}/mime"
    "${CMAKE_INSTALL_FULL_DATADIR}/mime/packages"
    "${CMAKE_INSTALL_FULL_DATAROOTDIR}/doc")

# libHarukaEngine.so es una lib PRIVADA del editor: vive en su directorio, no en el del
# enlazador. Sin estos filtros el rpm la anunciaría al resto del sistema como si fuera una
# librería pública, y a la vez se exigiría a sí mismo un soname que ningún repo provee.
set(CPACK_RPM_SPEC_MORE_DEFINE
"%define __provides_exclude_from ^${CMAKE_INSTALL_FULL_LIBDIR}/haruka-editor/.*$
%define __requires_exclude ^libHarukaEngine\\\\.so.*$")

# Caché del escritorio: sin esto el lanzador no sale en el menú hasta el siguiente relogin
# y los .hrk no se asocian. `|| :` porque un fallo aquí no debe tumbar la instalación.
set(_haruka_desktop_update "\
update-desktop-database -q &>/dev/null || :
touch --no-create ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor &>/dev/null || :
gtk-update-icon-cache -q ${CMAKE_INSTALL_FULL_DATADIR}/icons/hicolor &>/dev/null || :
update-mime-database ${CMAKE_INSTALL_FULL_DATADIR}/mime &>/dev/null || :
")
file(WRITE "${CMAKE_BINARY_DIR}/rpm_post.sh"   "${_haruka_desktop_update}")
file(WRITE "${CMAKE_BINARY_DIR}/rpm_postun.sh" "${_haruka_desktop_update}")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE   "${CMAKE_BINARY_DIR}/rpm_post.sh")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_BINARY_DIR}/rpm_postun.sh")

include(CPack)
