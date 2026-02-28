# HDR (High Dynamic Range)

[Previous: Parallax Mapping](06_parallax_mapping.md) | [Next: Bloom →](08_bloom.md)

---

Up to this point, every color we've computed has been stored in an 8-bit-per-channel framebuffer. That means each RGB component is clamped to the range **0.0 – 1.0** (or equivalently, 0 – 255). This regime is called **Low Dynamic Range (LDR)**, and it silently destroys information every frame. In this chapter we'll break free from that limitation with **High Dynamic Range (HDR)** rendering.

## The Problem with LDR

Consider a scene with two lights: a dim candle (`intensity = 0.8`) and a searchlight (`intensity = 50.0`). Under LDR rendering, every fragment shader output is clamped to `[0.0, 1.0]` before it reaches the framebuffer. The searchlight, which is 62× brighter than the candle, gets clamped to exactly the same maximum value (`1.0`) as the candle at full brightness. Both appear pure white.

```
LDR Clamping Problem:

Light Intensity:   0.0                1.0             50.0
                    ├──────────────────┤
                    │  representable   │← everything beyond 1.0
                    │  range (0–1)     │   is clamped to 1.0
                    ├──────────────────┤
                                       ↑
                           candle (0.8) and searchlight (50.0)
                           both end up near 1.0 — indistinguishable!
```

This causes several visible problems:

1. **Overexposure** — large areas become uniformly white, losing all detail.
2. **Loss of relative brightness** — you can't tell a 2.0 light from a 200.0 light.
3. **Washed-out scenes** — adding more lights doesn't make the scene feel brighter; it just makes more pixels white.
4. **Unrealistic lighting** — in the real world, your eyes adapt to extreme brightness differences. LDR can't simulate this.

## What is HDR?

**HDR rendering** means allowing color values to exceed 1.0 during intermediate computations. We render the scene into a framebuffer that can store floating-point values (not just 0–255 integers), perform all lighting math at full precision, and then — as a final step — **map** the HDR values back to the displayable `[0.0, 1.0]` range. That mapping step is called **tone mapping**.

```
HDR Pipeline:

┌────────────────┐     ┌─────────────────────┐     ┌──────────────┐
│  Scene Render  │────►│  Floating-Point FBO  │────►│  Tone Map    │
│  (colors can   │     │  (stores 0.0–∞)      │     │  (HDR → LDR) │
│   exceed 1.0)  │     │                      │     │  + Gamma     │
└────────────────┘     └─────────────────────┘     └──────┬───────┘
                                                          │
                                                          ▼
                                                   ┌──────────────┐
                                                   │   Screen     │
                                                   │  (0.0–1.0)   │
                                                   └──────────────┘
```

The key insight: by deferring the clamping to the very end and doing it *intelligently* (tone mapping), we preserve relative brightness differences that would otherwise be destroyed.

## Floating-Point Framebuffers

A standard framebuffer uses `GL_RGBA8` — 8 bits per channel, unsigned normalized, clamped to `[0, 1]`. For HDR we need a **floating-point** internal format:

| Format | Bits/channel | Range | Notes |
|---|---|---|---|
| `GL_RGBA16F` | 16 | ±65504 | Good balance of precision and memory. Recommended. |
| `GL_RGBA32F` | 32 | ±3.4×10³⁸ | Overkill for most rendering. 2× the memory of 16F. |

16-bit float (`GL_RGBA16F`) provides more than enough range and precision for HDR lighting and is the standard choice.

### Creating an HDR Framebuffer

```cpp
unsigned int hdrFBO;
glGenFramebuffers(1, &hdrFBO);
glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

// Create floating-point color texture
unsigned int colorBuffer;
glGenTextures(1, &colorBuffer);
glBindTexture(GL_TEXTURE_2D, colorBuffer);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
             SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D, colorBuffer, 0);

// Create depth renderbuffer
unsigned int rboDepth;
glGenRenderbuffers(1, &rboDepth);
glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
                      SCR_WIDTH, SCR_HEIGHT);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                          GL_RENDERBUFFER, rboDepth);

if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "Framebuffer not complete!" << std::endl;
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

The only difference from a standard FBO is the internal format: `GL_RGBA16F` instead of `GL_RGBA` (or `GL_RGB`). We also specify `GL_FLOAT` as the data type, though for `GL_RGBA16F` the driver stores half-floats internally.

### Rendering to the HDR FBO

```cpp
// 1. Render scene to floating-point FBO
glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    renderScene(shader); // lights can output values > 1.0
glBindFramebuffer(GL_FRAMEBUFFER, 0);

