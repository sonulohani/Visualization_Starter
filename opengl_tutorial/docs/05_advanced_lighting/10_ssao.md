# SSAO (Screen-Space Ambient Occlusion)

[Previous: Deferred Shading](09_deferred_shading.md) | [Next: PBR →](../06_pbr/01_theory.md)

---

Look around the room you're in right now. Notice how the corners where walls meet the ceiling are slightly darker, how the crevice under a bookshelf is shadowed, how the space between two objects placed close together appears dimmer. This subtle darkening in tight spaces, creases, and concavities is called **ambient occlusion** (AO), and it's a critical visual cue that makes scenes feel grounded and three-dimensional.

In this chapter we'll implement **Screen-Space Ambient Occlusion (SSAO)**, a technique that approximates ambient occlusion using only the information already available in our G-buffer — no ray tracing required.

## What Is Ambient Occlusion?

In our lighting model, the **ambient** component is a constant — every surface receives the same ambient light regardless of its surroundings. In reality, ambient light (light that has bounced around the environment) doesn't reach every surface equally. Points tucked inside a corner or surrounded by nearby geometry receive less indirect light because the surrounding surfaces block some of the hemisphere of incoming light.

```
Open surface:                    Occluded corner:
Most ambient light               Less ambient light reaches
reaches the point                 the point — nearby geometry
                                  blocks part of the hemisphere

      ↓  ↓  ↓  ↓  ↓                    ↓  ↓
   ─────────●─────────             ─────────┐
                                            │← wall blocks
                                            │  incoming light
                                    ────────● (darker)
```

True ambient occlusion would require tracing rays from every surface point into the hemisphere above it and checking how many are blocked by nearby geometry. This is expensive — far too expensive for real time with traditional rasterization.

## The SSAO Approach

SSAO approximates ambient occlusion entirely in **screen space**, using the depth buffer and (optionally) the normal buffer from our G-buffer. The core idea:

1. For each visible pixel, generate a set of **random sample points** in a hemisphere around the surface normal.
2. For each sample, project it back to screen space and compare its depth against the depth buffer.
3. If the sample point is **deeper** (farther from the camera) than what the depth buffer records at that screen position, it's inside some nearby geometry — it's **occluded**.
4. The ratio of occluded samples to total samples gives us the ambient occlusion factor.

```
Cross-section view of SSAO sampling:

                Normal
                  ↑
         sample ● │ ● sample (occluded — behind geometry)
                  │ 
      sample ●    │    ● sample
                  │
   ──────────────●──────────────── surface
                 ↑
            fragment being shaded

Hemisphere of samples around the surface normal:
- Samples in open space: NOT occluded → contribute to brightness
- Samples inside nearby geometry: occluded → contribute to darkness
```

## The SSAO Pipeline

SSAO integrates with the deferred rendering pipeline from the previous chapter:

```
┌──────────────┐     ┌─────────────────────────────────┐
│ Geometry Pass│────►│ G-Buffer:                        │
│              │     │  - Position (view space)         │
│              │     │  - Normal (view space)           │
│              │     │  - Albedo + Specular             │
└──────────────┘     └──────────┬──────────────────────┘
                                │
                    ┌───────────┼────────────┐
                    ▼           ▼            │
              ┌───────────┐                  │
              │ SSAO Pass │ uses Position    │
              │ (AO map)  │ and Normal       │
              └─────┬─────┘                  │
                    ▼                        │
              ┌───────────┐                  │
              │ Blur Pass │                  │
              │ (smooth)  │                  │
              └─────┬─────┘                  │
                    ▼                        ▼
              ┌─────────────────────────────────┐
              │ Lighting Pass                   │
              │ ambient *= AO                   │
              │ + diffuse + specular            │
              └─────────────────────────────────┘
```

Four passes total:
1. **Geometry pass** → G-buffer (position + normal in *view space*)
2. **SSAO pass** → generates a noisy AO texture
3. **Blur pass** → smooths the AO texture
4. **Lighting pass** → uses AO to modulate the ambient term

## Step 1: View-Space G-Buffer

SSAO works in **view space** (camera space) because depth comparisons are most natural there — the depth buffer stores view-space Z values. We need to modify our geometry pass to output view-space positions and normals.

### Modified Geometry Vertex Shader

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec2 TexCoords;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 viewPos = view * model * vec4(aPos, 1.0);
    FragPos = viewPos.xyz;
    TexCoords = aTexCoords;

    mat3 normalMatrix = transpose(inverse(mat3(view * model)));
    Normal = normalMatrix * aNormal;

    gl_Position = projection * viewPos;
}
```

The fragment shader writes these view-space values to the G-buffer exactly as before (positions to attachment 0, normals to attachment 1, albedo+spec to attachment 2).

## Step 2: The Sample Kernel

We need a set of random 3D sample points distributed in a **hemisphere** (not a full sphere — we only want points above the surface). These are generated once on the CPU and passed to the SSAO shader as uniforms.

### Generating the Kernel

```cpp
std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
std::default_random_engine generator;

