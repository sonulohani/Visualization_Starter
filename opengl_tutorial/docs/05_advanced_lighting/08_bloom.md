# Bloom

[Previous: HDR](07_hdr.md) | [Next: Deferred Shading →](09_deferred_shading.md)

---

If you've ever looked at a bright streetlight at night, you've seen it — that soft, luminous halo extending beyond the light's physical boundary. This is **bloom**, caused by the imperfect optics of cameras and human eyes scattering light internally. Bright objects appear to *glow*, bleeding light into neighboring pixels. It's one of the most recognizable post-processing effects in modern games, and it builds directly on the HDR rendering pipeline we set up in the previous chapter.

## Overview of the Bloom Pipeline

Bloom is a multi-step post-processing effect. Here's the complete pipeline at a glance:

```
┌──────────────┐     ┌──────────────────────────┐     ┌───────────────┐
│ Render Scene │────►│ HDR Framebuffer (2 MRTs)  │────►│ Bright-Pass   │
│ to HDR FBO   │     │  [0] Full color           │     │ Extraction    │
│              │     │  [1] Bright fragments only │     │ (auto via MRT)│
└──────────────┘     └──────────────────────────┘     └───────┬───────┘
                                                              │
                          ┌───────────────────────────────────┘
                          ▼
                    ┌─────────────┐     ┌─────────────┐
                    │ Gaussian    │────►│ Gaussian     │  ← ping-pong
                    │ Blur (H)    │     │ Blur (V)     │     N times
                    └─────────────┘     └──────┬──────┘
                                               │
                          ┌────────────────────┘
                          ▼
                    ┌─────────────────────────┐
                    │ Combine:                │
                    │ scene + blurred bloom   │     ┌──────────┐
                    │ + tone map + gamma      │────►│  Screen  │
                    └─────────────────────────┘     └──────────┘
```

The four stages are:

1. **Render the scene** to an HDR framebuffer with two color attachments (Multiple Render Targets).
2. **Extract bright fragments** — the second attachment receives only fragments brighter than a threshold.
3. **Blur the bright image** using a Gaussian filter (ping-pong between two framebuffers).
4. **Combine** the original scene and the blurred brightness, then tone map and gamma correct.

Let's build each stage.

## Stage 1: Multiple Render Targets (MRT)

So far our framebuffers have had a single color attachment. OpenGL allows a fragment shader to write to **multiple color attachments** simultaneously — this is called **Multiple Render Targets (MRT)**.

### Setting Up Two Color Attachments

```cpp
unsigned int hdrFBO;
glGenFramebuffers(1, &hdrFBO);
glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

unsigned int colorBuffers[2];
glGenTextures(2, colorBuffers);
for (unsigned int i = 0; i < 2; i++)
{
    glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                           GL_TEXTURE_2D, colorBuffers[i], 0);
}

// Tell OpenGL we're rendering to 2 color attachments
unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
glDrawBuffers(2, attachments);

// Depth renderbuffer (shared between both color outputs)
unsigned int rboDepth;
glGenRenderbuffers(1, &rboDepth);
glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                          GL_RENDERBUFFER, rboDepth);

if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "Framebuffer not complete!" << std::endl;
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

The key call is `glDrawBuffers` — it tells OpenGL that the fragment shader's `layout(location = 0)` output goes to `GL_COLOR_ATTACHMENT0` and `layout(location = 1)` goes to `GL_COLOR_ATTACHMENT1`.

### Writing to Both Attachments from the Fragment Shader

```glsl
#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

// ... uniforms and inputs ...

