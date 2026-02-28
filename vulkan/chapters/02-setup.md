# Chapter 2: Development Environment Setup

[<< Previous: Introduction](01-introduction.md) | [Next: Vulkan Instance >>](03-instance.md)

---

## What We Need

To write Vulkan applications, we need these tools:

| Tool | What It Does | Why We Need It |
|------|-------------|----------------|
| **Vulkan SDK** | Headers, libraries, validation layers, shader compiler | Core Vulkan development |
| **GLFW** | Creates windows, handles keyboard/mouse input | Vulkan doesn't create windows |
| **GLM** | Math library (vectors, matrices) | 3D transformations |
| **C++ Compiler** | Compiles our code | GCC, Clang, or MSVC |
| **CMake** | Build system | Manages compilation across platforms |
| **glslc** | Shader compiler (GLSL → SPIR-V) | Comes with the Vulkan SDK |

### Why GLFW?

Vulkan is a **pure graphics/compute API** — it knows nothing about windows, keyboards, or mice. That's by design (it keeps Vulkan platform-independent). GLFW is a lightweight library that:

- Creates a window on any OS (Windows, Linux, macOS)
- Handles keyboard and mouse input
- Creates the Vulkan surface (the connection between Vulkan and the window)

### Why GLM?

Graphics programming needs a lot of math — vectors (positions, directions), matrices (transformations), and operations like dot products and cross products. GLM (OpenGL Mathematics) provides all of this with a syntax that matches GLSL shaders.

```cpp
// GLM example — looks just like shader code
glm::vec3 position(1.0f, 2.0f, 3.0f);
glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0, 1, 0));
glm::vec4 transformed = rotation * glm::vec4(position, 1.0f);
```

---

## Linux Setup (Ubuntu / Debian)

### Step 1: Install the Vulkan SDK

```bash
# Add the LunarG repository
wget -qO- https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo tee /etc/apt/trusted.gpg.d/lunarg.asc
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list \
    https://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list

# Update and install
sudo apt update
sudo apt install vulkan-sdk
```

### Step 2: Install Dependencies

```bash
sudo apt install libglfw3-dev libglm-dev cmake build-essential
```

### Step 3: Verify Installation

```bash
# Check Vulkan is working
vulkaninfo --summary

# You should see something like:
# GPU0: NVIDIA GeForce RTX 3080 (type: DISCRETE_GPU)
# apiVersion = 1.3.xxx
# driverVersion = xxx.xx.xx
```

If `vulkaninfo` shows your GPU, you're good to go!

### Troubleshooting Linux

| Problem | Solution |
|---------|----------|
| `vulkaninfo` not found | Reinstall vulkan-sdk, check PATH |
| No GPU listed | Install your GPU's Vulkan driver (nvidia-driver-xxx or mesa-vulkan-drivers) |
| "Cannot create Vulkan instance" | `sudo apt install mesa-vulkan-drivers` for Intel/AMD integrated GPUs |

---

## Windows Setup

### Step 1: Download the Vulkan SDK

