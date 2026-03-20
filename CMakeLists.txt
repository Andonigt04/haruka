cmake_minimum_required(VERSION 3.14)
project(HarukaEngine)

set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

set(THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad")
set(GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm")
set(STB_DIR "${THIRD_PARTY_DIR}/stb")
set(IMGUI_DIR "${THIRD_PARTY_DIR}/imgui")
set(WEBSOCKETPP_DIR "${THIRD_PARTY_DIR}/websocketpp")
set(OPENAL_DIR "${THIRD_PARTY_DIR}/OpenAL")
set(IMGUIZMO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ImGuizmo")
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
find_package(PostgreSQL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

include_directories(
  src
  ${GLAD_DIR}/include
  ${GLM_DIR}
  ${STB_DIR}
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
  ${ASSIMP_INCLUDE_DIRS}
  ${WEBSOCKETPP_DIR}
  ${OPENAL_INCLUDE_DIR}
  ${PostgreSQL_INCLUDE_DIRS}
  ${IMGUIZMO_DIR}
  ${NLOHMANN_DIR}
  ${JSON_DIR}/include
  ${NFD_DIR}/src/include
  ${GTK3_INCLUDE_DIRS}
)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/shaders" DESTINATION "${CMAKE_CURRENT_BINARY_DIR}")

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

# Engine sources
file(GLOB_RECURSE ENGINE_SOURCES 
    "src/core/*.cpp" 
    "src/renderer/*.cpp"
    "src/io/*.cpp"
    "src/physics/*.cpp"
    "src/audio/*.cpp"
    "src/game/*.cpp"
)

# Network sources (cliente + servidor)
file(GLOB_RECURSE NETWORK_SOURCES
    "src/network/*.cpp"
)

# Database sources
file(GLOB_RECURSE DATABASE_SOURCES
    "src/database/*.cpp"
)

# Editor sources
file(GLOB_RECURSE EDITOR_SOURCES
    "src/editor/*.cpp"
)

set(GLAD_SOURCE "${GLAD_DIR}/src/glad.c")

# ==== Librería compartida del Motor ====
add_library(HarukaEngineLib SHARED
    ${ENGINE_SOURCES}
    ${GLAD_SOURCE}
    ${IMGUI_SOURCES}
)

target_include_directories(HarukaEngineLib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(HarukaEngineLib PUBLIC
    glfw 
    OpenGL::GL
    assimp::assimp
    OpenSSL::Crypto
    ${OPENAL_LIBRARY}
    ${CMAKE_DL_LIBS}
)

install(TARGETS HarukaEngineLib
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

add_custom_command(TARGET HarukaEngineLib POST_BUILD
    COMMAND mkdir -p "${CMAKE_BINARY_DIR}/lib"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:HarukaEngineLib>"
    "${CMAKE_BINARY_DIR}/lib/$<TARGET_FILE_NAME:HarukaEngineLib>"
    COMMENT "Copying HarukaEngineLib to lib directory"
)

# ==== Build options ====
option(BUILD_EDITOR "Build HarukaEditor" ON)
option(BUILD_RUNTIME "Build HarukaEngine" ON)
option(BUILD_SERVER "Build HarukaServer" ON)

# Runtime ejecutable (CLIENTE)
if(BUILD_RUNTIME)
  add_executable(HarukaEngine 
      src/main.cpp
      ${ENGINE_SOURCES}
      ${NETWORK_SOURCES}
      ${GLAD_SOURCE}
      ${IMGUI_SOURCES}
  )

  target_link_libraries(HarukaEngine 
      glfw 
      OpenGL::GL
      assimp::assimp
      OpenSSL::Crypto
      ${OPENAL_LIBRARY}
      ${CMAKE_DL_LIBS}
  )
endif()

# Editor ejecutable (CLIENTE)
if(BUILD_EDITOR)
  add_executable(HarukaEditor
      src/main_editor.cpp
      ${ENGINE_SOURCES}
      ${EDITOR_SOURCES}
      ${NETWORK_SOURCES}
      ${GLAD_SOURCE}
      ${IMGUI_SOURCES}
      ${IMGUIZMO_SOURCES}
      ${NFD_SOURCES}
  )

  target_link_libraries(HarukaEditor 
      glfw 
      OpenGL::GL
      assimp::assimp
      OpenSSL::Crypto
      ${OPENAL_LIBRARY}
      ${CMAKE_DL_LIBS}
      ${GTK3_LIBRARIES}
  )

  target_compile_options(HarukaEditor PRIVATE ${GTK3_CFLAGS_OTHER})
  target_compile_definitions(HarukaEditor PRIVATE HARUKA_EDITOR HARUKA_HAS_NFD=1)
endif()

# Servidor dedicado (CON DB)
if(BUILD_SERVER)
  add_executable(HarukaServer
      src/main_server.cpp
      ${NETWORK_SOURCES}
      ${DATABASE_SOURCES}
  )

  target_link_libraries(HarukaServer 
      OpenSSL::Crypto
      ${PostgreSQL_LIBRARIES}
      ${CMAKE_DL_LIBS}
  )
  
  target_compile_definitions(HarukaServer PRIVATE HARUKA_SERVER)
endif()

# Chat Test (UDP simple)
add_executable(ChatTest
    src/test/chat_test.cpp
)

target_link_libraries(ChatTest
    ${CMAKE_DL_LIBS}
)

# ==== Install rules ====
include(GNUInstallDirs)

install(TARGETS HarukaEngineLib LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(BUILD_RUNTIME)
  install(TARGETS HarukaEngine RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(BUILD_EDITOR)
  install(TARGETS HarukaEditor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(BUILD_SERVER)
  install(TARGETS HarukaServer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders DESTINATION ${CMAKE_INSTALL_BINDIR})

# ==== CPack ====
set(CPACK_PACKAGE_NAME "HarukaEngine")
set(CPACK_PACKAGE_VERSION "0.1.0")
set(CPACK_GENERATOR "ZIP;DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Haruka")
include(CPack)