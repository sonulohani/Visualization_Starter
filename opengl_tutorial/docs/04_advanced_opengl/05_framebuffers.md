# Framebuffers

[← Previous: Face Culling](04_face_culling.md) | [Next: Cubemaps →](06_cubemaps.md)

---

Up until now, every pixel we've rendered has gone straight to the screen. But what if you could render your entire scene to an off-screen image, then manipulate that image before displaying it? That's exactly what **framebuffers** let you do — and they unlock one of the most powerful categories of visual effects in real-time graphics: **post-processing**.

Inversion, grayscale, blur, sharpen, edge detection, bloom, HDR, motion blur — all of these are implemented by rendering the scene to a framebuffer, then processing the resulting image in a second pass.

## What Is a Framebuffer?

A **framebuffer** is a collection of buffers (color, depth, stencil) that serve as a render target. When you draw triangles, the results go into whatever framebuffer is currently bound.

You've actually been using a framebuffer all along — the **default framebuffer**. GLFW creates it when you open a window, and it's what `glfwSwapBuffers` displays on screen. Its ID is `0`.

```
  Default framebuffer (ID 0):
  ┌────────────────────────────────────┐
  │  Color buffer   (what you see)     │
  │  Depth buffer   (GL_DEPTH_TEST)    │
  │  Stencil buffer (GL_STENCIL_TEST)  │
  └────────────────────────────────────┘
          │
          ▼
      The screen
```

A **custom framebuffer** (also called an FBO — Framebuffer Object) works identically, but renders to textures or renderbuffers that *you* create. You can then read these textures in subsequent draw calls.

```
  Custom framebuffer (ID != 0):
  ┌────────────────────────────────────┐
  │  Color attachment  → Texture       │─── You can sample this texture!
  │  Depth/Stencil     → Renderbuffer  │
  └────────────────────────────────────┘
```

## Creating a Framebuffer

### Step 1: Generate and Bind

```cpp
unsigned int framebuffer;
glGenFramebuffers(1, &framebuffer);
glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
```

After binding, all subsequent render commands will draw into this framebuffer instead of the screen.

### Step 2: Create a Color Attachment (Texture)

The framebuffer needs somewhere to store color data. We create a texture for this:

```cpp
unsigned int textureColorbuffer;
glGenTextures(1, &textureColorbuffer);
glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0,
             GL_RGB, GL_UNSIGNED_BYTE, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

Note: we pass `NULL` as the data pointer — we're allocating the texture's memory but not filling it. The rendering process will fill it. No mipmaps needed since we'll always sample it at its exact size.

Now attach it to the framebuffer:

```cpp
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D, textureColorbuffer, 0);
```

The parameters:
- `GL_FRAMEBUFFER` — the target (could also be `GL_READ_FRAMEBUFFER` or `GL_DRAW_FRAMEBUFFER`)
- `GL_COLOR_ATTACHMENT0` — which attachment point (framebuffers can have multiple color attachments)
- `GL_TEXTURE_2D` — the texture type
- `textureColorbuffer` — the actual texture
- `0` — mipmap level

### Step 3: Create a Depth/Stencil Attachment (Renderbuffer)

We also need depth (and optionally stencil) testing. For this, we use a **renderbuffer** — a buffer optimized for use as a render target that you *don't* need to sample later:

```cpp
unsigned int rbo;
glGenRenderbuffers(1, &rbo);
glBindRenderbuffer(GL_RENDERBUFFER, rbo);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                      SCR_WIDTH, SCR_HEIGHT);
```

`GL_DEPTH24_STENCIL8` packs 24 bits of depth and 8 bits of stencil into a single buffer — the most common format.

Attach it:

```cpp
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                          GL_RENDERBUFFER, rbo);
```

### Renderbuffer vs Texture

Why use a renderbuffer for depth/stencil instead of a texture?

| | Texture attachment | Renderbuffer attachment |
|---|---|---|
| **Can sample in shader?** | Yes | No |
| **Performance** | Slightly slower | Optimized for render target |
| **Use when** | You need to read depth/stencil later (shadow maps) | You just need depth/stencil testing |

For post-processing, we only need to read the color — the depth/stencil just needs to work, not be readable. A renderbuffer is the right choice.

### Step 4: Check Completeness

A framebuffer must have at least one attachment and all attachments must be complete (allocated with correct dimensions). Always check:

```cpp
if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!"
              << std::endl;
