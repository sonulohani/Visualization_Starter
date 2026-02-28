# Normal Mapping

So far, our surfaces have been geometrically flat — a wall is a single quad with a single normal direction, and lighting reveals this flatness. To make a brick wall *look* like it has mortar grooves and raised bricks, we'd need thousands of tiny triangles. **Normal mapping** achieves the same visual effect by tricking the lighting system: instead of adding more geometry, we store per-pixel normal directions in a texture and use *those* for lighting calculations.

---

## The Problem: Flat Surfaces Look Flat

Consider a brick wall made of a single quad with 4 vertices. Even with a beautiful diffuse texture, the lighting treats the entire surface as perfectly flat because every fragment uses the same interpolated normal (pointing straight out from the surface):

```
  A textured flat wall:

  ┌────────────────────────────────┐
  │ [brick][brick][brick][brick]   │
  │ [brick][brick][brick][brick]   │  ← looks like a flat
  │ [brick][brick][brick][brick]   │     poster of bricks
  │ [brick][brick][brick][brick]   │
  └────────────────────────────────┘
  Normal: (0, 0, 1) everywhere
  Lighting: uniform across the entire surface
```

With normal mapping, each pixel gets its own normal direction, creating the illusion of depth:

```
  The same wall with normal mapping:

  ┌────────────────────────────────┐
  │ ╔════╗╔════╗╔════╗╔════╗      │
  │ ║    ║║    ║║    ║║    ║      │  ← looks like real bricks
  │ ╚════╝╚════╝╚════╝╚════╝      │     with grooves and
  │   ╔════╗╔════╗╔════╗          │     raised surfaces
  │   ║    ║║    ║║    ║          │
  │   ╚════╝╚════╝╚════╝          │
  └────────────────────────────────┘
  Normal: varies per pixel (from the normal map)
  Lighting: creates the illusion of bumps and grooves
```

---

## What Is a Normal Map?

A normal map is a texture where each pixel's RGB color encodes a **normal direction** instead of a visible color. The mapping is:

| Channel | Axis | Meaning |
|---------|------|---------|
| R | X | Left/right deviation |
| G | Y | Up/down deviation |
| B | Z | Outward direction |

Since texture values range from 0 to 1, and normals range from -1 to 1, we convert with:

```glsl
vec3 normal = texture(normalMap, TexCoords).rgb;
normal = normalize(normal * 2.0 - 1.0);
```

### Why Normal Maps Look Blue/Purple

Most normals point roughly "outward" from the surface (positive Z in tangent space). In the texture, `Z = 1.0` maps to a blue channel value of `1.0`, while `X ≈ 0` and `Y ≈ 0` map to R and G values around `0.5`. The result: `(0.5, 0.5, 1.0)` — a distinct blue/purple color.

```
  Normal Map Color Encoding

  Normal direction:     Texture color:
  ( 0,  0,  1)    →    (0.5, 0.5, 1.0)  = blue       (pointing straight out)
  ( 1,  0,  0)    →    (1.0, 0.5, 0.5)  = red-ish    (pointing right)
  (-1,  0,  0)    →    (0.0, 0.5, 0.5)  = cyan-ish   (pointing left)
  ( 0,  1,  0)    →    (0.5, 1.0, 0.5)  = green-ish  (pointing up)

  ┌──────────────────────────┐
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│  A typical normal map
  │▓▓████▓▓████▓▓████▓▓████▓│  is predominantly blue
  │▓▓████▓▓████▓▓████▓▓████▓│  because most normals
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│  point mostly "outward"
  │▓▓▓████▓▓████▓▓████▓▓▓▓▓▓│  (positive Z)
  │▓▓▓████▓▓████▓▓████▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  └──────────────────────────┘
```

---

## Tangent Space

The normals in a normal map are defined in **tangent space** — a coordinate system local to each point on the surface. This is essential: it means the same normal map can be applied to surfaces facing any direction. If the normals were in world space, the map would only work for surfaces in one specific orientation.

Tangent space is defined by three orthogonal axes at each surface point:

| Axis | Symbol | Direction |
|------|--------|-----------|
| **Tangent** | T | Along the surface in the U (texture) direction |
| **Bitangent** | B | Along the surface in the V (texture) direction |
| **Normal** | N | Perpendicular to the surface (the geometric normal) |

