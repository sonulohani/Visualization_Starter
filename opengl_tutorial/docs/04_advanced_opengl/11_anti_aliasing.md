# Anti-Aliasing

Look closely at any edge in your OpenGL scenes — the border of a cube against the background, the silhouette of a character, the edge of a textured quad. You'll notice tiny staircase-like steps, especially on diagonal and curved edges. These artifacts are called **aliasing** (or "jaggies"), and they're an unavoidable consequence of rasterizing smooth, continuous geometry onto a discrete pixel grid.

In this chapter we'll understand why aliasing happens, learn how **Multisample Anti-Aliasing (MSAA)** works, and implement it both for on-screen rendering and off-screen framebuffers.

---

## Why Aliasing Happens

A pixel is a small square on your screen. When a triangle's edge cuts diagonally across the pixel grid, the rasterizer must make a binary decision for each pixel: is the pixel covered by the triangle or not? There's no "partially covered" — it's all or nothing. The result is a staircase pattern:

```
Ideal diagonal line:         Rasterized result:

      ╲                       ┌──┐
       ╲                      │██│
        ╲                     │██├──┐
         ╲                    └──┤██│
          ╲                      │██├──┐
           ╲                     └──┤██│
            ╲                       │██├──┐
             ╲                      └──┤██│
                                       └──┘
                              "Staircase" / "jaggies"
```

Each step in the staircase is a pixel-wide jump. On a 1080p monitor, this is quite visible. On a 4K monitor it's less noticeable (smaller pixels = smaller steps), but it never fully disappears.

The problem is worse for:
- Nearly-horizontal or nearly-vertical edges (gentle angles create long steps)
- High-contrast boundaries (dark object on bright background)
- Thin geometry (wires, antennas, thin poles)

---

## Super Sample Anti-Aliasing (SSAA)

The brute-force solution: render the scene at a higher resolution (e.g., 4× the pixels), then downsample to the target resolution. Each displayed pixel becomes the average of multiple rendered pixels, smoothing out the staircases.

```
4× SSAA:
┌───┬───┐
│ a │ b │  Render at 2× width, 2× height
├───┼───┤  Final pixel = average(a, b, c, d)
│ c │ d │
└───┴───┘
  → one smooth pixel
```

SSAA produces excellent results, but it's extremely expensive: 4× SSAA means 4× the fragments to shade, 4× the memory for the framebuffer, and 4× the bandwidth. It's essentially rendering at 4K to display at 1080p. Modern games almost never use SSAA for this reason.

---

## Multisample Anti-Aliasing (MSAA)

MSAA is the standard hardware anti-aliasing technique. It achieves most of SSAA's quality at a fraction of the cost by being smart about what it samples multiple times.

### How MSAA Works

With regular rendering, OpenGL tests whether the **center point** of each pixel is inside the triangle. With 4× MSAA, each pixel has **4 sample points** at sub-pixel positions:

```
Regular rendering (1 sample):      4× MSAA (4 samples):
┌─────────────┐                    ┌─────────────┐
│             │                    │  ●       ●  │
│      ●      │                    │             │
│   (center)  │                    │       ●     │
│             │                    │  ●          │
└─────────────┘                    └─────────────┘
  1 coverage test                    4 coverage tests
```

For each pixel, MSAA determines:
1. **Coverage** — how many of the 4 sample points are inside the triangle
2. **Color** — calculated once (at the pixel center or centroid), not 4 times
3. **Storage** — the color is stored for each *covered* sample point

This is the key insight: the expensive fragment shader runs **once per pixel** (not once per sample), but the coverage test runs per sample. This makes MSAA much cheaper than SSAA.

### MSAA at a Triangle Edge

Let's trace what happens at an edge with 4× MSAA:

```
Triangle edge crosses this pixel:

┌─────────────────┐
│ ●(out)  ●(in)   │       Coverage: 2 out of 4 samples are inside
│    ╲             │       Fragment shader runs once → computes color C
│      ╲  ●(in)   │       Stored: samples 2,3 get color C
│ ●(out) ╲        │               samples 1,4 keep background color B
└─────────────────┘
                           Final pixel color = average of all 4 samples
                           = (B + C + C + B) / 4  =  50% triangle, 50% background
```

The result is a smooth blend at the edge instead of an abrupt on/off transition. Interior pixels (where all 4 samples are covered) look identical to non-MSAA rendering — only edge pixels are affected.

```
Without MSAA:                With 4× MSAA:

  bg  bg  bg  bg               bg  bg  bg  bg
  bg  ██  ██  bg               bg  ▓▓  ██  bg      ▓▓ = partially covered
  bg  ██  ██  bg               bg  ██  ██  ▓▓           (blended color)
  bg  bg  bg  bg               bg  ▓▓  ▓▓  bg

  (hard edges)                 (smooth edges)
```

---

## Enabling MSAA for On-Screen Rendering

Enabling MSAA for your default framebuffer (the window) is surprisingly simple — GLFW and OpenGL handle all the complexity:

### Step 1: Request Multisampled Buffers

Before creating the window, tell GLFW you want multisampled buffers:

```cpp
glfwWindowHint(GLFW_SAMPLES, 4);  // 4× MSAA
```

This must come before `glfwCreateWindow`. Common values are 2, 4, 8, or 16 samples. 4 is the sweet spot for most applications — good quality with minimal performance cost.

### Step 2: Enable Multisampling

After creating the OpenGL context:

```cpp
glEnable(GL_MULTISAMPLE);
```

That's it. OpenGL now performs 4× MSAA automatically for all rendering to the default framebuffer. No shader changes, no buffer setup — just two lines of code.

### Complete On-Screen MSAA Example

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(0.2, 0.8, 0.4, 1.0);
}
)";

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);  // request 4x MSAA
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "Anti-Aliasing (MSAA)", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);  // enable MSAA

    // Compile shaders
    unsigned int vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertShader);

    unsigned int fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertShader);
    glAttachShader(shaderProgram, fragShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    // Cube vertex data
    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f
    };

    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices,
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderProgram);

        float time = (float)glfwGetTime();
        glm::mat4 model = glm::rotate(glm::mat4(1.0f),
                                       time * glm::radians(45.0f),
                                       glm::vec3(0.5f, 1.0f, 0.0f));
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f),
                                      glm::vec3(0.0f),
                                      glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),
                           1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"),
                           1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
```

Try running this with and without the `GLFW_SAMPLES` hint — you'll see a clear difference at the cube's edges. With MSAA, the edges appear smooth; without it, you'll see staircase patterns.

---

## Off-Screen MSAA with Framebuffers

On-screen MSAA is easy, but what if you're rendering to a framebuffer for post-processing (as we learned in the framebuffers chapter)? You can't just attach a regular texture to a multisampled framebuffer — multisampled framebuffers need special multisampled textures.

The workflow becomes:

```
┌────────────────────────┐
│ 1. Multisampled FBO    │   Render scene here (with MSAA)
│    ┌──────────────┐    │
│    │ MS Texture   │    │   glTexImage2DMultisample
│    │ MS RBO       │    │   glRenderbufferStorageMultisample
│    └──────────────┘    │
└───────────┬────────────┘
            │ glBlitFramebuffer (resolves multisamples)
            ▼
┌────────────────────────┐
│ 2. Intermediate FBO    │   Regular (non-MS) framebuffer
│    ┌──────────────┐    │
│    │ Regular      │    │   This texture can be sampled normally
│    │ Texture      │    │
│    └──────────────┘    │
└───────────┬────────────┘
            │ Render screen quad with this texture
            ▼