std::vector<glm::vec3> ssaoKernel;
for (unsigned int i = 0; i < 64; ++i)
{
    glm::vec3 sample(
        randomFloats(generator) * 2.0f - 1.0f,   // X: [-1, 1]
        randomFloats(generator) * 2.0f - 1.0f,   // Y: [-1, 1]
        randomFloats(generator)                   // Z: [0, 1] — hemisphere!
    );
    sample = glm::normalize(sample);
    sample *= randomFloats(generator);

    // Accelerating interpolation: distribute more samples close to the origin
    float scale = float(i) / 64.0f;
    scale = lerp(0.1f, 1.0f, scale * scale);
    sample *= scale;
    ssaoKernel.push_back(sample);
}
```

The `lerp` helper:

```cpp
float lerp(float a, float b, float f)
{
    return a + f * (b - a);
}
```

### Why the Accelerating Distribution?

Samples closer to the surface (near the origin) are more likely to detect nearby geometry — which is what causes the most visible occlusion. By distributing more samples close to the center, we get better quality with fewer samples.

```
Uniform distribution:          Accelerating distribution:
●   ●   ●   ●   ●   ●        ● ● ● ●   ●       ●
 (samples spread evenly)        (most samples near center)
                                 → better at detecting
                                   nearby occlusion
```

### The Noise Texture

With only 64 samples, the AO result would show obvious banding patterns. To break up these patterns, we create a small **4×4 noise texture** containing random rotation vectors. By tiling this across the screen, each pixel gets a different random rotation of the sample kernel, effectively giving us much better coverage without more samples.

```cpp
std::vector<glm::vec3> ssaoNoise;
for (unsigned int i = 0; i < 16; i++)
{
    glm::vec3 noise(
        randomFloats(generator) * 2.0f - 1.0f,
        randomFloats(generator) * 2.0f - 1.0f,
        0.0f   // rotate around Z axis (the surface normal in tangent space)
    );
    ssaoNoise.push_back(noise);
}

unsigned int noiseTexture;
glGenTextures(1, &noiseTexture);
glBindTexture(GL_TEXTURE_2D, noiseTexture);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0,
             GL_RGB, GL_FLOAT, &ssaoNoise[0]);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
```

The `GL_REPEAT` wrapping ensures the 4×4 texture tiles seamlessly across the entire screen.

## Step 3: The SSAO Shader

This is the core of the technique. For each pixel, we:

1. Read the fragment's view-space position and normal from the G-buffer.
2. Construct a TBN matrix using the normal and a noise vector (for random rotation).
3. For each sample in the kernel, transform it to view space, project to screen space, and compare depths.

### SSAO Fragment Shader

```glsl
#version 330 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[64];

uniform mat4 projection;

// Tile the noise texture over the screen
// noiseScale = vec2(SCR_WIDTH/4.0, SCR_HEIGHT/4.0)
uniform vec2 noiseScale;

int kernelSize = 64;
float radius = 0.5;
float bias = 0.025;

void main()
{
    vec3 fragPos = texture(gPosition, TexCoords).xyz;
    vec3 normal  = normalize(texture(gNormal, TexCoords).rgb);
    vec3 randomVec = normalize(texture(texNoise, TexCoords * noiseScale).xyz);

    // Create TBN matrix to orient the hemisphere along the surface normal
    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // Transform sample from tangent space to view space
        vec3 samplePos = TBN * samples[i];
        samplePos = fragPos + samplePos * radius;

        // Project sample position to screen space to get its UV
        vec4 offset = vec4(samplePos, 1.0);
        offset = projection * offset;      // view → clip
        offset.xyz /= offset.w;            // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // [-1,1] → [0,1]

        // Get the depth at the sample's screen position
        float sampleDepth = texture(gPosition, offset.xy).z;

        // Range check: ignore samples that are too far from the fragment
        float rangeCheck = smoothstep(0.0, 1.0,
                                      radius / abs(fragPos.z - sampleDepth));

        // If the sample is behind stored geometry → occluded
        occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0)
                     * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / kernelSize);
    FragColor = occlusion;
}
```

Let's walk through the key parts.

### Constructing the TBN Matrix

The sample kernel is defined in tangent space (hemisphere around +Z). To orient it along the actual surface normal, we need a **TBN matrix** — just like in normal mapping. The noise vector provides the random tangent direction:

```glsl
vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
vec3 bitangent = cross(normal, tangent);
mat3 TBN       = mat3(tangent, bitangent, normal);
```

This is the Gram-Schmidt process: we project `randomVec` onto the surface plane (orthogonal to `normal`), normalize it to get the tangent, then cross-product for the bitangent.

### Projecting Samples to Screen Space

Each sample is transformed to view space (`fragPos + samplePos * radius`), then projected to clip space using the projection matrix. After perspective divide and the `[−1, 1] → [0, 1]` remapping, we have UV coordinates to sample the G-buffer:

```
View space sample position
        │
        ▼ projection matrix
