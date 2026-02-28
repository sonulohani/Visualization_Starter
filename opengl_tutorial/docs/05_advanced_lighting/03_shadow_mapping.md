# Shadow Mapping

Shadows are one of the most important visual cues for understanding spatial relationships in a 3D scene. Without shadows, objects seem to float and the scene looks flat and unconvincing. Adding shadows *dramatically* increases realism. In this chapter we'll implement **shadow mapping**, the most widely used real-time shadow technique.

---

## The Idea Behind Shadow Mapping

The core insight is simple: **a point is in shadow if something else is closer to the light.** We determine this in two passes:

1. **Shadow pass:** Render the scene from the light's point of view and store the depth of every visible fragment in a texture (the **shadow map** or **depth map**).
2. **Lighting pass:** Render the scene from the camera as normal. For each fragment, project it into the light's coordinate system and compare its depth to the value stored in the shadow map. If the fragment is farther from the light than the shadow map records, something is blocking the light — this fragment is in shadow.

```
  Shadow Mapping Overview

  Pass 1: Render from LIGHT's view        Pass 2: Render from CAMERA's view
  ┌───────────────────────┐                ┌───────────────────────┐
  │    ☀ Light (camera)    │                │    👁 Camera           │
  │     ╲                  │                │     ╲                  │
  │      ╲ depth = 3       │                │      ╲                 │
  │       ■ (cube)         │                │       ■ (lit)          │
  │      ╱                 │                │      ╱ ╲               │
  │     ╱  depth = 7       │                │     ╱   ╲  shadow!     │
  │    ▓▓▓ (floor)         │                │    ▓▓▓ ░░░ (floor)     │
  └───────────────────────┘                └───────────────────────┘
  Shadow map stores: 3 at cube,            Fragment on floor: depth from
  7 at floor behind cube                   light is 7, but shadow map
                                           says 3 → in shadow!
```

Let's implement this step by step.

---

## Step 1: Generate the Depth Map

First, we need a framebuffer that stores only depth — no color. This is our shadow map.

### Create the Depth Texture

```cpp
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

unsigned int depthMapFBO;
glGenFramebuffers(1, &depthMapFBO);

unsigned int depthMap;
glGenTextures(1, &depthMap);
glBindTexture(GL_TEXTURE_2D, depthMap);
glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
             SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT,
             GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
```

We use `GL_CLAMP_TO_BORDER` with a border color of `1.0` so that any lookup outside the shadow map returns maximum depth (no shadow). Without this, areas outside the light's view frustum would appear shadowed.

### Attach to the Framebuffer

```cpp
glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                       GL_TEXTURE_2D, depthMap, 0);
glDrawBuffer(GL_NONE);
glReadBuffer(GL_NONE);
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

We call `glDrawBuffer(GL_NONE)` and `glReadBuffer(GL_NONE)` because there is no color attachment. Without these calls, the framebuffer would be incomplete.

```
  Shadow Map FBO Structure

  ┌──────────────────────┐
  │  Framebuffer Object  │
  │                      │
  │  Color: GL_NONE      │  ← no color buffer
  │  Depth: depthMap     │  ← 1024×1024 depth texture
  │                      │
  └──────────────────────┘
```

---

## Step 2: The Light's View

For a **directional light** (like the sun), we use an orthographic projection since the light rays are parallel:

```cpp
float near_plane = 1.0f, far_plane = 7.5f;
glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f,
                                         near_plane, far_plane);
glm::mat4 lightView = glm::lookAt(
    glm::vec3(-2.0f, 4.0f, -1.0f),  // light position
    glm::vec3( 0.0f, 0.0f,  0.0f),  // looking at origin
    glm::vec3( 0.0f, 1.0f,  0.0f)   // up vector
);
glm::mat4 lightSpaceMatrix = lightProjection * lightView;
```

The **lightSpaceMatrix** transforms any world-space position into the light's clip space, which is what we need to index into the shadow map.

```
  Light's Orthographic View

  ┌───────────────────────────┐
  │         ☀ Light           │
  │         │                 │
  │  ┌──────┼──────┐          │
  │  │      │      │ ortho    │
  │  │  ■   │   ■  │ frustum  │
  │  │      │      │          │
  │  └──────┼──────┘          │
  │         │                 │
  │  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓ (floor)  │
  └───────────────────────────┘

  Everything inside the frustum gets
  rendered into the shadow map.
```

> **Why orthographic?** A directional light has no position — its rays are parallel. An orthographic projection preserves parallel lines, making it the correct choice. For a spotlight, you'd use a perspective projection instead.

---

## Step 3: The Depth Shader

The shadow pass only needs to write depth. The shaders are minimal:

### Vertex Shader (`shadow_depth.vert`)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}
```

