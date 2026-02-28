# Point Shadows

In the previous chapter we implemented shadow mapping for directional lights. A directional light casts parallel rays, so a single 2D depth map captured the entire shadow information. But what about **point lights**? A point light emits light in *all directions* — a single 2D depth map can only capture one direction. In this chapter we'll extend our shadow technique to handle omnidirectional shadows using **cubemap shadow maps**.

---

## The Problem

A directional light's shadow map captures one "slice" of the scene from a single direction. A point light, however, needs to know the depth in **every** direction around it — 360° in all axes. A single 2D texture simply can't store that.

```
  Directional Light:                 Point Light:
  shadows in ONE direction           shadows in ALL directions

        ☀ ──→──→──→                       ╲ │ ╱
                                        ── ☀ ──
      ▓▓▓▓▓▓▓▓▓▓▓▓▓                      ╱ │ ╲
      one depth map suffices
                                    Need depth in EVERY direction!
```

## The Solution: Cubemap Shadow Map

The answer is a **depth cubemap** — six 2D depth textures arranged as the faces of a cube, each capturing 90° of the scene from the point light's position. Together they cover the full 360° in all directions.

```
  Cubemap: 6 faces covering all directions

           ┌───────┐
           │  +Y   │
           │ (top)  │
  ┌───────┼───────┼───────┼───────┐
  │  -X   │  +Z   │  +X   │  -Z   │
  │(left) │(front)│(right)│(back) │
  └───────┼───────┼───────┼───────┘
           │  -Y   │
           │(bottom)│
           └───────┘

  Each face stores depth values for a 90° field of view.
  A 3D direction vector selects which face to sample.
```

Instead of sampling a 2D texture with `(x, y)`, we sample the cubemap with a 3D direction vector (`fragPos - lightPos`). OpenGL automatically selects the correct face and returns the stored depth.

---

## Setting Up the Depth Cubemap

### Create the Cubemap Texture

```cpp
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

unsigned int depthCubemap;
glGenTextures(1, &depthCubemap);
glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

for (unsigned int i = 0; i < 6; ++i)
{
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT,
                 GL_FLOAT, NULL);
}

glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
```

This creates six depth textures arranged as a cubemap. We use `GL_CLAMP_TO_EDGE` for all three axes (S, T, and R — cubemaps have a third wrap dimension).

### Create and Configure the Framebuffer

```cpp
unsigned int depthMapFBO;
glGenFramebuffers(1, &depthMapFBO);
glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthCubemap, 0);
glDrawBuffer(GL_NONE);
glReadBuffer(GL_NONE);
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

Note that we use `glFramebufferTexture` (not `glFramebufferTexture2D`) — this attaches the entire cubemap, all 6 faces, to the framebuffer at once. We'll use a geometry shader to direct output to individual faces.

---

## Light Space Transforms

For each of the 6 cubemap faces, we need a view-projection matrix looking from the light position in the corresponding direction.

### Projection Matrix

Since a cubemap face covers exactly 90° with a 1:1 aspect ratio:

```cpp
float aspect = (float)SHADOW_WIDTH / (float)SHADOW_HEIGHT;
float near = 1.0f;
float far = 25.0f;
glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), aspect,
                                          near, far);
```

### View Matrices (6 Faces)

```cpp
std::vector<glm::mat4> shadowTransforms;
shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
    lightPos + glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
    lightPos + glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
    lightPos + glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)));
shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
    lightPos + glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)));
shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
    lightPos + glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
    lightPos + glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)));
```

Each matrix combines the perspective projection with a `lookAt` pointing in one of the 6 axis-aligned directions. The up vectors are chosen to match OpenGL's cubemap face orientations.

```
  The 6 cubemap face directions:

  Face                Direction         Up Vector
  ────────────────    ───────────       ──────────
  POSITIVE_X (+X)     ( 1, 0, 0)       (0, -1,  0)
  NEGATIVE_X (-X)     (-1, 0, 0)       (0, -1,  0)
  POSITIVE_Y (+Y)     ( 0, 1, 0)       (0,  0,  1)
  NEGATIVE_Y (-Y)     ( 0,-1, 0)       (0,  0, -1)
  POSITIVE_Z (+Z)     ( 0, 0, 1)       (0, -1,  0)
  NEGATIVE_Z (-Z)     ( 0, 0,-1)       (0, -1,  0)
