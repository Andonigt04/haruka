cmake_minimum_required(VERSION 3.14)
project(HarukaEditor)

# ======= 1. Configuración de Rutas Dinámicas =======
# El motor se busca exclusivamente dentro de la carpeta de build del proyecto actual.
# Esto mantiene la raíz del repositorio limpia.
set(HARUKA_ENGINE_DIR "${CMAKE_BINARY_DIR}" CACHE PATH
    "Directorio donde se encuentran las carpetas de versiones del motor")

# Parámetro de versión (ej: cmake -B build -DENGINE_VERSION=1.0.0)
set(ENGINE_VERSION "" CACHE STRING "Versión del motor a usar")

if(ENGINE_VERSION STREQUAL "")
    message(STATUS "ENGINE_VERSION no especificada, escaneando ${HARUKA_ENGINE_DIR}...")
    
    # Busca carpetas con formato de versión (x.y.z) directamente en el build
    file(GLOB _versions RELATIVE "${HARUKA_ENGINE_DIR}" "${HARUKA_ENGINE_DIR}/[0-9]*.[0-9]*.[0-9]*")
    
    if(_versions)
        list(SORT _versions ORDER DESCENDING)
        list(GET _versions 0 ENGINE_VERSION)
    else()
        message(FATAL_ERROR "No se encontró ningún motor en ${HARUKA_ENGINE_DIR}. 
        Copia la carpeta de la versión (ej: 1.0.0/) dentro de build/ antes de configurar.")
    endif()
endif()

set(ENGINE_PATH "${HARUKA_ENGINE_DIR}/${ENGINE_VERSION}")
message(STATUS "Usando HarukaEngine v${ENGINE_VERSION} desde: ${ENGINE_PATH}")

# ======= 2. Dependencias del Sistema =======
find_package(PkgConfig REQUIRED)
find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(assimp REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(OpenAL REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# ======= 3. Localización de Componentes del Motor (Estructura x.x.x/...) =======

# Librería (.so) en x.x.x/bin/
find_library(HARUKA_ENGINE_LIBRARY
    NAMES HarukaEngine
    PATHS "${ENGINE_PATH}/bin"
    NO_DEFAULT_PATH
    REQUIRED)

# Headers en x.x.x/lib64/haruka-cpp/
find_path(HARUKA_ENGINE_INCLUDE_DIR
    NAMES core/application.h
    PATHS "${ENGINE_PATH}/lib64/haruka-cpp"
    NO_DEFAULT_PATH
    REQUIRED)

# Shaders en x.x.x/bin/shaders/
set(HARUKA_ENGINE_SHADERS_DIR "${ENGINE_PATH}/bin/shaders")

# ======= 4. Configuración de Compilación =======
set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

include_directories(
    ${HARUKA_ENGINE_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    "${THIRD_PARTY_DIR}/glad/include"
    "${THIRD_PARTY_DIR}/imgui"
    "${THIRD_PARTY_DIR}/imgui/backends"
    "${THIRD_PARTY_DIR}/ImGuizmo"
    "${THIRD_PARTY_DIR}/json/include"
    "${THIRD_PARTY_DIR}/nativefiledialog/src/include"
    ${GTK3_INCLUDE_DIRS}
)

# ======= 5. Fuentes y Target =======
set(IMGUI_SOURCES
    ${THIRD_PARTY_DIR}/imgui/imgui.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_demo.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_sdl3.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)

set(NFD_SOURCES
    ${THIRD_PARTY_DIR}/nativefiledialog/src/nfd_common.c
    ${THIRD_PARTY_DIR}/nativefiledialog/src/nfd_gtk.c
)

file(GLOB_RECURSE EDITOR_SOURCES "src/*.cpp")

add_executable(HarukaEditor
    ${EDITOR_SOURCES}
    ${IMGUI_SOURCES}
    ${NFD_SOURCES}
    "${THIRD_PARTY_DIR}/ImGuizmo/ImGuizmo.cpp"
    "${THIRD_PARTY_DIR}/glad/src/glad.c"
)

target_link_libraries(HarukaEditor PRIVATE
    ${HARUKA_ENGINE_LIBRARY}
    ${GTK3_LIBRARIES}
    SDL3::SDL3
    OpenGL::GL
    assimp::assimp
    OpenSSL::Crypto
    OpenAL::OpenAL
    ${CMAKE_DL_LIBS}
    pthread
    m
)

# RPATH para que el ejecutable encuentre el .so en su misma carpeta al ejecutarse
set_target_properties(HarukaEditor PROPERTIES
    BUILD_RPATH  "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

# ======= 6. Post-Build: Sincronización =======
# Copiamos la librería y shaders al nivel del ejecutable (HarukaEditor/) 
# para que la ejecución sea independiente de la carpeta de versión.
add_custom_command(TARGET HarukaEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${HARUKA_ENGINE_LIBRARY}"
        "$<TARGET_FILE_DIR:HarukaEditor>/libHarukaEngine.so"
    COMMENT "Desplegando motor v${ENGINE_VERSION}..."
)

if(EXISTS "${HARUKA_ENGINE_SHADERS_DIR}")
    add_custom_command(TARGET HarukaEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${HARUKA_ENGINE_SHADERS_DIR}"
            "$<TARGET_FILE_DIR:HarukaEditor>/shaders"
        COMMENT "Desplegando shaders del motor..."
    )
endif()