```
  Tangent Space on a Surface

              N (normal)
              │
              │
              │
              │
  ────────────●────────────── surface
             ╱ ╲
            ╱   ╲
           T     B
      (tangent) (bitangent)

  T is aligned with the U texture direction
  B is aligned with the V texture direction
  N is perpendicular to the surface

  Together, T, B, N form an orthonormal basis
  (the TBN matrix).
```

The **TBN matrix** transforms vectors from tangent space to world space:

```
TBN = [T | B | N]   (3×3 matrix, columns are T, B, N)
```

To transform a tangent-space normal to world space:

```glsl
vec3 worldNormal = TBN * tangentSpaceNormal;
```

---

## Computing Tangent and Bitangent

Given a triangle with vertices P1, P2, P3 and corresponding texture coordinates UV1, UV2, UV3, the tangent and bitangent are computed from the triangle's edges and UV differences:

```
  Triangle with UV coordinates:

  P3 (UV3)
   ╱╲
  ╱  ╲   Edge1 = P2 - P1
 ╱    ╲  Edge2 = P3 - P1
╱______╲
P1      P2
(UV1)  (UV2)

ΔUV1 = UV2 - UV1
ΔUV2 = UV3 - UV1
```

The tangent T and bitangent B satisfy:

```
Edge1 = ΔUV1.x * T + ΔUV1.y * B
Edge2 = ΔUV2.x * T + ΔUV2.y * B
```

Solving this system:

```
f = 1.0 / (ΔUV1.x * ΔUV2.y - ΔUV2.x * ΔUV1.y)

T = f * (ΔUV2.y * Edge1 - ΔUV1.y * Edge2)
B = f * (-ΔUV2.x * Edge1 + ΔUV1.x * Edge2)
```

In C++:

```cpp
glm::vec3 edge1 = pos2 - pos1;
glm::vec3 edge2 = pos3 - pos1;
glm::vec2 deltaUV1 = uv2 - uv1;
glm::vec2 deltaUV2 = uv3 - uv1;

float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

glm::vec3 tangent;
tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

glm::vec3 bitangent;
bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
```

---

## Two Approaches to Normal Mapping

There are two ways to use the TBN matrix:

### Approach 1: Transform Normal to World Space

Construct the TBN matrix in the vertex shader, pass it to the fragment shader, and transform the sampled tangent-space normal to world space:

```glsl
// Fragment shader
vec3 normal = texture(normalMap, TexCoords).rgb;
normal = normalize(normal * 2.0 - 1.0);
normal = normalize(TBN * normal);  // tangent space → world space
// Use 'normal' for world-space lighting calculations
```

### Approach 2: Transform Everything to Tangent Space

Compute the **inverse** TBN (which is the transpose, since TBN is orthonormal) and transform the light and view directions to tangent space in the vertex shader. Then in the fragment shader, the sampled normal is already in the correct space:

```glsl
// Vertex shader
mat3 TBN = transpose(mat3(T, B, N));  // world → tangent space
TangentLightPos = TBN * lightPos;
TangentViewPos  = TBN * viewPos;
TangentFragPos  = TBN * FragPos;

// Fragment shader
vec3 normal = texture(normalMap, TexCoords).rgb;
normal = normalize(normal * 2.0 - 1.0);
// Already in tangent space — use TangentLightPos, TangentViewPos directly
```

Approach 2 is more efficient because the matrix multiplication happens per-vertex (fewer times) rather than per-fragment. We'll use Approach 2 in our implementation.

```
  Approach 1 vs Approach 2

  Approach 1 (normal → world):
  ┌─────────────┐     ┌──────────────────┐
  │ Vertex      │     │ Fragment         │
  │ Pass TBN    │ ──→ │ TBN * normalMap  │  ← matrix multiply per fragment
  └─────────────┘     │ lighting in      │
                      │ world space      │
                      └──────────────────┘

  Approach 2 (everything → tangent):
  ┌─────────────────┐     ┌──────────────────┐
  │ Vertex          │     │ Fragment         │
  │ TBN⁻¹ * light  │ ──→ │ sample normalMap │  ← no matrix multiply!
  │ TBN⁻¹ * view   │     │ lighting in      │
  │ TBN⁻¹ * frag   │     │ tangent space    │
  └─────────────────┘     └──────────────────┘
```

---

## Gram-Schmidt Orthogonalization

After interpolation across a triangle, the tangent and normal vectors may no longer be perfectly orthogonal. We re-orthogonalize them in the vertex shader using the **Gram-Schmidt process**:

