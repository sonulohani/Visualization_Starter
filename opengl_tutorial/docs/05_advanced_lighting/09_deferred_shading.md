# Deferred Shading

[Previous: Bloom](08_bloom.md) | [Next: SSAO →](10_ssao.md)

---

Throughout this tutorial series we've used **forward rendering**: for every fragment of every object, we run the full lighting calculation for every light. This is simple and works well for a handful of lights, but it has a fundamental scaling problem. In this chapter we'll learn **deferred shading** (also called **deferred rendering**), a technique that decouples geometry processing from lighting, making it practical to render scenes with dozens or even hundreds of lights.

## The Problem with Forward Rendering

In forward rendering, the fragment shader runs once per visible fragment per draw call. For each fragment it evaluates lighting from every light source. If we have `N` objects and `L` lights, the cost is roughly proportional to `N × L`.

But there's a deeper issue: the fragment shader runs for *every* fragment that passes the early depth test within a draw call, but many of those fragments may later be overwritten by closer geometry (overdraw). We calculate expensive lighting for fragments that never appear on screen.

```
Forward Rendering — Wasted Work

Draw order: back wall, then front box (partially overlapping)

  Screen pixels:
  ┌───────────────────────┐
  │ Wall  │ Box  │ Wall   │
  │ (lit) │(lit) │ (lit)  │
  └───────────────────────┘

  Lighting calculated:
  Wall:  ALL wall fragments get lit (including behind the box)
  Box:   ALL box fragments get lit
  
  The wall fragments behind the box were lit for nothing!
  With 128 lights, that's 128 wasted evaluations per hidden pixel.
```

Sorting front-to-back helps (the depth test kills occluded fragments before they reach the fragment shader), but it doesn't eliminate the problem entirely, especially with complex, interlocking geometry.

## Enter Deferred Shading

Deferred shading splits rendering into two distinct passes:

**Pass 1 — Geometry Pass:** Render all objects, but instead of calculating lighting, store the geometry information (position, normal, color) into a set of screen-space textures called the **G-buffer** (Geometry Buffer).

**Pass 2 — Lighting Pass:** Render a single screen-filling quad. For each pixel, read the G-buffer data and calculate lighting. Since we only process pixels that are actually visible (they survived the depth test in Pass 1), no lighting work is wasted.

```
Deferred Shading Pipeline:

Pass 1: Geometry                    Pass 2: Lighting
┌────────────────┐                 ┌────────────────┐
│ For each object│                 │ For each pixel │
│   Store:       │                 │   Read G-buffer│
│   - Position   │──► G-Buffer ──►│   For each light│
│   - Normal     │                 │     Calculate  │
│   - Albedo     │                 │     lighting   │
│   - Specular   │                 │   Output color │
└────────────────┘                 └────────────────┘

Cost: O(objects) + O(pixels × lights)
                   ↑ only VISIBLE pixels!
```

The key insight: lighting cost is **O(pixels × lights)** instead of **O(fragments × lights)**. Since overdraw is eliminated by the depth test in the geometry pass, "pixels" here means only the final visible fragments — typically the screen resolution (e.g., 1920×1080 = ~2M pixels). This is independent of scene complexity.

## The G-Buffer

The G-buffer is a framebuffer with multiple color attachments, each storing a different piece of per-pixel geometry data. At minimum we need:

| Attachment | Data | Format | Why this format |
|---|---|---|---|
| 0 | **Position** (world-space XYZ) | `GL_RGBA16F` | Needs float precision; position values can be large |
| 1 | **Normal** (world-space XYZ) | `GL_RGBA16F` | Needs float precision; components in [-1, 1] |
| 2 | **Albedo (RGB) + Specular (A)** | `GL_RGBA` | 8-bit is fine for color; specular intensity in alpha |

Plus a **depth renderbuffer** for the depth test.