1. Go to [vulkan.lunarg.com/sdk/home](https://vulkan.lunarg.com/sdk/home)
2. Download the latest Windows installer
3. Run the installer — **check all components**
4. The installer sets environment variables automatically (`VULKAN_SDK`, `VK_SDK_PATH`)

### Step 2: Install GLFW

**Option A: vcpkg (recommended)**
```powershell
# If you don't have vcpkg:
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Install GLFW:
.\vcpkg install glfw3:x64-windows
.\vcpkg integrate install
```

**Option B: Manual download**
1. Go to [glfw.org/download](https://www.glfw.org/download.html)
2. Download the pre-compiled Windows binaries (64-bit)
3. Extract to a known location (e.g., `C:\Libraries\glfw`)

### Step 3: Install GLM

GLM is **header-only** — no compilation needed.

```powershell
# With vcpkg:
.\vcpkg install glm:x64-windows

# Or download from GitHub and extract anywhere
```

### Step 4: Verify

Open a command prompt:
```powershell
# Run the Vulkan info tool
vulkaninfo --summary
```

---

## macOS Setup

macOS doesn't natively support Vulkan, but **MoltenVK** translates Vulkan calls to Apple's Metal API.

```bash
# Install via Homebrew
brew install vulkan-sdk glfw glm cmake

# Verify
vulkaninfo --summary
```

> **Note**: MoltenVK doesn't support 100% of Vulkan features, but it covers everything in this tutorial.

---

## Project Structure

Let's create a clean project structure that we'll use throughout the tutorial:

```
vulkan-tutorial/
├── CMakeLists.txt          ← Build configuration
├── src/
│   └── main.cpp            ← Our application code
├── shaders/
│   ├── shader.vert         ← Vertex shader (GLSL)
│   ├── shader.frag         ← Fragment shader (GLSL)
│   └── compile.sh          ← Script to compile shaders
├── textures/               ← Texture images (later chapters)
├── models/                 ← 3D models (later chapters)
└── build/                  ← Build output (created by CMake)
```

### Create the Structure

```bash
mkdir -p vulkan-tutorial/{src,shaders,textures,models,build}
cd vulkan-tutorial
```

---

## CMakeLists.txt

This is the build configuration file. It tells CMake how to find Vulkan, GLFW, and GLM, and how to compile our code.

```cmake
cmake_minimum_required(VERSION 3.16)
project(VulkanTutorial LANGUAGES CXX)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Vulkan REQUIRED)
find_package(glfw3 3.3 REQUIRED)

# Create our executable
add_executable(VulkanApp src/main.cpp)

# Link libraries
target_link_libraries(VulkanApp PRIVATE
    Vulkan::Vulkan
    glfw
)

# Include GLM (header-only, usually found automatically)
# If GLM isn't found, uncomment and set the path:
# target_include_directories(VulkanApp PRIVATE /path/to/glm)
```

### What Each Line Means

| Line | Purpose |
|------|---------|
| `cmake_minimum_required(VERSION 3.16)` | Requires CMake 3.16+ (widely available) |
| `project(VulkanTutorial LANGUAGES CXX)` | Names the project, specifies C++ |
| `set(CMAKE_CXX_STANDARD 17)` | We use C++17 features (`std::optional`, `if constexpr`) |
| `find_package(Vulkan REQUIRED)` | Finds the Vulkan SDK on your system |
| `find_package(glfw3 3.3 REQUIRED)` | Finds GLFW 3.3+ |
| `target_link_libraries(...)` | Links Vulkan and GLFW to our program |

---

## The "Hello Vulkan" Test

Let's write a minimal program to verify everything is connected properly.

### src/main.cpp

```cpp
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <cstdlib>    // EXIT_SUCCESS, EXIT_FAILURE

int main() {
    // ─── Step 1: Initialize GLFW ───
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!\n";
        return EXIT_FAILURE;
    }

    // Check Vulkan support
    if (!glfwVulkanSupported()) {
        std::cerr << "GLFW: Vulkan is not supported!\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::cout << "GLFW initialized. Vulkan is supported!\n";

    // ─── Step 2: List Vulkan Extensions ───
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    std::cout << "\nAvailable Vulkan extensions (" << extensionCount << "):\n";
    for (const auto& ext : extensions) {
        std::cout << "  - " << ext.extensionName
                  << " (version " << ext.specVersion << ")\n";
    }

    // ─── Step 3: List Required Extensions for Window ───
    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);

    std::cout << "\nGLFW requires these extensions (" << glfwExtCount << "):\n";
    for (uint32_t i = 0; i < glfwExtCount; i++) {
        std::cout << "  - " << glfwExts[i] << "\n";
    }

    // ─── Step 4: Create a Test Window ───
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // No OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan Test", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window!\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::cout << "\nWindow created successfully!\n";
    std::cout << "Close the window or press ESC to exit.\n";

    // ─── Step 5: Main Loop ───
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    // ─── Cleanup ───
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Goodbye!\n";
    return EXIT_SUCCESS;
}
```

---

## Building and Running

### Linux / macOS

```bash
cd vulkan-tutorial

# Create build directory and generate build files
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Compile
cmake --build build

# Run
./build/VulkanApp
```

### Windows (Visual Studio)

```powershell
cd vulkan-tutorial

# Generate Visual Studio project
cmake -B build -G "Visual Studio 17 2022" -A x64

# Build
cmake --build build --config Debug

# Run
.\build\Debug\VulkanApp.exe
```

### Expected Output

```
GLFW initialized. Vulkan is supported!

Available Vulkan extensions (18):
  - VK_KHR_device_group_creation (version 1)
  - VK_KHR_external_fence_capabilities (version 1)
  - VK_KHR_external_memory_capabilities (version 1)
  - VK_KHR_external_semaphore_capabilities (version 1)
  - VK_KHR_get_physical_device_properties2 (version 2)
  - VK_KHR_get_surface_capabilities2 (version 1)
  - VK_KHR_surface (version 25)
  ... (more extensions) ...

GLFW requires these extensions (2):
  - VK_KHR_surface
  - VK_KHR_xcb_surface

Window created successfully!
Close the window or press ESC to exit.
```

A blank window should appear. If you see extension listings and the window opens, **your environment is working correctly!**

---

## Understanding the Vulkan Enumeration Pattern

Notice this pattern in our test code:

```cpp
// Step 1: Ask "how many?"
uint32_t count = 0;
vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

// Step 2: Allocate space
std::vector<VkExtensionProperties> items(count);

// Step 3: Ask "give me the data"
vkEnumerateInstanceExtensionProperties(nullptr, &count, items.data());
```

This **two-call pattern** appears everywhere in Vulkan:
1. First call with `nullptr` → returns the **count**
2. Second call with allocated array → fills in the **data**

You'll see this dozens of times. It exists because Vulkan can't return dynamically-sized arrays (it's a C API), so you must allocate the memory yourself.

---

## Shader Compilation Script

We'll need this in later chapters. Create it now:

### shaders/compile.sh (Linux/macOS)

```bash
#!/bin/bash
# Compile all GLSL shaders to SPIR-V

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Compiling shaders..."

for shader in "$SCRIPT_DIR"/*.vert "$SCRIPT_DIR"/*.frag "$SCRIPT_DIR"/*.comp; do
    if [ -f "$shader" ]; then
        output="${shader%.*}.spv"
        echo "  $shader → $output"
        glslc "$shader" -o "$output"
        if [ $? -ne 0 ]; then
            echo "  ERROR: Failed to compile $shader"
            exit 1
        fi
    fi
done

echo "Done!"
```

```bash
chmod +x shaders/compile.sh
```

### shaders/compile.bat (Windows)

```batch
@echo off
echo Compiling shaders...

for %%f in (*.vert *.frag *.comp) do (
    echo   %%f -^> %%~nf.spv
    glslc %%f -o %%~nf.spv
    if errorlevel 1 (
        echo   ERROR: Failed to compile %%f
        exit /b 1
    )
)

echo Done!
```

---

## Common Setup Problems and Solutions

### "CMake can't find Vulkan"

```bash
# Check if VULKAN_SDK is set
echo $VULKAN_SDK

# If empty, set it manually (Linux example):
export VULKAN_SDK=/path/to/vulkan-sdk/x86_64
export PATH=$VULKAN_SDK/bin:$PATH
export LD_LIBRARY_PATH=$VULKAN_SDK/lib:$LD_LIBRARY_PATH

# Then retry CMake
cmake -B build
```

### "CMake can't find GLFW"

```bash
# Linux: install the dev package
sudo apt install libglfw3-dev

# Or tell CMake where to find it:
cmake -B build -DGLFW3_DIR=/path/to/glfw/lib/cmake/glfw3
```

### "GLM not found"

GLM is header-only. If CMake doesn't find it automatically:

```cmake
# Add to CMakeLists.txt:
target_include_directories(VulkanApp PRIVATE /path/to/glm)
```

### "glslc not found"

```bash
# glslc comes with the Vulkan SDK. Check:
which glslc

# If not found, it's in the SDK's bin directory:
export PATH=$VULKAN_SDK/bin:$PATH
```

---

## Try It Yourself

1. **Set up the development environment** on your system following the instructions above.

2. **Build and run the test program**. Make sure you see the extension list and the window opens.

3. **Modify the test**: Change the window size to 1024x768 and the title to "My Vulkan App". Rebuild and verify.

4. **Add keyboard interaction**: Print a message when the spacebar is pressed:
   ```cpp
   if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
       std::cout << "Space pressed!\n";
   }
   ```

5. **Explore**: Run `vulkaninfo` (without `--summary`) and look at the full output. Find:
   - Your GPU's name and Vulkan version
   - How many queue families it has
   - What memory types are available

---

## Key Takeaways

- Vulkan development requires the **Vulkan SDK**, **GLFW** (for windows), and **GLM** (for math).
- **CMake** is the standard build system for cross-platform Vulkan projects.
- The **two-call enumeration pattern** (count → allocate → fill) is fundamental to Vulkan.
- `GLFW_CLIENT_API, GLFW_NO_API` tells GLFW **not** to create an OpenGL context.
- **glslc** compiles human-readable GLSL shaders into GPU-readable SPIR-V binary.
- Always verify your setup works before writing real Vulkan code!

---

[<< Previous: Introduction](01-introduction.md) | [Next: Vulkan Instance >>](03-instance.md)
