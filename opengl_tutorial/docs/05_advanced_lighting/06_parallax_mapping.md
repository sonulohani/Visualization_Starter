# Parallax Mapping

[Previous: Normal Mapping](05_normal_mapping.md) | [Next: HDR →](07_hdr.md)

---

Normal mapping gives flat surfaces the *illusion* of fine detail by perturbing per-pixel normals. The lighting looks bumpy, but if you stare at the silhouette or view at a grazing angle, you can tell the surface is still perfectly flat — the texture doesn't shift the way a truly bumpy surface would. **Parallax mapping** takes the illusion one step further: it offsets texture coordinates based on the viewing angle and a height map, making the surface appear to have actual geometric depth.

## From Normal Mapping to Parallax Mapping

Recall the two key limitations of normal mapping:

1. **No silhouette change** — the mesh outline is still flat.
2. **No self-occlusion** — high points don't block the view of low points behind them.

Parallax mapping addresses the second limitation. It doesn't change the silhouette (you'd need tessellation or displacement mapping for that), but it *does* shift the texture in a view-dependent way so that high regions appear to occlude low regions. The visual improvement at non-grazing angles is dramatic, especially on brick walls, cobblestones, and rocky surfaces.

```
Normal Mapping Only            Parallax Mapping
┌─────────────────┐           ┌─────────────────┐
│  Bumps look lit  │           │  Bumps look lit  │
│  correctly, but  │           │  AND shift with  │
│  texture doesn't │           │  view angle —    │
│  shift with view │           │  appears to have │
│  angle. Surface  │           │  real depth.     │
│  feels "painted" │           │  Much more 3D.   │
└─────────────────┘           └─────────────────┘
```

## The Height Map

A **height map** (also called a **displacement map**) is a grayscale texture that encodes per-texel surface elevation:

- **White (1.0)** = the highest point (surface sticks out toward the viewer)
- **Black (0.0)** = the lowest point (surface recedes away from the viewer)

Some implementations invert this convention and use a *depth map* (white = deepest). The math flips accordingly, but the idea is the same. In this tutorial we use a height map stored as a single-channel texture.

```
Height Map                   Interpreted Surface
┌─────────────┐             ╭─╮     ╭─╮
│░░▓▓████▓▓░░│             │ │  ╭──╯ ╰──╮
│░░▓▓████▓▓░░│   ──────►  ╯ ╰──╯        ╰──
│░░▓▓████▓▓░░│
└─────────────┘
 black=low  white=high
```

Typically you author or bake the height map alongside the diffuse and normal map for a given material.

## Basic Parallax Mapping

### The Core Idea

Imagine looking at a bumpy surface at an angle. Your eye traces a ray from the camera through a fragment on the flat polygon. If the surface had real geometry, that ray would hit the *actual* displaced surface at a different point than where it hits the flat polygon. Basic parallax mapping approximates this offset with a single texture lookup.

```
Side view (looking from the right):

   Eye
    ╲
     ╲  View ray
      ╲
       ╲
        ●  A  ← where the ray hits the FLAT surface
       /
      /  Actual displaced surface
     ╱‾‾╲___╱‾‾╲___╱
    B ← where the ray WOULD hit the bumpy surface

We want to sample the texture at B, not A.
The offset (A → B) depends on the view angle and the height at A.
```

### The Math

We work in **tangent space**, where the surface normal points along +Z. The view direction `V` is already transformed into tangent space (just like we did for normal mapping). The algorithm:

1. Sample the height map at the original texture coordinate to get `h`.
2. Scale `h` by a `heightScale` uniform that controls the effect strength.
3. Offset the texture coordinate along the XY components of the view direction, scaled by the height.

```glsl
vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
{
    float height = texture(depthMap, texCoords).r;
    vec2 p = viewDir.xy / viewDir.z * (height * heightScale);
    return texCoords - p;
}
```

Let's break this down:

- **`viewDir.xy`** — the horizontal components of the view direction in tangent space. This determines *which direction* to shift the texture.
- **`viewDir.z`** — the component perpendicular to the surface. At steep viewing angles, `viewDir.z` is small, so dividing by it *increases* the offset (more parallax shift when looking at a steep angle). At head-on angles, `viewDir.z` is close to 1.0, so the offset is minimal.
- **`height * heightScale`** — how far to shift. Taller points shift more; `heightScale` is a tuning parameter (typically `0.02` to `0.1`).

