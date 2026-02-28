# Blending

[← Previous: Stencil Testing](02_stencil_testing.md) | [Next: Face Culling →](04_face_culling.md)

---

So far, every fragment we've rendered has been fully opaque — solid colors that completely replace whatever was behind them. But the real world is full of semi-transparent things: glass, water, smoke, foliage with holes. The technique that lets us render transparency in OpenGL is called **blending**.

Blending is conceptually simple — mix the color of the incoming fragment with the color already in the framebuffer — but getting it to look correct requires understanding the alpha component, the blend equation, and a tricky sorting problem that has no perfect solution.

## The Alpha Component

You've seen `vec4` colors throughout this tutorial. The fourth component, **alpha**, represents opacity:

```
alpha = 1.0  →  fully opaque  (solid)
alpha = 0.5  →  50% transparent (see-through)
alpha = 0.0  →  fully transparent (invisible)
```

```
  alpha = 1.0         alpha = 0.5         alpha = 0.0

  ┌──────────┐       ┌──────────┐       ┌──────────┐
  │██████████│       │▓▓▓▓▓▓▓▓▓▓│       │          │
  │██ SOLID ██│       │▓▓ SEMI ▓▓│       │ INVISIBLE│
  │██████████│       │▓▓▓▓▓▓▓▓▓▓│       │          │
  └──────────┘       └──────────┘       └──────────┘
  (blocks behind)    (shows behind)     (fully gone)
```

Until now, alpha has been silently set to `1.0` in our fragment shaders. Time to put it to work.

## Discarding Fragments (Alpha Testing)

The simplest form of "transparency" isn't blending at all — it's throwing away fragments entirely. This is perfect for textures that have only two states: fully opaque or fully transparent. Think of grass, leaves, chain-link fences, or windows with frames.

### The Texture

Imagine a grass texture where the grass blades are green and the background is transparent (alpha = 0). If we load this as an RGB texture (3 channels), the transparent areas show up as black or white — not what we want. We need the **RGBA** format (4 channels) to preserve the alpha information.

### Loading RGBA Textures

When loading with stb_image, request 4 channels and use `GL_RGBA`:

```cpp
int width, height, nrChannels;
unsigned char* data = stbi_load("grass.png", &width, &height, &nrChannels, 0);

GLenum format;
if (nrChannels == 4)
    format = GL_RGBA;
else if (nrChannels == 3)
    format = GL_RGB;
// ... etc.

glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
             GL_UNSIGNED_BYTE, data);
```

For textures with transparency, also set the wrapping mode to `GL_CLAMP_TO_EDGE` to avoid interpolation artifacts at the borders:

```cpp
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
```

### Discarding in the Fragment Shader

The `discard` keyword tells OpenGL to throw away a fragment entirely — it won't write to the color, depth, or stencil buffers:

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture1;

void main()
{
    vec4 texColor = texture(texture1, TexCoords);
    if (texColor.a < 0.1)
        discard;
    FragColor = texColor;
}
```

We use `< 0.1` instead of `== 0.0` because of floating-point precision — texels with alpha very close to zero might not be exactly `0.0`.

```
  Without discard:               With discard:

  ┌────────────────────┐         ┌────────────────────┐
  │  ┌──────────────┐  │         │     /|\  /|\       │
  │  │ ████ ████ ██ │  │         │    / | \/ | \      │
  │  │ █ grass  █ █ │  │         │   /  |grass |  \   │
  │  │ ██ ████ ████ │  │         │  /   | /\ | \  \   │
  │  │ █ opaque box █  │         │      |/  \|       │
  │  └──────────────┘  │         │                    │
  │  ▓▓▓▓▓ ground ▓▓▓  │         │  ▓▓▓▓▓ ground ▓▓▓  │
  └────────────────────┘         └────────────────────┘
  (black/white background)       (transparent cutout!)
