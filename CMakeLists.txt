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

# El editor linka el TARGET del motor: hereda sus include-dirs (glm, imgui, glad, json, stb,
# nfd, core/renderer) y sus compile-definitions PUBLIC (GLM_FORCE_DEPTH_ZERO_TO_ONE), y NO
# recompila imgui/glad/nfd: el motor ya los compila en libHarukaEngine.so y los exporta.
set(HarukaEngine_LIBRARY HarukaEngine)

# ImGuizmo (gizmos del viewport): submódulo git propio del editor; el motor no lo trae.
set(IMGUIZMO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo/src")
if(NOT EXISTS "${IMGUIZMO_DIR}/ImGuizmo.h")
    message(FATAL_ERROR "ImGuizmo no encontrado en ${IMGUIZMO_DIR}. Recuerda: git submodule update --init --recursive")
endif()

# =====================================================================================
# --- Editor ---
# =====================================================================================
file(GLOB_RECURSE EDITOR_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
# main.cpp se añade explícitamente abajo (el glob lo recogería y duplicaría el símbolo main).
list(REMOVE_ITEM EDITOR_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# SDL3 explícito para el ejecutable (el motor lo enlaza PUBLIC de todas formas).
find_package(SDL3 QUIET)

option(BUILD_EDITOR "Build Haruka Editor" ON)
if(BUILD_EDITOR)
    add_executable(HarukaEditor
        src/main.cpp
        ${EDITOR_SOURCES}
        ${IMGUIZMO_DIR}/ImGuizmo.cpp
    )

    target_include_directories(HarukaEditor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${IMGUIZMO_DIR}
    )

    target_link_libraries(HarukaEditor PRIVATE ${HarukaEngine_LIBRARY})
    if(SDL3_FOUND)
        target_link_libraries(HarukaEditor PRIVATE SDL3::SDL3)
    endif()

    target_compile_definitions(HarukaEditor PRIVATE
        HARUKA_EDITOR
        HARUKA_HAS_NFD=1
        # Ruta del motor usada por el editor: la inyecta en los proyectos que crea
        # (template del motor) para que compilen contra el MISMO motor sin descargar nada.
        HARUKA_ENGINE_ROOT="${haruka_engine_SOURCE_DIR}"
    )

    # bin/ contiene el exe, libHarukaEngine.so queda en la raíz del build (variable de salida
    # del motor) y assets/shaders/ (shaders SPIR-V del motor). BUILD_RPATH: el exe está en
    # bin/, el .so en la raíz → ambos en el rpath para correr sin LD_LIBRARY_PATH.
    set_target_properties(HarukaEditor PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        BUILD_RPATH              "$ORIGIN:${CMAKE_BINARY_DIR}"
        INSTALL_RPATH            "$ORIGIN:$ORIGIN/lib"
    )
endif()

# =====================================================================================
# --- Release layout ---
# =====================================================================================
option(RELEASE "Build the distributable release layout" OFF)
include(GNUInstallDirs)

if(BUILD_EDITOR AND RELEASE)
    install(TARGETS HarukaEditor RUNTIME DESTINATION .)
    install(TARGETS HarukaEngine LIBRARY DESTINATION lib)
    install(DIRECTORY "${SHADER_OUT_DIR}/" DESTINATION assets/shaders OPTIONAL
        FILES_MATCHING
        PATTERN "*.spv"
        PATTERN "*.vert"
        PATTERN "*.frag"
        PATTERN "*.geom"
        PATTERN "*.comp")
endif()

# ==== CPack ====
set(CPACK_PACKAGE_NAME "HarukaEditor")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_GENERATOR "ZIP;DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Haruka")
include(CPack)
