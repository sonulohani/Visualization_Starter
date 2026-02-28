# Debugging

If you've made it this far in the tutorial, you've undoubtedly encountered a black screen at some point — maybe several times. You changed a single line, recompiled, ran the program, and… nothing. No error message, no crash, just an empty void staring back at you. Welcome to one of OpenGL's most infamous traits: **silent failure**.

Unlike most APIs that throw exceptions or return error codes from every function, OpenGL operates as a global state machine that quietly sets internal error flags when something goes wrong. If you don't explicitly check for those flags, you'll never know an error occurred. This makes debugging OpenGL programs uniquely challenging, but the good news is that there are several powerful techniques and tools available once you know where to look.

In this chapter we'll cover three progressively more powerful debugging methods, walk through common problem scenarios, and learn how to systematically track down those maddening black screens.

---

## Method 1: glGetError()

The oldest and most universally available debugging tool in OpenGL is `glGetError()`. After any OpenGL function call, the driver may set one or more error flags. Calling `glGetError()` retrieves the oldest flag and removes it from the queue, returning `GL_NO_ERROR` when the queue is empty.

### Error Codes

| Flag | Value | Meaning |
|:-----|:------|:--------|
| `GL_NO_ERROR` | 0 | No error recorded |
| `GL_INVALID_ENUM` | 0x0500 | An unacceptable enum value was passed |
| `GL_INVALID_VALUE` | 0x0501 | A numeric argument was out of range |
| `GL_INVALID_OPERATION` | 0x0502 | The operation is not allowed in the current state |
| `GL_OUT_OF_MEMORY` | 0x0505 | Not enough memory to execute the command |
| `GL_INVALID_FRAMEBUFFER_OPERATION` | 0x0506 | Read/write to an incomplete framebuffer |

### A Practical Helper Function

Manually calling `glGetError()` and printing the result everywhere is tedious. Let's build a helper that automatically prints the file and line number where the error occurred:

```cpp
#include <iostream>
#include <string>
#include <glad/glad.h>

GLenum checkError_(const char *file, int line)
{
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR)
    {
        std::string error;
        switch (errorCode)
        {
            case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                  error = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:              error = "INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:                  error = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:  error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        std::cout << error << " | " << file << " (" << line << ")" << std::endl;
    }
    return errorCode;
}
#define checkError() checkError_(__FILE__, __LINE__)
```

Now sprinkle `checkError()` after suspicious calls:

```cpp
glBindBuffer(GL_ARRAY_BUFFER, vbo);
checkError();

glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
checkError();

glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
checkError();
```

If something is wrong, you'll see output like:

```
INVALID_OPERATION | main.cpp (42)
```

### Limitations

`glGetError()` tells you **what** went wrong, but not always **where** — an error flag might have been set several calls ago if you didn't check frequently enough. This is why the `while` loop in our helper drains all pending errors: there could be multiple flags queued.

For OpenGL 3.3 core profile programs, `glGetError()` is always available and is your baseline debugging tool. But there's a much better approach if your hardware supports it.

---

## Method 2: Debug Output (OpenGL 4.3+ / GL_KHR_debug)

Starting with OpenGL 4.3, the **debug output** extension (`GL_KHR_debug`) allows OpenGL to call **your** function whenever something noteworthy happens. Instead of polling for errors, the driver pushes detailed messages to a callback — including the source, type, severity, and a human-readable description. This is a massive improvement over `glGetError()`.

> **Note:** Debug output requires OpenGL 4.3+ or the `GL_KHR_debug` extension. Most modern drivers on desktop support this. Check with `glGetString(GL_EXTENSIONS)` or use GLAD to confirm availability.

### Requesting a Debug Context

First, tell GLFW to create a **debug context** before creating the window:

```cpp
glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
```

A debug context gives the driver permission to generate debug messages. Without it, the driver may suppress output for performance. After the context is created and GLAD is loaded, verify:

```cpp
int flags;
glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
{
    std::cout << "Debug context active." << std::endl;
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(debugCallback, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
}
```

Let's break down each call:

- **`glEnable(GL_DEBUG_OUTPUT)`** — activates debug output.
- **`glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS)`** — makes callbacks happen on the same thread as the OpenGL call, so your stack traces are meaningful. Without this, messages may arrive asynchronously and the call stack won't point to the actual offending function.
- **`glDebugMessageCallback(debugCallback, nullptr)`** — registers our callback function.
- **`glDebugMessageControl(...)`** — enables all messages (all sources, all types, all severities). We'll filter inside the callback.

### The Callback Function

```cpp
void APIENTRY debugCallback(GLenum source, GLenum type, unsigned int id,
                            GLenum severity, GLsizei length,
                            const char *message, const void *userParam)
{
    // Ignore non-significant codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::cout << "---------------" << std::endl;
    std::cout << "Debug message (" << id << "): " << message << std::endl;

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:      std::cout << "Source: Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
    }
    std::cout << std::endl;

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
    }
    std::cout << std::endl;

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
        case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
        case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
    }
    std::cout << std::endl;
    std::cout << std::endl;
}
```

### Understanding the Message Components

Each debug message carries metadata that helps you pinpoint the problem:

```
┌──────────────────────────────────────────────────────────────────────┐
│                     Debug Message Anatomy                            │
│                                                                      │
│  Source ──── Who generated it?                                       │
│    ├── API              (OpenGL call errors)                         │
│    ├── Window System    (WGL/GLX/EGL issues)                         │
│    ├── Shader Compiler  (GLSL compilation/linking)                   │
│    ├── Third Party      (external debugger tools)                    │
│    ├── Application      (your own messages via glDebugMessageInsert) │
│    └── Other                                                         │
│                                                                      │
│  Type ───── What kind of message?                                    │
│    ├── Error            (spec violation, will likely fail)            │
│    ├── Deprecated       (using removed/old features)                 │
│    ├── Undefined        (undefined behavior invoked)                 │
│    ├── Portability      (may not work on other hardware)             │
│    ├── Performance      (suboptimal usage patterns)                  │
│    └── Marker/Groups    (annotations for debug tools)                │
│                                                                      │
│  Severity ── How bad is it?                                          │
│    ├── High             (crash / major bug)                          │
│    ├── Medium           (significant issue, wrong output likely)     │
│    ├── Low              (minor issue, might affect quality)          │
│    └── Notification     (informational, not an error)                │
└──────────────────────────────────────────────────────────────────────┘
```

### Filtering Messages

In practice the driver emits many notification-level messages that clutter output. You can filter them out in two ways:

**Inside the callback** (what we did above by ignoring specific IDs):

```cpp
if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
```

**Using `glDebugMessageControl`** to disable categories at the driver level:

```cpp
// Disable all notification messages
glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
```

### Debug Groups and Labels

For complex rendering pipelines, you can annotate sections of your code with labels that appear in external tools like RenderDoc:

```cpp
glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Shadow Pass");
// ... shadow mapping draw calls ...
glPopDebugGroup();

glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Lighting Pass");
// ... lighting draw calls ...
glPopDebugGroup();
```

You can also label individual objects so they show up with friendly names in debuggers:

```cpp
glObjectLabel(GL_BUFFER, vbo, -1, "Cube Vertex Buffer");
glObjectLabel(GL_TEXTURE, diffuseMap, -1, "Brick Diffuse Texture");
glObjectLabel(GL_PROGRAM, shaderProgram, -1, "Phong Lighting Shader");
```