┌────────────────────────┐
│ 3. Default Framebuffer │   Display on screen (with optional post-processing)
└────────────────────────┘
```

### Step 1: Create a Multisampled Framebuffer

```cpp
unsigned int msFBO;
glGenFramebuffers(1, &msFBO);
glBindFramebuffer(GL_FRAMEBUFFER, msFBO);
```

### Step 2: Create a Multisampled Texture

A multisampled texture stores N color values per pixel. You create it with `glTexImage2DMultisample` instead of `glTexImage2D`:

```cpp
unsigned int msTexture;
glGenTextures(1, &msTexture);
glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msTexture);
glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,
                        4,                // number of samples
                        GL_RGB,           // internal format
                        SCR_WIDTH,
                        SCR_HEIGHT,
                        GL_TRUE);         // fixed sample locations
glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D_MULTISAMPLE, msTexture, 0);
```

The `GL_TRUE` for fixed sample locations tells the driver to use the same sub-pixel sample positions for every pixel. This is almost always what you want.

Note: you cannot set filter or wrap parameters on a multisampled texture — there's no `glTexParameteri` for `GL_TEXTURE_2D_MULTISAMPLE`. The texture is resolved (downsampled) before any filtering happens.

### Step 3: Create a Multisampled Renderbuffer for Depth/Stencil

```cpp
unsigned int msRBO;
glGenRenderbuffers(1, &msRBO);
glBindRenderbuffer(GL_RENDERBUFFER, msRBO);
glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4,
                                 GL_DEPTH24_STENCIL8,
                                 SCR_WIDTH, SCR_HEIGHT);
glBindRenderbuffer(GL_RENDERBUFFER, 0);

glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                          GL_RENDERBUFFER, msRBO);

if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "Multisampled framebuffer is not complete!" << std::endl;
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

### Step 4: Create an Intermediate (Non-Multisampled) FBO

We need a regular FBO to resolve the multisampled image into. This FBO has a normal texture that we can sample in a shader:

```cpp
unsigned int intermediateFBO;
glGenFramebuffers(1, &intermediateFBO);
glBindFramebuffer(GL_FRAMEBUFFER, intermediateFBO);

unsigned int screenTexture;
glGenTextures(1, &screenTexture);
glBindTexture(GL_TEXTURE_2D, screenTexture);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT,
             0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D, screenTexture, 0);

if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "Intermediate framebuffer is not complete!" << std::endl;
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

### Step 5: Render, Resolve, Post-Process

In the render loop:

```cpp
// 1. Render scene to the multisampled FBO
glBindFramebuffer(GL_FRAMEBUFFER, msFBO);
glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
glEnable(GL_DEPTH_TEST);
drawScene();  // your normal rendering code

// 2. Blit (resolve) the multisampled FBO into the intermediate FBO
glBindFramebuffer(GL_READ_FRAMEBUFFER, msFBO);
glBindFramebuffer(GL_DRAW_FRAMEBUFFER, intermediateFBO);
glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT,
                  0, 0, SCR_WIDTH, SCR_HEIGHT,
                  GL_COLOR_BUFFER_BIT, GL_NEAREST);

// 3. Render the resolved texture to the screen (with optional post-processing)
glBindFramebuffer(GL_FRAMEBUFFER, 0);
glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
glClear(GL_COLOR_BUFFER_BIT);
glDisable(GL_DEPTH_TEST);