```glsl
vec3 T = normalize(vec3(model * vec4(aTangent, 0.0)));
vec3 N = normalize(vec3(model * vec4(aNormal, 0.0)));

// Re-orthogonalize T with respect to N
T = normalize(T - dot(T, N) * N);

// Compute B as cross product (guaranteed perpendicular to both)
vec3 B = cross(N, T);
```

This ensures the TBN basis is orthonormal even after vertex interpolation and model transformations, preventing subtle lighting artifacts.

```
  Gram-Schmidt Process

  Before:                    After:
  N ↑     T →                N ↑     T →
    │   ╱                      │   │
    │  ╱  (not quite 90°)      │   │  (exactly 90°)
    │ ╱                        │   │
    │╱                         │   │

  Step 1: Remove N component from T:
          T' = T - dot(T, N) * N
  Step 2: Normalize T'
  Step 3: B = cross(N, T')
```

---

## Vertex Data with Tangents

We add tangent (and optionally bitangent) vectors to our vertex data. For a simple quad:

```cpp
// positions
glm::vec3 pos1(-1.0f,  1.0f, 0.0f);
glm::vec3 pos2(-1.0f, -1.0f, 0.0f);
glm::vec3 pos3( 1.0f, -1.0f, 0.0f);
glm::vec3 pos4( 1.0f,  1.0f, 0.0f);

// texture coordinates
glm::vec2 uv1(0.0f, 1.0f);
glm::vec2 uv2(0.0f, 0.0f);
glm::vec2 uv3(1.0f, 0.0f);
glm::vec2 uv4(1.0f, 1.0f);

// normal (same for all — flat surface)
glm::vec3 nm(0.0f, 0.0f, 1.0f);

// --- Triangle 1: pos1, pos2, pos3 ---
glm::vec3 edge1 = pos2 - pos1;
glm::vec3 edge2 = pos3 - pos1;
glm::vec2 deltaUV1 = uv2 - uv1;
glm::vec2 deltaUV2 = uv3 - uv1;

float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

glm::vec3 tangent1;
tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

glm::vec3 bitangent1;
bitangent1.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
bitangent1.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
bitangent1.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

// --- Triangle 2: pos1, pos3, pos4 ---
edge1 = pos3 - pos1;
edge2 = pos4 - pos1;
deltaUV1 = uv3 - uv1;
deltaUV2 = uv4 - uv1;

f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

glm::vec3 tangent2;
tangent2.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
tangent2.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
tangent2.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

glm::vec3 bitangent2;
bitangent2.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
bitangent2.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
bitangent2.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

float quadVertices[] = {
    // positions          // normal    // texcoords  // tangent             // bitangent
    pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
    pos2.x, pos2.y, pos2.z, nm.x, nm.y, nm.z, uv2.x, uv2.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
    pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,

    pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
    pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
    pos4.x, pos4.y, pos4.z, nm.x, nm.y, nm.z, uv4.x, uv4.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
};
```

The vertex layout is now 14 floats per vertex:

```
  Vertex Layout (14 floats, 56 bytes):
  ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┬──────┐
  │ posX │ posY │ posZ │ nrmX │ nrmY │ nrmZ │ texU │ texV │ tanX │ tanY │ tanZ │ btaX │ btaY │ btaZ │
  └──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┴──────┘
   attr 0 (3)    attr 1 (3)    attr 2 (2)    attr 3 (3)         attr 4 (3)
```

---

## Normal Mapping Shaders

### Vertex Shader (`normal_mapping.vert`)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    vs_out.FragPos = vec3(model * vec4(aPos, 1.0));
    vs_out.TexCoords = aTexCoords;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * aNormal);

    // Gram-Schmidt re-orthogonalization
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    // Inverse TBN (transpose, since it's orthonormal)
    mat3 TBN = transpose(mat3(T, B, N));

    vs_out.TangentLightPos = TBN * lightPos;
    vs_out.TangentViewPos  = TBN * viewPos;
    vs_out.TangentFragPos  = TBN * vs_out.FragPos;

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

### Fragment Shader (`normal_mapping.frag`)

