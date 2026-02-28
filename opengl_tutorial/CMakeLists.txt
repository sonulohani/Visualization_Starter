# =============================================================================
# OpenGL Tutorial — CMake Build System
# =============================================================================
#
# Build instructions:
#   mkdir build && cd build
#   cmake ..
#   make -j$(nproc)
#
# Executables are placed in build/bin/
#
# ---- Installing dependencies (Ubuntu/Debian) ----
#   sudo apt install cmake pkg-config
#   sudo apt install libglfw3-dev libglm-dev libassimp-dev libfreetype-dev
#
# ---- Installing dependencies (Fedora) ----
#   sudo dnf install cmake pkgconfig
#   sudo dnf install glfw-devel glm-devel assimp-devel freetype-devel
#
# ---- Installing dependencies (Arch) ----
#   sudo pacman -S cmake pkgconf glfw glm assimp freetype2
#
# ---- Installing dependencies (macOS with Homebrew) ----
#   brew install cmake glfw glm assimp freetype
#
# ---- GLAD (included in vendor/) ----
#   GLAD is included as source in vendor/glad/. If you need to regenerate it:
#     1. Go to https://glad.dav1d.de/
#     2. Language: C/C++, Specification: OpenGL, Profile: Core, Version: 3.3
#     3. Generate and download
#     4. Place glad.h in vendor/glad/include/glad/
#     5. Place khrplatform.h in vendor/glad/include/KHR/
#     6. Place glad.c in vendor/glad/src/
#
# ---- stb_image (included in vendor/) ----
#   stb_image is a single-header library included in vendor/stb/.
#   Download from: https://github.com/nothings/stb/blob/master/stb_image.h
#
# =============================================================================

cmake_minimum_required(VERSION 3.16)
project(OpenGLTutorial LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Place all executables in build/bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# Dependencies
# =============================================================================

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(assimp REQUIRED)
find_package(Freetype REQUIRED)

# =============================================================================
# GLAD — local OpenGL loader library (compiled from source)
# =============================================================================

add_library(glad STATIC vendor/glad/src/gl.c)
target_include_directories(glad PUBLIC vendor/glad/include)

# =============================================================================
# stb_image — header-only image loading library
#
# We create an INTERFACE library so targets can simply link against it.
# Each executable that uses stb_image must define STB_IMAGE_IMPLEMENTATION
# in exactly one .cpp file before including <stb_image.h>.
# =============================================================================

add_library(stb_image STATIC vendor/stb/stb_image_impl.cpp)
target_include_directories(stb_image PUBLIC vendor/stb)

# =============================================================================
# Shared include directory for our reusable headers (shader.h, camera.h, etc.)
# =============================================================================

add_library(tutorial_common INTERFACE)
target_include_directories(tutorial_common INTERFACE include)
target_link_libraries(tutorial_common INTERFACE
    glad
    stb_image
    glfw
    OpenGL::GL
    glm::glm
)

# =============================================================================
# Helper function: add_tutorial(<name> <source_file> [ASSIMP] [FREETYPE])
#
# Creates an executable linked against the common tutorial libraries.
# Pass ASSIMP or FREETYPE to link those optional dependencies.
# =============================================================================

function(add_tutorial TARGET_NAME SOURCE_FILE)
    cmake_parse_arguments(TUT "ASSIMP;FREETYPE" "" "" ${ARGN})

    add_executable(${TARGET_NAME} ${SOURCE_FILE})
    target_link_libraries(${TARGET_NAME} PRIVATE tutorial_common)

    if(TUT_ASSIMP)
        target_link_libraries(${TARGET_NAME} PRIVATE assimp::assimp)
    endif()

    if(TUT_FREETYPE)
        target_link_libraries(${TARGET_NAME} PRIVATE Freetype::Freetype)
    endif()
endfunction()

# =============================================================================
# Tutorial executables
# =============================================================================

# -- 01: Getting Started ------------------------------------------------------

add_tutorial(01_hello_window
    src/01_getting_started/hello_window/main.cpp)

add_tutorial(02_hello_triangle
    src/01_getting_started/hello_triangle/main.cpp)

add_tutorial(03_shaders
    src/01_getting_started/shaders/main.cpp)

add_tutorial(04_textures
    src/01_getting_started/textures/main.cpp)

add_tutorial(05_transformations
    src/01_getting_started/transformations/main.cpp)

add_tutorial(06_coordinate_systems
    src/01_getting_started/coordinate_systems/main.cpp)

add_tutorial(07_camera
    src/01_getting_started/camera/main.cpp)

add_tutorial(07b_opengl_context
    src/01_getting_started/opengl_context/main.cpp)

# -- 02: Lighting --------------------------------------------------------------

add_tutorial(08_colors
    src/02_lighting/colors/main.cpp)

add_tutorial(09_basic_lighting
    src/02_lighting/basic_lighting/main.cpp)

# -- 03: Model Loading ---------------------------------------------------------

add_tutorial(10_model_loading
    src/03_model_loading/main.cpp
    ASSIMP)