// 2. Render a full-screen quad using the HDR texture, applying tone mapping
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
hdrShader.use();
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, colorBuffer);
hdrShader.setFloat("exposure", exposure);
renderQuad();
```

## Tone Mapping

Tone mapping is the process of compressing the wide range of HDR values into the `[0.0, 1.0]` range that displays can show. There are many tone mapping operators; we'll look at the two most common.

### Reinhard Tone Mapping

The simplest and most well-known operator:

```glsl
vec3 mapped = hdrColor / (hdrColor + vec3(1.0));
```

This maps all positive values into `[0, 1)`:

| Input | Output |
|---|---|
| 0.0 | 0.0 |
| 0.5 | 0.33 |
| 1.0 | 0.5 |
| 5.0 | 0.83 |
| 100.0 | 0.99 |
| ∞ | 1.0 |

```
Reinhard Curve:

output
1.0 ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ asymptote
     │                      ╱───────────────
     │                 ╱───╱
     │            ╱───╱
0.5 ─│ ─ ─ ─╱───╱
     │   ╱──╱
     │ ╱─╱
     │╱╱
0.0 ─┼─────────────────────────────────── input
     0          1.0         5.0        50.0
```

**Pros:** Simple, no parameters, preserves relative brightness.
**Cons:** No control over brightness level. Dark scenes stay dark; bright scenes are compressed aggressively.

### Exposure Tone Mapping

A more controllable approach inspired by camera exposure:

```glsl
vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);
```

The `exposure` parameter acts like a camera's exposure setting:

| Exposure | Effect |
|---|---|
| 0.1 | Very dark — only the brightest lights are visible |
| 1.0 | Baseline |
| 2.0 | Brighter — reveals more detail in dark areas |
| 5.0 | Very bright — dark areas fully visible, brights start washing out |

```
Exposure Tone Mapping Curves:

output
1.0 ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
     │          exposure = 5.0 ╱────────────
     │         ╱──────────────╱
     │    ╱───╱  exposure = 1.0
0.5 ─│───╱──╱─────────────────
     │  ╱ ╱      exposure = 0.2
     │ ╱╱           ╱────────────────────
     │╱╱       ╱───╱
0.0 ─┼─────────────────────────────────── input
     0          1.0         5.0        50.0
```

Exposure tone mapping gives you direct control over the overall brightness of the final image. This is what we'll primarily use.

### Applying Gamma Correction

Tone mapping outputs linear-space values in `[0, 1]`. Monitors expect gamma-encoded values, so we apply gamma correction as a final step:

```glsl
// Full HDR fragment shader for the screen quad
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D hdrBuffer;
uniform float exposure;