```

---

## The Geometry Shader Approach

Instead of rendering the scene 6 times (once per cubemap face), we can render it **once** and use a **geometry shader** to emit each triangle to all 6 faces. The geometry shader sets `gl_Layer` to select which cubemap face receives each emitted triangle.

```
  Without geometry shader:           With geometry shader:

  Render pass 1 → face +X           Render pass 1 → ALL 6 faces
  Render pass 2 → face -X              (geometry shader duplicates
  Render pass 3 → face +Y               each triangle 6 times,
  Render pass 4 → face -Y               setting gl_Layer for each)
  Render pass 5 → face +Z
  Render pass 6 → face -Z

  6 draw calls                       1 draw call
  (slower)                           (faster, single-pass)
```

### Depth Vertex Shader (`point_shadow_depth.vert`)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;

void main()
{
    gl_Position = model * vec4(aPos, 1.0);
}
```

The vertex shader only applies the model transform — the light-space transformation happens in the geometry shader.

### Depth Geometry Shader (`point_shadow_depth.geom`)

```glsl
#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

uniform mat4 shadowMatrices[6];

out vec4 FragPos;

void main()
{
    for (int face = 0; face < 6; ++face)
    {
        gl_Layer = face;
        for (int i = 0; i < 3; ++i)
        {
            FragPos = gl_in[i].gl_Position;
            gl_Position = shadowMatrices[face] * FragPos;
            EmitVertex();
        }
        EndPrimitive();
    }
}
```

Key points:

- `layout (triangles) in` — receives triangles from the vertex shader.
- `layout (triangle_strip, max_vertices = 18) out` — outputs up to 18 vertices (3 per face × 6 faces).
- `gl_Layer = face` — directs the triangle to the corresponding cubemap face.
- Each input triangle is emitted 6 times, once per face, transformed by the face's shadow matrix.

### Depth Fragment Shader (`point_shadow_depth.frag`)

```glsl
#version 330 core
in vec4 FragPos;

uniform vec3 lightPos;
uniform float far_plane;

void main()
{
    float lightDistance = length(FragPos.xyz - lightPos);
    lightDistance = lightDistance / far_plane;
    gl_FragDepth = lightDistance;
}
```

Unlike directional shadow mapping where OpenGL writes depth automatically, here we write **linear depth** manually. We compute the actual distance from the fragment to the light and normalize it by `far_plane` to store a value in [0, 1].

Why linear depth? The default perspective depth buffer is non-linear (more precision near the near plane). For omnidirectional shadows, we want to compare distances consistently across all directions, so we use linear distance.

---

## The Shadow Pass

```cpp
glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    depthShader.use();

    for (unsigned int i = 0; i < 6; ++i)
        depthShader.setMat4("shadowMatrices[" + std::to_string(i) + "]",
                             shadowTransforms[i]);

    depthShader.setFloat("far_plane", far_plane);
    depthShader.setVec3("lightPos", lightPos);
    renderScene(depthShader);
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

---

## Sampling the Cubemap Shadow

In the main lighting shader, we sample the cubemap with a 3D direction vector:

### Fragment Shader Additions (`point_shadow.frag`)

```glsl
uniform samplerCube depthMap;
uniform float far_plane;

float ShadowCalculation(vec3 fragPos)
{
    vec3 fragToLight = fragPos - lightPos;

    float closestDepth = texture(depthMap, fragToLight).r;

    // closestDepth is in [0, 1] range — map back to [0, far_plane]
    closestDepth *= far_plane;

    float currentDepth = length(fragToLight);

    float bias = 0.05;
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    return shadow;
}
```

The direction `fragToLight` points from the light to the fragment — OpenGL uses this direction to select the cubemap face and sample the stored depth.

```
  Cubemap Sampling

          ☀ lightPos
         ╱│╲
        ╱ │ ╲  fragToLight = fragPos - lightPos
       ╱  │  ╲
      ╱   │   ╲
    ╱ closestDepth ╲
   ■      │      ● fragPos
   (stored in     (currentDepth = length(fragToLight))
    cubemap)

  if currentDepth > closestDepth → in shadow
```

---

## PCF for Cubemap Shadows

With directional shadow mapping, PCF sampled a 2D grid of offsets. For cubemaps, we need **3D offsets**. A common approach uses 20 sample directions — the directions toward the edges and diagonals of a cube:

```glsl
vec3 sampleOffsetDirections[20] = vec3[]
(
    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
    vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
    vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
    vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0,  1, -1), vec3( 0, -1, -1)
);
```

The PCF function:

```glsl
float ShadowCalculation(vec3 fragPos)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);

    float shadow = 0.0;
    float bias = 0.15;
    int samples = 20;

    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / far_plane)) / 25.0;

    for (int i = 0; i < samples; ++i)
    {
        float closestDepth = texture(depthMap,
            fragToLight + sampleOffsetDirections[i] * diskRadius).r;
        closestDepth *= far_plane;
        if (currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);

    return shadow;
}
```

The `diskRadius` scales the offset based on viewing distance — farther objects get softer shadows, which is a nice visual approximation of how real shadows work. It also prevents nearby objects from having excessively blurry shadows.

```
  PCF sampling pattern (20 directions):

         ●     ●     ●
        ╱ ╲   │   ╱ ╲
       ●   ●──●──●   ●
        ╲ ╱   │   ╲ ╱
         ●     ●     ●
        ╱     │     ╲
       ●──────●──────●
               │
               ●

  Each ● is a sample direction.
  Average the shadow results for soft edges.
