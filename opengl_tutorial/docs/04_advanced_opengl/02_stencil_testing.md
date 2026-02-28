# Stencil Testing

[← Previous: Depth Testing](01_depth_testing.md) | [Next: Blending →](03_blending.md)

---

In the previous chapter we explored the depth buffer — a per-pixel buffer that determines which fragments are visible based on distance. The **stencil buffer** is another per-pixel buffer, but instead of storing depth, it stores an integer value that you control. Think of it as a mask you paint onto the screen: you decide where rendering is allowed and where it's blocked. This makes it one of OpenGL's most versatile tools for effects like object outlines, portal rendering, planar reflections, and more.

## What Is the Stencil Buffer?

The stencil buffer is an 8-bit buffer (values 0–255) that exists alongside the color buffer and depth buffer. Every pixel on screen has a corresponding stencil value. The name comes from the physical tool — a stencil is a sheet with cutouts that you paint through, so paint only appears where the cutouts are.

```
  Color buffer:        Depth buffer:        Stencil buffer:
  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐
  │ RGBA  RGBA   │     │ 0.32  0.45   │     │  0    0    0 │
  │ RGBA  RGBA   │     │ 0.98  0.12   │     │  0    1    1 │
  │ RGBA  RGBA   │     │ 0.67  0.89   │     │  0    1    1 │
  │ RGBA  RGBA   │     │ 0.55  0.73   │     │  0    0    0 │
  └──────────────┘     └──────────────┘     └──────────────┘
    (what you see)      (fragment depth)      (your mask)
```

The stencil test happens *after* the fragment shader runs but *before* the depth test. Here's the full per-fragment pipeline:

```
  Fragment Shader
       │
       ▼
  Stencil Test ──── fail ──► Fragment discarded
       │
       │ pass
       ▼
  Depth Test ────── fail ──► Fragment discarded (stencil may still update)
       │
       │ pass
       ▼
  Color written to framebuffer
  Depth buffer updated
  Stencil buffer updated (based on glStencilOp)
```

## Enabling Stencil Testing

Just like depth testing, stencil testing must be explicitly enabled:

```cpp
glEnable(GL_STENCIL_TEST);
```

And the stencil buffer must be cleared each frame, just like the color and depth buffers:

```cpp
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
```

The stencil buffer is cleared to `0` by default. You can change the clear value with:

```cpp
glClearStencil(0); // 0 is default
```

## The Stencil Mask

The **stencil mask** controls which bits of the stencil buffer can be written to. It's a bitmask that's AND'd with the value before writing:

```cpp
glStencilMask(0xFF); // all 8 bits writable (default) — writing enabled
glStencilMask(0x00); // no bits writable — writing disabled
```

Think of it like a write-protect switch:
- `0xFF` (binary `11111111`) — full write access
- `0x00` (binary `00000000`) — read-only
- `0x0F` (binary `00001111`) — only the lower 4 bits can be written

For most use cases you'll toggle between `0xFF` (write) and `0x00` (don't write).

## Configuring the Stencil Test: glStencilFunc

`glStencilFunc` determines *when* a fragment passes the stencil test:

```cpp
glStencilFunc(GLenum func, GLint ref, GLuint mask);
```

| Parameter | Purpose |
|---|---|
| `func` | The comparison function |
| `ref` | The reference value to compare against |
| `mask` | AND'd with both `ref` and the stored stencil value before comparison |

The test is: `(ref & mask) func (stencil & mask)`

### Available Functions

| Function | Fragment passes if |
|---|---|
| `GL_NEVER` | Never (all fragments fail) |
| `GL_LESS` | `(ref & mask) < (stencil & mask)` |
| `GL_LEQUAL` | `(ref & mask) <= (stencil & mask)` |
| `GL_GREATER` | `(ref & mask) > (stencil & mask)` |
| `GL_GEQUAL` | `(ref & mask) >= (stencil & mask)` |
| `GL_EQUAL` | `(ref & mask) == (stencil & mask)` |
| `GL_NOTEQUAL` | `(ref & mask) != (stencil & mask)` |
| `GL_ALWAYS` | Always (all fragments pass) |

### Example

```cpp
glStencilFunc(GL_EQUAL, 1, 0xFF);
```