```glsl
#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec2 TexCoords;
    vec3 TangentLightPos;
    vec3 TangentViewPos;
    vec3 TangentFragPos;
} fs_in;

uniform sampler2D diffuseMap;
uniform sampler2D normalMap;

void main()
{
    // Sample the normal map and transform from [0,1] to [-1,1]
    vec3 normal = texture(normalMap, fs_in.TexCoords).rgb;
    normal = normalize(normal * 2.0 - 1.0);

    // Sample the diffuse color
    vec3 color = texture(diffuseMap, fs_in.TexCoords).rgb;

    // Ambient
    vec3 ambient = 0.1 * color;

    // Diffuse (everything is in tangent space)
    vec3 lightDir = normalize(fs_in.TangentLightPos - fs_in.TangentFragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * color;

    // Specular (Blinn-Phong)
    vec3 viewDir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = vec3(0.2) * spec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
```

All lighting calculations happen in tangent space. The normal from the normal map is already in tangent space, the light position and view position have been transformed to tangent space in the vertex shader — everything matches.

---

## Complex Meshes: Assimp Tangent Calculation

For complex models loaded with Assimp, you don't need to compute tangents manually. Use the `aiProcess_CalcTangentSpace` flag:

```cpp
const aiScene* scene = importer.ReadFile(path,
    aiProcess_Triangulate       |
    aiProcess_GenSmoothNormals  |
    aiProcess_CalcTangentSpace);
```

Then extract the tangent and bitangent from each vertex:

```cpp
glm::vec3 tangent;
tangent.x = mesh->mTangents[i].x;
tangent.y = mesh->mTangents[i].y;
tangent.z = mesh->mTangents[i].z;

glm::vec3 bitangent;
bitangent.x = mesh->mBitangents[i].x;
bitangent.y = mesh->mBitangents[i].y;
bitangent.z = mesh->mBitangents[i].z;
```

Assimp handles all the edge cases (shared vertices, smoothing groups, etc.) that our simple per-triangle computation doesn't.

---

## Complete C++ Source Code

This program renders a quad textured with a brick wall diffuse texture and a corresponding normal map. A light orbits the surface so you can see the bumps react to changing light direction. Press **N** to toggle normal mapping on and off.

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
void renderQuad();

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool normalMapping = true;
bool normalMappingKeyPressed = false;

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
                                          "LearnOpenGL - Normal Mapping",
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

    Shader shader("shaders/normal_mapping.vert",
                   "shaders/normal_mapping.frag");

    unsigned int diffuseMap = loadTexture("textures/brickwall.jpg");
    unsigned int normalMapTex = loadTexture("textures/brickwall_normal.jpg");

    shader.use();
    shader.setInt("diffuseMap", 0);
    shader.setInt("normalMap", 1);

    glm::vec3 lightPos(0.5f, 1.0f, 0.3f);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        // Rotate the quad to see normal mapping from different angles
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model,
            glm::radians(static_cast<float>(glfwGetTime()) * -10.0f),
            glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
        shader.setMat4("model", model);

        shader.setVec3("viewPos", camera.Position);
        shader.setVec3("lightPos", lightPos);
        shader.setBool("normalMapping", normalMapping);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, normalMapTex);

        renderQuad();

        std::cout << (normalMapping ? "Normal mapping: ON" :
                      "Normal mapping: OFF") << "\r" << std::flush;

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

unsigned int quadVAO = 0, quadVBO = 0;
void renderQuad()
{
    if (quadVAO == 0)
    {
        glm::vec3 pos1(-1.0f,  1.0f, 0.0f);
        glm::vec3 pos2(-1.0f, -1.0f, 0.0f);
        glm::vec3 pos3( 1.0f, -1.0f, 0.0f);
        glm::vec3 pos4( 1.0f,  1.0f, 0.0f);

        glm::vec2 uv1(0.0f, 1.0f);
        glm::vec2 uv2(0.0f, 0.0f);
        glm::vec2 uv3(1.0f, 0.0f);
        glm::vec2 uv4(1.0f, 1.0f);

        glm::vec3 nm(0.0f, 0.0f, 1.0f);

        // Triangle 1
        glm::vec3 edge1 = pos2 - pos1;
        glm::vec3 edge2 = pos3 - pos1;
        glm::vec2 deltaUV1 = uv2 - uv1;
        glm::vec2 deltaUV2 = uv3 - uv1;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y -
                           deltaUV2.x * deltaUV1.y);

        glm::vec3 tangent1, bitangent1;
        tangent1.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent1.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent1.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        bitangent1.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        bitangent1.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        bitangent1.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

        // Triangle 2
        edge1 = pos3 - pos1;
        edge2 = pos4 - pos1;
        deltaUV1 = uv3 - uv1;
        deltaUV2 = uv4 - uv1;

        f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        glm::vec3 tangent2, bitangent2;
        tangent2.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent2.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent2.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        bitangent2.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
        bitangent2.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
        bitangent2.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);

        float quadVertices[] = {
            // pos                  // normal         // texcoords  // tangent                          // bitangent
            pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
            pos2.x, pos2.y, pos2.z, nm.x, nm.y, nm.z, uv2.x, uv2.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
            pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,

            pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
            pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
            pos4.x, pos4.y, pos4.z, nm.x, nm.y, nm.z, uv4.x, uv4.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
        };

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              14 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                              14 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                              14 * sizeof(float), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE,
                              14 * sizeof(float), (void*)(8 * sizeof(float)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE,
                              14 * sizeof(float), (void*)(11 * sizeof(float)));
        glBindVertexArray(0);
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
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
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS && !normalMappingKeyPressed)
    {
        normalMapping = !normalMapping;
        normalMappingKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_RELEASE)
    {
        normalMappingKeyPressed = false;
    }
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

