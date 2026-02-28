# The OpenGL Context

[Previous: What is OpenGL?](01_opengl.md) | [Next: Creating a Window](03_creating_a_window.md)

---

Throughout this tutorial you've been creating an OpenGL context every time you call `glfwCreateWindow` and `glfwMakeContextCurrent` — but what *is* a context, really? Why does OpenGL need one? And what happens when you want to use OpenGL from more than one thread?

This chapter demystifies the OpenGL context and shows how to use **shared contexts** for multi-threaded rendering.

---

## What Is an OpenGL Context?

An OpenGL context is the **entire collection of state** that OpenGL tracks behind the scenes. Every `glBind*`, `glEnable`, `glUseProgram`, and draw call operates on the *currently active* context.

Think of it like a workspace on your desktop:

```
  ┌──────────────────────────────────────────────────┐
  │              OpenGL Context                       │
  │                                                   │
  │   Currently bound VAO ─────────▶ VAO #3           │
  │   Currently active program ────▶ Program #7       │
  │   Currently bound textures ────▶ [Tex #1, Tex #5] │
  │   Depth test ──────────────────▶ ENABLED          │
  │   Clear color ─────────────────▶ (0.2, 0.3, 0.3) │
  │   Viewport ────────────────────▶ (0, 0, 800, 600) │
  │   ...hundreds more settings...                    │
  │                                                   │
  │   Owned objects:                                  │
  │     Buffers:        [1, 2, 3, 4, 5]              │
  │     Textures:       [1, 2, 3, 4, 5]              │
  │     Shaders:        [1, 2, 3]                    │
  │     Programs:       [1, 2]                        │
  │     VAOs:           [1, 2, 3]                     │
  │     Framebuffers:   [1]                           │
  │                                                   │
  └──────────────────────────────────────────────────┘
```

A context contains:

- **All OpenGL objects** you create: buffers, textures, shaders, programs, VAOs, framebuffers, etc.
- **All state settings**: depth test on/off, blending mode, viewport dimensions, clear color, etc.
- **The currently bound/active** variant of each object type.
- **A connection to a window's framebuffer** (the surface you render to).

Without an active context, no OpenGL call will work. This is why `glfwMakeContextCurrent` must be called before `gladLoadGLLoader` — GLAD needs an active context to query the driver for function pointers.

## Why Does OpenGL Need a Context?

Three fundamental reasons:

### 1. GPU Resource Ownership

The GPU needs to know *which set of resources* a draw call refers to. If two programs both create a texture with ID 1, the context keeps them isolated. Each context owns its own set of object IDs.

### 2. State Isolation

Without contexts, every OpenGL call would need to pass every single piece of state as parameters. Instead, OpenGL uses the state machine pattern: bind state once, then issue commands that use it.

```cpp
// Without a context (hypothetical — this is NOT real OpenGL):
glDrawArrays(GL_TRIANGLES, 0, 3,
             vao, program, texture, viewport,
             depth_test, blend_mode, ...);

// With a context (actual OpenGL):
glBindVertexArray(vao);       // set state
glUseProgram(program);        // set state
glDrawArrays(GL_TRIANGLES, 0, 3);  // uses state from context
```

### 3. Thread Safety

OpenGL was designed when single-threaded rendering was the norm. Each thread can have *at most one* current context, and each context can be current on *at most one* thread. This simple rule avoids data races in the driver.

```
  Thread A                    Thread B
  ────────                    ────────
  context_1 is current        context_2 is current
  glDraw...() ─▶ GPU          glDraw...() ─▶ GPU
  (uses context_1)            (uses context_2)
```

## Context Lifecycle

Here is the lifecycle you've been using without thinking much about it:

```cpp
// 1. Initialize GLFW
glfwInit();
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

// 2. Create window — this also creates a context
GLFWwindow* window = glfwCreateWindow(800, 600, "Hello", nullptr, nullptr);

// 3. Make context current on THIS thread
glfwMakeContextCurrent(window);

// 4. Load OpenGL functions (needs a current context)
gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

// 5. Use OpenGL...
glClearColor(0.2f, 0.3f, 0.3f, 1.0f);

// 6. When done, destroy window (this also destroys the context)
glfwDestroyWindow(window);
glfwTerminate();
```

Key rules:
- **One context per window.** `glfwCreateWindow` always creates both.
- **`glfwMakeContextCurrent` binds a context to the calling thread.** No OpenGL calls work until this is called.
- **GLAD loads function pointers from the *current* context.** If you switch contexts, you may need to reload.

## The One-Thread-One-Context Rule

This is the most critical rule of OpenGL threading:

> **A context can be current on at most one thread at a time. A thread can have at most one current context.**

Violating this rule causes undefined behavior — usually crashes or corrupted rendering.

