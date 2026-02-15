cmake_minimum_required(VERSION 3.14)
project(HarukaEngine)

set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad")
set(GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")
set(STB_DIR "${THIRD_PARTY_DIR}/stb")

find_package(glfw3 REQUIRED)
find_package(assimp REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(
  src
  ${GLAD_DIR}/include
  ${GLM_DIR}
  ${STB_DIR}
  ${ASSIMP_INCLUDE_DIRS}
)



file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/shaders" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.c")
set(GLAD_SOURCE "${GLAD_DIR}/src/glad.c")

add_executable(${PROJECT_NAME} ${SOURCES} ${GLAD_SOURCE})

target_link_libraries(${PROJECT_NAME} 
    glfw 
    OpenGL::GL
    assimp::assimp
    ${CMAKE_DL_LIBS}
)