```

---

## Complete Lighting Shader

### Vertex Shader (`point_shadow.vert`)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = transpose(inverse(mat3(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
```

### Fragment Shader (`point_shadow.frag`)

```glsl
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D diffuseTexture;
uniform samplerCube depthMap;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform float far_plane;
uniform bool shadows;

vec3 sampleOffsetDirections[20] = vec3[]
(
    vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1),
    vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
    vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
    vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
    vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0,  1, -1), vec3( 0, -1, -1)
);

float ShadowCalculation(vec3 fragPos)
{
    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);

    float shadow = 0.0;
    float bias = 0.15;
    int samples = 20;

    float viewDistance = length(viewPos - fragPos);
    float diskRadius = (1.0 + (viewDistance / far_plane)) / 25.0;

    for (int i = 0; i < samples; ++i)
    {
        float closestDepth = texture(depthMap,
            fragToLight + sampleOffsetDirections[i] * diskRadius).r;
        closestDepth *= far_plane;
        if (currentDepth - bias > closestDepth)
            shadow += 1.0;
    }
    shadow /= float(samples);

    return shadow;
}

void main()
{
    vec3 color = texture(diffuseTexture, TexCoords).rgb;
    vec3 normal = normalize(Normal);
    vec3 lightColor = vec3(0.3);

    // Ambient
    vec3 ambient = 0.3 * color;

    // Diffuse
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;

    // Attenuation
    float distance = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    diffuse *= attenuation;
    specular *= attenuation;

    // Shadow
    float shadow = shadows ? ShadowCalculation(FragPos) : 0.0;
    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;

    FragColor = vec4(lighting, 1.0);
}
```

---

## Complete C++ Source Code

This program renders a room (an inverted cube) with several cubes inside, lit by a central point light that casts omnidirectional shadows. Press **S** to toggle shadows.

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
#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path);
void renderScene(const Shader& shader);
void renderCube();

const unsigned int SCR_WIDTH = 1024;
const unsigned int SCR_HEIGHT = 768;
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool shadowsEnabled = true;
bool shadowsKeyPressed = false;

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
                                          "LearnOpenGL - Point Shadows",
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
    glEnable(GL_CULL_FACE);

    Shader shader("shaders/point_shadow.vert",
                   "shaders/point_shadow.frag");
    Shader depthShader("shaders/point_shadow_depth.vert",
                        "shaders/point_shadow_depth.frag",
                        "shaders/point_shadow_depth.geom");

    unsigned int woodTexture = loadTexture("textures/wood.png");

    // Configure depth cubemap
    unsigned int depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);

    unsigned int depthCubemap;
    glGenTextures(1, &depthCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
                     GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0,
                     GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                         depthCubemap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    shader.use();
    shader.setInt("diffuseTexture", 0);
    shader.setInt("depthMap", 1);

    glm::vec3 lightPos(0.0f, 0.0f, 0.0f);
    float near_plane = 1.0f;
    float far_plane = 25.0f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        lightPos.z = static_cast<float>(sin(glfwGetTime() * 0.5) * 3.0);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. Create depth cubemap
        float aspect = (float)SHADOW_WIDTH / (float)SHADOW_HEIGHT;
        glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), aspect,
                                                  near_plane, far_plane);
        std::vector<glm::mat4> shadowTransforms;
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
            lightPos + glm::vec3( 1.0f,  0.0f,  0.0f),
            glm::vec3(0.0f, -1.0f,  0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
            lightPos + glm::vec3(-1.0f,  0.0f,  0.0f),
            glm::vec3(0.0f, -1.0f,  0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
            lightPos + glm::vec3( 0.0f,  1.0f,  0.0f),
            glm::vec3(0.0f,  0.0f,  1.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
            lightPos + glm::vec3( 0.0f, -1.0f,  0.0f),
            glm::vec3(0.0f,  0.0f, -1.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
            lightPos + glm::vec3( 0.0f,  0.0f,  1.0f),
            glm::vec3(0.0f, -1.0f,  0.0f)));
        shadowTransforms.push_back(shadowProj * glm::lookAt(lightPos,
            lightPos + glm::vec3( 0.0f,  0.0f, -1.0f),
            glm::vec3(0.0f, -1.0f,  0.0f)));

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
            glClear(GL_DEPTH_BUFFER_BIT);
            depthShader.use();
            for (unsigned int i = 0; i < 6; ++i)
                depthShader.setMat4("shadowMatrices[" + std::to_string(i) + "]",
                                     shadowTransforms[i]);
            depthShader.setFloat("far_plane", far_plane);
            depthShader.setVec3("lightPos", lightPos);
            renderScene(depthShader);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // 2. Render scene with point shadows
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
        shader.setVec3("lightPos", lightPos);
        shader.setVec3("viewPos", camera.Position);
        shader.setBool("shadows", shadowsEnabled);
        shader.setFloat("far_plane", far_plane);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, woodTexture);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, depthCubemap);

        renderScene(shader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void renderScene(const Shader& shader)
{
    // Room (inverted cube)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::scale(model, glm::vec3(5.0f));
    shader.setMat4("model", model);
    glDisable(GL_CULL_FACE);
    shader.setBool("reverse_normals", true);
    renderCube();
    shader.setBool("reverse_normals", false);
    glEnable(GL_CULL_FACE);

    // Cubes inside the room
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(4.0f, -3.5f, 0.0f));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(2.0f, 3.0f, 1.0f));
    model = glm::scale(model, glm::vec3(0.75f));
    shader.setMat4("model", model);
    renderCube();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-3.0f, -1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.5f, 1.0f, 1.5f));
    model = glm::scale(model, glm::vec3(0.5f));
    shader.setMat4("model", model);
    renderCube();

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(-1.5f, 2.0f, -3.0f));
    model = glm::rotate(model, glm::radians(60.0f),
                         glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
    model = glm::scale(model, glm::vec3(0.75f));
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

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && !shadowsKeyPressed)
    {
        shadowsEnabled = !shadowsEnabled;
        shadowsKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_RELEASE)
        shadowsKeyPressed = false;
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

