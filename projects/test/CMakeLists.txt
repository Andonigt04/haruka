cmake_minimum_required(VERSION 3.14)
project(TestGame)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Rutas
set(ENGINE_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../build")
set(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# Buscar librería en múltiples ubicaciones
find_library(HarukaEngineLib_LIBRARY 
    NAMES HarukaEngineLib
    PATHS 
        ${ENGINE_BUILD_DIR}
        ${ENGINE_BUILD_DIR}/lib
        ${ENGINE_BUILD_DIR}/CMakeFiles/HarukaEngineLib.dir
    NO_DEFAULT_PATH
)

if(NOT HarukaEngineLib_LIBRARY)
    message(FATAL_ERROR "HarukaEngineLib not found. Compile the engine first.")
endif()

find_path(HarukaEngineLib_INCLUDE_DIR NAMES core/scene.h PATHS ${ENGINE_ROOT}/src NO_DEFAULT_PATH REQUIRED)

include_directories(
    ${HarukaEngineLib_INCLUDE_DIR}
    ${ENGINE_ROOT}/third_party/glm
    ${ENGINE_ROOT}/third_party/json/include
    ${ENGINE_ROOT}/third_party/glad/include
)

file(GLOB_RECURSE PROJECT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.cpp"
)

# Remover init_test.cpp del build normal
list(REMOVE_ITEM PROJECT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/scripts/init_test.cpp")

add_library(Test SHARED ${PROJECT_SOURCES})
target_link_libraries(Test PRIVATE ${HarukaEngineLib_LIBRARY})