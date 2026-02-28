# Creating a Window

Before we can render anything with OpenGL, we need two things: a **window** to draw into and a loaded set of **OpenGL function pointers**. In this chapter, we'll set up our development environment with GLFW and GLAD, write a proper CMake build system, and create our first window with an OpenGL context.

---

## Why GLFW?

OpenGL has no idea what a "window" is. It only knows about contexts, framebuffers, and draw calls. The operating system manages windows — and every OS does it differently:

- **Windows**: Win32 API (`CreateWindowEx`, `wglCreateContext`)
- **macOS**: Cocoa / AppKit (`NSWindow`, `NSOpenGLContext`)
- **Linux**: X11 or Wayland (`XCreateWindow`, `glXCreateContext`)

Writing and maintaining all of that is a project in itself. **GLFW** abstracts this away into a clean, cross-platform C API. It handles:

- Window creation and management
- OpenGL/Vulkan context creation
- Keyboard, mouse, and joystick input
- Monitor and display enumeration
- High-DPI support

## Why GLAD?

Here is a subtle but critical issue: **most OpenGL functions are not directly available to your program at compile time**. The function addresses live inside your GPU driver and must be loaded at runtime.

On Linux, this means calling `glXGetProcAddress`. On Windows, `wglGetProcAddress`. And you'd need to do this for every single function — there are over 500 in OpenGL 3.3 alone.

**GLAD** is a loader generator that automates all of this. You generate a header tailored to your target OpenGL version, include it, call `gladLoadGLLoader(...)` once, and every OpenGL function becomes available as a normal C function call.

## Installing Dependencies

### Option 1: System Package Manager (Linux)

```bash
# Ubuntu / Debian
sudo apt install libglfw3-dev libglm-dev

# Fedora
sudo dnf install glfw-devel glm-devel

# Arch Linux
sudo pacman -S glfw glm
```

### Option 2: Building GLFW from Source

```bash
git clone https://github.com/glfw/glfw.git
cd glfw
mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=ON
make -j$(nproc)
sudo make install
```

### Generating GLAD

GLAD is not a pre-built library — you generate it from a web service or a Python tool.