This means: a fragment passes the stencil test only if the stencil buffer at that pixel contains `1`. (`1 & 0xFF == stencilValue & 0xFF` → stencil value must be 1.)

## Configuring Stencil Actions: glStencilOp

`glStencilFunc` controls *when* a fragment passes. `glStencilOp` controls *what happens to the stencil buffer value* in each of three scenarios:

```cpp
glStencilOp(GLenum sfail, GLenum dpfail, GLenum dppass);
```

| Parameter | When it triggers |
|---|---|
| `sfail` | The stencil test **fails** |
| `dpfail` | The stencil test passes, but the depth test **fails** |
| `dppass` | **Both** stencil and depth tests pass |

### Available Actions

| Action | Effect on stencil value |
|---|---|
| `GL_KEEP` | Keep the current value (do nothing) |
| `GL_ZERO` | Set to 0 |
| `GL_REPLACE` | Set to the `ref` value from `glStencilFunc` |
| `GL_INCR` | Increment by 1 (clamp at 255) |
| `GL_INCR_WRAP` | Increment by 1 (wrap to 0 after 255) |
| `GL_DECR` | Decrement by 1 (clamp at 0) |
| `GL_DECR_WRAP` | Decrement by 1 (wrap to 255 after 0) |
| `GL_INVERT` | Bitwise invert all bits |

The default is `glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP)` — never modify the stencil buffer.

### Example

```cpp
glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
```

This means:
- If the stencil test fails → keep the current stencil value
- If the stencil test passes but depth fails → keep the current stencil value
- If both pass → replace the stencil value with `ref` (from `glStencilFunc`)

## Practical Example: Object Outlining

The most classic stencil buffer application is drawing colored outlines around objects. This is used everywhere: selected objects in editors, highlighted enemies in games, UI focus indicators.

The strategy is beautifully simple:

```
  Step 1: Draw object normally,        Step 2: Draw scaled-up object
          writing 1 to stencil                  ONLY where stencil ≠ 1

  ┌────────────────────┐               ┌────────────────────┐
  │                    │               │                    │
  │    ┌──────────┐    │               │   ╔══════════════╗ │
  │    │ 1  1  1  │    │               │   ║  ┌────────┐  ║ │
  │    │ 1  1  1  │    │               │   ║  │ object │  ║ │
  │    │ 1  1  1  │    │               │   ║  └────────┘  ║ │
  │    └──────────┘    │               │   ╚══════════════╝ │
  │  (stencil = 1      │               │   (outline only    │
  │   where object is) │               │    drawn outside)  │
  └────────────────────┘               └────────────────────┘
```

Here's the step-by-step algorithm:

### Step 1: Draw Objects Normally, Writing to the Stencil Buffer

Configure the stencil to always pass and write `1` wherever a fragment is rendered:

```cpp
glStencilFunc(GL_ALWAYS, 1, 0xFF); // always pass, ref = 1
glStencilMask(0xFF);               // enable stencil writing
// draw objects
```

After this step, every pixel covered by an object has stencil value `1`. Everything else is `0`.

### Step 2: Draw Scaled-Up Objects Where Stencil ≠ 1

Now disable stencil writing and only draw where the stencil is NOT `1` — which is exactly the outline region:

```cpp
glStencilFunc(GL_NOTEQUAL, 1, 0xFF); // pass only where stencil ≠ 1
glStencilMask(0x00);                  // disable stencil writing
glDisable(GL_DEPTH_TEST);            // outline should draw on top
// draw objects scaled up slightly with a solid color shader
```

The scaled-up version extends slightly beyond the original object. But thanks to the stencil test, only the *extended* pixels (the outline) are rendered — the interior is masked out because the stencil buffer already has `1` there.

### Step 3: Restore State

```cpp
glStencilMask(0xFF);
glStencilFunc(GL_ALWAYS, 0, 0xFF);
glEnable(GL_DEPTH_TEST);
```

## Complete Source Code

Here's a complete implementation that draws two cubes with colored outlines on a floor.

### Single-Color Shader (outline.vs / outline.fs)

The outline shader is minimal — it just outputs a solid color:

**outline.vs:**

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

**outline.fs:**