```
G-Buffer Layout (conceptual):

Screen pixel (x, y):

  Attachment 0 (Position):    [FragPos.x, FragPos.y, FragPos.z, —]
  Attachment 1 (Normal):      [Normal.x,  Normal.y,  Normal.z,  —]
  Attachment 2 (Albedo+Spec): [Color.r,   Color.g,   Color.b,   Specular]
  Depth buffer:               [depth]

Every visible pixel has all the data needed for lighting,
stored after the geometry pass — no per-object state required.
```

### Setting Up the G-Buffer

```cpp
unsigned int gBuffer;
glGenFramebuffers(1, &gBuffer);
glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);

unsigned int gPosition, gNormal, gAlbedoSpec;

// Position color buffer (16-bit float for world-space positions)
glGenTextures(1, &gPosition);
glBindTexture(GL_TEXTURE_2D, gPosition);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
             SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D, gPosition, 0);

// Normal color buffer (16-bit float for normals)
glGenTextures(1, &gNormal);
glBindTexture(GL_TEXTURE_2D, gNormal);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
             SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                       GL_TEXTURE_2D, gNormal, 0);

// Albedo + specular color buffer (8-bit is fine for color data)
glGenTextures(1, &gAlbedoSpec);
glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
             SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
                       GL_TEXTURE_2D, gAlbedoSpec, 0);

// Tell OpenGL which color attachments we'll use for rendering
unsigned int attachments[3] = {
    GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2
};
glDrawBuffers(3, attachments);

// Depth renderbuffer
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
```

> **Why `GL_NEAREST` filtering?** The G-buffer textures store discrete per-pixel data (positions, normals). Linear filtering would interpolate between neighboring pixels' positions/normals, which is physically meaningless and produces artifacts. Always use nearest-neighbor filtering for G-buffer textures.

## Geometry Pass

The geometry pass renders all objects using a simple shader that writes to the three G-buffer attachments instead of computing lighting.

### Vertex Shader (`g_buffer.vert`)

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
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;
    TexCoords = aTexCoords;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    Normal = normalMatrix * aNormal;

    gl_Position = projection * view * worldPos;
}
```

### Fragment Shader (`g_buffer.frag`)

```glsl
#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_specular1;

void main()
{
    gPosition = FragPos;
    gNormal = normalize(Normal);
    gAlbedoSpec.rgb = texture(texture_diffuse1, TexCoords).rgb;
    gAlbedoSpec.a = texture(texture_specular1, TexCoords).r;
}
```

This shader does no lighting calculation at all — it simply stores the data that the lighting pass will need. This is what makes the geometry pass so fast: it's just a data dump into textures.

### Running the Geometry Pass

```cpp
glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    shaderGeometryPass.use();
    shaderGeometryPass.setMat4("projection", projection);
    shaderGeometryPass.setMat4("view", view);
    for (unsigned int i = 0; i < objectPositions.size(); i++) {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, objectPositions[i]);
        model = glm::scale(model, glm::vec3(0.25f));
        shaderGeometryPass.setMat4("model", model);
        backpack.Draw(shaderGeometryPass);
    }
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

## Lighting Pass

Now comes the payoff. We render a full-screen quad, sample the G-buffer textures, and calculate lighting for every visible pixel. Because we're working purely in screen space, the lighting cost is independent of the number of objects in the scene.

### Fragment Shader (`deferred_lighting.frag`)

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedoSpec;

struct Light {
    vec3 Position;
    vec3 Color;
    float Linear;
    float Quadratic;
};

const int NR_LIGHTS = 32;
uniform Light lights[NR_LIGHTS];
uniform vec3 viewPos;