> **Why subtract?** We subtract `p` because we want to shift the texture coordinate *toward* the viewer. In tangent space, the view direction points toward the camera, so subtracting moves us "backward" along the ray to approximate where it would intersect the displaced surface.

### Using the Offset

In the fragment shader, you compute the offset *before* any other texture lookups. Then use the adjusted coordinates for everything — diffuse, normal, specular:

```glsl
void main()
{
    vec3 viewDir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);
    vec2 texCoords = ParallaxMapping(fs_in.TexCoords, viewDir);

    // Discard fragments that fall outside [0,1] after offset
    if(texCoords.x > 1.0 || texCoords.y > 1.0 ||
       texCoords.x < 0.0 || texCoords.y < 0.0)
        discard;

    vec3 normal = texture(normalMap, texCoords).rgb;
    normal = normalize(normal * 2.0 - 1.0);

    vec3 color = texture(diffuseMap, texCoords).rgb;
    // ... proceed with Blinn-Phong lighting using `texCoords` everywhere
}
```

The boundary check (`discard` for out-of-range coordinates) prevents ugly artifacts at the edges of the mesh where the offset pushes coordinates outside the texture.

### The `heightScale` Uniform

This parameter is crucial for getting good results:

| `heightScale` | Effect |
|---|---|
| `0.0` | No parallax at all — identical to normal mapping |
| `0.01–0.03` | Subtle depth, good for fine surface detail |
| `0.05–0.1` | Noticeable depth, works well for bricks and stones |
| `> 0.1` | Exaggerated depth, often causes visible artifacts |

Expose this as a uniform so you can tweak it at runtime.

## The Problem with Basic Parallax

Basic parallax mapping uses a single height sample and a linear approximation. This works acceptably at moderate viewing angles, but **breaks down at steep (grazing) angles** where the approximation diverges badly from the true intersection point:

```
At steep angles, the single-sample approximation (A→B')
misses the real intersection point (B) by a wide margin:

   Eye ←──── steep angle
     ╲
      ╲
       ╲
        ● A (flat surface hit)
   B' ● ← where basic parallax THINKS the surface is
      ╱‾‾╲__
   B ●       ╲___╱‾‾  ← where it ACTUALLY is (much farther)
```

The result is swimming, stretching, or "floating" textures at steep angles. To fix this, we need a better algorithm.

## Steep Parallax Mapping

Instead of one sample, **steep parallax mapping** marches through the height field in discrete steps, effectively ray-marching through the depth layers until it finds where the view ray intersects the surface.

### The Algorithm

1. Divide the depth range `[0.0, 1.0]` into **N equally-spaced layers**.
2. Starting from the top (depth = 0), step downward layer by layer.
3. At each layer, sample the height map and compare it to the current layer depth.
4. When the current layer depth **exceeds** the sampled height → we've gone below the surface. The current texture coordinate is our approximation.

```
View ray stepping through layers:

Layer 0  ─ ─ ─ ─ ─●─ ─ ─ ─ ─    height sample: 0.2 > layer depth 0.0 → continue
Layer 1  ─ ─ ─ ─●─ ─ ─ ─ ─ ─    height sample: 0.4 > layer depth 0.2 → continue
Layer 2  ─ ─ ─●─ ─ ─ ─ ─ ─ ─    height sample: 0.5 > layer depth 0.4 → continue
Layer 3  ─ ─●─ ─ ─ ─ ─ ─ ─ ─    height sample: 0.5 ≤ layer depth 0.6 → STOP!
                                  ↑ intersection found between layer 2 and 3
```

### Implementation

```glsl
vec2 SteepParallaxMapping(vec2 texCoords, vec3 viewDir)
{
    // Dynamically choose layer count based on viewing angle:
    // more layers at steep angles, fewer when looking head-on
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers,
                          max(dot(vec3(0.0, 0.0, 1.0), viewDir), 0.0));

    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;

    // Direction and magnitude to shift texCoords per layer
    vec2 P = viewDir.xy / viewDir.z * heightScale;
    vec2 deltaTexCoords = P / numLayers;

    vec2  currentTexCoords     = texCoords;
    float currentDepthMapValue = texture(depthMap, currentTexCoords).r;

    while(currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(depthMap, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }

    return currentTexCoords;
}
```

### Dynamic Layer Count

The `mix` trick is important: when `viewDir` is nearly perpendicular to the surface (`dot ≈ 1.0`), the ray passes through the height field almost vertically, so fewer layers suffice. At steep angles (`dot ≈ 0.0`), the ray traverses more horizontal distance, requiring more layers to avoid missing features. This balances quality and performance.