These labels cost nothing in release builds (just don't call them) and save enormous time when debugging complex scenes.

### Inserting Your Own Messages

You can push custom messages into the debug stream for logging purposes:

```cpp
glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER,
                     0, GL_DEBUG_SEVERITY_NOTIFICATION, -1,
                     "Starting post-processing pass");
```

---

## Method 3: External Debugging Tools

When `glGetError()` and debug output aren't enough — for example, when you need to see exactly what's in a texture at a specific draw call, or inspect the depth buffer — external tools become invaluable.

### RenderDoc

**RenderDoc** is a free, open-source graphics debugger that works with OpenGL (3.2+ core profile), Vulkan, and D3D. It's the single most useful tool for OpenGL development.

```
┌─────────────────────────────────────────────────────────┐
│                    RenderDoc Workflow                     │
│                                                         │
│  1. Launch your app through RenderDoc                    │
│  2. Press F12 (or PrintScreen) to capture a frame        │
│  3. Browse the list of draw calls                        │
│  4. For each draw call, inspect:                         │
│     ├── Input/Output textures                            │
│     ├── Vertex/Index buffers                             │
│     ├── Shader source and uniforms                       │
│     ├── Pipeline state (blend, depth, stencil)           │
│     ├── Framebuffer attachments                          │
│     └── Mesh viewer (see geometry in 3D)                 │
│  5. Find the broken draw call and fix it                 │
└─────────────────────────────────────────────────────────┘
```

**Quick walkthrough:**

1. **Download** RenderDoc from [renderdoc.org](https://renderdoc.org).
2. **Launch your application** by setting the executable path in RenderDoc's "Launch Application" tab.
3. **Capture a frame** by pressing the capture hotkey while your app runs.
4. **Open the capture** — you'll see a timeline of API calls in the "Event Browser."
5. **Click on a draw call** to see the pipeline state at that exact moment: which textures were bound, what the framebuffer looked like, the values of shader uniforms, and more.
6. **Texture Viewer** — view any texture or framebuffer attachment. Invaluable for checking if your shadow map is correct, if your normal map loaded properly, or if your FBO attachment has the right format.
7. **Mesh Viewer** — see your geometry in 3D at any draw call. Great for catching vertex attribute mismatches.

> **Note:** RenderDoc requires a **core profile** context (no compatibility profile). If you've been following this tutorial, you already have one.

### Other Tools

**Nsight Graphics (NVIDIA)**
NVIDIA's proprietary debugger integrates with Visual Studio and offers frame debugging, GPU trace, and shader profiling. Particularly powerful if you're on NVIDIA hardware and want performance analysis alongside correctness debugging.

**Radeon GPU Profiler / Radeon Developer Panel (AMD)**
AMD's replacement for the discontinued GPU PerfStudio. Focuses on profiling (occupancy, wavefront analysis, memory bandwidth) rather than frame debugging, but the Developer Panel also supports frame capture.

**apitrace**
An open-source tool that records every OpenGL call your application makes into a trace file, which you can then replay and inspect. Useful when you need to see the exact sequence of API calls.

```bash
# Record a trace
apitrace trace ./my_opengl_app

# Replay it
apitrace replay my_opengl_app.trace

# View it in the GUI
qapitrace my_opengl_app.trace
```

---

## Common Debugging Scenarios

### The Black Screen Checklist

A black screen is the most common OpenGL problem. Work through this checklist systematically:

1. **Is `glClear` being called?** — Verify you're clearing with a non-black color temporarily: `glClearColor(0.2f, 0.3f, 0.3f, 1.0f)`. If the background color shows, the window and context are fine.
2. **Is the shader program linked and active?** — Check `glGetProgramiv(program, GL_LINK_STATUS, &success)` and print `glGetProgramInfoLog` if it fails. Call `glUseProgram(program)` before drawing.
3. **Are you sending vertex data?** — Verify your VBO has data (`glBufferData` was called with the right size and pointer), and that your VAO is bound before the draw call.
4. **Do vertex attribute pointers match the shader?** — If the shader expects `layout (location = 0) in vec3 aPos` but you configured attribute 1, nothing will render.
5. **Are you calling `glDrawArrays` / `glDrawElements` with the right count?** — A count of 0, or the wrong primitive type, produces nothing.
6. **Is the projection matrix correct?** — A zero matrix, an identity matrix when you need perspective, or extreme near/far planes can cause everything to be clipped.
7. **Is the model matrix putting objects in view?** — If objects are translated far away or scaled to zero, the camera won't see them.
8. **Is face culling eating your geometry?** — Try `glDisable(GL_CULL_FACE)` temporarily. If objects appear, your winding order is wrong.
9. **Is depth testing configured correctly?** — `glEnable(GL_DEPTH_TEST)` must be called, and the depth buffer must be cleared each frame with `glClear(GL_DEPTH_BUFFER_BIT)`.
10. **Is the viewport set correctly?** — After window creation and on resize, call `glViewport(0, 0, width, height)`.
11. **Are you drawing to the right framebuffer?** — If you bound an FBO and forgot to bind back to the default framebuffer (0) before the swap, you won't see anything.
12. **Is `glfwSwapBuffers` being called?** — Without it, the back buffer is never displayed.

### Shader Not Compiling

Always check compilation status after `glCompileShader`:

```cpp
int success;
char infoLog[1024];
glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
if (!success)
{
    glGetShaderInfoLog(shader, 1024, NULL, infoLog);
    std::cout << "SHADER COMPILATION ERROR:\n" << infoLog << std::endl;
}
```

And linking status after `glLinkProgram`:

```cpp
glGetProgramiv(program, GL_LINK_STATUS, &success);
if (!success)
{
    glGetProgramInfoLog(program, 1024, NULL, infoLog);
    std::cout << "PROGRAM LINKING ERROR:\n" << infoLog << std::endl;
}
```

Common GLSL mistakes:
- Missing semicolons (the error message often points to the *next* line)
- `in` / `out` variable names don't match between vertex and fragment shader
- Using `texture2D()` instead of `texture()` (the former is legacy)
- Passing the wrong type to a function (e.g., `vec3` where `vec4` is expected)

### Texture Shows Black

If an object renders with the right shape but is solid black:

1. **Did the image load?** — Check the return value of `stbi_load()` or your image loader. Print width, height, and channels.
2. **Is the texture generated and bound?** — Verify `glGenTextures`, `glBindTexture`, and `glTexImage2D` were all called.
3. **Is the right texture unit active?** — Before binding, call `glActiveTexture(GL_TEXTURE0)` (or the appropriate unit). Set the sampler uniform to the matching unit number.
4. **Are mipmaps generated?** — If you set the minification filter to a mipmap variant (`GL_LINEAR_MIPMAP_LINEAR`) but forgot to call `glGenerateMipmap(GL_TEXTURE_2D)`, the texture is considered incomplete and shows black.
5. **Is `GL_UNPACK_ALIGNMENT` correct?** — Some image formats (especially single-channel) aren't 4-byte aligned. Call `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` before `glTexImage2D`.

### Z-Fighting

When two surfaces overlap at nearly the same depth, you'll see flickering stripes as the depth buffer can't decide which is in front:

```
┌───────────────────────────────┐
│        Z-Fighting              │
│                               │
│    ▓▓░░▓▓░░▓▓░░▓▓░░▓▓       │  Two surfaces at nearly
│    ░░▓▓░░▓▓░░▓▓░░▓▓░░       │  the same depth create
│    ▓▓░░▓▓░░▓▓░░▓▓░░▓▓       │  alternating stripe artifacts
│    ░░▓▓░░▓▓░░▓▓░░▓▓░░       │
└───────────────────────────────┘
```

Fixes:
- **Move surfaces apart.** Even `0.001` units can be enough.
- **Increase the near plane** of your projection. A near plane of `0.1` gives far better depth precision than `0.001`.
- **Use `glPolygonOffset`** to bias the depth of one surface:
  ```cpp
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.0f, 1.0f);
  // Draw the surface that should be "behind"
  glDisable(GL_POLYGON_OFFSET_FILL);
  ```
- **Use a logarithmic depth buffer** for extreme depth ranges (advanced topic).

### Objects Not Appearing

If specific objects are missing but others render:

- **Check the model matrix.** Print or debug-visualize the position, rotation, and scale. A common mistake is applying transformations in the wrong order.
- **Check face culling.** If the object's winding order is clockwise but you're culling clockwise faces, it disappears.
- **Check scissor/viewport.** An object outside the viewport or scissor rectangle is silently clipped.
- **Check the draw call.** Make sure you're binding the right VAO and passing the right vertex count.

---

## Shader Debugging Tips

GLSL shaders have no `printf`. You can't set breakpoints in a fragment shader (at least not without external tools). The classic workaround is to **output intermediate values as colors**.

### Visualizing Normals

```glsl
// In the fragment shader:
FragColor = vec4(Normal * 0.5 + 0.5, 1.0);
```

This maps normal vectors from the `[-1, 1]` range to `[0, 1]` (valid color range). Each surface direction shows as a distinct color:

```
Normal Visualization Color Mapping
──────────────────────────────────
  +X → red      (1.0, 0.5, 0.5)
  -X → dark     (0.0, 0.5, 0.5)
  +Y → green    (0.5, 1.0, 0.5)
  -Y → dark     (0.5, 0.0, 0.5)
  +Z → blue     (0.5, 0.5, 1.0)
  -Z → dark     (0.5, 0.5, 0.0)
```

If you see unexpected colors, your normals are wrong — perhaps the normal matrix isn't applied, or the vertex data has incorrect normals.

### Visualizing Depth

```glsl
// Linear depth (near/far must be uniforms)
float ndc = gl_FragCoord.z * 2.0 - 1.0;
float linearDepth = (2.0 * near * far) / (far + near - ndc * (far - near));
FragColor = vec4(vec3(linearDepth / far), 1.0);
```

Close objects appear dark, distant objects appear white. This is useful for verifying that your depth buffer has the precision you expect.

### Visualizing Texture Coordinates

```glsl
FragColor = vec4(TexCoords, 0.0, 1.0);
```

The R channel maps to U (horizontal) and the G channel to V (vertical), producing a gradient from black (0,0) to yellow (1,1). Distortions or seams in this gradient reveal UV mapping issues.

### Checking Specific Values

Want to know if a uniform is reaching the shader? Output it:

```glsl
// Is lightPos being set correctly?
FragColor = vec4(abs(lightPos) / 10.0, 1.0);
```

Or use a conditional to highlight a specific condition:

```glsl
// Highlight fragments where the dot product is negative (backfaces)
float diff = dot(Normal, lightDir);
if (diff < 0.0)
    FragColor = vec4(1.0, 0.0, 0.0, 1.0);  // bright red = problem
else
    FragColor = vec4(vec3(diff), 1.0);
```

---

## Complete Source Code

Here's a complete program that demonstrates both debugging methods. It intentionally includes a bug (incorrect texture unit) that the debug output catches:

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <string>

// ─── Debug callback ─────────────────────────────────────────────────────────

void APIENTRY debugCallback(GLenum source, GLenum type, unsigned int id,
                            GLenum severity, GLsizei length,
                            const char *message, const void *userParam)
{
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::cout << "---------------" << std::endl;
    std::cout << "Debug message (" << id << "): " << message << std::endl;

    switch (source)
    {
        case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
        case GL_DEBUG_SOURCE_APPLICATION:      std::cout << "Source: Application"; break;
        case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
    }
    std::cout << std::endl;

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
        case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
        case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
        case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
        case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
    }
    std::cout << std::endl;

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
        case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
        case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
    }
    std::cout << std::endl;
    std::cout << std::endl;
}