void main()
{
    // Retrieve data from G-buffer
    vec3 FragPos  = texture(gPosition, TexCoords).rgb;
    vec3 Normal   = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse  = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;

    // Accumulate lighting from all lights
    vec3 lighting = Diffuse * 0.1; // ambient component
    vec3 viewDir = normalize(viewPos - FragPos);

    for(int i = 0; i < NR_LIGHTS; i++)
    {
        vec3 lightDir = normalize(lights[i].Position - FragPos);

        // Diffuse
        vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * lights[i].Color;

        // Specular (Blinn-Phong)
        vec3 halfwayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(Normal, halfwayDir), 0.0), 16.0);
        vec3 specular = lights[i].Color * spec * Specular;

        // Attenuation
        float distance = length(lights[i].Position - FragPos);
        float attenuation = 1.0 / (1.0 + lights[i].Linear * distance
                                   + lights[i].Quadratic * distance * distance);
        diffuse  *= attenuation;
        specular *= attenuation;

        lighting += diffuse + specular;
    }

    FragColor = vec4(lighting, 1.0);
}
```

32 lights, and each pixel evaluates all of them — but only once per visible pixel. With forward rendering, each of those 32 light evaluations would be repeated for every overlapping fragment, including all the hidden ones.

### Running the Lighting Pass

```cpp
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
shaderLightingPass.use();
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, gPosition);
glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, gNormal);
glActiveTexture(GL_TEXTURE2);
glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

// Send all light data
for (unsigned int i = 0; i < lightPositions.size(); i++) {
    shaderLightingPass.setVec3("lights[" + std::to_string(i) + "].Position",
                               lightPositions[i]);
    shaderLightingPass.setVec3("lights[" + std::to_string(i) + "].Color",
                               lightColors[i]);
    shaderLightingPass.setFloat("lights[" + std::to_string(i) + "].Linear", linear);
    shaderLightingPass.setFloat("lights[" + std::to_string(i) + "].Quadratic", quadratic);
}
shaderLightingPass.setVec3("viewPos", camera.Position);

renderQuad();
```

## Combining Deferred with Forward Rendering

Deferred shading has one major limitation: it can't handle **transparent** objects. The G-buffer stores a single set of geometry data per pixel — there's no room for blending. The solution is a **hybrid** approach:

1. Render all **opaque** objects using deferred shading (geometry pass → lighting pass).
2. Copy the G-buffer's **depth buffer** to the default framebuffer.
3. Render **transparent** objects using traditional forward rendering, with the depth information from the deferred pass.

### Copying the Depth Buffer

```cpp
// After the deferred lighting pass, copy depth from G-buffer to default framebuffer
glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default framebuffer
glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT,
                  0, 0, SCR_WIDTH, SCR_HEIGHT,
                  GL_DEPTH_BUFFER_BIT, GL_NEAREST);
glBindFramebuffer(GL_FRAMEBUFFER, 0);

// Now render transparent objects with forward shading
// They'll correctly depth-test against the deferred geometry
forwardShader.use();
// ... render transparent objects ...
```

`glBlitFramebuffer` copies the depth information so that forward-rendered transparent objects are correctly occluded by deferred-rendered opaque geometry.

## Light Volumes (Optimization)

In our lighting pass, every pixel evaluates all 32 lights — even pixels that are far from a given light and would receive negligible contribution. With 128 or 256 lights, this becomes wasteful.

**Light volumes** solve this: instead of a full-screen quad, render a **sphere** (or bounding shape) around each light. Only pixels within the sphere's screen footprint evaluate that light.

```
Full-screen quad (wasteful):          Light volume (efficient):