```glsl
float numLayers = mix(maxLayers, minLayers,
                      max(dot(vec3(0.0, 0.0, 1.0), viewDir), 0.0));
```

| Viewing angle | `dot(N, V)` | `numLayers` |
|---|---|---|
| Head-on (0°) | ~1.0 | minLayers (8) |
| 45° | ~0.7 | ~15 |
| Grazing (80°+) | ~0.1 | maxLayers (32) |

### Stair-stepping Artifacts

Steep parallax mapping is a significant improvement over basic parallax, but the discrete layers can produce visible **stair-stepping** (aliasing) — you can see horizontal "shelves" in the displacement, especially with fewer layers. This is where Parallax Occlusion Mapping comes in.

## Parallax Occlusion Mapping (POM)

Parallax Occlusion Mapping improves on steep parallax by **interpolating** between the two layers surrounding the intersection point — the last layer *above* the surface and the first layer *below*. This gives a much smoother result at essentially the same cost (just a few extra arithmetic operations).

### The Idea

After the steep parallax loop finds the first layer below the surface, we look at:
- **Before:** the texture coordinate and depth at the previous layer (still above the surface)
- **After:** the texture coordinate and depth at the current layer (below the surface)

We linearly interpolate between these two points based on the relative depth differences:

```
Layer 2 ─ ─ ─ ─ ─ ─ ─   (above surface)
         ╲              depthBefore: how far layer 2 is ABOVE the height
          ╲
           ● ← interpolated intersection
          ╱
         ╱              depthAfter: how far layer 3 is BELOW the height
Layer 3 ─ ─ ─ ─ ─ ─ ─   (below surface)

weight = depthAfter / (depthAfter + depthBefore)
finalTexCoords = mix(currentTexCoords, prevTexCoords, weight)
```

### Implementation

```glsl
vec2 ParallaxOcclusionMapping(vec2 texCoords, vec3 viewDir)
{
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers,
                          max(dot(vec3(0.0, 0.0, 1.0), viewDir), 0.0));

    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;

    vec2 P = viewDir.xy / viewDir.z * heightScale;
    vec2 deltaTexCoords = P / numLayers;

    vec2  currentTexCoords     = texCoords;
    float currentDepthMapValue = texture(depthMap, currentTexCoords).r;

    while(currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(depthMap, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }

    // Get texture coordinates BEFORE the intersection (previous layer)
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // Depth differences for interpolation
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(depthMap, prevTexCoords).r
                        - currentLayerDepth + layerDepth;

    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight
                        + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}
```

> **Note:** `afterDepth` is negative (layer is below the surface), and `beforeDepth` is positive (previous layer was above). The division `afterDepth / (afterDepth - beforeDepth)` correctly yields a weight between 0 and 1.

### Comparison

```
                    Basic Parallax     Steep Parallax     POM
Samples per pixel      1                 8–32              8–32 + lerp
Quality at steep       Poor (swim)       Good (stairs)     Excellent
Performance            Cheapest          Moderate          Moderate
Recommended for        Subtle effects    Moderate depth    Best quality
```

## Integrating Parallax with Normal Mapping

Parallax mapping works best alongside normal mapping. The key rule: **apply the parallax offset first**, then use the adjusted texture coordinates for *all* subsequent texture lookups:

```glsl
void main()
{
    vec3 viewDir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);

    // 1. Offset texture coordinates via parallax
    vec2 texCoords = ParallaxOcclusionMapping(fs_in.TexCoords, viewDir);

    if(texCoords.x > 1.0 || texCoords.y > 1.0 ||
       texCoords.x < 0.0 || texCoords.y < 0.0)
        discard;

    // 2. Sample ALL textures with the offset coordinates
    vec3 diffuse  = texture(diffuseMap, texCoords).rgb;
    vec3 normal   = texture(normalMap, texCoords).rgb;
    normal = normalize(normal * 2.0 - 1.0);

    // 3. Lighting in tangent space (Blinn-Phong)
    vec3 lightDir = normalize(fs_in.TangentLightPos - fs_in.TangentFragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    // Ambient
    vec3 ambient = 0.1 * diffuse;

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuseColor = diff * diffuse;

    // Specular
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = vec3(0.2) * spec;

    FragColor = vec4(ambient + diffuseColor + specular, 1.0);
}
```

## Self-Shadowing (Optional)

For an extra layer of realism, you can trace a ray from the fragment toward the light source through the height map. If the ray encounters a height sample that blocks it, the fragment is in shadow from the parallax perspective.