void main()
{
    const float gamma = 2.2;
    vec3 hdrColor = texture(hdrBuffer, TexCoords).rgb;

    // Exposure tone mapping
    vec3 mapped = vec3(1.0) - exp(-hdrColor * exposure);

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / gamma));

    FragColor = vec4(mapped, 1.0);
}
```

> **Order matters:** Always tone map first, *then* gamma correct. Tone mapping operates on linear-space HDR values; gamma correction is the very last step before display.

## Demonstration Scene: A Dark Tunnel with Bright Lights

To see HDR in action, we'll build a scene where the brightness range is extreme: a dark tunnel with very bright lights. Without HDR, the lights blow out to solid white and the dark areas have no detail. With HDR and adjustable exposure, you can "expose" for the bright end or the dark end, or find a middle ground.

### The Scene

- A long wooden corridor (textured box stretched along the Z axis)
- 4 point lights inside the tunnel, with intensities of `10.0`, `50.0`, `100.0`, and `200.0`
- Very dim ambient (`0.0`) so that only the point lights illuminate the scene

### Lighting Shader (Scene Pass)

The scene fragment shader computes standard Blinn-Phong lighting but allows the result to exceed 1.0:

```glsl
#version 330 core
out vec4 FragColor;

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

        float distance    = length(fs_in.FragPos - lights[i].Position);
        float attenuation = 1.0 / (distance * distance);

        vec3 diffuse  = lights[i].Color * diff * attenuation;
        vec3 specular = lights[i].Color * spec * attenuation;

        lighting += diffuse + specular;
    }

    FragColor = vec4(color * lighting, 1.0);
}
```

Notice that `lights[i].Color` can be `(200.0, 200.0, 200.0)`. The resulting `FragColor` easily exceeds 1.0 — but since we're rendering into a `GL_RGBA16F` framebuffer, those values are preserved.

## Complete Source Code

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

bool hdrEnabled  = true;
bool hdrKeyPressed = false;
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "HDR Rendering", NULL, NULL);
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

    Shader shader("hdr_lighting.vert", "hdr_lighting.frag");
    Shader hdrShader("hdr.vert", "hdr.frag");

    unsigned int woodTexture = loadTexture("resources/wood.png", true);

    // ---------- HDR Framebuffer ----------
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);

    unsigned int colorBuffer;
    glGenTextures(1, &colorBuffer);
    glBindTexture(GL_TEXTURE_2D, colorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, colorBuffer, 0);

    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
                          SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, rboDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------- Lights ----------
    std::vector<glm::vec3> lightPositions;
    lightPositions.push_back(glm::vec3( 0.0f,  0.0f, 49.5f));
    lightPositions.push_back(glm::vec3(-1.4f, -1.9f,  9.0f));
    lightPositions.push_back(glm::vec3( 0.0f, -1.8f,  4.0f));
    lightPositions.push_back(glm::vec3( 0.8f, -1.7f,  6.0f));

    std::vector<glm::vec3> lightColors;
    lightColors.push_back(glm::vec3(200.0f, 200.0f, 200.0f));
    lightColors.push_back(glm::vec3( 10.0f,   0.0f,   0.0f));
    lightColors.push_back(glm::vec3(  0.0f,   0.0f,  50.0f));
    lightColors.push_back(glm::vec3(  0.0f,  100.0f,   0.0f));

    shader.use();
    shader.setInt("diffuseTexture", 0);
    for (unsigned int i = 0; i < lightPositions.size(); i++) {
        shader.setVec3("lights[" + std::to_string(i) + "].Position",
                       lightPositions[i]);
        shader.setVec3("lights[" + std::to_string(i) + "].Color",
                       lightColors[i]);
    }

    hdrShader.use();
    hdrShader.setInt("hdrBuffer", 0);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        // 1. Render scene into floating-point framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            glm::mat4 projection = glm::perspective(
                glm::radians(camera.Zoom),
                (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
            glm::mat4 view = camera.GetViewMatrix();

            shader.use();
            shader.setMat4("projection", projection);
            shader.setMat4("view", view);
            shader.setVec3("viewPos", camera.Position);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, woodTexture);

            // Render tunnel (a large stretched cube)
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.0f, 25.0f));
            model = glm::scale(model, glm::vec3(2.5f, 2.5f, 27.5f));
            shader.setMat4("model", model);
            shader.setInt("inverse_normals", 1);
            renderCube();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. Tone map HDR buffer to screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        hdrShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffer);
        hdrShader.setBool("hdr", hdrEnabled);
        hdrShader.setFloat("exposure", exposure);
        renderQuad();

        std::cout << "HDR: " << (hdrEnabled ? "on" : "off")
                  << " | exposure: " << exposure << "\r" << std::flush;

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteFramebuffers(1, &hdrFBO);
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

    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS && !hdrKeyPressed) {
        hdrEnabled = !hdrEnabled;
        hdrKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_H) == GLFW_RELEASE)
        hdrKeyPressed = false;

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        exposure += 1.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        exposure -= 1.0f * deltaTime;
    if (exposure < 0.01f) exposure = 0.01f;
}

unsigned int cubeVAO = 0, cubeVBO = 0;
void renderCube()
{
    if (cubeVAO == 0) {
        float vertices[] = {
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
            // bottom face
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
             1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
             1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
            -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
            -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
            // top face
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

void framebuffer_size_callback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow*, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    camera.ProcessMouseMovement(xpos - lastX, lastY - ypos);
    lastX = xpos;
    lastY = ypos;
}

void scroll_callback(GLFWwindow*, double, double yoffset)
{
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
        if (nrComponents == 1) {
            internalFormat = dataFormat = GL_RED;
        } else if (nrComponents == 3) {
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
            dataFormat = GL_RGB;
        } else { // nrComponents == 4
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

### HDR Quad Vertex Shader (`hdr.vert`)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 1.0);
}
```