// ─── glGetError fallback ────────────────────────────────────────────────────

GLenum checkError_(const char *file, int line)
{
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR)
    {
        std::string error;
        switch (errorCode)
        {
            case GL_INVALID_ENUM:                  error = "INVALID_ENUM"; break;
            case GL_INVALID_VALUE:                  error = "INVALID_VALUE"; break;
            case GL_INVALID_OPERATION:              error = "INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:                  error = "OUT_OF_MEMORY"; break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:  error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        std::cout << error << " | " << file << " (" << line << ")" << std::endl;
    }
    return errorCode;
}
#define checkError() checkError_(__FILE__, __LINE__)

// ─── Shader sources ─────────────────────────────────────────────────────────

const char *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    ourColor = aColor;
}
)";

const char *fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 ourColor;

void main()
{
    FragColor = vec4(ourColor, 1.0);
}
)";

// ─── Helpers ────────────────────────────────────────────────────────────────

unsigned int compileShader(GLenum type, const char *source)
{
    unsigned int shader = glCreateShader(type);
    glCompileShader(shader);

    // Intentional bug: forgot to attach source before compiling!
    // The debug output will catch this.
    // Fix: move glShaderSource before glCompileShader
    glShaderSource(shader, 1, &source, NULL);

    int success;
    char infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        std::cout << "SHADER COMPILATION ERROR:\n" << infoLog << std::endl;
    }

    return shader;
}