```glsl
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(0.04, 0.28, 0.26, 1.0);
}
```

### Object Shader (stencil_testing.vs / stencil_testing.fs)

**stencil_testing.vs:**

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

**stencil_testing.fs:**

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture1;

void main()
{
    FragColor = texture(texture1, TexCoords);
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
                                          "Stencil Testing", nullptr, nullptr);
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
    glDepthFunc(GL_LESS);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

    Shader shader("stencil_testing.vs", "stencil_testing.fs");
    Shader outlineShader("outline.vs", "outline.fs");

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

    unsigned int cubeTexture = loadTexture("resources/textures/marble.jpg");
    unsigned int floorTexture = loadTexture("resources/textures/metal.png");

    shader.use();
    shader.setInt("texture1", 0);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT |
                GL_STENCIL_BUFFER_BIT);

        // --- PASS 1: Draw floor (no stencil writing) ---
        outlineShader.use(); // (we need view/proj set on both shaders)
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        outlineShader.setMat4("view", view);
        outlineShader.setMat4("projection", projection);

        shader.use();
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        glStencilMask(0x00); // don't write to stencil for the floor
        glBindVertexArray(planeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTexture);
        glm::mat4 model = glm::mat4(1.0f);
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // --- PASS 2: Draw cubes, writing 1 to stencil buffer ---
        glStencilFunc(GL_ALWAYS, 1, 0xFF);
        glStencilMask(0xFF);

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

        // --- PASS 3: Draw scaled cubes as outlines ---
        glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
        glStencilMask(0x00);
        glDisable(GL_DEPTH_TEST);

        outlineShader.use();
        float scale = 1.1f;

        glBindVertexArray(cubeVAO);
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.0f, 0.0f, -1.0f));
        model = glm::scale(model, glm::vec3(scale));
        outlineShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(2.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(scale));
        outlineShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // --- Restore state ---
        glStencilMask(0xFF);
        glStencilFunc(GL_ALWAYS, 0, 0xFF);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &planeVBO);

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

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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

Two textured cubes sitting on a metal floor, each surrounded by a dark teal outline:

```
  ┌──────────────────────────────────────┐
  │                                      │
  │     ╔══════════╗    ╔══════════╗     │
  │     ║┌────────┐║    ║┌────────┐║     │
  │     ║│ marble │║    ║│ marble │║     │
  │     ║│  cube  │║    ║│  cube  │║     │
  │     ║└────────┘║    ║└────────┘║     │
  │     ╚══════════╝    ╚══════════╝     │
  │  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓  │
  │  ▓▓▓▓▓▓▓▓▓ metal floor ▓▓▓▓▓▓▓▓▓▓  │
  └──────────────────────────────────────┘
```

The outlines are cleanly drawn around the cubes without affecting the floor. Move the camera around to see how the outlines remain consistent from every angle.

---

## Key Takeaways

- The **stencil buffer** is an 8-bit per-pixel buffer you can use as a programmable mask.
- `glStencilFunc` defines the test condition (when fragments pass).
- `glStencilOp` defines what happens to stencil values on fail/pass.
- `glStencilMask` controls which bits can be written.
- The stencil test runs *before* the depth test in the fragment pipeline.
- **Object outlining** is the classic stencil buffer technique: draw normally (writing 1 to stencil), then draw scaled up (only where stencil ≠ 1).

## Exercises

1. Change the outline color to bright orange `(1.0, 0.5, 0.0)`. Then try making the outline color a uniform so you can change it at runtime.
2. Modify the scale factor for the outline. What happens with `1.05`? What about `1.2`? Find a value that looks clean for your scene.
3. Try outlining only one of the two cubes. You'll need to be more careful about when you write to the stencil buffer.
4. Instead of scaling the object uniformly, try scaling only along the surface normals. This produces more uniform outline thickness. (Hint: in the outline vertex shader, offset each vertex along its normal by a small amount.)
5. Use the stencil buffer to create a "window" effect: render a quad that writes to the stencil buffer, then only render a second scene where the stencil was written. This is the basis for portal rendering.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Depth Testing](01_depth_testing.md) | [Table of Contents](../../README.md) | [Next: Blending →](03_blending.md) |