Clip space
        │
        ▼ perspective divide (/w)
NDC [-1, 1]
        │
        ▼ * 0.5 + 0.5
Screen UV [0, 1]  ← sample G-buffer here
```

### The Range Check

Without a range check, a floor fragment might detect the wall behind it (far away in depth) as "occlusion," which is physically wrong. The `smoothstep` range check ensures that only nearby geometry contributes:

```glsl
float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
```

When `|fragPos.z - sampleDepth|` is much larger than `radius`, `rangeCheck` drops to 0, ignoring that sample.

### The Bias

The small `bias` (0.025) prevents self-occlusion — without it, even a perfectly flat surface would show slight AO due to floating-point precision issues. It's similar to the bias used in shadow mapping.

## Step 4: Blur Pass

The raw SSAO output is noisy because we use a small 4×4 noise texture. A simple box blur smooths it out:

```glsl
#version 330 core
out float FragColor;

in vec2 TexCoords;

uniform sampler2D ssaoInput;

void main()
{
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x)
    {
        for (int y = -2; y < 2; ++y)
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoInput, TexCoords + offset).r;
        }
    }
    FragColor = result / (4.0 * 4.0);
}
```

This 4×4 box blur is cheap and eliminates the noise pattern effectively. The result is a smooth AO map.

```
Before blur:                    After blur:
┌─────────────────┐            ┌─────────────────┐
│ ░▓░▓░▓░▓░▓░▓░  │            │                  │
│ ▓░▓░▓░▓░▓░▓░▓  │            │   ▒▒▒▒           │
│ ░▓░▓░▓░▓░▓░▓░  │  ────►    │  ▒████▒          │
│ ▓░▓░▓░▓░▓░▓░▓  │            │  ▒████▒          │
│  (noisy)        │            │   ▒▒▒▒  (smooth) │
└─────────────────┘            └─────────────────┘
```

## Step 5: Applying AO to Lighting

In the final lighting pass (the deferred lighting shader), we sample the blurred AO texture and multiply the **ambient** component by it:

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;
uniform sampler2D ssao;

struct Light {
    vec3 Position;
    vec3 Color;
    float Linear;
    float Quadratic;
};

uniform Light light;
uniform vec3 viewPos;

void main()
{
    vec3 FragPos  = texture(gPosition, TexCoords).rgb;
    vec3 Normal   = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse  = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;
    float AmbientOcclusion = texture(ssao, TexCoords).r;

    // Ambient: modulated by AO
    vec3 ambient = vec3(0.3 * Diffuse * AmbientOcclusion);

    // Diffuse + Specular (unaffected by AO)
    vec3 viewDir  = normalize(-FragPos); // in view space, camera is at origin
    vec3 lightDir = normalize(light.Position - FragPos);

    vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.Color;

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(Normal, halfwayDir), 0.0), 8.0);
    vec3 specular = light.Color * spec * Specular;

    float distance    = length(light.Position - FragPos);
    float attenuation = 1.0 / (1.0 + light.Linear * distance
                                + light.Quadratic * distance * distance);
    diffuse  *= attenuation;
    specular *= attenuation;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
```

The key line: `vec3 ambient = vec3(0.3 * Diffuse * AmbientOcclusion);`. Corners and creases where `AmbientOcclusion` is low (close to 0) get darker ambient light, while open surfaces where `AmbientOcclusion` is close to 1.0 receive full ambient.

> **Why only ambient?** Diffuse and specular lighting come from specific light sources — they already account for direction and distance. Ambient occlusion specifically simulates the blocking of *indirect* (ambient) light, so it only affects the ambient term.

## Complete Source Code

### SSAO FBO Setup