unsigned int createShaderProgram()
{
    // Fix the bug above: attach source THEN compile
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    int success;
    char infoLog[1024];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 1024, NULL, infoLog);
        std::cout << "VERTEX SHADER ERROR:\n" << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 1024, NULL, infoLog);
        std::cout << "FRAGMENT SHADER ERROR:\n" << infoLog << std::endl;
    }

    unsigned int program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        std::cout << "PROGRAM LINKING ERROR:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

// ─── Main ───────────────────────────────────────────────────────────────────

void framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);

    GLFWwindow *window = glfwCreateWindow(800, 600, "Debugging Demo", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Enable debug output
    int flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT)
    {
        std::cout << "Debug context active." << std::endl;
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                              GL_DEBUG_SEVERITY_NOTIFICATION,
                              0, nullptr, GL_FALSE);
    }

    // Shader setup
    unsigned int shaderProgram = createShaderProgram();

    // A simple colored triangle
    float vertices[] = {
        // positions         // colors
        -0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glObjectLabel(GL_BUFFER, VBO, -1, "Triangle VBO");
    glObjectLabel(GL_VERTEX_ARRAY, VAO, -1, "Triangle VAO");
    glObjectLabel(GL_PROGRAM, shaderProgram, -1, "Color Shader");

    glEnable(GL_DEPTH_TEST);

    // Render loop
    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                                  800.0f / 600.0f,
                                                  0.1f, 100.0f);
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));
        glm::mat4 model = glm::mat4(1.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"),
                           1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),
                           1, GL_FALSE, glm::value_ptr(model));

        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Draw Triangle");
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glPopDebugGroup();

        checkError();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}