```

This technique is sometimes called **alpha testing** (the fixed-function name from older OpenGL) and works perfectly for binary transparency. But it can't do semi-transparency — a fragment is either kept or discarded, nothing in between.

## True Blending

For semi-transparent objects like tinted glass or colored water, we need actual blending — combining the incoming fragment's color with whatever is already in the framebuffer.

### Enabling Blending

```cpp
glEnable(GL_BLEND);
```

### The Blend Equation

When blending is enabled, OpenGL computes the final color using this equation:

```
C_result = C_source × F_source  +  C_destination × F_destination
```

Where:
- **C_source** — the color output by the fragment shader (the new fragment)
- **C_destination** — the color already stored in the framebuffer (what's behind)
- **F_source** — the source factor (how much of the new fragment to use)
- **F_destination** — the destination factor (how much of the existing color to keep)

```
  Fragment shader outputs:          Framebuffer already contains:
  ┌──────────────────────┐          ┌──────────────────────┐
  │  C_source (RGBA)     │          │  C_destination (RGB)  │
  │  the "incoming" color│          │  the "behind" color   │
  └──────────┬───────────┘          └──────────┬───────────┘
             │                                  │
             │        ┌───────────────┐         │
             └───────►│ Blend Equation ├◄───────┘
                      └───────┬───────┘
                              │
                              ▼
                    C_result (written to framebuffer)
```

### Setting the Blend Function

```cpp
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

This is the most common blending setup. It means:

- **F_source** = source alpha
- **F_destination** = 1 - source alpha

Let's trace through an example. A red fragment with 60% opacity landing on a blue background:

```
C_source      = (1.0, 0.0, 0.0, 0.6)    // red, 60% opaque
C_destination = (0.0, 0.0, 1.0, 1.0)    // blue (already in framebuffer)
F_source      = source alpha = 0.6
F_destination = 1.0 - 0.6   = 0.4

C_result = (1.0, 0.0, 0.0) × 0.6  +  (0.0, 0.0, 1.0) × 0.4
         = (0.6, 0.0, 0.0)         +  (0.0, 0.0, 0.4)
         = (0.6, 0.0, 0.4)   // a purple-ish mix
```

This is exactly what you'd expect intuitively: a partially transparent red object in front of blue gives a reddish-purple result.

### All Blend Factors

Here are the most commonly used blend factors:

| Factor | Value |
|---|---|
| `GL_ZERO` | `(0, 0, 0, 0)` |
| `GL_ONE` | `(1, 1, 1, 1)` |
| `GL_SRC_COLOR` | Source color components |
| `GL_ONE_MINUS_SRC_COLOR` | `1 -` source color components |
| `GL_SRC_ALPHA` | Source alpha (repeated for all) |
| `GL_ONE_MINUS_SRC_ALPHA` | `1 -` source alpha |
| `GL_DST_COLOR` | Destination color components |
| `GL_ONE_MINUS_DST_COLOR` | `1 -` destination color |
| `GL_DST_ALPHA` | Destination alpha |
| `GL_ONE_MINUS_DST_ALPHA` | `1 -` destination alpha |
| `GL_CONSTANT_COLOR` | A constant color set by `glBlendColor` |

### The Blend Equation Mode

The default blend equation is `GL_FUNC_ADD` (addition), but you can change it:

```cpp
glBlendEquation(GL_FUNC_ADD);              // src * Fs + dst * Fd (default)
glBlendEquation(GL_FUNC_SUBTRACT);         // src * Fs - dst * Fd
glBlendEquation(GL_FUNC_REVERSE_SUBTRACT); // dst * Fd - src * Fs
glBlendEquation(GL_MIN);                   // min(src, dst)
glBlendEquation(GL_MAX);                   // max(src, dst)
```

For standard transparency, `GL_FUNC_ADD` with `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` is what you want 99% of the time.

You can also set separate blend factors for RGB and alpha channels:

```cpp
glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
```

This uses standard alpha blending for RGB but keeps the alpha channel writing as-is, which is useful in some compositing scenarios.

## The Sorting Problem

Here's where blending gets tricky. Depth testing and blending don't play well together.

### The Problem

Consider two transparent windows at different depths:

```
  Camera →    Window A (near)     Window B (far)       Wall (opaque)
              alpha = 0.5         alpha = 0.5

  Correct (sorted back-to-front):
  1. Draw wall    →  framebuffer has wall color
  2. Draw B       →  blends B with wall      ✓
  3. Draw A       →  blends A with (B+wall)  ✓

  Wrong (unsorted):
  1. Draw wall    →  framebuffer has wall color
  2. Draw A       →  blends A with wall, writes to depth buffer
  3. Draw B       →  depth test FAILS (B is behind A) → discarded!
                     B is invisible even though A is transparent!
```