void main()
{
    // Normal lighting calculation (Blinn-Phong)
    vec3 lighting = /* ... calculate as usual ... */;
    vec3 result = lighting * texture(diffuseTexture, TexCoords).rgb;

    FragColor = vec4(result, 1.0);

    // Extract bright fragments for bloom
    float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        BrightColor = vec4(result, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
```

## Stage 2: Extracting Bright Fragments

The brightness test uses the **luminance formula** — a perceptual weighting of the RGB components that reflects how the human eye perceives brightness:

```glsl
float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
```

These weights come from the ITU-R BT.709 standard (the sRGB color space). Green contributes the most to perceived brightness, blue the least.

The threshold `1.0` works naturally with HDR: any fragment whose luminance exceeds 1.0 (impossible in LDR) is considered "bright enough to bloom." Because we render to `GL_RGBA16F`, these super-bright values are preserved.

```
After the scene render pass, we have two textures:

colorBuffers[0]:                    colorBuffers[1]:
┌─────────────────────┐            ┌─────────────────────┐
│                     │            │                     │
│   Full scene with   │            │   Only the bright   │
│   all lighting      │            │   fragments (rest   │
│   (HDR values)      │            │   is black)         │
│                     │            │                     │
│    ◉ ← bright light │            │    ◉ ← same light   │
│                     │            │                     │
└─────────────────────┘            └─────────────────────┘
```

## Stage 3: Gaussian Blur

Now we need to blur `colorBuffers[1]` (the bright fragments) so the bloom "spreads." We use a **Gaussian blur** — a weighted average that emphasizes nearby pixels and falls off smoothly with distance.

### Why Gaussian?

A Gaussian (bell curve) distribution produces the most natural-looking blur because it smoothly attenuates with distance, without harsh edges:

```
Gaussian Distribution (1D):

weight
  │      ╱─╲
  │    ╱     ╲
  │  ╱    center ╲
  │╱   (highest   ╲
  ├──── weight) ────╲────
  │                    ╲
 0└──────────────────────── distance from center
```

### The Separable Property

A 2D Gaussian blur with a kernel of size `N×N` would require `N²` texture samples per pixel — expensive. But the 2D Gaussian is **separable**: it can be decomposed into two 1D passes (horizontal, then vertical) with identical results at `2N` samples instead of `N²`:

```
2D Gaussian (expensive)         Two 1D passes (cheap, same result)
┌───────────────┐               ┌───────────────┐   ┌───────────────┐
│ ■ ■ ■ ■ ■ ■ ■│               │ → → → → → → → │   │ ↓ ↓ ↓ ↓ ↓ ↓ ↓│
│ ■ ■ ■ ■ ■ ■ ■│               │ → → → → → → → │   │ ↓ ↓ ↓ ↓ ↓ ↓ ↓│
│ ■ ■ ■ ■ ■ ■ ■│    ═════►     │ → → → → → → → │ + │ ↓ ↓ ↓ ↓ ↓ ↓ ↓│
│ ■ ■ ■ ■ ■ ■ ■│               │ → → → → → → → │   │ ↓ ↓ ↓ ↓ ↓ ↓ ↓│
│ ■ ■ ■ ■ ■ ■ ■│               │ → → → → → → → │   │ ↓ ↓ ↓ ↓ ↓ ↓ ↓│
└───────────────┘               └───────────────┘   └───────────────┘
  N² samples per pixel            N samples/pass × 2 passes = 2N total
  (e.g., 5×5 = 25)               (e.g., 5 + 5 = 10)
```

### Ping-Pong Framebuffers

To perform the two-pass blur, we set up two framebuffers that we alternate between — a **ping-pong** setup:

1. **Pass 1 (horizontal):** Read from the bright texture → write blurred result to FBO A.
2. **Pass 2 (vertical):** Read from FBO A → write blurred result to FBO B.
3. **Repeat** N times for a wider blur (each iteration spreads the glow further).

```cpp
unsigned int pingpongFBO[2];
unsigned int pingpongBuffers[2];
glGenFramebuffers(2, pingpongFBO);
glGenTextures(2, pingpongBuffers);
for (unsigned int i = 0; i < 2; i++)
{
    glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
    glBindTexture(GL_TEXTURE_2D, pingpongBuffers[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, pingpongBuffers[i], 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Ping-pong framebuffer not complete!" << std::endl;
}
```

> **Why `GL_CLAMP_TO_EDGE`?** During blurring, samples near the screen edge would read outside the texture bounds. Clamping prevents dark borders from seeping in.

### The Blur Shader

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D image;
uniform bool horizontal;

// Gaussian weights for a 5-tap kernel (sigma ≈ 1.4)
// Center + 4 neighbors on each side = 9 total taps
const float weight[5] = float[] (
    0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216
);

void main()
{
    vec2 tex_offset = 1.0 / textureSize(image, 0); // size of one texel
    vec3 result = texture(image, TexCoords).rgb * weight[0]; // center

    if(horizontal)
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(image, TexCoords + vec2(tex_offset.x * i, 0.0)).rgb
                      * weight[i];
            result += texture(image, TexCoords - vec2(tex_offset.x * i, 0.0)).rgb
                      * weight[i];
        }
    }
    else
    {
        for(int i = 1; i < 5; ++i)
        {
            result += texture(image, TexCoords + vec2(0.0, tex_offset.y * i)).rgb
                      * weight[i];
            result += texture(image, TexCoords - vec2(0.0, tex_offset.y * i)).rgb
                      * weight[i];
        }
    }

    FragColor = vec4(result, 1.0);
}
```

The `weight` array contains pre-computed Gaussian weights. For each texel, we sample the center pixel (weighted by `weight[0]`), then the 4 pixels on each side. Because the kernel is symmetric, we process both the positive and negative offsets in the same loop.

### Running the Blur

```cpp
bool horizontal = true;
bool first_iteration = true;
unsigned int amount = 10; // number of blur iterations

shaderBlur.use();
for (unsigned int i = 0; i < amount; i++)
{
    glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
    shaderBlur.setBool("horizontal", horizontal);

    // First iteration reads from the bright texture;
    // subsequent iterations read from the other ping-pong buffer
    glBindTexture(GL_TEXTURE_2D,
        first_iteration ? colorBuffers[1] : pingpongBuffers[!horizontal]);

    renderQuad();

    horizontal = !horizontal;
    if (first_iteration) first_iteration = false;
}
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

After this loop, `pingpongBuffers[!horizontal]` contains the final blurred bloom texture. More iterations (e.g., 10–20) produce a wider, softer glow.

## Stage 4: Combining Scene + Bloom

The final step composites the original HDR scene with the blurred bloom image, then applies tone mapping and gamma correction:

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D scene;
uniform sampler2D bloomBlur;
uniform bool bloom;
uniform float exposure;

void main()
{
    const float gamma = 2.2;
    vec3 hdrColor = texture(scene, TexCoords).rgb;
    vec3 bloomColor = texture(bloomBlur, TexCoords).rgb;

    if(bloom)
        hdrColor += bloomColor; // additive blending

    // Exposure tone mapping
    vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
    // Gamma correction
    result = pow(result, vec3(1.0 / gamma));

    FragColor = vec4(result, 1.0);
}
```

The bloom is added to the scene **before** tone mapping. This way the tone mapper handles both the original scene colors and the bloom's contribution together, preventing the bloom from pushing values into ugly clipping.

## Complete Source Code

### Scene Fragment Shader (`bloom_scene.frag`)

```glsl
#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

struct Light {
    vec3 Position;
    vec3 Color;
};

uniform Light lights[4];
uniform sampler2D diffuseTexture;
uniform vec3 viewPos;

void main()
{
    vec3 color = texture(diffuseTexture, fs_in.TexCoords).rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);

    vec3 lighting = vec3(0.0);
    for(int i = 0; i < 4; i++)
    {
        vec3 lightDir = normalize(lights[i].Position - fs_in.FragPos);
        float diff = max(dot(lightDir, normal), 0.0);

        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);

        float dist = length(fs_in.FragPos - lights[i].Position);
        float attenuation = 1.0 / (dist * dist);

        lighting += (diff * color + spec * vec3(0.2)) * lights[i].Color * attenuation;
    }

    FragColor = vec4(lighting, 1.0);

    float brightness = dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        BrightColor = vec4(FragColor.rgb, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
```

### Light Box Fragment Shader (`bloom_light.frag`)

For the light cubes themselves (rendered as glowing boxes), we output their color directly — they should always contribute to bloom:

```glsl
#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

uniform vec3 lightColor;

void main()
{
    FragColor = vec4(lightColor, 1.0);
    float brightness = dot(lightColor, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > 1.0)
        BrightColor = vec4(lightColor, 1.0);
    else
        BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
```

### C++ Application

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include "shader.h"
#include "camera.h"
#include "stb_image.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path, bool gammaCorrection);
void renderCube();
void renderQuad();

const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool bloom       = true;
bool bloomKeyPressed = false;
float exposure   = 1.0f;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Bloom", NULL, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    Shader shaderScene("bloom_scene.vert", "bloom_scene.frag");
    Shader shaderLight("bloom_light.vert", "bloom_light.frag");
    Shader shaderBlur("blur.vert", "blur.frag");
    Shader shaderFinal("bloom_final.vert", "bloom_final.frag");

    unsigned int woodTexture      = loadTexture("resources/wood.png", true);
    unsigned int containerTexture = loadTexture("resources/container2.png", true);

    // ---------- HDR Framebuffer with 2 color attachments ----------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                     SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
                          SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "HDR Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------- Ping-pong Framebuffers for blur ----------
    unsigned int pingpongFBO[2];
    unsigned int pingpongBuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongBuffers);
    for (unsigned int i = 0; i < 2; i++) {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                     SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, pingpongBuffers[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Ping-pong framebuffer not complete!" << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------- Lights ----------
    std::vector<glm::vec3> lightPositions;
    lightPositions.push_back(glm::vec3( 0.0f, 0.5f,  1.5f));
    lightPositions.push_back(glm::vec3(-4.0f, 0.5f, -3.0f));
    lightPositions.push_back(glm::vec3( 3.0f, 0.5f,  1.0f));
    lightPositions.push_back(glm::vec3(-0.8f, 2.4f, -1.0f));

    std::vector<glm::vec3> lightColors;
    lightColors.push_back(glm::vec3( 5.0f,  5.0f,  5.0f));
    lightColors.push_back(glm::vec3(10.0f,  0.0f,  0.0f));
    lightColors.push_back(glm::vec3( 0.0f,  0.0f, 15.0f));
    lightColors.push_back(glm::vec3( 0.0f,  5.0f,  0.0f));

    shaderScene.use();
    shaderScene.setInt("diffuseTexture", 0);
    for (unsigned int i = 0; i < lightPositions.size(); i++) {
        shaderScene.setVec3("lights[" + std::to_string(i) + "].Position",
                            lightPositions[i]);
        shaderScene.setVec3("lights[" + std::to_string(i) + "].Color",
                            lightColors[i]);
    }

    shaderFinal.use();
    shaderFinal.setInt("scene", 0);
    shaderFinal.setInt("bloomBlur", 1);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // ======== Pass 1: Render scene to HDR FBO ========
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            shaderScene.use();
            shaderScene.setMat4("projection", projection);
            shaderScene.setMat4("view", view);
            shaderScene.setVec3("viewPos", camera.Position);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, woodTexture);

            // Floor
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
            model = glm::scale(model, glm::vec3(12.5f, 0.5f, 12.5f));
            shaderScene.setMat4("model", model);
            renderCube();

            // Container cubes
            glBindTexture(GL_TEXTURE_2D, containerTexture);
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 1.5f, 0.0f));
            model = glm::scale(model, glm::vec3(0.5f));
            shaderScene.setMat4("model", model);
            renderCube();

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(2.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(0.5f));
            shaderScene.setMat4("model", model);
            renderCube();

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-1.0f, -1.0f, 2.0f));
            model = glm::rotate(model, glm::radians(60.0f),
                                glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
            shaderScene.setMat4("model", model);
            renderCube();

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 2.7f, 4.0f));
            model = glm::rotate(model, glm::radians(23.0f),
                                glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
            model = glm::scale(model, glm::vec3(1.25f));
            shaderScene.setMat4("model", model);
            renderCube();

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-2.0f, 1.0f, -3.0f));
            model = glm::rotate(model, glm::radians(124.0f),
                                glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
            shaderScene.setMat4("model", model);
            renderCube();

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-3.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(0.5f));
            shaderScene.setMat4("model", model);
            renderCube();

            // Light cubes (self-illuminated)
            shaderLight.use();
            shaderLight.setMat4("projection", projection);
            shaderLight.setMat4("view", view);
            for (unsigned int i = 0; i < lightPositions.size(); i++) {
                model = glm::mat4(1.0f);
                model = glm::translate(model, lightPositions[i]);
                model = glm::scale(model, glm::vec3(0.25f));
                shaderLight.setMat4("model", model);
                shaderLight.setVec3("lightColor", lightColors[i]);
                renderCube();
            }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ======== Pass 2: Gaussian Blur ========
        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;
        shaderBlur.use();
        for (unsigned int i = 0; i < amount; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            shaderBlur.setBool("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D,
                first_iteration ? colorBuffers[1] : pingpongBuffers[!horizontal]);
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration) first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ======== Pass 3: Combine scene + bloom ========
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderFinal.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongBuffers[!horizontal]);
        shaderFinal.setBool("bloom", bloom);
        shaderFinal.setFloat("exposure", exposure);
        renderQuad();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteFramebuffers(1, &hdrFBO);
    glDeleteFramebuffers(2, pingpongFBO);
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

    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !bloomKeyPressed) {
        bloom = !bloom;
        bloomKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE)
        bloomKeyPressed = false;

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        exposure += 1.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        exposure -= 1.0f * deltaTime;
    if (exposure < 0.01f) exposure = 0.01f;
}