```

### Step 5: Unbind

Don't forget to re-bind the default framebuffer so your normal rendering isn't affected:

```cpp
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

## The Rendering Strategy

With a custom framebuffer, our render loop changes to a two-pass approach:

```
  PASS 1: Render scene to custom framebuffer
  ┌──────────────────────────────────────────┐
  │  glBindFramebuffer(GL_FRAMEBUFFER, fbo)  │
  │  glClear(...)                            │
  │  draw cubes, floor, etc.                 │ → color goes to TEXTURE
  │                                          │ → depth goes to RBO
  └──────────────────────────────────────────┘

  PASS 2: Render a screen quad with the texture
  ┌──────────────────────────────────────────┐
  │  glBindFramebuffer(GL_FRAMEBUFFER, 0)    │
  │  glClear(...)                            │
  │  bind the framebuffer's color texture    │
  │  draw a fullscreen quad                  │ → applies post-processing
  │                                          │   in fragment shader
  └──────────────────────────────────────────┘
```

The "screen quad" is just two triangles that cover the entire screen, textured with the framebuffer's color attachment. The fragment shader for this quad is where the magic happens — we can apply any image processing effect we want.

## The Screen Quad

A screen quad in NDC coordinates (from -1 to 1) with texture coordinates:

```cpp
float quadVertices[] = {
    // positions   // texCoords
    -1.0f,  1.0f,  0.0f, 1.0f,
    -1.0f, -1.0f,  0.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 0.0f,

    -1.0f,  1.0f,  0.0f, 1.0f,
     1.0f, -1.0f,  1.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 1.0f
};
```

```
  NDC coordinates:             Texture coordinates:

  (-1, 1)────────(1, 1)        (0,1)────────(1,1)
     │              │             │              │
     │  screen      │             │   texture    │
     │  quad        │             │   mapping    │
     │              │             │              │
  (-1,-1)────────(1,-1)        (0,0)────────(1,0)
```

Set up its VAO:

```cpp
unsigned int quadVAO, quadVBO;
glGenVertexArrays(1, &quadVAO);
glGenBuffers(1, &quadVBO);
glBindVertexArray(quadVAO);
glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices,
             GL_STATIC_DRAW);
glEnableVertexAttribArray(0);
glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                      (void*)0);
glEnableVertexAttribArray(1);
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                      (void*)(2 * sizeof(float)));
```

### Basic Screen Shader

The simplest screen shader just passes the texture through:

**screen.vs:**

```glsl
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
```

**screen.fs:**

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;

void main()
{
    FragColor = texture(screenTexture, TexCoords);
}
```

If you render with this shader, the result is identical to not using a framebuffer at all. The power comes when you modify the fragment shader.

## Post-Processing Effects

### 1. Color Inversion

Invert every color component:

```glsl
void main()
{
    vec3 color = texture(screenTexture, TexCoords).rgb;
    FragColor = vec4(1.0 - color, 1.0);
}
```

```
  Original:                    Inverted:
  ┌────────────────────┐       ┌────────────────────┐
  │   ██ bright ██     │       │   ░░ dark   ░░     │
  │   ░░ dark   ░░     │       │   ██ bright ██     │
  │   ▓▓ medium ▓▓     │       │   ▒▒ medium ▒▒     │
  └────────────────────┘       └────────────────────┘