```
  ─── VALID ───

  Thread A ───────▶ Context 1 (current)
  Thread B ───────▶ Context 2 (current)


  ─── INVALID ───

  Thread A ┐
           ├─────▶ Context 1    ✗ TWO threads, ONE context
  Thread B ┘
```

To move a context to a different thread, you must first release it:

```cpp
// Thread A releases, Thread B acquires
glfwMakeContextCurrent(nullptr);     // Thread A: release
// ... signal Thread B ...
glfwMakeContextCurrent(window);      // Thread B: acquire
```

## Shared Contexts — Sharing Resources Across Threads

What if you want to load textures in a background thread while the main thread renders? You can't use the same context from two threads. The solution: **shared contexts**.

When you create a second window/context, you can pass the first window as the `share` parameter:

```cpp
// Main rendering window
GLFWwindow* main_window = glfwCreateWindow(800, 600, "Main", nullptr, nullptr);

// Second context that SHARES resources with main_window
glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);  // invisible — just for resource loading
GLFWwindow* loader_window = glfwCreateWindow(1, 1, "Loader", nullptr, main_window);
//                                                             ▲▲▲▲▲▲▲▲▲▲▲▲
//                                                             share parameter
```

### What Gets Shared

Not everything is shared between contexts. Here is the split:

| Shared (across contexts)               | NOT Shared (per context)                 |
|----------------------------------------|------------------------------------------|
| Textures                               | VAOs (Vertex Array Objects)              |
| Buffers (VBO, EBO, UBO, SSBO, etc.)   | Framebuffer Objects (FBOs)               |
| Shader and Program Objects             | Transform Feedback Objects               |
| Renderbuffers                          | Vertex attribute bindings                |
| Sampler Objects                        | All other state (viewport, depth, etc.)  |
| Sync Objects                           |                                          |

The key insight: **data objects** (textures, buffers, shaders) are shared, but **container objects** (VAOs, FBOs) that hold references and bindings are *not* shared. Each context must create its own VAOs and FBOs, but the buffers and textures they reference can be the same.

### Why VAOs Are Not Shared

A VAO stores *bindings* — which buffer is bound to which attribute slot, with what stride and offset. These bindings are state, and state is per-context. Sharing them would violate the thread isolation model.

If you load a VBO of vertex data on the loader thread, the main thread can use that VBO — but it must create its own VAO and call `glVertexAttribPointer` to set up the bindings.

## Multi-Threaded Rendering: A Practical Example

Here is the simplest useful pattern: **load resources on a background thread** while the main thread renders.

### The Architecture

```
  ┌──────────────────────┐       ┌──────────────────────┐
  │    MAIN THREAD        │       │   LOADER THREAD       │
  │                       │       │                       │
  │  context: main_window │       │  context: loader_ctx  │
  │  (current)            │       │  (current)            │
  │                       │       │                       │
  │  Render loop:         │       │  Load textures:       │
  │  - draw scene         │       │  - stbi_load()        │
  │  - check flag         │◀─ ─ ─│  - glTexImage2D()     │
  │  - if ready, use tex  │ flag  │  - glFinish()         │
  │                       │       │  - set flag = true    │
  └──────────────────────┘       └──────────────────────┘
              │                             │
              └──────── SHARED ─────────────┘
                  Textures, Buffers, Shaders
```

### The Code

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <atomic>
#include <thread>
#include <iostream>

// Triangle vertices: position (x, y) + color (r, g, b)
float vertices[] = {
    // positions      // colors
    -0.5f, -0.5f,    1.0f, 0.0f, 0.0f,   // bottom-left  (red)
     0.5f, -0.5f,    0.0f, 1.0f, 0.0f,   // bottom-right (green)
     0.0f,  0.5f,    0.0f, 0.0f, 1.0f    // top-center   (blue)
};

const char* vertex_shader_src = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec3 aColor;
    out vec3 ourColor;
    void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
        ourColor = aColor;
    }
)";

const char* fragment_shader_src = R"(
    #version 330 core
    in vec3 ourColor;
    out vec4 FragColor;
    void main() {
        FragColor = vec4(ourColor, 1.0);
    }
)";

// Shared state: the loader thread puts the VBO ID here when done
std::atomic<unsigned int> shared_vbo{0};
std::atomic<bool>         data_ready{false};

/// Compile a shader and check for errors
unsigned int CompileShader(unsigned int type, const char* source)
{
    unsigned int shader = glCreateShader(type);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error:\n" << log << std::endl;
    }
    return shader;
}

/// Build a shader program from vertex + fragment source
unsigned int BuildShaderProgram(const char* vs_src, const char* fs_src)
{
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vs_src);
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fs_src);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