```

> **Note:** This example requests OpenGL 4.3 for debug output. If your hardware only supports 3.3, remove the debug output setup and rely on `checkError()` and RenderDoc instead.

---

## Key Takeaways

- OpenGL fails **silently** — you must actively check for errors.
- **`glGetError()`** is universally available but requires frequent polling and only tells you the error type.
- **Debug output** (OpenGL 4.3+ / `GL_KHR_debug`) is vastly superior: the driver calls your callback with detailed, categorized messages including source, type, and severity.
- Always enable **synchronous debug output** (`GL_DEBUG_OUTPUT_SYNCHRONOUS`) during development so that stack traces point to the actual OpenGL call.
- **RenderDoc** is the most important external tool — learn it early and use it often.
- **Label your objects** with `glObjectLabel` and use `glPushDebugGroup` / `glPopDebugGroup` to make external tools (and your debug callback) more informative.
- When debugging shaders, **output intermediate values as colors** — visualize normals, depth, UVs, and arbitrary values.
- For black screens, work through the **12-item checklist** systematically rather than guessing.

## Exercises

1. **Intentional errors:** Add an intentional error (e.g., pass `GL_FLOAT` as a texture target to `glBindTexture`) and verify that both `glGetError()` and the debug callback catch it. Compare the information each provides.
2. **Severity filter:** Modify the debug callback to use colored terminal output (ANSI escape codes): red for high severity, yellow for medium, cyan for low. This makes scanning output much faster.
3. **Normal visualization:** Take one of your earlier lit scenes (e.g., the Phong lighting scene) and add a debug mode (toggled with a key press) that renders normals as colors instead of the final lit color.
4. **RenderDoc exploration:** Install RenderDoc and capture a frame from one of your previous programs (e.g., the model loading chapter). Inspect the draw calls, look at the texture viewer, and examine the mesh viewer.
5. **Debug groups:** Add debug groups to your most complex program's render loop (e.g., shadow pass, geometry pass, lighting pass) and verify they appear in RenderDoc's event browser.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: PBR](../06_pbr/) | [07. In Practice](.) | [Next: Text Rendering →](02_text_rendering.md) |