```

### 2. Grayscale

Convert to grayscale. The simple approach averages the channels, but the weighted version accounts for human perception (we're more sensitive to green):

```glsl
void main()
{
    vec3 color = texture(screenTexture, TexCoords).rgb;
    // Perceptually weighted grayscale
    float gray = dot(color, vec3(0.2126, 0.7152, 0.0722));
    FragColor = vec4(vec3(gray), 1.0);
}
```

The weights (0.2126, 0.7152, 0.0722) come from the ITU-R BT.709 standard — the same one used in HDTV.

### 3. Kernel Effects (Convolution)

This is where things get really interesting. A **kernel** (or convolution matrix) is a small grid of values — typically 3×3 — that is applied to each pixel and its neighbors. The center value is multiplied with the current pixel, and the surrounding values are multiplied with the neighboring pixels. The results are summed to produce the output.

```
  A 3×3 kernel applied to pixel P:

  ┌─────┬─────┬─────┐
  │ k00 │ k01 │ k02 │     output = k00×TL + k01×T + k02×TR
  ├─────┼─────┼─────┤            + k10×L  + k11×P + k12×R
  │ k10 │ k11 │ k12 │            + k20×BL + k21×B + k22×BR
  ├─────┼─────┼─────┤
  │ k20 │ k21 │ k22 │     (TL=top-left, T=top, TR=top-right, etc.)
  └─────┴─────┴─────┘
```

### Implementing Kernel Convolution

In the fragment shader, we sample the 9 neighbors and apply the kernel:

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;

const float offset = 1.0 / 300.0; // depends on resolution

void main()
{
    vec2 offsets[9] = vec2[](
        vec2(-offset,  offset), // top-left
        vec2( 0.0f,    offset), // top-center
        vec2( offset,  offset), // top-right
        vec2(-offset,  0.0f),   // center-left
        vec2( 0.0f,    0.0f),   // center-center
        vec2( offset,  0.0f),   // center-right
        vec2(-offset, -offset), // bottom-left
        vec2( 0.0f,   -offset), // bottom-center
        vec2( offset, -offset)  // bottom-right
    );

    // === SHARPEN KERNEL ===
    float kernel[9] = float[](
        -1, -1, -1,
        -1,  9, -1,
        -1, -1, -1
    );

    vec3 sampleTex[9];
    for (int i = 0; i < 9; i++)
    {
        sampleTex[i] = vec3(texture(screenTexture,
                                     TexCoords + offsets[i]));
    }
    vec3 color = vec3(0.0);
    for (int i = 0; i < 9; i++)
        color += sampleTex[i] * kernel[i];

    FragColor = vec4(color, 1.0);
}
```

### Common Kernels

**Sharpen** — enhances edges and detail:

```
  ┌────┬────┬────┐
  │ -1 │ -1 │ -1 │
  ├────┼────┼────┤
  │ -1 │  9 │ -1 │
  ├────┼────┼────┤
  │ -1 │ -1 │ -1 │
  └────┴────┴────┘
```

```glsl
float kernel[9] = float[](
    -1, -1, -1,
    -1,  9, -1,
    -1, -1, -1
);
```

**Blur** — averages neighboring pixels for a softening effect:

```
  ┌──────┬──────┬──────┐
  │ 1/16 │ 2/16 │ 1/16 │
  ├──────┼──────┼──────┤
  │ 2/16 │ 4/16 │ 2/16 │
  ├──────┼──────┼──────┤
  │ 1/16 │ 2/16 │ 1/16 │
  └──────┴──────┴──────┘
```

```glsl
float kernel[9] = float[](
    1.0 / 16, 2.0 / 16, 1.0 / 16,
    2.0 / 16, 4.0 / 16, 2.0 / 16,
    1.0 / 16, 2.0 / 16, 1.0 / 16
);
```

**Edge Detection** — highlights edges, everything else goes black:

```
  ┌────┬────┬────┐
  │  1 │  1 │  1 │
  ├────┼────┼────┤
  │  1 │ -8 │  1 │
  ├────┼────┼────┤
  │  1 │  1 │  1 │
  └────┴────┴────┘
```

```glsl
float kernel[9] = float[](
    1,  1,  1,
    1, -8,  1,
    1,  1,  1
);
```

> **Note:** Kernels whose values sum to 1 preserve overall brightness (like blur). Kernels whose values sum to 0 produce edge-detected or embossed images. Kernels summing to more than 1 brighten the image.

## Complete Source Code

Here's a complete program that renders a scene to a framebuffer and applies post-processing effects. Press `1`–`5` to switch between: normal, inversion, grayscale, sharpen, blur, and edge detection.

### Scene Vertex Shader (framebuffer_scene.vs)

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

### Scene Fragment Shader (framebuffer_scene.fs)

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

### Screen Vertex Shader (framebuffer_screen.vs)

```glsl
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
```

### Screen Fragment Shader (framebuffer_screen.fs)

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform int effect; // 0=normal, 1=invert, 2=grayscale, 3=sharpen, 4=blur, 5=edge

const float offset = 1.0 / 300.0;

void main()
{
    if (effect == 0)
    {
        // Normal — pass through
        FragColor = texture(screenTexture, TexCoords);
    }
    else if (effect == 1)
    {
        // Inversion
        FragColor = vec4(1.0 - texture(screenTexture, TexCoords).rgb, 1.0);
    }
    else if (effect == 2)
    {
        // Grayscale (perceptual weights)
        vec3 color = texture(screenTexture, TexCoords).rgb;
        float gray = dot(color, vec3(0.2126, 0.7152, 0.0722));
        FragColor = vec4(vec3(gray), 1.0);
    }
    else
    {
        // Kernel-based effects
        vec2 offsets[9] = vec2[](
            vec2(-offset,  offset),
            vec2( 0.0f,    offset),
            vec2( offset,  offset),
            vec2(-offset,  0.0f),
            vec2( 0.0f,    0.0f),
            vec2( offset,  0.0f),
            vec2(-offset, -offset),
            vec2( 0.0f,   -offset),
            vec2( offset, -offset)
        );

        float kernel[9];
        if (effect == 3)
        {
            // Sharpen
            kernel = float[](
                -1, -1, -1,
                -1,  9, -1,
                -1, -1, -1
            );
        }
        else if (effect == 4)
        {
            // Blur (Gaussian approximation)
            kernel = float[](
                1.0/16, 2.0/16, 1.0/16,
                2.0/16, 4.0/16, 2.0/16,
                1.0/16, 2.0/16, 1.0/16
            );
        }
        else if (effect == 5)
        {
            // Edge detection
            kernel = float[](
                 1,  1,  1,
                 1, -8,  1,
                 1,  1,  1
            );
        }

        vec3 color = vec3(0.0);
        for (int i = 0; i < 9; i++)
            color += vec3(texture(screenTexture,
                                   TexCoords + offsets[i])) * kernel[i];

        FragColor = vec4(color, 1.0);
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

int currentEffect = 0;

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
                                          "Framebuffers", nullptr, nullptr);
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

    Shader sceneShader("framebuffer_scene.vs", "framebuffer_scene.fs");
    Shader screenShader("framebuffer_screen.vs", "framebuffer_screen.fs");

    // --- Scene geometry ---
    float cubeVertices[] = {
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

    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
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

    // Screen quad VAO
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));

    unsigned int cubeTexture = loadTexture("resources/textures/marble.jpg");
    unsigned int floorTexture = loadTexture("resources/textures/metal.png");

    sceneShader.use();
    sceneShader.setInt("texture1", 0);
    screenShader.use();
    screenShader.setInt("screenTexture", 0);

    // --- Create framebuffer ---
    unsigned int framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // Color attachment (texture)
    unsigned int textureColorbuffer;
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, textureColorbuffer, 0);

    // Depth+stencil attachment (renderbuffer)
    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!"
                  << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        // ========== PASS 1: Render scene to framebuffer ==========
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        sceneShader.use();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        sceneShader.setMat4("view", view);
        sceneShader.setMat4("projection", projection);

        // Cubes
        glBindVertexArray(cubeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cubeTexture);
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.0f, 0.0f, -1.0f));
        sceneShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(2.0f, 0.0f, 0.0f));
        sceneShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Floor
        glBindVertexArray(planeVAO);
        glBindTexture(GL_TEXTURE_2D, floorTexture);
        model = glm::mat4(1.0f);
        sceneShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // ========== PASS 2: Render screen quad with effect ==========
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDisable(GL_DEPTH_TEST);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        screenShader.use();
        screenShader.setInt("effect", currentEffect);
        glBindVertexArray(quadVAO);
        glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteFramebuffers(1, &framebuffer);

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

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) currentEffect = 0;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) currentEffect = 1;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) currentEffect = 2;
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) currentEffect = 3;
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) currentEffect = 4;
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) currentEffect = 5;
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

