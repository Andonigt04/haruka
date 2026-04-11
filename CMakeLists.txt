cmake_minimum_required(VERSION 3.14)
project(HarukaEditor)

# ==== Selección y compilación forzada del motor ====
set(HARUKA_ENGINE_INCLUDE_DIR "" CACHE PATH "Ruta a los headers públicos del motor HarukaEngine")
set(HARUKA_ENGINE_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/lib64/libHarukaEngine.so" CACHE FILEPATH "Ruta a la librería HarukaEngine.so/.a" FORCE)

file(GLOB ENGINE_INCLUDE_CANDIDATES "${CMAKE_CURRENT_SOURCE_DIR}/include/engines/*")
set(_found_engine_include "")
foreach(_candidate ${ENGINE_INCLUDE_CANDIDATES})
    if(EXISTS "${_candidate}/core/application.h")
        set(_found_engine_include "${_candidate}")
        break()
    endif()
endforeach()
if(_found_engine_include)
    set(HARUKA_ENGINE_INCLUDE_DIR "${_found_engine_include}" CACHE PATH "Ruta a los headers públicos del motor HarukaEngine" FORCE)
    message(STATUS "HARUKA_ENGINE_INCLUDE_DIR no definido, usando: ${HARUKA_ENGINE_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "No se encontró ninguna carpeta de engine válida (con core/application.h) en include/engines. Define HARUKA_ENGINE_INCLUDE_DIR manualmente.")
endif()



set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== Optimization Flags =====
if(MSVC)
    # Visual Studio optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Oi /Ot /Oy /GL /fp:fast /arch:AVX2")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /OPT:REF /OPT:ICF")
else()
    # GCC/Clang optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -mtune=native -flto -ffast-math -fno-signed-zeros -fno-trapping-math")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-O3 -flto")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-O3 -flto")
endif()

# Default to Release build
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad")
set(GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")
set(STB_DIR "${THIRD_PARTY_DIR}/stb")
set(IMGUI_DIR "${THIRD_PARTY_DIR}/imgui")
set(OPENAL_DIR "${THIRD_PARTY_DIR}/OpenAL")
set(IMGUIZMO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo")
set(GLAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad)
set(JSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/json")
set(NFD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/nativefiledialog")

set(NFD_SOURCES
    ${NFD_DIR}/src/nfd_common.c
    ${NFD_DIR}/src/nfd_gtk.c
)

set(NFD_INCLUDE_DIRS "${NFD_DIR}/src/include")
set(GTK3_INCLUDE_DIRS ${GTK3_INCLUDE_DIRS})

find_package(glfw3 REQUIRED)
find_package(assimp REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(OpenAL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(
    ${HARUKA_ENGINE_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLAD_DIR}/include
    ${GLM_DIR}
    ${STB_DIR}
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${ASSIMP_INCLUDE_DIRS}
    ${OPENAL_INCLUDE_DIR}
    ${IMGUIZMO_DIR}
    ${NLOHMANN_DIR}
    ${JSON_DIR}/include
    ${NFD_DIR}/src/include
    ${GTK3_INCLUDE_DIRS}
)

#file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/shaders" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# ImGui sources
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# ImGuiZmo sources
set(IMGUIZMO_SOURCES
    ${IMGUIZMO_DIR}/ImGuizmo.cpp
)

# Editor sources
file(GLOB_RECURSE EDITOR_SOURCES "src/*.cpp")

# Editor ejecutable
option(BUILD_EDITOR "Build Haruka" ON)
if(BUILD_EDITOR)
    add_executable(HarukaEditor
        src/main.cpp
        ${EDITOR_SOURCES}
        ${IMGUIZMO_SOURCES}
        ${IMGUI_SOURCES}
        ${NFD_SOURCES}
        ${GLAD_DIR}/src/glad.c
    )

    # Enlaza contra la librería precompilada del motor
    target_link_libraries(HarukaEditor 
        PRIVATE ${HARUKA_ENGINE_LIBRARY}
        ${GTK3_LIBRARIES}
        glfw
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
endif()


# ==== Install rules ====
include(GNUInstallDirs)

if(BUILD_EDITOR)
    install(TARGETS HarukaEditor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_INSTALL_BINDIR})

# ==== CPack ====
set(CPACK_PACKAGE_NAME "HarukaEditor")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_GENERATOR "ZIP;DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Haruka")
include(CPack)