screenShader.use();
glBindVertexArray(quadVAO);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, screenTexture);
glDrawArrays(GL_TRIANGLES, 0, 6);
```

The crucial function is `glBlitFramebuffer`. It copies pixels from the read framebuffer to the draw framebuffer. When the source is multisampled and the destination is not, this **resolves** the multisampled image — averaging the samples per pixel. The last parameter (`GL_NEAREST`) specifies the filter if the source and destination are different sizes.

### Complete Off-Screen MSAA Example

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char* sceneVS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char* sceneFS = R"(
#version 330 core
out vec4 FragColor;
void main()
{
    FragColor = vec4(0.2, 0.8, 0.4, 1.0);
}
)";

const char* screenVS = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;
out vec2 TexCoords;
void main()
{
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// Post-processing: grayscale effect to demonstrate the pipeline works
const char* screenFS = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D screenTexture;
void main()
{
    vec3 color = texture(screenTexture, TexCoords).rgb;
    // Apply a simple vignette effect
    vec2 uv = TexCoords * 2.0 - 1.0;
    float vignette = 1.0 - dot(uv, uv) * 0.3;
    FragColor = vec4(color * vignette, 1.0);
}
)";

unsigned int compileShaderProgram(const char* vs, const char* fs)
{
    unsigned int v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, NULL);
    glCompileShader(v);

    unsigned int f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, NULL);
    glCompileShader(f);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v);
    glDeleteShader(f);
    return prog;
}

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
                                          "Off-Screen MSAA + Post-Processing",
                                          NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    unsigned int sceneShader = compileShaderProgram(sceneVS, sceneFS);
    unsigned int screenShader = compileShaderProgram(screenVS, screenFS);

    // --- Cube VAO ---
    float cubeVertices[] = {
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f
    };

    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices,
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // --- Screen quad VAO ---
    float quadVertices[] = {
        // positions   // texcoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // --- Multisampled FBO ---
    unsigned int msFBO;
    glGenFramebuffers(1, &msFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, msFBO);

    // Multisampled color texture
    unsigned int msColorBuffer;
    glGenTextures(1, &msColorBuffer);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, msColorBuffer);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB,
                            SCR_WIDTH, SCR_HEIGHT, GL_TRUE);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D_MULTISAMPLE, msColorBuffer, 0);

    // Multisampled depth/stencil renderbuffer
    unsigned int msRBO;
    glGenRenderbuffers(1, &msRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, msRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4,
                                     GL_DEPTH24_STENCIL8,
                                     SCR_WIDTH, SCR_HEIGHT);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, msRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR: Multisampled FBO is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- Intermediate FBO (non-multisampled, for resolving) ---
    unsigned int intermediateFBO;
    glGenFramebuffers(1, &intermediateFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, intermediateFBO);

    unsigned int screenTexture;
    glGenTextures(1, &screenTexture);
    glBindTexture(GL_TEXTURE_2D, screenTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, SCR_WIDTH, SCR_HEIGHT,
                 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, screenTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR: Intermediate FBO is not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Set screen texture sampler
    glUseProgram(screenShader);
    glUniform1i(glGetUniformLocation(screenShader, "screenTexture"), 0);

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        // --- Pass 1: Render scene to multisampled FBO ---
        glBindFramebuffer(GL_FRAMEBUFFER, msFBO);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        glUseProgram(sceneShader);

        float time = (float)glfwGetTime();
        glm::mat4 model = glm::rotate(glm::mat4(1.0f),
                                       time * glm::radians(45.0f),
                                       glm::vec3(0.5f, 1.0f, 0.0f));
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f),
                                      glm::vec3(0.0f),
                                      glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(sceneShader, "model"),
                           1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(sceneShader, "view"),
                           1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(sceneShader, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // --- Pass 2: Blit multisampled FBO to intermediate FBO ---
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, intermediateFBO);
        glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT,
                          0, 0, SCR_WIDTH, SCR_HEIGHT,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        // --- Pass 3: Render screen quad with post-processing ---
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);

        glUseProgram(screenShader);
        glBindVertexArray(quadVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, screenTexture);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteTextures(1, &msColorBuffer);
    glDeleteTextures(1, &screenTexture);
    glDeleteRenderbuffers(1, &msRBO);
    glDeleteFramebuffers(1, &msFBO);
    glDeleteFramebuffers(1, &intermediateFBO);
    glDeleteProgram(sceneShader);
    glDeleteProgram(screenShader);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
```

This program renders a rotating cube to a multisampled framebuffer, resolves it to a regular texture, and displays it with a vignette post-processing effect — all while maintaining anti-aliased edges.

---

## Custom Anti-Aliasing in Shaders