The scene renders normally by default (press `1`). Try each effect:

- **1** — Normal: the scene as-is
- **2** — Inversion: all colors flipped (dark becomes light, etc.)
- **3** — Grayscale: the scene in black and white
- **4** — Sharpen: edges pop, details become crisp
- **5** — Blur: a soft, dreamy look
- **6** — Edge Detection: only outlines visible, everything else is black

```
  Normal (1):           Inversion (2):        Edge Detection (6):

  ┌──────────────┐      ┌──────────────┐      ┌──────────────┐
  │  ┌──┐  ┌──┐ │      │  ┌──┐  ┌──┐ │      │  ┌──┐  ┌──┐ │
  │  │██│  │██│ │      │  │░░│  │░░│ │      │  ││ │  ││ │ │
  │  └──┘  └──┘ │      │  └──┘  └──┘ │      │  └──┘  └──┘ │
  │▓▓▓▓▓▓▓▓▓▓▓▓▓│      │░░░░░░░░░░░░░│      │─────────────│
  └──────────────┘      └──────────────┘      └──────────────┘
```

---

## Key Takeaways

- A **framebuffer** is a render target consisting of color, depth, and stencil attachments.
- The **default framebuffer** (ID 0) is the screen. Custom framebuffers render to textures.
- Use **texture attachments** for color (you'll sample them later) and **renderbuffer attachments** for depth/stencil (faster, write-only).
- Always check `glCheckFramebufferStatus` after setting up attachments.
- The **two-pass rendering** pattern (scene → FBO → screen quad with effect) is the foundation of all post-processing.
- **Kernel convolution** (3×3 matrix applied to pixel neighbors) enables sharpen, blur, and edge detection effects.

## Exercises

1. Create an "emboss" effect using this kernel: `(-2, -1, 0, -1, 1, 1, 0, 1, 2)`. How does it compare to edge detection?
2. Implement a **vignette** effect: darken the edges of the screen based on distance from the center. (Hint: compute `length(TexCoords - 0.5)` and use it to darken.)
3. Combine two effects: apply grayscale AND edge detection at the same time. You'll need to first convert to grayscale, then apply the kernel.
4. The blur kernel is very weak (3×3). Implement a stronger blur by doing multiple passes — render to a second framebuffer with blur, then blur that result again into the first framebuffer, and repeat. This is the basis of Gaussian blur.
5. Make the framebuffer size different from the window size (e.g., half resolution). Render the scene at low resolution, then display it full-screen. This is one form of resolution scaling used in games for performance.
6. Render the scene into a smaller viewport within the framebuffer (like a picture-in-picture) by using `glViewport` before the scene pass.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Face Culling](04_face_culling.md) | [Table of Contents](../../README.md) | [Next: Cubemaps →](06_cubemaps.md) |