**Web method** (easiest):
1. Go to [https://glad.dav1d.de/](https://glad.dav1d.de/)
2. Set **Language** to C/C++
3. Set **Specification** to OpenGL
4. Set **Profile** to Core
5. Set **API gl** to Version 3.3
6. Check **Generate a loader**
7. Click **Generate** and download the zip

You'll get:
```
glad/
├── include/
│   ├── glad/
│   │   └── glad.h
│   └── KHR/
│       └── khrplatform.h
└── src/
    └── glad.c
```

**Python method**:
```bash
pip install glad
glad --api gl:core=3.3 --out-path glad --generator c
```

## Project Structure

Let's set up a clean project layout:

```
opengl_tutorial/
├── CMakeLists.txt
├── src/
│   └── main.cpp
├── include/
│   └── (your header files)
└── third_party/
    └── glad/
        ├── include/
        │   ├── glad/
        │   │   └── glad.h
        │   └── KHR/
        │       └── khrplatform.h
        └── src/
            └── glad.c
```

## CMake Build System

Here is a complete `CMakeLists.txt` that sets up the project with GLFW and GLAD:

```cmake
cmake_minimum_required(VERSION 3.16)
project(OpenGLTutorial LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- GLAD (built from source we generated) ---
add_library(glad STATIC
    third_party/glad/src/glad.c
)
target_include_directories(glad PUBLIC
    third_party/glad/include
)

# --- Find GLFW ---
find_package(glfw3 3.3 REQUIRED)

# --- Main executable ---
add_executable(${PROJECT_NAME}
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    glfw
    glad
)

# On Linux, we also need these system libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        dl
        pthread
    )
endif()
```

Build commands:

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./OpenGLTutorial
```

## Creating a Window — Step by Step

Let's build up the code piece by piece, understanding every line.

### Step 1: Include Headers

```cpp
#include <glad/glad.h>  // MUST be included before GLFW
#include <GLFW/glfw3.h>
#include <iostream>
```

> **Important**: GLAD must be included *before* GLFW. GLAD defines the OpenGL function prototypes, and GLFW's header detects this and avoids redefining them. Reversing the order causes compilation errors.

### Step 2: Initialize GLFW

```cpp
int main()
{
    glfwInit();
```

`glfwInit()` initializes the GLFW library. It must be called before any other GLFW function. It returns `GLFW_TRUE` on success.

### Step 3: Configure with Window Hints

```cpp
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
```

Window hints configure the next window that will be created:

- **`CONTEXT_VERSION_MAJOR/MINOR`**: We want OpenGL 3.3. If the system can't provide at least this version, window creation will fail.
- **`OPENGL_PROFILE`**: We want the core profile (no deprecated functions).
- **`OPENGL_FORWARD_COMPAT`**: Required on macOS to get a core profile context.

### Step 4: Create the Window

```cpp
    GLFWwindow* window = glfwCreateWindow(800, 600, "LearnOpenGL", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
```

`glfwCreateWindow` takes:
1. **Width** (800 pixels)
2. **Height** (600 pixels)
3. **Title** ("LearnOpenGL")
4. **Monitor** (nullptr = windowed mode; pass a monitor for fullscreen)
5. **Share** (nullptr = no context sharing with another window)

It returns a `GLFWwindow*` pointer, or `nullptr` on failure.

`glfwMakeContextCurrent` tells GLFW: "Make this window's OpenGL context the current one for this thread." All subsequent OpenGL calls will target this context.

### Step 5: Load OpenGL Functions with GLAD

```cpp
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
```

This is the critical step. `gladLoadGLLoader` takes a function that can look up OpenGL function addresses. GLFW provides exactly such a function: `glfwGetProcAddress`.

After this call succeeds, *every* OpenGL function (for the version we requested) is available for use.

### Step 6: Set the Viewport

```cpp
    glViewport(0, 0, 800, 600);
```

`glViewport` tells OpenGL the size of the rendering window so it can map its normalized device coordinates (-1 to 1) to screen coordinates:

```
  OpenGL NDC                    Screen Pixels
  (-1,1)────(1,1)              (0,600)────(800,600)
    │          │       ──▶       │              │
    │          │                 │              │
  (-1,-1)───(1,-1)             (0,0)──────(800,0)
```

The four parameters are: `x`, `y` (lower-left corner), `width`, `height`.

### Step 7: The Render Loop

```cpp
    while (!glfwWindowShouldClose(window))
    {
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
```

This is the **render loop** (also called the game loop). It runs continuously until the user closes the window.

- **`glfwWindowShouldClose`**: Returns `true` if the user clicked the close button (or we programmatically set the close flag).
- **`glfwSwapBuffers`**: Swaps the front and back buffers (more on double buffering in the next chapter).
- **`glfwPollEvents`**: Checks for input events (keyboard, mouse, window resize) and calls any registered callbacks.

### Step 8: Clean Up

```cpp
    glfwTerminate();
    return 0;
}
```

`glfwTerminate` destroys all remaining windows, frees resources, and de-initializes GLFW.

## Complete Working Example

Here is the full program assembled together:

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

int main()
{
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW for OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create a windowed mode window and its OpenGL context
    GLFWwindow* window = glfwCreateWindow(800, 600, "LearnOpenGL", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Load OpenGL function pointers with GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Set the viewport
    glViewport(0, 0, 800, 600);

    // Render loop
    while (!glfwWindowShouldClose(window))
    {
        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();
    }

    // Clean up
    glfwTerminate();
    return 0;
}
```

When you run this, you should see a window appear — likely filled with garbage pixels or black. That's expected! We haven't told OpenGL to clear or draw anything yet. In the next chapter, we'll add a proper clear color and handle input.

## What Happens Behind the Scenes

Let's trace the sequence of events when this program runs:

```
  1. glfwInit()
     └─▶ Initializes platform-specific window system

  2. glfwWindowHint(...)
     └─▶ Stores configuration for next window creation

  3. glfwCreateWindow(800, 600, ...)
     ├─▶ Creates an OS window (X11/Win32/Cocoa)
     ├─▶ Creates an OpenGL 3.3 core context
     └─▶ Returns a GLFWwindow handle

  4. glfwMakeContextCurrent(window)
     └─▶ Binds the OpenGL context to the current thread

  5. gladLoadGLLoader(...)
     ├─▶ Queries the driver for each GL function address
     └─▶ Stores function pointers (glViewport, glClear, etc.)

  6. Render loop
     ├─▶ glfwSwapBuffers: display what was rendered
     └─▶ glfwPollEvents: check OS event queue

  7. glfwTerminate()
     └─▶ Destroys window, context, frees everything
```

---

## Key Takeaways

- **GLFW** handles cross-platform window creation and input. You configure it with *window hints* before creating a window.
- **GLAD** loads OpenGL function pointers from the GPU driver at runtime. Always call `gladLoadGLLoader` after making a context current and before any `gl*` calls.
- The **render loop** is the heartbeat of every OpenGL application: process input, render, swap buffers, repeat.
- Always include `glad.h` *before* `glfw3.h`.
- `glViewport` maps OpenGL's normalized coordinates to actual pixel coordinates on screen.

## Exercises

1. Try changing the window size to 1280×720. Does it still work? What happens to `glViewport` if you don't update it to match?
2. Try requesting OpenGL 4.5 instead of 3.3. Does your system support it? What happens if it doesn't?
3. Try creating two windows (two calls to `glfwCreateWindow`). You'll need to switch contexts with `glfwMakeContextCurrent` — what issues do you encounter?
4. Set `GLFW_RESIZABLE` to `GLFW_FALSE` as a window hint and observe the difference.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: What is OpenGL?](01_opengl.md) | | [Next: Hello Window →](03_hello_window.md) |