It's worth mentioning that MSAA isn't the only approach to anti-aliasing. Many modern applications use **post-process anti-aliasing** techniques that run as a full-screen shader pass:

- **FXAA (Fast Approximate Anti-Aliasing)** — detects edges in the final image and blurs them. Very cheap but can blur textures.
- **SMAA (Subpixel Morphological Anti-Aliasing)** — more sophisticated edge detection, better quality than FXAA.
- **TAA (Temporal Anti-Aliasing)** — uses data from previous frames to smooth edges over time. High quality but can cause ghosting.

These techniques work on the final image (not during rasterization) and can be combined with MSAA. If you're doing off-screen rendering with post-processing anyway, you might consider FXAA as a lighter alternative to MSAA. However, MSAA remains the gold standard for geometric edge quality and is well worth understanding.

You can also sample individual MSAA samples in a shader using `texelFetch` with a `sampler2DMS`, which enables custom resolve logic:

```glsl
uniform sampler2DMS screenTextureMS;

void main()
{
    ivec2 texSize = textureSize(screenTextureMS);
    ivec2 texCoord = ivec2(gl_FragCoord.xy);

    vec4 color = vec4(0.0);
    for (int i = 0; i < 4; i++)
        color += texelFetch(screenTextureMS, texCoord, i);
    color /= 4.0;

    FragColor = color;
}
```

This gives you full control over how the multisampled data is resolved — you could apply tone mapping per sample, for example, to avoid aliasing artifacts in HDR rendering.

---

## Key Takeaways

- **Aliasing** (jaggies) occurs because continuous geometry is rasterized onto a discrete pixel grid. Diagonal and curved edges are most affected.
- **SSAA** renders at a higher resolution and downsamples. High quality but very expensive (4× fragment count).
- **MSAA** tests coverage at multiple sub-pixel sample points but runs the fragment shader only once per pixel. Much cheaper than SSAA while producing excellent edge quality.
- For **on-screen MSAA**: `glfwWindowHint(GLFW_SAMPLES, 4)` + `glEnable(GL_MULTISAMPLE)`. Two lines of code, done.
- For **off-screen MSAA** (framebuffers):
  - Create a multisampled FBO with `glTexImage2DMultisample` and `glRenderbufferStorageMultisample`
  - Render the scene to this FBO
  - Use `glBlitFramebuffer` to **resolve** (downsample) into a regular FBO
  - Use the resolved texture for post-processing or display
- The **resolve step** (blit) averages the multisampled data per pixel, producing the smooth anti-aliased result.
- `GL_TEXTURE_2D_MULTISAMPLE` textures cannot be sampled with `texture()` — they must be resolved via blit or sampled per-sample with `texelFetch`.
- MSAA primarily smooths **geometric edges** (triangle boundaries). It doesn't help with aliasing inside textures (texture filtering handles that) or shader-generated patterns.

## Exercises

1. Modify the on-screen MSAA example to let the user **toggle MSAA** on/off with a key press (using `glEnable(GL_MULTISAMPLE)` / `glDisable(GL_MULTISAMPLE)`). Observe the difference on the cube's edges.
2. Change the sample count from 4 to 2, 8, and 16. Observe the quality improvement and measure the FPS impact. Is 16× noticeably better than 4×?
3. In the off-screen MSAA example, replace the vignette post-processing with a **grayscale** effect or an **edge detection** kernel. Verify that anti-aliasing is preserved through the post-processing pipeline.
4. Implement the custom resolve approach: instead of using `glBlitFramebuffer`, write a fragment shader that uses `sampler2DMS` and `texelFetch` to manually average the samples. Add a **weighted** average where center samples contribute more.
5. (Challenge) Implement **FXAA** as a post-processing shader and compare the visual quality against hardware MSAA. Run both with the same scene and examine edges closely. Where does each method excel?

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Instancing](10_instancing.md) | | [Next: Advanced Lighting →](../05_advanced_lighting/01_advanced_lighting.md) |
