# Depth Testing

[Previous: Model Loading](../03_model_loading/03_model.md) | [Next: Stencil Testing →](02_stencil_testing.md)

---

We've actually been using depth testing since we started rendering 3D scenes — that `glEnable(GL_DEPTH_TEST)` and `glClear(GL_DEPTH_BUFFER_BIT)` you've been writing in every project. But what exactly is happening behind the scenes? Understanding depth testing deeply will help you debug visual artifacts, implement advanced effects, and appreciate one of the most fundamental algorithms in real-time rendering.

## The Depth Buffer (Z-Buffer)

When you render a 3D scene, multiple fragments (potential pixels) can map to the same screen position. A cube face behind a wall, a character behind a pillar — OpenGL needs to decide which fragment actually gets displayed. This is called the **visibility problem**, and the depth buffer solves it.

The **depth buffer** (also called the **Z-buffer**) is a screen-sized buffer that stores one depth value per pixel. Every time a fragment is about to be written to the screen, OpenGL compares its depth value against whatever is already stored in the depth buffer at that position:

```
Fragment arrives at pixel (320, 240) with depth 0.7

  Depth buffer at (320, 240) currently stores: 0.5

  0.7 > 0.5 → this fragment is FARTHER away → DISCARD it

  ─────────────────────────────────────────────────

Fragment arrives at pixel (100, 100) with depth 0.3

  Depth buffer at (100, 100) currently stores: 0.8

  0.3 < 0.8 → this fragment is CLOSER → KEEP it, update depth to 0.3
```

This happens automatically for every fragment, millions of times per frame. The result: closer objects correctly occlude farther ones, regardless of the order you draw them in.

```
  Without depth testing:          With depth testing:

  ┌───────────────────┐           ┌───────────────────┐
  │   ┌─────┐         │           │   ┌─────┐         │
  │   │ Box │         │           │   │ Box │         │
  │   │ ┌───┼───┐     │           │   │ ┌───┤   │     │
  │   └─┼───┘   │     │           │   └─┤   │   │     │
  │     │ Wall  │     │           │     │Wall│   │     │
  │     └───────┘     │           │     └───────┘     │
  └───────────────────┘           └───────────────────┘
  (last drawn wins)               (closest wins)
```

## What We've Been Doing

Since the lighting chapters, our setup code has included:

```cpp
glEnable(GL_DEPTH_TEST);
```

And our render loop clears the depth buffer each frame:

```cpp
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
```

Without `glEnable(GL_DEPTH_TEST)`, OpenGL skips the depth comparison entirely and just overwrites whatever was there before — the last object drawn "wins." Without clearing the depth buffer each frame, depth values from the previous frame persist, causing bizarre flickering as new fragments fight against stale depth data.

## The Depth Test Function