```
  Correct rendering:          Incorrect rendering:

  ┌──────────────────┐        ┌──────────────────┐
  │    ┌────────┐    │        │    ┌────────┐    │
  │    │ ▓▓▓▓▓▓ │    │        │    │ ▓▓▓▓▓▓ │    │
  │    │ ▓A+B+W▓ │    │        │    │ ▓A + W▓ │    │
  │    │ ▓▓▓▓▓▓ │    │        │    │ ▓▓▓▓▓▓ │    │
  │    └────────┘    │        │    └────────┘    │
  │  (both windows   │        │  (window B is    │
  │   visible)       │        │   missing!)      │
  └──────────────────┘        └──────────────────┘
```

### The Solution: Sort Transparent Objects

The standard approach:

1. **Draw all opaque objects first** (with depth testing and depth writing ON)
2. **Sort transparent objects** by distance from the camera, farthest first
3. **Draw transparent objects back-to-front** (with depth testing ON but depth writing OFF)

Disabling depth writes for transparent objects (`glDepthMask(GL_FALSE)`) ensures they don't block other transparent objects behind them, while still being correctly occluded by opaque objects.

### Sorting by Distance

We calculate the distance from the camera to each transparent object and sort them:

```cpp
std::map<float, glm::vec3> sorted;
for (unsigned int i = 0; i < windows.size(); i++)
{
    float distance = glm::length(camera.Position - windows[i]);
    sorted[distance] = windows[i];
}
```

`std::map` automatically sorts by key (distance), so we can iterate in order. To draw back-to-front, we iterate in reverse:

```cpp
for (auto it = sorted.rbegin(); it != sorted.rend(); ++it)
{
    model = glm::mat4(1.0f);
    model = glm::translate(model, it->second);
    shader.setMat4("model", model);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
```

### Limitations

This sorting approach works for many scenes but isn't perfect:

- **Overlapping transparent objects** — two transparent meshes that interpenetrate can't be sorted correctly by a single distance value per object (you'd need per-triangle sorting).
- **Performance cost** — sorting every frame adds CPU overhead.
- **Ties** — if two objects are at the same distance, `std::map` will overwrite one. For production code, use a `std::multimap` or a `std::vector` with `std::sort`.

More advanced techniques like **Order-Independent Transparency (OIT)** exist, but they are significantly more complex (using per-pixel linked lists, weighted blended OIT, etc.) and beyond our scope here.

## Complete Source Code

Here's a complete program rendering a scene with textured cubes, grass patches (using `discard`), and semi-transparent windows (using blending with sorting).

### Vertex Shader (blending.vs)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

### Fragment Shader (blending.fs)

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture1;