/// Background thread: creates vertex data in a shared VBO
void LoaderThread(GLFWwindow* loader_ctx)
{
    // Make the shared context current on THIS thread
    glfwMakeContextCurrent(loader_ctx);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // Create a VBO and upload vertex data (shared between contexts)
    unsigned int vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // CRITICAL: glFinish ensures the GPU has completed all commands
    // before we signal the main thread. Without this, the main thread
    // might try to use the VBO before the data is actually uploaded.
    glFinish();

    // Signal: VBO is ready
    shared_vbo.store(vbo);
    data_ready.store(true);

    std::cout << "[Loader] VBO " << vbo << " uploaded and ready.\n";

    // Release the context (good practice)
    glfwMakeContextCurrent(nullptr);
}

void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

int main()
{
    // ── Initialize GLFW ─────────────────────────────────────────────
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // ── Create the MAIN window + context ────────────────────────────
    GLFWwindow* main_window = glfwCreateWindow(
        800, 600, "Multi-Context Demo", nullptr, nullptr);
    if (!main_window)
    {
        std::cerr << "Failed to create main window\n";
        glfwTerminate();
        return -1;
    }

    // ── Create the LOADER context (invisible, shares with main) ─────
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* loader_ctx = glfwCreateWindow(
        1, 1, "Loader", nullptr, main_window);
    //                               share ▲▲▲▲▲▲▲▲▲▲▲▲
    if (!loader_ctx)
    {
        std::cerr << "Failed to create loader context\n";
        glfwDestroyWindow(main_window);
        glfwTerminate();
        return -1;
    }

    // ── Launch the loader thread ────────────────────────────────────
    std::thread loader(LoaderThread, loader_ctx);

    // ── Set up the main context ─────────────────────────────────────
    glfwMakeContextCurrent(main_window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(main_window, FramebufferSizeCallback);

    // Build shaders on the main thread (programs are shared, but we
    // create them here since the main thread uses them for drawing)
    unsigned int shader_program = BuildShaderProgram(
        vertex_shader_src, fragment_shader_src);

    // ── VAO setup (VAOs are NOT shared — must be per-context) ───────
    unsigned int vao = 0;
    bool vao_configured = false;

    // ── Render loop ─────────────────────────────────────────────────
    while (!glfwWindowShouldClose(main_window))
    {
        // Input
        if (glfwGetKey(main_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(main_window, true);

        // Clear
        glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Once the loader thread signals data is ready, create a VAO
        // and bind the shared VBO to it
        if (data_ready.load() && !vao_configured)
        {
            unsigned int vbo = shared_vbo.load();

            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);

            // position attribute (location = 0)
            glVertexAttribPointer(
                0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);

            // color attribute (location = 1)
            glVertexAttribPointer(
                1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                (void*)(2 * sizeof(float)));
            glEnableVertexAttribArray(1);

            glBindVertexArray(0);
            vao_configured = true;

            std::cout << "[Main] VAO configured with shared VBO.\n";
        }

        // Draw the triangle (only if data is ready)
        if (vao_configured)
        {
            glUseProgram(shader_program);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        glfwSwapBuffers(main_window);
        glfwPollEvents();
    }

    // ── Cleanup ─────────────────────────────────────────────────────
    loader.join();

    if (vao_configured)
    {
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &shared_vbo);
    }
    glDeleteProgram(shader_program);

    glfwDestroyWindow(loader_ctx);
    glfwDestroyWindow(main_window);
    glfwTerminate();
    return 0;
}
```

### Walking Through the Code

Let's trace the execution step by step:

**1. Two windows, one visible:**

```cpp
GLFWwindow* main_window  = glfwCreateWindow(800, 600, "Main", nullptr, nullptr);
GLFWwindow* loader_ctx   = glfwCreateWindow(1, 1, "Loader", nullptr, main_window);
//                                                            share ▲▲▲▲▲▲▲▲▲▲▲
```

The `main_window` parameter in the second call tells GLFW: "share OpenGL objects between these two contexts." The loader window is invisible (`GLFW_VISIBLE = GLFW_FALSE`) — we only need its context.

**2. Loader thread uploads data:**

```cpp
void LoaderThread(GLFWwindow* loader_ctx) {
    glfwMakeContextCurrent(loader_ctx);   // context current on THIS thread
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);  // load GL functions

    glGenBuffers(1, &vbo);                // create buffer (shared!)
    glBufferData(...);                    // upload vertex data
    glFinish();                           // wait for GPU to finish

    data_ready.store(true);               // signal main thread
}
```

Note `glFinish()` — this is critical. Without it, the main thread might try to read the VBO before the GPU has finished writing the data. `glFinish` blocks until all queued GPU commands have completed.

**3. Main thread creates its own VAO:**

VAOs are *not* shared. Even though the VBO (with the actual vertex data) was created on the loader thread, the main thread must create its own VAO and bind the shared VBO to it:

```cpp
if (data_ready.load() && !vao_configured) {
    glGenVertexArrays(1, &vao);           // main thread's VAO
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, shared_vbo);  // shared VBO!
    glVertexAttribPointer(...);           // configure attributes
    vao_configured = true;
}
```

**4. Drawing uses shared data through a local VAO:**

```cpp
glUseProgram(shader_program);  // program is shared too
glBindVertexArray(vao);        // local VAO → points to shared VBO
glDrawArrays(GL_TRIANGLES, 0, 3);
```

## Synchronization: `glFinish` vs. `glFlush` vs. Sync Objects

When sharing data across contexts, you need to ensure one context's operations are visible to another:

| Method | What It Does | When to Use |
|--------|-------------|-------------|
| `glFlush()` | Submits queued commands to GPU, returns immediately | Not sufficient for cross-context sync |
| `glFinish()` | Blocks until ALL queued commands complete | Simple but heavy — stalls the CPU |
| `glFenceSync` / `glClientWaitSync` | Creates a fence; you can check/wait for it | Best practice for production code |

For our simple example, `glFinish` is perfectly fine. In a real application, you'd use fence sync objects for better performance:

```cpp
// Producer thread (after uploading data):
GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
glFlush();  // ensure the fence is submitted