The algorithm mirrors steep parallax mapping, but traces in the *light* direction instead of the view direction:

```glsl
float ParallaxSelfShadow(vec2 texCoords, vec3 lightDir)
{
    float initialHeight = texture(depthMap, texCoords).r;
    float numLayers = 16.0;
    float layerDepth = initialHeight / numLayers;

    vec2 deltaTexCoords = lightDir.xy / lightDir.z * heightScale / numLayers;

    vec2  currentTexCoords = texCoords + deltaTexCoords;
    float currentDepthMapValue = texture(depthMap, currentTexCoords).r;
    float currentLayerDepth = initialHeight - layerDepth;

    float shadowMultiplier = 0.0;

    while(currentLayerDepth > 0.0)
    {
        if(currentDepthMapValue > currentLayerDepth)
        {
            shadowMultiplier = 1.0;
            break;
        }
        currentTexCoords += deltaTexCoords;
        currentDepthMapValue = texture(depthMap, currentTexCoords).r;
        currentLayerDepth -= layerDepth;
    }

    return 1.0 - shadowMultiplier * 0.5; // soften the shadow
}
```

Then multiply your lighting result by this shadow factor.

## Complete Source Code

Below is a complete program that renders a textured quad (a brick wall) using parallax occlusion mapping combined with normal mapping. Use the arrow keys to adjust `heightScale` at runtime.

### Vertex Shader (`parallax.vert`)

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
    vs_out.FragPos   = vec3(model * vec4(aPos, 1.0));
    vs_out.TexCoords = aTexCoords;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N); // re-orthogonalize
    vec3 B = cross(N, T);

    mat3 TBN = transpose(mat3(T, B, N));
    vs_out.TangentLightPos = TBN * lightPos;
    vs_out.TangentViewPos  = TBN * viewPos;
    vs_out.TangentFragPos  = TBN * vs_out.FragPos;

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

### Fragment Shader (`parallax.frag`)

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
uniform sampler2D depthMap;

uniform float heightScale;

vec2 ParallaxOcclusionMapping(vec2 texCoords, vec3 viewDir)
{
    const float minLayers = 8.0;
    const float maxLayers = 32.0;
    float numLayers = mix(maxLayers, minLayers,
                          max(dot(vec3(0.0, 0.0, 1.0), viewDir), 0.0));

    float layerDepth    = 1.0 / numLayers;
    float currentLayerDepth = 0.0;

    vec2 P = viewDir.xy / viewDir.z * heightScale;
    vec2 deltaTexCoords = P / numLayers;

    vec2  currentTexCoords     = texCoords;
    float currentDepthMapValue = texture(depthMap, currentTexCoords).r;

    while(currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = texture(depthMap, currentTexCoords).r;
        currentLayerDepth += layerDepth;
    }

    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(depthMap, prevTexCoords).r
                        - currentLayerDepth + layerDepth;

    float weight = afterDepth / (afterDepth - beforeDepth);
    return prevTexCoords * weight + currentTexCoords * (1.0 - weight);
}