### HDR Quad Fragment Shader (`hdr.frag`)

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D hdrBuffer;
uniform bool hdr;
uniform float exposure;

void main()
{
    const float gamma = 2.2;
    vec3 hdrColor = texture(hdrBuffer, TexCoords).rgb;

    if(hdr)
    {
        // Exposure tone mapping
        vec3 result = vec3(1.0) - exp(-hdrColor * exposure);
        // Gamma correction
        result = pow(result, vec3(1.0 / gamma));
        FragColor = vec4(result, 1.0);
    }
    else
    {
        // Without HDR: just gamma correct the clamped values
        vec3 result = pow(hdrColor, vec3(1.0 / gamma));
        FragColor = vec4(result, 1.0);
    }
}
```

## What You Should See

A dark wooden tunnel stretching into the distance, illuminated by four very bright colored lights — a white light deep in the tunnel, and red, blue, and green lights scattered along the way.

- **With HDR off (press H):** The areas near lights blow out to solid white/color. Moving close to a light source turns everything into a washed-out blob. You lose all surface detail near the lights.
- **With HDR on (default):** Even near the brightest light (200.0), the wood grain texture remains visible. The relative brightness of the four lights is clear — the white one at the far end is obviously the brightest.
- **Adjusting exposure (Up/Down arrows):** Low exposure reveals the bright end of the tunnel while the near end goes dark. High exposure lights up the near end but makes the far end blow out. A medium exposure shows detail everywhere.

## When to Use HDR

| Scenario | HDR needed? |
|---|---|
| Simple scenes with mild lighting | Optional (nice to have) |
| Scenes with bright lights + dark areas | Yes — crucial for detail |
| Post-processing effects (bloom, lens flare) | Yes — they rely on values > 1.0 |
| Physically-based rendering (PBR) | Yes — PBR light values are physically scaled |
| Mobile/low-end hardware | Optional — `GL_RGBA16F` has some cost |

In modern rendering pipelines, HDR is essentially a requirement. It's the foundation for bloom (next chapter), eye adaptation, and physically-based lighting.

## Key Takeaways

- LDR rendering clamps all color values to `[0.0, 1.0]`, destroying brightness information and causing overexposure.
- HDR rendering uses **floating-point framebuffers** (`GL_RGBA16F`) to store unclamped color values during intermediate rendering.
- **Tone mapping** converts HDR values to displayable LDR values. Two common operators:
  - **Reinhard:** `color / (color + 1.0)` — simple, no parameters.
  - **Exposure:** `1.0 - exp(-color * exposure)` — adjustable, more control.
- **Gamma correction** is always applied as the final step, after tone mapping.
- HDR preserves **relative brightness** — you can tell a 10.0 light from a 200.0 light.
- Adjustable exposure mimics how cameras (and eyes) adapt to different brightness levels.

## Exercises

1. **Reinhard vs. Exposure:** Modify the `hdr.frag` shader to support both tone mapping operators, switchable at runtime (e.g., press `R` to toggle). Compare the results — where does Reinhard look better? Where does exposure mapping win?
2. **Auto-exposure:** Compute the average luminance of the HDR framebuffer (you can downsample to 1×1 using mipmaps or a compute pass) and use it to automatically set the exposure. The scene should smoothly adapt as you look at bright or dark areas.
3. **More lights:** Add 12 more lights with random colors and high intensity values (20–500). Without HDR the scene is a white mess; with HDR you should still see distinct colored lights.
4. **Compare formats:** Swap `GL_RGBA16F` for `GL_RGBA32F` and measure the performance difference. Is the extra precision visible?
5. **Filmic tone mapping:** Implement the ACES (Academy Color Encoding System) filmic tone mapping curve and compare it to Reinhard and exposure. The ACES curve is widely used in games for its cinematic look.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Parallax Mapping](06_parallax_mapping.md) | [05. Advanced Lighting](.) | [Next: Bloom →](08_bloom.md) |