A rotating brick wall where the individual bricks appear to protrude from the surface, with mortar grooves casting mini-shadows between them. The effect is convincing even though the geometry is completely flat:

```
  ┌──────────────────────────────────────────┐
  │ LearnOpenGL - Normal Mapping      [—][×]│
  ├──────────────────────────────────────────┤
  │                                          │
  │                                          │
  │         ╔═══╗╔═══╗╔═══╗╔═══╗            │
  │         ║   ║║   ║║   ║║   ║            │
  │         ╚═══╝╚═══╝╚═══╝╚═══╝            │
  │           ╔═══╗╔═══╗╔═══╗               │
  │           ║   ║║   ║║   ║               │
  │           ╚═══╝╚═══╝╚═══╝               │
  │         ╔═══╗╔═══╗╔═══╗╔═══╗            │
  │         ║   ║║   ║║   ║║   ║            │
  │         ╚═══╝╚═══╝╚═══╝╚═══╝            │
  │                                          │
  └──────────────────────────────────────────┘
   Normal mapping ON (press N to toggle)
   Each brick edge catches the light differently.
```

- **With normal mapping ON**: individual bricks appear raised, mortar grooves are visible in the lighting, and specular highlights follow the per-pixel normals.
- **With normal mapping OFF**: the quad is lit as a single flat surface — boring and unrealistic.

---

## Key Takeaways

- **Normal maps** store per-pixel normal directions in a texture, creating the illusion of surface detail without extra geometry.
- Normal maps are predominantly **blue/purple** because most normals point outward (positive Z in tangent space).
- **Tangent space** is a per-surface coordinate system defined by the Tangent (T), Bitangent (B), and Normal (N) vectors.
- The **TBN matrix** transforms between tangent space and world space.
- Tangent and bitangent are computed from **triangle edges and UV differences**, or automatically by Assimp.
- **Gram-Schmidt orthogonalization** ensures the TBN basis remains orthonormal after interpolation.
- **Approach 2** (transform lighting vectors to tangent space in the vertex shader) is more efficient than transforming the normal per-fragment.
- Only use `GL_SRGB` for diffuse textures — normal maps must be loaded as `GL_RGB` since they contain linear data.

## Exercises

1. **Toggle comparison:** With the N key, toggle normal mapping on and off while the quad rotates. Observe how the lighting changes dramatically — especially the specular highlights.
2. **Multiple surfaces:** Render several quads at different orientations (floor, walls, ceiling) all using the same normal map. Verify that the tangent-space approach works correctly regardless of surface orientation.
3. **Approach 1:** Implement the alternative approach — transform the sampled normal to world space in the fragment shader using `TBN * normal`. Compare the visual result (should be identical) and consider the performance implications.
4. **Assimp integration:** Load a complex model with Assimp using `aiProcess_CalcTangentSpace`. Apply a normal map and verify the tangent vectors are computed correctly.
5. **Create a normal map:** Using an image editor or a tool like [NormalMap-Online](https://cpetry.github.io/NormalMap-Online/), generate a normal map from a grayscale heightmap. Apply it to a surface and observe the results.
6. **Combine with shadows:** Integrate normal mapping into the shadow mapping shader from Chapter 3. Use the perturbed normals for lighting but the geometric normals for shadow bias calculation.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Point Shadows](04_point_shadows.md) | [05. Advanced Lighting](.) | [Next: Parallax Mapping →](06_parallax_mapping.md) |