void main()
{
    vec3 viewDir = normalize(fs_in.TangentViewPos - fs_in.TangentFragPos);
    vec2 texCoords = ParallaxOcclusionMapping(fs_in.TexCoords, viewDir);

    if(texCoords.x > 1.0 || texCoords.y > 1.0 ||
       texCoords.x < 0.0 || texCoords.y < 0.0)
        discard;

    vec3 normal = texture(normalMap, texCoords).rgb;
    normal = normalize(normal * 2.0 - 1.0);

    vec3 color = texture(diffuseMap, texCoords).rgb;

    // Blinn-Phong in tangent space
    vec3 lightDir   = normalize(fs_in.TangentLightPos - fs_in.TangentFragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    vec3 ambient  = 0.1 * color;
    float diff    = max(dot(normal, lightDir), 0.0);
    vec3 diffuse  = diff * color;
    float spec    = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    vec3 specular = vec3(0.2) * spec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
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
#include "shader.h"
#include "camera.h"
#include "stb_image.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path);

const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

float heightScale = 0.1f;

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
                                          "Parallax Mapping", NULL, NULL);
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

    Shader shader("parallax.vert", "parallax.frag");

    unsigned int diffuseMap = loadTexture("resources/bricks2.jpg");
    unsigned int normalMap  = loadTexture("resources/bricks2_normal.jpg");
    unsigned int depthMap   = loadTexture("resources/bricks2_disp.jpg");

    shader.use();
    shader.setInt("diffuseMap", 0);
    shader.setInt("normalMap",  1);
    shader.setInt("depthMap",   2);

    glm::vec3 lightPos(0.5f, 1.0f, 0.3f);

    // Set up a quad with tangent/bitangent vectors
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
    // normal
    glm::vec3 nm(0.0f, 0.0f, 1.0f);

    // Calculate tangent/bitangent for triangle 1
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

    // Calculate tangent/bitangent for triangle 2
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
        // positions          // normals       // texcoords  // tangent          // bitangent
        pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
        pos2.x, pos2.y, pos2.z, nm.x, nm.y, nm.z, uv2.x, uv2.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,
        pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent1.x, tangent1.y, tangent1.z, bitangent1.x, bitangent1.y, bitangent1.z,

        pos1.x, pos1.y, pos1.z, nm.x, nm.y, nm.z, uv1.x, uv1.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
        pos3.x, pos3.y, pos3.z, nm.x, nm.y, nm.z, uv3.x, uv3.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
        pos4.x, pos4.y, pos4.z, nm.x, nm.y, nm.z, uv4.x, uv4.y, tangent2.x, tangent2.y, tangent2.z, bitangent2.x, bitangent2.y, bitangent2.z,
    };

    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(8 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 14 * sizeof(float), (void*)(11 * sizeof(float)));
    glBindVertexArray(0);

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
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::rotate(model, glm::radians((float)glfwGetTime() * -10.0f),
                            glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
        shader.setMat4("model", model);
        shader.setVec3("viewPos", camera.Position);
        shader.setVec3("lightPos", lightPos);
        shader.setFloat("heightScale", heightScale);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, normalMap);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, depthMap);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
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

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) {
        heightScale += 0.5f * deltaTime;
        if (heightScale > 1.0f) heightScale = 1.0f;
        std::cout << "heightScale: " << heightScale << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        heightScale -= 0.5f * deltaTime;
        if (heightScale < 0.0f) heightScale = 0.0f;
        std::cout << "heightScale: " << heightScale << std::endl;
    }
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
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow*, double, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

unsigned int loadTexture(const char* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format = GL_RED;
        if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                     format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                        format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                        format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
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

A slowly rotating brick wall quad. At head-on angles the parallax effect is subtle — the bricks look slightly raised. As you orbit to a steep angle, the bricks clearly appear to have depth: mortar lines recede, brick faces occlude each other, and the texture shifts convincingly. Use the up/down arrow keys to crank `heightScale` up and down. At 0 the surface is flat; at high values the depth illusion is exaggerated (and eventually breaks).

## Key Takeaways

- Parallax mapping offsets texture coordinates based on a **height map** and the **viewing direction** to fake geometric depth on a flat surface.
- **Basic parallax** uses one height sample — cheap but inaccurate at steep angles.
- **Steep parallax mapping** ray-marches through depth layers for better accuracy, but can show stair-stepping.
- **Parallax Occlusion Mapping (POM)** adds linear interpolation between layers, eliminating most artifacts at minimal extra cost.
- Always apply parallax offset **before** sampling normal maps and diffuse textures.
- `heightScale` controls the effect intensity — keep it in the `0.02–0.1` range for realistic results.
- **Self-shadowing** traces from the fragment toward the light for additional realism.

## Exercises

1. **Toggle between methods:** Add a keyboard toggle (e.g., `1`/`2`/`3`) to switch between basic parallax, steep parallax, and POM at runtime. Observe the quality differences, especially at steep viewing angles.
2. **Adjust layer count:** Reduce `minLayers` to 2 and `maxLayers` to 4. The stair-stepping in steep parallax becomes obvious. Find the minimum layer count where POM still looks acceptable.
3. **Self-shadowing:** Implement the self-shadowing technique described above. Apply it to the brick wall scene — mortar lines should darken when they're occluded from the light.
4. **Multiple quads:** Render a floor made of several quads, each with parallax mapping. Make sure the effect tiles seamlessly. (Hint: you may need to adjust UV coordinates and the `discard` boundary check.)
5. **Depth map inversion:** Some engines use a depth map (white = deep) instead of a height map (white = high). Modify the shader to work with an inverted map (`height = 1.0 - texture(depthMap, texCoords).r`). Verify the results are identical.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Normal Mapping](05_normal_mapping.md) | [05. Advanced Lighting](.) | [Next: HDR →](07_hdr.md) |