### Fragment Shader (`shadow_depth.frag`)

```glsl
#version 330 core

void main()
{
    // Depth is written automatically by OpenGL
}
```

That's it! The fragment shader does nothing — OpenGL writes the depth value to the depth attachment automatically.

---

## Step 4: Rendering the Shadow Map

In the render loop, the first pass renders the scene into the shadow map:

```cpp
// 1. Render depth map
glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    depthShader.use();
    depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    renderScene(depthShader);
glBindFramebuffer(GL_FRAMEBUFFER, 0);

// 2. Reset viewport and render scene normally
glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
```

---

## Step 5: Using the Shadow Map in the Lighting Pass

Now we render the scene from the camera's perspective, passing the shadow map and lightSpaceMatrix to the main shader.

### Vertex Shader (`shadow_mapping.vert`)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;
out vec4 FragPosLightSpace;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = transpose(inverse(mat3(model))) * aNormal;
    TexCoords = aTexCoords;
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
```

### Fragment Shader (`shadow_mapping.frag`)

```glsl
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;
in vec4 FragPosLightSpace;

uniform sampler2D diffuseTexture;
uniform sampler2D shadowMap;

uniform vec3 lightPos;
uniform vec3 viewPos;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // Perspective divide (for ortho this is a no-op, but needed for perspective)
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform from [-1,1] to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // Fragments outside the light's frustum are not in shadow
    if (projCoords.z > 1.0)
        return 0.0;

    // Get closest depth from shadow map (from light's perspective)
    float closestDepth = texture(shadowMap, projCoords.xy).r;

    // Get current fragment's depth from light's perspective
    float currentDepth = projCoords.z;

    // Adaptive bias to reduce shadow acne
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    // PCF (Percentage-Closer Filtering) for soft shadow edges
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMap,
                                     projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main()
{
    vec3 color = texture(diffuseTexture, TexCoords).rgb;
    vec3 normal = normalize(Normal);
    vec3 lightColor = vec3(0.3);

    // Ambient
    vec3 ambient = 0.3 * lightColor;

    // Diffuse
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular (Blinn-Phong)
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;

    // Shadow
    float shadow = ShadowCalculation(FragPosLightSpace);
    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;

    FragColor = vec4(lighting, 1.0);
}
```

Let's walk through the `ShadowCalculation` function step by step.

---

## Shadow Calculation: Step by Step

### 1. Perspective Divide

```glsl
vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
```

After multiplication by the lightSpaceMatrix, the coordinates are in clip space. We divide by `w` to get normalized device coordinates (NDC). For an orthographic projection `w` is always 1.0, but this step is essential if you ever switch to a perspective projection (for spotlights).

### 2. Transform to [0,1] Range

```glsl
projCoords = projCoords * 0.5 + 0.5;
```

NDC coordinates range from -1 to 1, but texture coordinates and depth values range from 0 to 1. This maps them to the correct range.

### 3. Sample the Shadow Map

```glsl
float closestDepth = texture(shadowMap, projCoords.xy).r;
float currentDepth = projCoords.z;
```

`closestDepth` is the depth of the nearest surface to the light at this position (from the shadow map). `currentDepth` is how far *this* fragment is from the light.

### 4. Compare

```glsl
float shadow = currentDepth > closestDepth ? 1.0 : 0.0;
```

If the fragment is farther from the light than the closest recorded surface, it's in shadow.

```
  Shadow Test Visualization

  ☀ Light
   │
   │  depth = 3 (stored in shadow map)
   │
   ■ ← cube surface (closest to light)
   │
   │  depth = 7 (current fragment)
   │
   ▓ ← floor fragment

  currentDepth (7) > closestDepth (3) → IN SHADOW!
```

---

## Shadow Acne

If you run the basic shadow test without any adjustments, you'll see a disturbing pattern of alternating light and dark stripes across all surfaces — even surfaces that should be fully lit:

```
  Shadow Acne (Moiré pattern)

  ┌──────────────────────────┐
  │ ▓░▓░▓░▓░▓░▓░▓░▓░▓░▓░▓░ │
  │ ░▓░▓░▓░▓░▓░▓░▓░▓░▓░▓░▓ │
  │ ▓░▓░▓░▓░▓░▓░▓░▓░▓░▓░▓░ │
  │ ░▓░▓░▓░▓░▓░▓░▓░▓░▓░▓░▓ │
  │ ▓░▓░▓░▓░▓░▓░▓░▓░▓░▓░▓░ │
  └──────────────────────────┘
  These stripes are shadow acne — an artifact
  of limited shadow map resolution.