```cpp
// SSAO framebuffer
unsigned int ssaoFBO, ssaoBlurFBO;
glGenFramebuffers(1, &ssaoFBO);
glGenFramebuffers(1, &ssaoBlurFBO);

// SSAO color buffer (single-channel float)
glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
unsigned int ssaoColorBuffer;
glGenTextures(1, &ssaoColorBuffer);
glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SCR_WIDTH, SCR_HEIGHT, 0,
             GL_RED, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D, ssaoColorBuffer, 0);
if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "SSAO Framebuffer not complete!" << std::endl;

// SSAO blur buffer
glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
unsigned int ssaoColorBufferBlur;
glGenTextures(1, &ssaoColorBufferBlur);
glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SCR_WIDTH, SCR_HEIGHT, 0,
             GL_RED, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

### Full C++ Application

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>
#include <vector>
#include "shader.h"
#include "camera.h"
#include "model.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
void renderQuad();
void renderCube();
float lerp(float a, float b, float f);

const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool ssaoEnabled = true;
bool ssaoKeyPressed = false;

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
                                          "SSAO", NULL, NULL);
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

    Shader shaderGeometryPass("ssao_geometry.vert", "ssao_geometry.frag");
    Shader shaderSSAO("ssao.vert", "ssao.frag");
    Shader shaderSSAOBlur("ssao.vert", "ssao_blur.frag");
    Shader shaderLighting("ssao.vert", "ssao_lighting.frag");

    Model backpack("resources/objects/backpack/backpack.obj");

    // ---------- G-Buffer ----------
    unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    unsigned int gPosition, gNormal, gAlbedoSpec;

    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, gPosition, 0);

    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                           GL_TEXTURE_2D, gNormal, 0);

    glGenTextures(1, &gAlbedoSpec);
    glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
                           GL_TEXTURE_2D, gAlbedoSpec, 0);

    unsigned int attachments[3] = {
        GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
    };
    glDrawBuffers(3, attachments);

    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
                          SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "G-Buffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------- SSAO Framebuffers ----------
    unsigned int ssaoFBO, ssaoBlurFBO;
    glGenFramebuffers(1, &ssaoFBO);
    glGenFramebuffers(1, &ssaoBlurFBO);

    unsigned int ssaoColorBuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glGenTextures(1, &ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SCR_WIDTH, SCR_HEIGHT, 0,
                 GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, ssaoColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO FBO not complete!" << std::endl;

    unsigned int ssaoColorBufferBlur;
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glGenTextures(1, &ssaoColorBufferBlur);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SCR_WIDTH, SCR_HEIGHT, 0,
                 GL_RED, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "SSAO Blur FBO not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ---------- Sample Kernel ----------
    std::uniform_real_distribution<float> randomFloats(0.0, 1.0);
    std::default_random_engine generator;

    std::vector<glm::vec3> ssaoKernel;
    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator)
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);
        float scale = float(i) / 64.0f;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;
        ssaoKernel.push_back(sample);
    }

    // ---------- Noise Texture ----------
    std::vector<glm::vec3> ssaoNoise;
    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise(
            randomFloats(generator) * 2.0f - 1.0f,
            randomFloats(generator) * 2.0f - 1.0f,
            0.0f
        );
        ssaoNoise.push_back(noise);
    }
    unsigned int noiseTexture;
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0,
                 GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // ---------- Light ----------
    glm::vec3 lightPos  = glm::vec3(2.0f, 4.0f, -2.0f);
    glm::vec3 lightColor = glm::vec3(0.2f, 0.2f, 0.7f);

    // ---------- Configure shaders ----------
    shaderSSAO.use();
    shaderSSAO.setInt("gPosition", 0);
    shaderSSAO.setInt("gNormal", 1);
    shaderSSAO.setInt("texNoise", 2);
    shaderSSAO.setVec2("noiseScale",
        glm::vec2(SCR_WIDTH / 4.0f, SCR_HEIGHT / 4.0f));
    for (unsigned int i = 0; i < 64; ++i)
        shaderSSAO.setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);

    shaderSSAOBlur.use();
    shaderSSAOBlur.setInt("ssaoInput", 0);

    shaderLighting.use();
    shaderLighting.setInt("gPosition", 0);
    shaderLighting.setInt("gNormal", 1);
    shaderLighting.setInt("gAlbedoSpec", 2);
    shaderLighting.setInt("ssao", 3);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        processInput(window);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 50.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // ======== Pass 1: Geometry (view-space G-buffer) ========
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            shaderGeometryPass.use();
            shaderGeometryPass.setMat4("projection", projection);
            shaderGeometryPass.setMat4("view", view);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.5f, 0.0f));
            model = glm::rotate(model, glm::radians(-90.0f),
                                glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(1.0f));
            shaderGeometryPass.setMat4("model", model);
            backpack.Draw(shaderGeometryPass);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ======== Pass 2: SSAO ========
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            shaderSSAO.use();
            shaderSSAO.setMat4("projection", projection);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, gPosition);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, gNormal);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, noiseTexture);
            renderQuad();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ======== Pass 3: Blur SSAO ========
        glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
            glClear(GL_COLOR_BUFFER_BIT);
            shaderSSAOBlur.use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
            renderQuad();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ======== Pass 4: Lighting ========
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderLighting.use();
        // Send light in view space
        glm::vec3 lightPosView = glm::vec3(view * glm::vec4(lightPos, 1.0));
        shaderLighting.setVec3("light.Position", lightPosView);
        shaderLighting.setVec3("light.Color", lightColor);
        shaderLighting.setFloat("light.Linear", 0.09f);
        shaderLighting.setFloat("light.Quadratic", 0.032f);
        shaderLighting.setBool("ssaoEnabled", ssaoEnabled);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
        renderQuad();

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteFramebuffers(1, &gBuffer);
    glDeleteFramebuffers(1, &ssaoFBO);
    glDeleteFramebuffers(1, &ssaoBlurFBO);
    glfwTerminate();
    return 0;
}

float lerp(float a, float b, float f)
{
    return a + f * (b - a);
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

    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS && !ssaoKeyPressed) {
        ssaoEnabled = !ssaoEnabled;
        ssaoKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_O) == GLFW_RELEASE)
        ssaoKeyPressed = false;
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
```