// Consumer thread (before using the data):
glWaitSync(fence, 0, GL_TIMEOUT_IGNORED);  // GPU waits
// or
glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, timeout_ns);  // CPU waits
```

## Common Multi-Context Pitfalls

### 1. Forgetting `glfwMakeContextCurrent`

```cpp
// WRONG — no context is current, OpenGL calls are undefined
std::thread t([] {
    glGenBuffers(1, &vbo);  // ✗ crash or silent failure
});
```

Always make a context current before any `gl*` call.

### 2. Using One Context from Two Threads

```cpp
// WRONG — main_window's context used from two threads
glfwMakeContextCurrent(main_window);
std::thread t([&] {
    glfwMakeContextCurrent(main_window);  // ✗ undefined behavior!
});
```

One context, one thread. Always.

### 3. Forgetting to Share Contexts

```cpp
// WRONG — no sharing, VBO from ctx2 is invisible to ctx1
GLFWwindow* w2 = glfwCreateWindow(1, 1, "", nullptr, nullptr);
//                                              no share ▲▲▲▲▲▲▲
```

Pass the main window as the last parameter to enable sharing.

### 4. Using VAOs Across Contexts

```cpp
// WRONG — VAOs are NOT shared
// Loader thread:
glGenVertexArrays(1, &vao);
glBindVertexArray(vao);
glBindBuffer(GL_ARRAY_BUFFER, vbo);
glVertexAttribPointer(...);

// Main thread:
glBindVertexArray(vao);  // ✗ this VAO doesn't exist in this context!
```

Create VAOs on the thread that uses them.

### 5. Missing Synchronization

```cpp
// WRONG — no guarantee data is uploaded when main thread reads it
// Loader:
glBufferData(...);
data_ready = true;  // ✗ GPU might not be done yet!

// Main:
if (data_ready) glDrawArrays(...);  // ✗ might read garbage
```

Always call `glFinish()` or use a `glFenceSync` before signaling readiness.

## When to Use Multi-Context Rendering

| Use Case | Multi-Context Needed? |
|----------|----------------------|
| Loading textures in background | Yes — most common use case |
| Streaming level geometry | Yes — upload while rendering |
| Computing shader compiles | Maybe — can be done async on GL 4.6 |
| Simple applications | No — single thread is fine |
| Real-time rendering + UI | Consider it — separate UI context |

For most learning projects, a single context on the main thread is sufficient. Multi-context shines in production where you want to avoid stalling the render loop while loading assets.

## Key Takeaways

- The **OpenGL context** holds all state and owns all objects. No GL call works without a current context.
- **One thread, one context** — the fundamental threading rule of OpenGL.
- **Shared contexts** let multiple contexts access the same textures, buffers, and shaders.
- **VAOs and FBOs are NOT shared** — each context must create its own.
- **Synchronization** (`glFinish` or fence syncs) is mandatory when sharing data across contexts.
- For most learning projects, stick with a single-threaded context. Use multi-context when you need background asset loading or parallel resource creation.

## Exercises

1. **Add a second triangle.** Modify the loader thread to upload two separate VBOs (one per triangle). Have the main thread create two VAOs and draw both.

2. **Texture loading.** Change the loader thread to load a texture with `stbi_load` and upload it with `glTexImage2D`. Use the shared texture in the main thread's fragment shader.

3. **Fence sync.** Replace `glFinish()` in the loader with a `glFenceSync` / `glClientWaitSync` pair. Measure if there's a timing difference.

4. **Two visible windows.** Create two visible windows sharing a context. Render the same triangle in both with different clear colors.

---

[Previous: What is OpenGL?](01_opengl.md) | [Next: Creating a Window](03_creating_a_window.md)