┌─────────────────────────┐          ┌─────────────────────────┐
│ All pixels evaluate     │          │                         │
│ ALL 128 lights          │          │    ╭───╮                │
│                         │          │    │ L │ ← only these   │
│         L1  L2          │          │    ╰───╯   pixels eval  │
│       L3      L4        │          │        ╭──╮ light L     │
│                         │          │        │L2│             │
│                         │          │        ╰──╯             │
└─────────────────────────┘          └─────────────────────────┘
```

The radius of the sphere is determined by the light's attenuation — the distance at which the light's contribution drops below a perceptible threshold (e.g., `5/256`). We won't implement this fully here, but it's the standard optimization for deferred renderers with many lights.

## Disadvantages of Deferred Shading

Deferred shading isn't a silver bullet. Be aware of these trade-offs:

| Disadvantage | Explanation |
|---|---|
| **High memory usage** | Multiple full-screen textures at float precision. At 1080p, a 3-attachment G-buffer with 16-bit floats uses ~50 MB. At 4K, ~200 MB. |
| **No MSAA** | Standard MSAA doesn't work with deferred rendering — you'd need to store G-buffer data per-sample, which multiplies memory usage. Custom AA (FXAA, TAA) is used instead. |
| **Single material model** | The lighting pass uses one shader for all pixels. Supporting multiple lighting models (subsurface scattering, anisotropic, etc.) requires workarounds. |
| **No transparency** | As discussed — requires a forward pass for transparent objects. |
| **Bandwidth-heavy** | Reading 3+ textures per pixel per frame is bandwidth-intensive, which can be a bottleneck on bandwidth-limited GPUs (mobile). |

## Complete Source Code

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

const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));
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
                                          "Deferred Shading", NULL, NULL);
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

    Shader shaderGeometryPass("g_buffer.vert", "g_buffer.frag");
    Shader shaderLightingPass("deferred_lighting.vert", "deferred_lighting.frag");
    Shader shaderLightBox("deferred_light_box.vert", "deferred_light_box.frag");

    // Load model (use any model you have — e.g., the backpack from model loading)
    Model backpack("resources/objects/backpack/backpack.obj");

    // Object positions: a 3×3 grid of models
    std::vector<glm::vec3> objectPositions;
    objectPositions.push_back(glm::vec3(-3.0, -0.5, -3.0));
    objectPositions.push_back(glm::vec3( 0.0, -0.5, -3.0));
    objectPositions.push_back(glm::vec3( 3.0, -0.5, -3.0));
    objectPositions.push_back(glm::vec3(-3.0, -0.5,  0.0));
    objectPositions.push_back(glm::vec3( 0.0, -0.5,  0.0));
    objectPositions.push_back(glm::vec3( 3.0, -0.5,  0.0));
    objectPositions.push_back(glm::vec3(-3.0, -0.5,  3.0));
    objectPositions.push_back(glm::vec3( 0.0, -0.5,  3.0));
    objectPositions.push_back(glm::vec3( 3.0, -0.5,  3.0));

    // ---------- G-Buffer Setup ----------
    unsigned int gBuffer;
    glGenFramebuffers(1, &gBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    unsigned int gPosition, gNormal, gAlbedoSpec;

    // Position
    glGenTextures(1, &gPosition);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, gPosition, 0);

    // Normal
    glGenTextures(1, &gNormal);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F,
                 SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                           GL_TEXTURE_2D, gNormal, 0);

    // Albedo + Specular
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

    // ---------- Generate 32 Random Lights ----------
    const unsigned int NR_LIGHTS = 32;
    std::vector<glm::vec3> lightPositions;
    std::vector<glm::vec3> lightColors;
    std::mt19937 rng(42); // fixed seed for reproducibility
    std::uniform_real_distribution<float> distXZ(-3.0f, 3.0f);
    std::uniform_real_distribution<float> distY(0.5f, 2.0f);
    std::uniform_real_distribution<float> distColor(0.5f, 1.0f);

    for (unsigned int i = 0; i < NR_LIGHTS; i++) {
        float xPos = distXZ(rng);
        float yPos = distY(rng);
        float zPos = distXZ(rng);
        lightPositions.push_back(glm::vec3(xPos, yPos, zPos));

        float rColor = distColor(rng);
        float gColor = distColor(rng);
        float bColor = distColor(rng);
        lightColors.push_back(glm::vec3(rColor, gColor, bColor));
    }

    shaderLightingPass.use();
    shaderLightingPass.setInt("gPosition", 0);
    shaderLightingPass.setInt("gNormal", 1);
    shaderLightingPass.setInt("gAlbedoSpec", 2);

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
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // ======== Pass 1: Geometry ========
        glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            shaderGeometryPass.use();
            shaderGeometryPass.setMat4("projection", projection);
            shaderGeometryPass.setMat4("view", view);
            for (unsigned int i = 0; i < objectPositions.size(); i++) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, objectPositions[i]);
                model = glm::scale(model, glm::vec3(0.25f));
                shaderGeometryPass.setMat4("model", model);
                backpack.Draw(shaderGeometryPass);
            }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ======== Pass 2: Lighting ========
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderLightingPass.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gPosition);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, gNormal);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, gAlbedoSpec);

        for (unsigned int i = 0; i < lightPositions.size(); i++) {
            shaderLightingPass.setVec3(
                "lights[" + std::to_string(i) + "].Position", lightPositions[i]);
            shaderLightingPass.setVec3(
                "lights[" + std::to_string(i) + "].Color", lightColors[i]);
            const float linear    = 0.7f;
            const float quadratic = 1.8f;
            shaderLightingPass.setFloat(
                "lights[" + std::to_string(i) + "].Linear", linear);
            shaderLightingPass.setFloat(
                "lights[" + std::to_string(i) + "].Quadratic", quadratic);
        }
        shaderLightingPass.setVec3("viewPos", camera.Position);
        renderQuad();

        // ======== Forward pass: render light cubes ========
        // Copy depth from G-buffer so light cubes depth-test correctly
        glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, SCR_WIDTH, SCR_HEIGHT,
                          0, 0, SCR_WIDTH, SCR_HEIGHT,
                          GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        shaderLightBox.use();
        shaderLightBox.setMat4("projection", projection);
        shaderLightBox.setMat4("view", view);
        for (unsigned int i = 0; i < lightPositions.size(); i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, lightPositions[i]);
            model = glm::scale(model, glm::vec3(0.125f));
            shaderLightBox.setMat4("model", model);
            shaderLightBox.setVec3("lightColor", lightColors[i]);
            renderCube();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteFramebuffers(1, &gBuffer);
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
```