## What You Should See

A model (the backpack or your chosen mesh) illuminated by a single light. Press `O` to toggle SSAO on and off:

- **SSAO off:** The ambient lighting is uniform everywhere — the model looks slightly flat, as if floating in evenly lit space.
- **SSAO on:** Creases in the model, areas where straps overlap, the space between the backpack and the ground, and any tight corners all darken subtly. The model immediately looks more grounded, more solid, more *present* in the scene.

The effect is subtle but unmistakable. Try looking at the model from different angles — the AO adapts to whatever geometry is nearby in screen space.

## Tuning Parameters

| Parameter | Range | Effect |
|---|---|---|
| `kernelSize` | 16–128 | More samples = smoother AO but slower. 64 is a good default. |
| `radius` | 0.1–2.0 | How far from the surface to check for occluders. Larger = more global AO. |
| `bias` | 0.01–0.1 | Prevents self-occlusion on flat surfaces. Too large removes valid AO. |
| Blur size | 2×2 – 6×6 | Larger blur = smoother but loses fine detail. 4×4 is typical. |
| Noise texture | 4×4 | Larger noise textures give better randomization but diminishing returns. |

## Key Takeaways

- **Ambient occlusion** darkens creases, corners, and concavities where indirect light can't easily reach, adding subtle but crucial visual depth.
- **SSAO** approximates AO using only screen-space data (positions and normals from the G-buffer), making it efficient for real time.
- The algorithm generates a **hemisphere of random samples** around each pixel's normal, projects them to screen space, and compares against the depth buffer.
- A small **4×4 noise texture** with `GL_REPEAT` wrapping randomizes sample orientations, reducing banding with fewer samples.
- The **range check** (`smoothstep`) prevents distant geometry from falsely contributing occlusion.
- A **blur pass** smooths the noisy raw AO output into a clean map.
- SSAO only modulates the **ambient** component of lighting — direct illumination (diffuse + specular) is unaffected.
- SSAO is a screen-space approximation — it can't detect occlusion from objects outside the camera's view. Despite this limitation, it provides a dramatic visual improvement at reasonable cost.

## Exercises

1. **Visualize the AO map:** Output the raw (pre-blur) SSAO texture to the screen. Then output the post-blur version. Compare the noise patterns and see how the blur cleans them up.
2. **Adjust radius:** Try `radius = 0.1`, `0.5`, `1.0`, and `2.0`. Small radii detect fine-detail occlusion (straps, wrinkles); large radii give broader, more global darkening. Find the best value for your scene.
3. **Reduce samples:** Lower `kernelSize` to 16 and increase the blur size to 6×6. The result is faster but softer. At what sample count does the quality become unacceptable even with blur?
4. **Depth reconstruction:** Instead of storing world/view positions in the G-buffer (expensive: 3 floats per pixel), reconstruct the position from the depth buffer and the inverse projection matrix. This saves one entire G-buffer attachment.
5. **HBAO:** Research Horizon-Based Ambient Occlusion (HBAO). It traces rays along the depth buffer in screen space and typically produces higher-quality results than traditional SSAO. Implement a basic version.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Deferred Shading](09_deferred_shading.md) | [05. Advanced Lighting](.) | [Next: PBR →](../06_pbr/01_theory.md) |