// renderCube() and renderQuad() are the same as in the HDR chapter.
// For brevity they are not repeated here — refer to 07_hdr.md.

unsigned int cubeVAO = 0, cubeVBO = 0;
void renderCube()
{
    if (cubeVAO == 0) {
        float vertices[] = {
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glBindVertexArray(cubeVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

unsigned int quadVAO = 0, quadVBO = 0;
void renderQuad()
{
    if (quadVAO == 0) {
        float quadVertices[] = {
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };
        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void framebuffer_size_callback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow*, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    camera.ProcessMouseMovement(xpos - lastX, lastY - ypos);
    lastX = xpos; lastY = ypos;
}

void scroll_callback(GLFWwindow*, double, double yoffset) {
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

unsigned int loadTexture(const char* path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum internalFormat, dataFormat;
        if (nrComponents == 1) { internalFormat = dataFormat = GL_RED; }
        else if (nrComponents == 3) {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        } else {
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
            dataFormat = GL_RGBA;
        }
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                     dataFormat, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    } else {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }
    return textureID;
}
```

## What You Should See

A wooden floor with several container cubes scattered around, illuminated by four colored light cubes (white, red, blue, green). The light cubes themselves glow with a soft halo of their respective colors that bleeds into the surrounding area. Toggle bloom on/off with `B` — without bloom the lights are just bright boxes; with bloom they radiate a warm glow that makes the scene feel more vibrant and cinematic.

## Key Takeaways

- Bloom makes bright objects appear to glow by blurring and adding their luminance back into the scene.
- The pipeline uses **Multiple Render Targets** to extract bright fragments during the scene render pass.
- **Gaussian blur** is separable into two 1D passes (horizontal + vertical), making it much cheaper than a full 2D kernel.
- **Ping-pong framebuffers** alternate between two FBOs for iterative blurring.
- The bloom texture is additively combined with the scene **before** tone mapping.
- Bloom depends on HDR — without values exceeding 1.0, there's no meaningful brightness threshold to extract.

## Exercises

1. **Blur iterations:** Change the blur iteration count from 10 to 2, then to 50. Observe how the glow radius changes. Find a value that looks natural.
2. **Bloom intensity:** Multiply the bloom color by a `bloomStrength` uniform before adding it to the scene. Add keyboard control to adjust this value.
3. **Brightness threshold:** Change the threshold from `1.0` to `0.5` or `2.0`. A lower threshold makes more of the scene bloom; a higher one restricts it to only the brightest sources.
4. **Larger kernel:** Expand the Gaussian kernel from 5 to 9 weights. You'll need to compute appropriate Gaussian weights for a wider kernel. Compare the blur quality.
5. **Downsampled blur:** Perform the blur at half resolution (create ping-pong buffers at `SCR_WIDTH/2 × SCR_HEIGHT/2`). This dramatically reduces the blur cost and gives a wider glow per iteration. Upsample when combining. Compare the visual result.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: HDR](07_hdr.md) | [05. Advanced Lighting](.) | [Next: Deferred Shading →](09_deferred_shading.md) |