A room with floating cubes, each casting omnidirectional shadows from a central point light that moves slowly back and forth:

```
  ┌──────────────────────────────────────────┐
  │ LearnOpenGL - Point Shadows       [—][×]│
  ├──────────────────────────────────────────┤
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  │▓▓             ■              ▓▓▓▓▓▓▓▓▓▓▓│
  │▓▓           ▓▓▓▓▓            ▓▓▓▓▓▓▓▓▓▓│
  │▓▓    ■    ▓▓▓▓▓▓▓▓▓    ☀          ▓▓▓▓▓│
  │▓▓   ▓▓▓  ▓▓▓▓▓▓▓▓▓          ■    ▓▓▓▓▓│
  │▓▓  ▓▓▓▓▓  ▓▓▓▓▓▓▓         ▓▓▓▓▓  ▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  └──────────────────────────────────────────┘
  Shadows extend in all directions from each cube.
  Press S to toggle shadows on/off.
```

---

## Key Takeaways

- Point lights require **omnidirectional shadow maps** — a **depth cubemap** captures depth in all 6 axis-aligned directions.
- Use a **geometry shader** to render all 6 cubemap faces in a single pass by setting `gl_Layer` per face.
- Store **linear depth** (distance from light / far_plane) rather than relying on the non-linear default depth buffer.
- Sample the cubemap with a **3D direction vector** (`fragPos - lightPos`) — OpenGL selects the correct face automatically.
- **PCF for cubemaps** uses 3D offset directions (typically 20 samples) instead of a 2D kernel.
- Scale the PCF `diskRadius` by viewing distance for perceptually-appropriate shadow softness.

## Exercises

1. **Multiple point lights:** Add a second point light to the scene. You'll need a second depth cubemap and a second shadow calculation in the fragment shader.
2. **No geometry shader:** Instead of using a geometry shader, render the scene 6 times (once per cubemap face) using `glFramebufferTexture2D` to attach individual faces. Compare the performance.
3. **Shadow map resolution:** Try 256, 512, and 2048 for the cubemap faces. How does quality change? Note the 6× memory cost compared to a single 2D shadow map of the same resolution.
4. **Visualize the cubemap:** Render a small sphere in the scene textured with the depth cubemap to see what the light "sees" in each direction.
5. **Colored shadows:** Instead of binary shadow (0 or 1), store the surface color in the cubemap and use it to tint shadows (simulating colored light transmission through stained glass or similar).

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Shadow Mapping](03_shadow_mapping.md) | [05. Advanced Lighting](.) | [Next: Normal Mapping →](05_normal_mapping.md) |
