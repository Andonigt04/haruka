cmake_minimum_required(VERSION 3.14)
project(HarukaEditor)

# ==== 1. Configuración de Estándares y Optimización ====
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -ffast-math")
endif()

# ==== 2. Selección del Binario del Motor (Intercambiable) ====
# Puedes cambiar estas rutas desde la terminal con:
# cmake .. -DENGINE_SO=/otra/ruta/libHarukaEngine.so -DENGINE_INC=/otra/ruta/include
set(ENGINE_SO "${CMAKE_CURRENT_SOURCE_DIR}/lib64/libHarukaEngine.so" CACHE FILEPATH "Ruta al archivo .so del motor")
set(ENGINE_INC "${CMAKE_CURRENT_SOURCE_DIR}/include/haruka-cpp" CACHE PATH "Ruta a los headers del motor" FORCE)

message(STATUS "-- Motor vinculado: ${ENGINE_SO}")

# Creamos un "alias" importado para el motor
add_library(HarukaEngine SHARED IMPORTED)
set_target_properties(HarukaEngine PROPERTIES
    IMPORTED_LOCATION "${ENGINE_SO}"
    INTERFACE_INCLUDE_DIRECTORIES "${ENGINE_INC}"
)

# ==== 3. Dependencias del Sistema y Fallback ====
find_package(PkgConfig REQUIRED)
find_package(SDL3 REQUIRED)
find_package(Vulkan REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(OpenAL REQUIRED)

# Buscamos ASIO (necesario para la red) y GTK3 (para diálogos de archivos)
find_path(ASIO_INCLUDE_DIR asio.hpp)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

macro(find_editor_dependency NAME HEADER_FILE LOCAL_PATH)
    find_path(${NAME}_INCLUDE_DIR ${HEADER_FILE} PATH_SUFFIXES ${NAME} imgui nfd json glm stb)
    if(NOT ${NAME}_INCLUDE_DIR)
        message(STATUS "-- Usando copia local de ${NAME}")
        set(${NAME}_INCLUDE_DIR "${THIRD_PARTY_DIR}/${LOCAL_PATH}")
    endif()
endmacro()

find_editor_dependency(IMGUI "imgui.h" "imgui")
find_editor_dependency(STB "stb_image.h" "stb")
find_editor_dependency(GLM "glm/glm.hpp" "glm")
find_editor_dependency(NFD "nfd.h" "nativefiledialog/src/include")
find_editor_dependency(NLOHMANN_JSON "nlohmann/json.hpp" "json/include")
find_editor_dependency(IMGUIZMO "ImGuizmo.h" "ImGuizmo")

# ==== 4. Fuentes del Editor y Terceros ====
file(GLOB_RECURSE EDITOR_SOURCES "src/*.cpp")

set(IMGUI_SOURCES
    ${IMGUI_INCLUDE_DIR}/imgui.cpp
    ${IMGUI_INCLUDE_DIR}/imgui_draw.cpp
    ${IMGUI_INCLUDE_DIR}/imgui_tables.cpp
    ${IMGUI_INCLUDE_DIR}/imgui_widgets.cpp
    ${IMGUI_INCLUDE_DIR}/imgui_demo.cpp
    ${IMGUI_INCLUDE_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_INCLUDE_DIR}/backends/imgui_impl_vulkan.cpp
)
set(NFD_SOURCES "${THIRD_PARTY_DIR}/nativefiledialog/src/nfd_common.c" "${THIRD_PARTY_DIR}/nativefiledialog/src/nfd_gtk.c")
set(IMGUIZMO_SOURCES ${IMGUIZMO_INCLUDE_DIR}/ImGuizmo.cpp)

# ==== 5. Ejecutable ====
add_executable(HarukaEditor
    ${EDITOR_SOURCES}
    ${IMGUI_SOURCES}
    ${IMGUIZMO_SOURCES}
    ${NFD_SOURCES}
)

# ==== 6. Configuración de Compilación y Enlace ====

# RPATH: Configura el binario para que busque el .so en su carpeta de origen al ejecutarse
get_filename_component(ENGINE_LIB_DIR "${ENGINE_SO}" DIRECTORY)
set_target_properties(HarukaEditor PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "${ENGINE_LIB_DIR}"
)

target_compile_definitions(HarukaEditor PRIVATE 
    HARUKA_EDITOR 
    HARUKA_HAS_NFD=1 
    ASIO_STANDALONE
)

target_include_directories(HarukaEditor PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    ${ENGINE_INC}
    ${ENGINE_INC}/core
    ${ENGINE_INC}/core/components
    ${CMAKE_CURRENT_SOURCE_DIR}/include/engines/include/engines/haruka-cpp/core
    ${Vulkan_INCLUDE_DIRS}
    ${SDL3_INCLUDE_DIRS}
    ${IMGUI_INCLUDE_DIR}
    ${IMGUI_INCLUDE_DIR}/backends
    ${STB_DIR}
    ${GLM_DIR}
    ${NFD_INCLUDE_DIR}
    ${NLOHMANN_JSON_INCLUDE_DIR}
    ${IMGUIZMO_INCLUDE_DIR}
    ${ASIO_INCLUDE_DIR}
)

target_link_libraries(HarukaEditor PRIVATE
    HarukaEngine
    SDL3::SDL3
    Vulkan::Vulkan
    PkgConfig::GTK3
    OpenSSL::Crypto
    OpenAL::OpenAL
    ${CMAKE_DL_LIBS}
    pthread
    m
)

# ==== 7. Instalación ====
include(GNUInstallDirs)
install(TARGETS HarukaEditor RUNTIME DESTINATION bin)
install(FILES "${ENGINE_SO}" DESTINATION /usr/lib64)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION bin)