By default, a fragment passes the depth test if its depth is **less than** the stored value (i.e., it's closer to the camera). You can change this behavior with `glDepthFunc`:

```cpp
glDepthFunc(GL_LESS);  // this is the default
```

Here are all available comparison functions:

| Function | Passes if | Use case |
|---|---|---|
| `GL_ALWAYS` | Always | Disables depth testing (everything drawn) |
| `GL_NEVER` | Never | Nothing passes (useful for debugging) |
| `GL_LESS` | fragment < buffer | **Default** — closer fragments win |
| `GL_EQUAL` | fragment == buffer | Only exact depth matches (multi-pass) |
| `GL_LEQUAL` | fragment <= buffer | Closer or equal (used for skyboxes) |
| `GL_GREATER` | fragment > buffer | Farther fragments win (reverse depth) |
| `GL_NOTEQUAL` | fragment != buffer | Anything except exact match |
| `GL_GEQUAL` | fragment >= buffer | Farther or equal |

For most scenarios, `GL_LESS` is exactly what you want. We'll use `GL_LEQUAL` when we implement skyboxes in the cubemaps chapter.

## Depth Value Precision

### The Depth Range

Depth values in OpenGL are stored as floating-point numbers in the range `[0.0, 1.0]`:

- `0.0` = on the **near plane** (closest to camera)
- `1.0` = on the **far plane** (farthest from camera)

The depth buffer is cleared to `1.0` each frame (maximum distance), so any fragment that renders will be closer and will pass the default `GL_LESS` test.

### Linear vs Non-Linear Depth

You might expect depth to be distributed linearly between the near and far planes. If the near plane is at `0.1` and the far plane is at `100.0`, an object at distance `50.0` would be at depth `0.5`, right?

**No.** OpenGL uses a **non-linear** depth distribution. The depth value is computed as:

```
              1/z  -  1/near
F_depth  =  ─────────────────
             1/far  -  1/near
```

This produces a distribution where most of the precision is concentrated near the camera:

```
Distance (z)    Linear depth    Actual depth (non-linear)
──────────────────────────────────────────────────────────
     0.1            0.000              0.000
     1.0            0.009              0.900
     2.0            0.019              0.950
     5.0            0.049              0.980
    10.0            0.099              0.990
    50.0            0.499              0.998
   100.0            1.000              1.000
```

```
Depth value
1.0 ┤                              ·················
    │                      ········
    │                  ····
    │               ···
    │            ···
    │          ··
    │        ··
    │      ··
    │    ··
    │  ··
0.0 ┤··
    └──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬─ Distance
      0.1  10  20  30  40  50  60  70  80  90  100
         near                              far
```

### Why Non-Linear?

This distribution is intentional and actually quite clever. Think about what you see in a typical scene:

- Objects near the camera (a weapon, a dashboard, your hands) occupy a large portion of the screen and need precise depth sorting to avoid visual artifacts.
- Objects far away (distant mountains, the skybox) occupy fewer pixels and small depth errors are invisible.

The `1/z` relationship gives roughly **90% of the depth precision to the first 10% of the distance range**. This is exactly where you need it most.

> **Tip:** Choose your near and far planes carefully. A near plane that's too small (e.g., `0.001`) wastes precious precision. A good starting point is `near = 0.1` and `far = 100.0`. Only increase the range if your scene requires it.

## Visualizing the Depth Buffer

Understanding the depth buffer becomes much easier when you can actually *see* it. OpenGL makes the current fragment's depth value available in the fragment shader via `gl_FragCoord.z`.

### Raw Depth Values

The simplest visualization just outputs the depth value as a grayscale color:

```glsl
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(vec3(gl_FragCoord.z), 1.0);
}
```

If you run this, you'll likely see an almost entirely **white** image with very subtle variations. This isn't a bug — it's the non-linear depth distribution at work. Most of the objects in your scene are at depth values above `0.95`, even if they're relatively close to the camera.

```
  What you expect:          What you actually see:

  ┌──────────────────┐      ┌──────────────────┐
  │  ░░ ██ ░░░░░░░░  │      │  ▓▓ ██ ▓▓▓▓▓▓▓▓  │
  │  ░░ ██ ░░ ██ ░░  │      │  ▓▓ ██ ▓▓ ██ ▓▓  │
  │  ░░░░░░░░ ██ ░░  │      │  ▓▓▓▓▓▓▓▓ ██ ▓▓  │
  │  ░░░░░░░░░░░░░░  │      │  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓  │
  └──────────────────┘      └──────────────────┘
  (nice gradient)            (almost all white)
```

### Linearizing Depth

To get a meaningful visualization, we need to reverse the non-linear transformation and convert back to linear depth:

```glsl
#version 330 core
out vec4 FragColor;

float near = 0.1;
float far  = 100.0;

float LinearizeDepth(float depth)
{
    float ndc = depth * 2.0 - 1.0; // back to NDC range [-1, 1]
    return (2.0 * near * far) / (far + near - ndc * (far - near));
}

void main()
{
    float depth = LinearizeDepth(gl_FragCoord.z) / far; // normalize to [0, 1]
    FragColor = vec4(vec3(depth), 1.0);
}
```

The math works as follows:

1. `gl_FragCoord.z` is in `[0, 1]`. We remap it to NDC (Normalized Device Coordinates) range `[-1, 1]` by doing `depth * 2.0 - 1.0`.
2. We apply the inverse of the projection's depth transformation to recover the actual linear distance from the camera.
3. We divide by `far` to normalize the result back to `[0, 1]` for display.

Now closer objects appear dark (low depth value) and farther objects appear brighter — a smooth gradient that actually conveys distance.

## Z-Fighting

When two surfaces are extremely close to each other (or coplanar), the limited precision of the depth buffer can't reliably determine which one is in front. The result is **Z-fighting**: a shimmering, flickering pattern where pixels alternate between the two surfaces frame to frame.

```
  Two overlapping surfaces with Z-fighting:

  ┌────────────────────────────────┐
  │▓▓░░▓▓▓▓░░▓▓░░░░▓▓▓▓░░▓▓▓▓░░▓▓│
  │░░▓▓░░▓▓▓▓░░▓▓▓▓░░▓▓░░░░▓▓░░▓▓│  ← pixels flicker
  │▓▓▓▓░░░░▓▓▓▓░░▓▓▓▓░░▓▓▓▓░░▓▓░░│     between surfaces
  │░░▓▓▓▓░░▓▓░░▓▓░░▓▓▓▓░░▓▓▓▓▓▓░░│
  └────────────────────────────────┘
```

Z-fighting is especially common with:

- A floor plane and a decal/shadow quad placed directly on it
- Coplanar walls at the same depth
- Distant objects where depth precision is extremely low

### Preventing Z-Fighting

**1. Don't place surfaces too close together.** Even a tiny offset (e.g., `0.001` units) is usually enough to separate them in depth.

**2. Increase near plane distance.** Moving the near plane from `0.001` to `0.1` dramatically improves depth precision throughout the scene:

```cpp
glm::mat4 projection = glm::perspective(glm::radians(45.0f),
    (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
//                                        ^^^
//                              keep this as large as practical
```

**3. Use `glPolygonOffset`.** This tells OpenGL to slightly offset the depth values of subsequent draw calls. It's commonly used for rendering decals or shadow maps:

```cpp
glEnable(GL_POLYGON_OFFSET_FILL);
glPolygonOffset(1.0f, 1.0f);  // offset factor and units
// draw the surface that should be "on top"
glDisable(GL_POLYGON_OFFSET_FILL);
```

The depth offset is computed as: `offset = factor × DZ + r × units` where `DZ` is the maximum depth slope of the polygon and `r` is the smallest value that produces a resolvable depth difference.

## Depth Mask

Sometimes you want to test *against* the depth buffer without *writing to* it. This is done with `glDepthMask`:

```cpp
glDepthMask(GL_FALSE); // disable depth writes
// fragments are still tested against the depth buffer,
// but passing fragments don't update the stored depth
```

```cpp
glDepthMask(GL_TRUE);  // re-enable depth writes (default)
```

This is useful when rendering transparent objects: you want them to be occluded by opaque objects (so depth testing should be on), but you don't want them to block other transparent objects behind them (so depth writing should be off). We'll use this technique extensively in the blending chapter.

## Complete Source Code

Here is a complete program that renders a scene with two cubes and a floor, with a toggle between normal rendering, raw depth visualization, and linearized depth visualization. Press `1`, `2`, or `3` to switch modes.

### Vertex Shader (depth_testing.vs)

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

### Fragment Shader (depth_testing.fs)

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture1;
uniform int displayMode; // 0 = normal, 1 = raw depth, 2 = linear depth

float near = 0.1;
float far  = 100.0;

float LinearizeDepth(float depth)
{
    float ndc = depth * 2.0 - 1.0;
    return (2.0 * near * far) / (far + near - ndc * (far - near));
}

void main()
{
    if (displayMode == 0)
    {
        FragColor = texture(texture1, TexCoords);
    }
    else if (displayMode == 1)
    {
        FragColor = vec4(vec3(gl_FragCoord.z), 1.0);
    }
    else if (displayMode == 2)
    {
        float depth = LinearizeDepth(gl_FragCoord.z) / far;
        FragColor = vec4(vec3(depth), 1.0);
    }
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

int displayMode = 0; // 0 = normal, 1 = raw depth, 2 = linear depth

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
                                          "Depth Testing", nullptr, nullptr);
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

    Shader shader("depth_testing.vs", "depth_testing.fs");

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
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        shader.setInt("displayMode", displayMode);

        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        // Draw cubes
        glBindVertexArray(cubeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cubeTexture);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.0f, 0.0f, -1.0f));
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(2.0f, 0.0f, 0.0f));
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Draw floor
        glBindVertexArray(planeVAO);
        glBindTexture(GL_TEXTURE_2D, floorTexture);
        model = glm::mat4(1.0f);
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 6);

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

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        displayMode = 0; // normal
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        displayMode = 1; // raw depth
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
        displayMode = 2; // linear depth
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