void main()
{
    vec4 texColor = texture(texture1, TexCoords);
    if (texColor.a < 0.1)
        discard;
    FragColor = texColor;
}
```

### Main Program (main.cpp)

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>

#include <iostream>
#include <map>
#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "Blending", nullptr, nullptr);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader shader("blending.vs", "blending.fs");

    float cubeVertices[] = {
        // positions          // texture coords
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
    };

    float planeVertices[] = {
         5.0f, -0.5f,  5.0f,  2.0f, 0.0f,
        -5.0f, -0.5f,  5.0f,  0.0f, 0.0f,
        -5.0f, -0.5f, -5.0f,  0.0f, 2.0f,
         5.0f, -0.5f,  5.0f,  2.0f, 0.0f,
        -5.0f, -0.5f, -5.0f,  0.0f, 2.0f,
         5.0f, -0.5f, -5.0f,  2.0f, 2.0f
    };

    float transparentVertices[] = {
        // positions         // texture coords
         0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
         0.0f, -0.5f,  0.0f,  0.0f,  1.0f,
         1.0f, -0.5f,  0.0f,  1.0f,  1.0f,

         0.0f,  0.5f,  0.0f,  0.0f,  0.0f,
         1.0f, -0.5f,  0.0f,  1.0f,  1.0f,
         1.0f,  0.5f,  0.0f,  1.0f,  0.0f
    };

    // Cube VAO
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    // Floor VAO
    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), &planeVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    // Transparent quad VAO
    unsigned int transparentVAO, transparentVBO;
    glGenVertexArrays(1, &transparentVAO);
    glGenBuffers(1, &transparentVBO);
    glBindVertexArray(transparentVAO);
    glBindBuffer(GL_ARRAY_BUFFER, transparentVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(transparentVertices),
                 transparentVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    unsigned int cubeTexture = loadTexture("resources/textures/marble.jpg");
    unsigned int floorTexture = loadTexture("resources/textures/metal.png");
    unsigned int windowTexture = loadTexture("resources/textures/window.png");

    std::vector<glm::vec3> windows {
        glm::vec3(-1.5f, 0.0f, -0.48f),
        glm::vec3( 1.5f, 0.0f,  0.51f),
        glm::vec3( 0.0f, 0.0f,  0.7f),
        glm::vec3(-0.3f, 0.0f, -2.3f),
        glm::vec3( 0.5f, 0.0f, -0.6f)
    };

    shader.use();
    shader.setInt("texture1", 0);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        // 1. Draw opaque objects first
        // Floor
        glBindVertexArray(planeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTexture);
        glm::mat4 model = glm::mat4(1.0f);
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Cubes
        glBindVertexArray(cubeVAO);
        glBindTexture(GL_TEXTURE_2D, cubeTexture);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.0f, 0.0f, -1.0f));
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(2.0f, 0.0f, 0.0f));
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // 2. Sort transparent objects by distance (back-to-front)
        std::map<float, glm::vec3> sorted;
        for (unsigned int i = 0; i < windows.size(); i++)
        {
            float distance = glm::length(camera.Position - windows[i]);
            sorted[distance] = windows[i];
        }

        // 3. Draw transparent objects (back-to-front)
        glBindVertexArray(transparentVAO);
        glBindTexture(GL_TEXTURE_2D, windowTexture);
        for (auto it = sorted.rbegin(); it != sorted.rend(); ++it)
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, it->second);
            shader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteVertexArrays(1, &transparentVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteBuffers(1, &transparentVBO);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

unsigned int loadTexture(char const* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        if (nrComponents == 4)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                            GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                            GL_CLAMP_TO_EDGE);
        }
        else
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
```

### What You Should See

A scene with two marble cubes on a metal floor. Five semi-transparent windows float at various positions. As you move the camera, the windows correctly blend with the scene behind them. The back-to-front sorting ensures that overlapping transparent windows look correct from most angles.

```
  ┌──────────────────────────────────────┐
  │         ┌─────┐      ┌─────┐        │
  │    ┌────┤glass├──┐   │glass│        │
  │    │cube│     │  │   └─────┘        │
  │    └────┤     ├──┘                  │
  │         └─────┘    ┌─────┐          │
  │                    │glass│          │
  │  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓└─────┘▓▓▓▓▓▓▓  │
  │  ▓▓▓▓▓▓▓▓▓ metal floor ▓▓▓▓▓▓▓▓▓  │
  └──────────────────────────────────────┘
```

---

## Key Takeaways

- **Alpha** is the fourth color component representing opacity (`1.0` = opaque, `0.0` = transparent).
- **`discard`** in the fragment shader throws away fragments entirely — perfect for binary transparency (grass, fences).
- **Blending** (`glEnable(GL_BLEND)`) combines source and destination colors using the blend equation.
- The standard blend function is `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)`.
- **Transparent objects must be sorted back-to-front** and drawn after all opaque objects — depth testing alone doesn't handle transparency correctly.
- Use `glDepthMask(GL_FALSE)` when drawing transparent objects to prevent them from blocking each other in the depth buffer.

## Exercises

1. Remove the sorting code and draw the windows in their original order. Rotate the camera and find angles where the rendering is clearly wrong. Then re-enable sorting and compare.
2. Change the window texture's alpha to different values (0.3, 0.7, 0.9) and observe how the blending changes. You can do this in the fragment shader by multiplying `texColor.a` by a uniform.
3. Try different blend functions: `glBlendFunc(GL_ONE, GL_ONE)` (additive blending) makes things glow. Experiment with other combinations from the table above.
4. Implement grass patches using the `discard` technique: create quads with a grass texture that has transparency, and place several of them in the scene at different angles (crossing each other to create the illusion of 3D grass).
5. Add `glDepthMask(GL_FALSE)` before drawing transparent objects and `glDepthMask(GL_TRUE)` after. Does the result change? Move the camera so one transparent window is behind an opaque cube — does it still get occluded correctly? Why?

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Stencil Testing](02_stencil_testing.md) | [Table of Contents](../../README.md) | [Next: Face Culling →](04_face_culling.md) |