```

### Why It Happens

The shadow map has finite resolution. Multiple fragments on a surface may sample the same shadow map texel. Due to the angle of the surface relative to the light, some fragments end up with a depth value *slightly* above the shadow map's value — they think they're in shadow when they aren't.

```
  Side view: light at an angle to surface

  ☀───────────────
        ╲       shadow map samples
         ╲      one depth per texel
          ╲
  ─ ─ ─ ─ █ ─ ─ ─ ─ █ ─ ─ ─ ─  ← stored depth (discrete steps)
  ──────────────────────────────  ← actual surface (continuous)
           ↑              ↑
      fragment exactly    fragment slightly
      at stored depth     behind stored depth
      → lit               → shadow acne!
```

### Fix: Shadow Bias

Add a small offset (bias) to push the comparison depth away from the surface:

```glsl
float bias = 0.005;
float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
```

Surfaces at steep angles to the light need more bias. We can make the bias **adaptive**:

```glsl
float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
```

When the surface faces the light directly (`dot ≈ 1`), the bias is minimal (`0.005`). When the surface is at a steep angle (`dot ≈ 0`), the bias is larger (`0.05`).

### Peter Panning

Too much bias introduces a different artifact: shadows appear to **detach** from their objects, as if the object is floating slightly above the surface. This is called "Peter Panning" (after the boy who lost his shadow).

```
  Peter Panning (too much bias)

       ■■■■■ cube                  ■■■■■ cube
       ▓▓▓▓▓ shadow                 ▓▓▓▓▓ shadow
  ─────────────── floor         ────GAP────── floor
                                     ↑
  Correct (small bias)          Shadow detached (too much bias)
```

**Fix:** Use `glCullFace(GL_FRONT)` during the shadow pass. By rendering only back faces into the shadow map, the stored depth is the back surface of each object. This naturally provides a bias equal to the object's thickness, eliminating acne without Peter Panning:

```cpp
// During shadow pass
glCullFace(GL_FRONT);
renderScene(depthShader);
glCullFace(GL_BACK);  // restore
```

> **Caveat:** This technique requires meshes to be closed (watertight). Open surfaces like a single-sided plane won't benefit because they have no back face.

---

## Over-Sampling Fix

Fragments outside the light's frustum should not be in shadow. The `z > 1.0` check handles this:

```glsl
if (projCoords.z > 1.0)
    return 0.0;
```

The `GL_CLAMP_TO_BORDER` texture wrap mode with border color `1.0` handles the x/y boundaries — any lookup outside the texture returns depth 1.0 (the farthest possible), so the comparison always passes (no shadow).

---

## PCF: Percentage-Closer Filtering

With a single shadow map sample, shadow edges are hard and jaggy — you get either 100% shadow or 100% light with nothing in between. **Percentage-Closer Filtering** (PCF) softens the edges by averaging multiple samples:

```glsl
float shadow = 0.0;
vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
for (int x = -1; x <= 1; ++x)
{
    for (int y = -1; y <= 1; ++y)
    {
        float pcfDepth = texture(shadowMap,
                                 projCoords.xy + vec2(x, y) * texelSize).r;
        shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
    }
}
shadow /= 9.0;
```

This samples a 3×3 grid of texels around the fragment's shadow map position. Each sample independently tests for shadow, and we average the results. The output ranges from 0.0 (fully lit) to 1.0 (fully shadowed), with intermediate values at shadow edges producing a soft transition.

```
  Shadow Edges: Without vs With PCF

  Without PCF:                    With PCF:
  ┌──────────────────┐            ┌──────────────────┐
  │████████          │            │████████          │
  │████████          │            │███████░          │
  │████████          │            │██████░░          │
  │████████          │            │█████░░░          │
  │████████          │            │████░░░░          │
  │                  │            │                  │
  └──────────────────┘            └──────────────────┘
   Hard, aliased edge             Soft, filtered edge
