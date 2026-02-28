# What is OpenGL?

Welcome to the very beginning of your OpenGL journey. Before we write a single line of code, let's understand *what* OpenGL actually is, where it came from, and how the pieces fit together. This foundation will save you countless hours of confusion later on.

---

## OpenGL Is a Specification, Not a Library

This is the single most important thing to understand upfront: **OpenGL is not a library**. It is a *specification* — a formal document that describes a set of functions, their parameters, their return values, and what they should do. The specification is maintained by the [Khronos Group](https://www.khronos.org/), a consortium of hardware and software companies.

So who actually *implements* OpenGL? Your **GPU driver** does. When you call an OpenGL function like `glDrawArrays`, you're calling into code written by NVIDIA, AMD, Intel, or whoever made your graphics card and its driver. This means:

- Different drivers may have slightly different behavior or bugs.
- The available OpenGL version depends on your GPU hardware *and* driver.
- There is no single "OpenGL source code" you can download and compile.

Think of it like this: the OpenGL specification is like the HTTP standard — it describes how things should work, and different web servers (Apache, Nginx) implement it differently.

## A Brief History of OpenGL

OpenGL has been around since 1992, making it one of the oldest graphics APIs still in active use.

### The Early Days: Immediate Mode (OpenGL 1.x–2.x)

The original OpenGL used what is called **immediate mode** (also known as the fixed-function pipeline). Drawing a triangle looked something like this:

```cpp
// OLD immediate mode — do NOT use this
glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, 0.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.5f, -0.5f, 0.0f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.5f, 0.0f);
glEnd();
```

This was easy to learn but terribly inefficient. Each `glVertex3f` call sent a single vertex from the CPU to the GPU one at a time. With millions of vertices per frame, that's a lot of overhead.

### The Modern Era: Core Profile (OpenGL 3.3+)

Starting with OpenGL 3.0, the Khronos Group introduced a **deprecation model** and eventually a clean split between two profiles:

- **Compatibility Profile**: includes all the old functions for backward compatibility.
- **Core Profile**: strips out all deprecated functions, forcing you to use the modern, efficient way.

**We will be using OpenGL 3.3 Core Profile throughout this tutorial.** This version was chosen because:

1. It's supported on virtually all modern GPUs (including integrated graphics).
2. It contains all the fundamental features of modern OpenGL.
3. The concepts transfer directly to OpenGL 4.x.
4. It forces good habits — no reliance on deprecated shortcuts.

In modern OpenGL, you write **shaders** (small programs that run on the GPU), manage your own **vertex buffers**, and have much more control — and responsibility.

### Timeline

| Year | Version | Key Feature                                  |
|------|---------|----------------------------------------------|
| 1992 | 1.0     | Original release, immediate mode             |
| 2004 | 2.0     | Programmable shaders (GLSL) introduced       |
| 2008 | 3.0     | Deprecation model, framebuffer objects        |
| 2010 | 3.3     | Core profile solidified — our target version |
| 2010 | 4.0     | Tessellation shaders                         |
| 2017 | 4.6     | SPIR-V support, latest version               |

## OpenGL vs. Other Graphics APIs

You may be wondering how OpenGL compares to alternatives. Here is a quick comparison:

| Feature            | OpenGL              | Vulkan                 | DirectX 11/12       | Metal               |
|--------------------|---------------------|------------------------|---------------------|----------------------|
| **Platform**       | Cross-platform      | Cross-platform         | Windows/Xbox only   | Apple only           |
| **Level**          | High-level          | Very low-level         | Mid to low-level    | Mid-level            |
| **Learning curve** | Moderate            | Extremely steep        | Moderate to steep   | Moderate             |
| **Verbosity**      | ~50 lines for a triangle | ~800+ lines for a triangle | ~200 lines       | ~150 lines           |
| **Driver overhead**| Higher (driver does more) | Minimal (you do more) | Varies by version  | Low                  |
| **Best for**       | Learning, prototyping, cross-platform apps | AAA games, performance-critical apps | Windows games | macOS/iOS apps |

**For learning 3D graphics concepts, OpenGL is still the best starting point.** The concepts (shaders, buffers, textures, transformations) transfer to every other API.

## The Graphics Pipeline — A High-Level Overview

When you tell OpenGL to draw something, your data goes through a series of stages called the **graphics pipeline**. Here is the big picture:

```
                        THE OPENGL GRAPHICS PIPELINE

  ┌──────────────┐     ┌───────────────┐     ┌────────────────┐
  │  Your C++    │     │ Vertex Shader │     │ Shape Assembly │
  │  Application │────▶│ (per vertex)  │────▶│ (primitives)   │
  └──────────────┘     └───────────────┘     └────────────────┘
                                                      │
                              ┌────────────────────────┘
                              ▼
                    ┌──────────────────┐     ┌────────────────┐
                    │ Geometry Shader  │────▶│ Rasterization  │
                    │ (optional)       │     │ (fragments)    │
                    └──────────────────┘     └────────────────┘
                                                      │
                              ┌────────────────────────┘
                              ▼
                    ┌──────────────────┐     ┌────────────────┐
                    │ Fragment Shader  │────▶│ Tests/Blending │
                    │ (per pixel)      │     │ (depth, alpha) │
                    └──────────────────┘     └────────────────┘
                                                      │
                              ┌────────────────────────┘
                              ▼
                       ┌──────────────┐
                       │ Framebuffer  │
                       │ (screen)     │
                       └──────────────┘
```

Let's walk through each stage:

1. **Application (CPU side)**: Your C++ code defines vertex data (positions, colors, texture coordinates), uploads it to the GPU, sets up shaders, and issues draw calls.

2. **Vertex Shader**: Runs *once per vertex*. Its main job is to transform vertex positions (e.g., applying model, view, and projection matrices). This is where 3D coordinates get projected onto a 2D screen.

3. **Shape Assembly** (Primitive Assembly): Takes the output vertices and assembles them into primitives — triangles, lines, or points — based on what you told OpenGL to draw.

4. **Geometry Shader** (optional): Can modify, discard, or even create new primitives. We won't use this in early chapters.

5. **Rasterization**: Converts each primitive into **fragments** — potential pixels on the screen. A fragment contains interpolated data for a specific screen pixel.

6. **Fragment Shader**: Runs *once per fragment*. Determines the final color of each pixel. This is where lighting, texturing, and all the visual magic happens.

7. **Tests and Blending**: Depth testing (which fragments are in front), stencil testing, and alpha blending (transparency) happen here. Fragments that fail tests are discarded.

8. **Framebuffer**: The final pixel colors are written to the framebuffer, which is displayed on your screen.

The stages highlighted in **bold** are the ones you *must* program yourself in modern OpenGL: the **Vertex Shader** and the **Fragment Shader**. There is no default — if you don't provide them, nothing will render.

## Core Profile vs. Compatibility Profile

When creating an OpenGL context, you choose a **profile**:

- **Core Profile**: Only modern functions are available. If you try to call a deprecated function (like `glBegin`), you'll get an error or undefined behavior. This is what we use.

- **Compatibility Profile**: All functions from all OpenGL versions are available, including deprecated ones. Useful for legacy code, but teaches bad habits.

We explicitly request a core profile context when setting up our window:

```cpp
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
```

## OpenGL as a State Machine

OpenGL operates as a large **state machine**. Instead of passing every parameter to every function, you *set state* and then *perform actions* that use that state.

For example, to draw with a specific texture:

```cpp
glBindTexture(GL_TEXTURE_2D, myTexture);  // set state: "current texture is myTexture"
glDrawArrays(GL_TRIANGLES, 0, 3);         // draw using whatever state is currently set
```

The current state of OpenGL is commonly called the **OpenGL context**. It includes:

- The currently bound buffers
- The currently active shader program
- The currently bound textures
- The current clear color
- The current viewport dimensions
- Depth testing on/off
- And hundreds more settings...

This is why you'll see a lot of `glBind*` calls in OpenGL code — they're switching which objects are "current" in the state machine before performing operations.

## Objects in OpenGL

Modern OpenGL manages resources through **objects**. An object is a collection of options/state identified by a numeric ID. The general workflow is always:

```cpp
unsigned int objectId;
glGenObjects(1, &objectId);         // 1. Generate an object, get its ID
glBindObject(GL_TARGET, objectId);  // 2. Bind it (make it the current one)
// ... configure the object ...      // 3. Set options on the bound object
glBindObject(GL_TARGET, 0);         // 4. Unbind (optional but clean)
```

> Note: `glGenObjects` and `glBindObject` are pseudo-code — the real functions include the object type in their name, like `glGenBuffers`, `glBindBuffer`, etc.

Here are the main object types you'll encounter:

| Object Type         | Purpose                                      |
|---------------------|----------------------------------------------|
| **Buffer Object**   | Stores vertex data, indices, etc. on the GPU |
| **Vertex Array Object (VAO)** | Stores vertex attribute configuration |
| **Texture Object**  | Stores image data for texturing              |
| **Shader Object**   | A single compiled shader (vertex/fragment)   |
| **Program Object**  | Links multiple shaders into a usable program |
| **Framebuffer Object** | An off-screen render target               |

## Extensions

Because OpenGL is implemented by different vendors, new features sometimes appear as **extensions** before they become part of the official specification. An extension adds new functions or new constants.

Extensions are named with a prefix indicating who created them:

- `GL_ARB_*` — Approved by the Architecture Review Board (likely to become standard)
- `GL_EXT_*` — Agreed upon by multiple vendors
- `GL_NV_*` — NVIDIA-specific
- `GL_AMD_*` — AMD-specific

In the past, using extensions required writing a lot of platform-specific code to load function pointers. Today, libraries like **GLAD** handle this for us automatically.

## Why We Need Helper Libraries

OpenGL itself only deals with rendering — it has no concept of windows, input, file loading, or math. We need helper libraries for all the surrounding functionality:

### GLFW — Window and Input

[GLFW](https://www.glfw.org/) provides:
- Cross-platform window creation (Windows, macOS, Linux)
- OpenGL context creation
- Keyboard, mouse, and gamepad input
- Monitor and display management

Without GLFW (or a similar library like SDL, SFML), you'd have to write platform-specific code using Win32 on Windows, Cocoa on macOS, or X11/Wayland on Linux — just to open a window.

### GLAD — OpenGL Function Loader

Here's a subtle problem: OpenGL functions aren't linked to your program at compile time. Their addresses must be queried from the driver *at runtime*. This is because different drivers may support different OpenGL versions and extensions.

[GLAD](https://glad.dav1d.de/) is a function loader generator that:
- Queries the driver for function addresses
- Creates function pointers for every OpenGL function
- Lets you call OpenGL functions normally after initialization

Without GLAD (or a similar library like GLEW, GL3W), you'd have to manually call `wglGetProcAddress` (Windows) or `glXGetProcAddress` (Linux) for *every single* OpenGL function.

### GLM — Mathematics

[GLM](https://github.com/g-truc/glm) (OpenGL Mathematics) provides:
- Vector types: `vec2`, `vec3`, `vec4`
- Matrix types: `mat3`, `mat4`
- Transformations: translate, rotate, scale
- Projections: perspective, orthographic
- All designed to match GLSL types and conventions

OpenGL doesn't provide any math functions — you need to compute transformation matrices yourself.

### Summary of Libraries

```
  ┌──────────────────────────────────────────────┐
  │              Your Application                 │
  ├──────────┬──────────┬────────────┬────────────┤
  │   GLFW   │   GLAD   │    GLM     │  stb_image │
  │ (window) │ (loader) │  (math)    │ (textures) │
  ├──────────┴──────────┴────────────┴────────────┤
  │             OpenGL Driver (GPU)                │
  └──────────────────────────────────────────────-─┘
```

---

## Key Takeaways

- OpenGL is a **specification**, not a library. Your GPU driver provides the actual implementation.
- We use **OpenGL 3.3 Core Profile** — modern, widely supported, and forces good practices.
- The **graphics pipeline** transforms vertices into pixels through a series of stages, two of which (vertex and fragment shaders) you must write yourself.
- OpenGL is a **state machine**: you bind objects, set state, then issue commands.
- We need **GLFW** for windows, **GLAD** for loading OpenGL functions, and **GLM** for math.

## Exercises

1. Look up what OpenGL version your GPU supports. On Linux, run `glxinfo | grep "OpenGL version"`. On Windows, use [GPU-Z](https://www.techpowerup.com/gpuz/) or [OpenGL Extensions Viewer](https://www.realtech-vr.com/home/glview).
2. Read the first page of the [official OpenGL 3.3 specification](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf) — just the introduction. Notice how it reads like a technical standard, not a tutorial.
3. Think about why the vertex shader and fragment shader are the two *required* stages. What would happen if either was missing?

---

| | Navigation | |
|:---|:---:|---:|
| | | [Next: The OpenGL Context →](02_opengl_context.md) |
