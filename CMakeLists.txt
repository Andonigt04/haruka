cmake_minimum_required(VERSION 3.14)
project(HarukaEditor)

# ======= 1. Engine Location =======
# HARUKA_ENGINE_DIR: parent of the versioned engine folders.
# Each installed engine version lives under a x.x.x/ subdirectory.
# Default: build/ — run `cmake --install` for haruka-cpp into build/<version>
set(HARUKA_ENGINE_DIR "${CMAKE_BINARY_DIR}" CACHE PATH
    "Directory that contains versioned engine folders (e.g. build/0.3.0/)")

# ENGINE_VERSION: pick a specific version, or leave empty to auto-select latest.
set(ENGINE_VERSION "" CACHE STRING "Engine version to use (e.g. 0.3.0)")

if(ENGINE_VERSION STREQUAL "")
    message(STATUS "ENGINE_VERSION not set, scanning ${HARUKA_ENGINE_DIR} ...")
    file(GLOB _versions RELATIVE "${HARUKA_ENGINE_DIR}"
         "${HARUKA_ENGINE_DIR}/[0-9]*.[0-9]*.[0-9]*")
    if(_versions)
        list(SORT _versions)      # lexicographic ascending
        list(REVERSE _versions)   # newest first
        list(GET _versions 0 ENGINE_VERSION)
    else()
        message(FATAL_ERROR
            "No engine version found under ${HARUKA_ENGINE_DIR}.\n"
            "Install haruka-cpp first:\n"
            "  cmake --install haruka-cpp/build --prefix haruka/build/<version>")
    endif()
endif()

set(ENGINE_PATH "${HARUKA_ENGINE_DIR}/${ENGINE_VERSION}")
message(STATUS "HarukaEngine v${ENGINE_VERSION} -> ${ENGINE_PATH}")

# ======= 2. System Dependencies =======
set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(SDL3     REQUIRED)
find_package(OpenGL   REQUIRED)
find_package(assimp   REQUIRED)
find_package(OpenSSL  REQUIRED)
find_package(OpenAL   REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /Ot /GL /fp:fast /arch:AVX2")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto -ffast-math")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-O3 -flto")
endif()

# ======= 3. Engine Components (layout: x.x.x/bin/, x.x.x/lib64/) =======

# .so -> x.x.x/bin/
find_library(HARUKA_ENGINE_LIBRARY
    NAMES HarukaEngine
    PATHS "${ENGINE_PATH}/bin"
    NO_DEFAULT_PATH
    REQUIRED)
message(STATUS "Engine library  : ${HARUKA_ENGINE_LIBRARY}")

# Headers -> x.x.x/lib64/haruka-cpp/
find_path(HARUKA_ENGINE_INCLUDE_DIR
    NAMES core/application.h
    PATHS "${ENGINE_PATH}/lib64/haruka-cpp"
    NO_DEFAULT_PATH
    REQUIRED)
message(STATUS "Engine headers  : ${HARUKA_ENGINE_INCLUDE_DIR}")

# Shaders -> x.x.x/bin/shaders/
set(HARUKA_ENGINE_SHADERS_DIR "${ENGINE_PATH}/bin/shaders")
if(NOT EXISTS "${HARUKA_ENGINE_SHADERS_DIR}")
    message(WARNING
        "Engine shaders not found: ${HARUKA_ENGINE_SHADERS_DIR}\n"
        "Build haruka-cpp before running the editor.")
endif()

# ======= 4. Third-Party Paths =======
set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

include_directories(
    ${HARUKA_ENGINE_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    "${THIRD_PARTY_DIR}/glad/include"
    "${THIRD_PARTY_DIR}/glm"
    "${THIRD_PARTY_DIR}/stb"
    "${THIRD_PARTY_DIR}/imgui"
    "${THIRD_PARTY_DIR}/imgui/backends"
    "${THIRD_PARTY_DIR}/ImGuizmo"
    "${THIRD_PARTY_DIR}/json/include"
    "${THIRD_PARTY_DIR}/nativefiledialog/src/include"
    ${GTK3_INCLUDE_DIRS}
)

# ======= 5. Sources =======
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

# ======= 6. Editor Executable =======
# Binary goes into the same versioned bin/ so SDL_GetBasePath() resolves shaders.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${ENGINE_PATH}/bin")

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
    ${OPENAL_LIBRARY}
    ${CMAKE_DL_LIBS}
    pthread
    m
)

target_compile_options(HarukaEditor PRIVATE ${GTK3_CFLAGS_OTHER})
target_compile_definitions(HarukaEditor PRIVATE HARUKA_EDITOR HARUKA_HAS_NFD=1)

# Find libHarukaEngine.so next to the binary at runtime.
set_target_properties(HarukaEditor PROPERTIES
    BUILD_RPATH   "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

# ======= 7. Post-Build: Deploy to bin/ =======
# The binary already lands in ENGINE_PATH/bin via CMAKE_RUNTIME_OUTPUT_DIRECTORY.
# Copy shaders there too so SDL_GetBasePath()+shaders/<name>.spv works.
add_custom_command(TARGET HarukaEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${HARUKA_ENGINE_SHADERS_DIR}"
        "$<TARGET_FILE_DIR:HarukaEditor>/shaders"
    COMMENT "Deploying shaders -> ${ENGINE_VERSION}/bin/shaders/"
)

# ==== Install ====
include(GNUInstallDirs)
install(TARGETS HarukaEditor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${HARUKA_ENGINE_LIBRARY}" DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY "${HARUKA_ENGINE_SHADERS_DIR}/"
        DESTINATION "${CMAKE_INSTALL_BINDIR}/shaders"
        FILES_MATCHING PATTERN "*.spv")

# ==== CPack ====
set(CPACK_PACKAGE_NAME    "HarukaEditor")
set(CPACK_PACKAGE_VERSION "${ENGINE_VERSION}")
set(CPACK_GENERATOR       "ZIP;DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Haruka")
include(CPack)