```

For higher quality, increase the kernel size (e.g., 5×5), but be aware this increases the number of texture samples quadratically.

---

## Complete C++ Source Code

This program renders a floor with several cubes casting shadows from a directional light.

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path);
void renderScene(const Shader& shader);
void renderCube();
void renderQuad();

const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

unsigned int planeVAO = 0;

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
                                          "LearnOpenGL - Shadow Mapping",
                                          NULL, NULL);
    if (window == NULL)
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

    Shader shader("shaders/shadow_mapping.vert",
                   "shaders/shadow_mapping.frag");
    Shader depthShader("shaders/shadow_depth.vert",
                        "shaders/shadow_depth.frag");

    // Floor
    float planeVertices[] = {
         25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
        -25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
        -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,

         25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
        -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,
         25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,  25.0f, 25.0f
    };

    unsigned int planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(6 * sizeof(float)));
    glBindVertexArray(0);

    unsigned int woodTexture = loadTexture("textures/wood.png");

    // Configure depth map FBO
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);

    unsigned int depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT,
                 GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    shader.use();
    shader.setInt("diffuseTexture", 0);
    shader.setInt("shadowMap", 1);

    glm::vec3 lightPos(-2.0f, 4.0f, -1.0f);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. Render depth map from light's perspective
        glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f,
                                                1.0f, 7.5f);
        glm::mat4 lightView = glm::lookAt(lightPos,
                                           glm::vec3(0.0f),
                                           glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        depthShader.use();
        depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
            glClear(GL_DEPTH_BUFFER_BIT);
            glCullFace(GL_FRONT);
            renderScene(depthShader);
            glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. Render scene as normal with shadow mapping
        glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);
        shader.setVec3("viewPos", camera.Position);
        shader.setVec3("lightPos", lightPos);
        shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, woodTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);

        renderScene(shader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);

    glfwTerminate();
    return 0;
}

void renderScene(const Shader& shader)
{
    // Floor
    glm::mat4 model = glm::mat4(1.0f);
    shader.setMat4("model", model);
    glBindVertexArray(planeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Cubes
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 1.5f, 0.0f));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(2.0f, 0.0f, 1.0f));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.0f, 0.0f, 2.0f));
    model = glm::rotate(model, glm::radians(60.0f),
                         glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
    model = glm::scale(model, glm::vec3(0.25f));
    shader.setMat4("model", model);
    renderCube();
}

unsigned int cubeVAO = 0, cubeVBO = 0;
void renderCube()
{
    if (cubeVAO == 0)
    {
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
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
             1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
            -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f,
        };
        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        glBindVertexArray(cubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                              (void*)(6 * sizeof(float)));
        glBindVertexArray(0);
    }
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

unsigned int loadTexture(char const* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrComponents == 1)      format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
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
```

---

## What You Should See

A floor with three cubes, lit by a directional light with shadows:

```
  ┌──────────────────────────────────────────┐
  │ LearnOpenGL - Shadow Mapping      [—][×]│
  ├──────────────────────────────────────────┤
  │                          ☀               │
  │                                          │
  │           ■              ■               │
  │          ▓▓▓                             │
  │     ■   ▓▓▓▓▓           ▓▓              │
  │    ▓▓  ▓▓▓▓▓▓▓         ▓▓▓▓             │
  │░░░░▓▓░░▓▓▓▓▓▓▓░░░░░░░░▓▓▓▓░░░░░░░░░░░░│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  └──────────────────────────────────────────┘
  Cubes cast shadows on the floor and on each other.
  Shadow edges are softened by PCF.
```

---

## Key Takeaways

- **Shadow mapping** is a two-pass technique: render depth from the light, then compare during the camera pass.
- A **depth-only framebuffer** stores the shadow map — no color attachment needed.
- For directional lights, use an **orthographic** light projection.
- The **lightSpaceMatrix** transforms world positions into the light's clip space for shadow map lookups.
- **Shadow acne** is caused by limited shadow map resolution; fix it with a **bias** (preferably adaptive).
- **Peter Panning** is caused by too much bias; fix it by rendering **back faces** into the shadow map.
- **PCF** softens shadow edges by averaging multiple depth comparisons in a kernel around the sample point.
- Use `GL_CLAMP_TO_BORDER` with border color 1.0 to prevent shadows outside the light's frustum.

## Exercises

1. **Move the light:** Make the light position orbit around the scene using `sin`/`cos` on `glfwGetTime()`. Observe how the shadows change dynamically.
2. **Shadow map resolution:** Try different shadow map sizes (256, 512, 1024, 2048, 4096). Compare the quality and performance trade-offs.
3. **Larger PCF kernel:** Increase the PCF kernel from 3×3 to 5×5 or 7×7. How does the shadow softness change? At what point does the performance cost become noticeable?
4. **Visualize the depth map:** Render the shadow map texture to a screen-space quad to see what the light "sees." This is invaluable for debugging.
5. **Spotlight shadows:** Replace the orthographic projection with a perspective projection to simulate a spotlight. Adjust the FOV to control the cone angle.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Gamma Correction](02_gamma_correction.md) | [05. Advanced Lighting](.) | [Next: Point Shadows →](04_point_shadows.md) |