### Light Box Shaders

**Vertex shader (`deferred_light_box.vert`):**

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

**Fragment shader (`deferred_light_box.frag`):**

```glsl
#version 330 core
out vec4 FragColor;

uniform vec3 lightColor;

void main()
{
    FragColor = vec4(lightColor, 1.0);
}
```

## What You Should See

A 3×3 grid of backpack models (or your chosen model) illuminated by 32 small colored light cubes scattered throughout the scene. Each light casts its own pool of colored light, and the lighting combines naturally — areas between lights are darker, areas near multiple lights show additive color mixing. The light cubes themselves are rendered with forward shading on top of the deferred result.

Despite 32 lights, the frame rate should be excellent. Try increasing to 128 or 256 lights — deferred shading handles it gracefully.

## Key Takeaways

- **Forward rendering** costs `O(fragments × lights)` with wasted work on hidden fragments. It doesn't scale well with many lights.
- **Deferred shading** splits rendering into a **geometry pass** (fill the G-buffer) and a **lighting pass** (light only visible pixels), costing `O(pixels × lights)`.
- The **G-buffer** stores per-pixel position, normal, and albedo+specular in screen-space textures using MRT.
- Use `GL_NEAREST` filtering on G-buffer textures to avoid interpolation artifacts.
- Transparent objects must be rendered in a **separate forward pass** after copying depth from the G-buffer.
- **Light volumes** (spheres around each light) reduce per-pixel light evaluations from `O(L)` to `O(nearby lights)`.
- Trade-offs: high memory, no standard MSAA, single material model, no transparency.

## Exercises

1. **Visualize the G-buffer:** Render each G-buffer texture (position, normal, albedo) to the screen individually. This is a great debugging tool. (Hint: just sample one G-buffer texture in the lighting pass and output it directly.)
2. **Increase light count:** Bump the light count to 128, then 256. At what point does your GPU struggle? Compare the FPS to forward rendering with the same light count.
3. **Light volumes:** Instead of evaluating all lights for every pixel, render a sphere mesh around each light. Use backface culling so that only pixels inside the sphere are shaded. This dramatically improves performance at high light counts.
4. **View-space G-buffer:** Store positions and normals in view space instead of world space. This saves a matrix multiplication in the lighting pass and positions can be reconstructed from depth, saving an entire G-buffer attachment.
5. **Transparent objects:** Add a semi-transparent window object to the scene. Implement the hybrid approach: render it with forward shading after copying the G-buffer depth, using alpha blending.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Bloom](08_bloom.md) | [05. Advanced Lighting](.) | [Next: SSAO →](10_ssao.md) |