- **Mode 1** (press `1`): The scene renders normally with textured cubes on a metal floor.
- **Mode 2** (press `2`): Everything appears almost white — this is the raw non-linear depth buffer.
- **Mode 3** (press `3`): A smooth gradient from dark (near) to light (far) — the linearized depth buffer. Move the camera closer and farther to see the values change.

---

## Key Takeaways

- The **depth buffer** stores a per-pixel depth value to resolve which fragment is closest to the camera.
- Always `glEnable(GL_DEPTH_TEST)` and `glClear(GL_DEPTH_BUFFER_BIT)` each frame.
- OpenGL uses **non-linear depth** (`1/z` relationship) to concentrate precision near the camera.
- `gl_FragCoord.z` gives you the fragment's depth in the shader — useful for visualization and special effects.
- **Z-fighting** occurs when two surfaces are too close for the depth buffer to distinguish; fix it with offsets, better near/far ratios, or `glPolygonOffset`.
- `glDepthMask(GL_FALSE)` disables depth writes while keeping depth tests active.

## Exercises

1. Modify the depth test function to `GL_ALWAYS`. What happens? What about `GL_GREATER`? Think about why the scene looks the way it does with each function.
2. Experiment with different near and far plane values. Set `near = 0.001` and `far = 1000.0` — create two closely overlapping quads and observe Z-fighting. Then try `near = 0.1` and `far = 100.0` and compare.
3. Add a third visualization mode that shows depth as a heatmap (e.g., blue for close, red for far) instead of grayscale. Use the linearized depth to drive the color mixing.
4. Try disabling `glClear(GL_DEPTH_BUFFER_BIT)` in the render loop. What visual artifact do you see? Why does it happen?
5. Render a floor plane at exactly `y = 0.0` and a second plane at `y = 0.001`. Move the camera far away and observe Z-fighting on distant parts. Then try using `glPolygonOffset` to fix it.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Model Loading](../03_model_loading/03_model.md) | [Table of Contents](../../README.md) | [Next: Stencil Testing →](02_stencil_testing.md) |
