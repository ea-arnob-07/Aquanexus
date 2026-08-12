
cmake_minimum_required(VERSION 3.20)
project(AquaVillage3D_Cinematic VERSION 4.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(AQUA_SOURCES
    src/main.cpp
    src/App.cpp
    src/Camera.cpp
    src/Mesh.cpp
    src/Scene.cpp
    src/Shader.cpp
    src/Simulation.cpp
    src/UI.cpp
)

add_executable(AquaVillage3D_Cinematic ${AQUA_SOURCES})

# Keep GLSL as real runtime assets (same architecture style as custom OpenGL engines).
add_custom_command(TARGET AquaVillage3D_Cinematic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:AquaVillage3D_Cinematic>/shaders)
target_include_directories(AquaVillage3D_Cinematic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(AquaVillage3D_Cinematic PRIVATE GLM_ENABLE_EXPERIMENTAL NOMINMAX)

if(MSVC)
    target_compile_options(AquaVillage3D_Cinematic PRIVATE /W4 /permissive-)
else()
    target_compile_options(AquaVillage3D_Cinematic PRIVATE -Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(AquaVillage3D_Cinematic PRIVATE -O3)
    endif()
endif()

if(EMSCRIPTEN)
    find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp)
    if(NOT GLM_INCLUDE_DIR)
        message(FATAL_ERROR "GLM headers not found. Set -DGLM_INCLUDE_DIR=/path/to/glm")
    endif()
    target_include_directories(AquaVillage3D_Cinematic PRIVATE ${GLM_INCLUDE_DIR})
    target_link_options(AquaVillage3D_Cinematic PRIVATE
        "-sUSE_GLFW=3"
        "-sFULL_ES3=1"
        "-sMIN_WEBGL_VERSION=2"
        "-sMAX_WEBGL_VERSION=2"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sASSERTIONS=0"
        "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/shaders@/shaders"
        "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
    set_target_properties(AquaVillage3D_Cinematic PROPERTIES SUFFIX ".html")
else()
    find_package(OpenGL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
    pkg_check_modules(GLEW REQUIRED IMPORTED_TARGET glew)
    find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp REQUIRED)
    target_include_directories(AquaVillage3D_Cinematic PRIVATE ${GLM_INCLUDE_DIR})
    target_link_libraries(AquaVillage3D_Cinematic PRIVATE OpenGL::GL PkgConfig::GLFW3 PkgConfig::GLEW)
    if(MINGW)
        target_link_options(AquaVillage3D_Cinematic PRIVATE -Wl,--enable-auto-import)
    endif()
endif()

set_target_properties(AquaVillage3D_Cinematic PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

install(TARGETS AquaVillage3D_Cinematic RUNTIME DESTINATION .)
