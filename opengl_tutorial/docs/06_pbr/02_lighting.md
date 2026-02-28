# PBR Lighting

[Previous: PBR Theory](01_theory.md) | [Next: IBL — Diffuse Irradiance →](03_ibl_diffuse.md)

---

With the theory behind us, it's time to translate the Cook-Torrance BRDF into working code. In this chapter we implement a complete PBR lighting shader, first with uniform material parameters, then with textures. By the end you'll have a grid of spheres demonstrating how roughness and metallic values shape the appearance of physically based materials.

## Overview

Here's our plan:

1. Write the four core BRDF functions in GLSL (NDF, Geometry, Fresnel, and the combined Cook-Torrance).
2. Build a PBR fragment shader that loops over point lights and evaluates the reflectance equation.
3. Render a 7×7 grid of spheres with varying roughness and metallic.
4. Upgrade to texture-based PBR with albedo, normal, metallic, roughness, and AO maps.

## The PBR Vertex Shader

The vertex shader is straightforward — we need world-space positions and normals for lighting, plus texture coordinates:

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 WorldPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat3 normalMatrix;

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalMatrix * aNormal;
    TexCoords = aTexCoords;

    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
```

We pass a pre-computed `normalMatrix` (the transpose of the inverse of the upper-left 3×3 of the model matrix) to correctly transform normals even when the model matrix has non-uniform scaling.

## The PBR Fragment Shader — Step by Step

Let's build the fragment shader incrementally. First, the skeleton and uniforms:

```glsl
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoords;

// Material parameters (uniform-based for now)
uniform vec3  albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;

// Lights
uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];

uniform vec3 camPos;

const float PI = 3.14159265359;
```

### Step 1: Normal Distribution Function (D)

The Trowbridge-Reitz GGX distribution tells us what fraction of microfacets are aligned with the halfway vector:

```glsl
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}
```

We square the roughness parameter (`a = roughness²`) because Disney's research showed this gives more perceptually linear control — the visual difference between roughness 0.0 and 0.5 is roughly comparable to the difference between 0.5 and 1.0.

### Step 2: Geometry Function (G)

Smith's method combines obstruction (light is blocked before reaching the microfacet) and shadowing (reflected light is blocked before reaching the viewer):

```glsl
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
```

Note the `k` computation uses `(roughness + 1)² / 8` — this is the formula for **direct lighting**. When we move to image-based lighting (IBL) in later chapters, we'll use a different `k`.

### Step 3: Fresnel Equation (F)

Schlick's approximation gives us the ratio of light that gets reflected versus refracted:

```glsl
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
```

This returns a `vec3` because the Fresnel response can be different per color channel (especially for metals — gold reflects red and green more than blue).

### Step 4: The Main Function — Reflectance Equation

Now we tie everything together:

```glsl
void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);

    // Calculate reflectance at normal incidence.
    // For dielectrics, F0 is 0.04; for metals, F0 is the albedo color.
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Reflectance equation — accumulate contribution from each light
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 4; ++i)
    {
        // Per-light radiance
        vec3 L = normalize(lightPositions[i] - WorldPos);
        vec3 H = normalize(V + L);
        float distance    = length(lightPositions[i] - WorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance     = lightColors[i] * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        // kS is the Fresnel reflectance — the specular contribution
        vec3 kS = F;
        // kD is everything that wasn't reflected — the diffuse contribution
        vec3 kD = vec3(1.0) - kS;
        // Metals have no diffuse
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

        // Add this light's contribution
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Ambient lighting (a very rough approximation until we do IBL)
    vec3 ambient = vec3(0.03) * albedo * ao;

    vec3 color = ambient + Lo;

    // HDR tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
```

A few important details:

- **The `+ 0.0001` in the denominator** prevents division by zero when `NdotV` or `NdotL` is zero (at grazing angles).
- **`kD *= 1.0 - metallic`** ensures metals produce no diffuse reflection.
- **Attenuation uses inverse-square law** (`1 / distance²`) — physically correct, unlike the quadratic polynomial we used with Phong.
- **Tone mapping and gamma correction** are applied at the end because PBR operates in linear HDR space.

## Complete PBR Fragment Shader (Uniform-Based)

Here is the full shader in one piece for easy reference:

```glsl
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3  albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;

uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];

uniform vec3 camPos;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 4; ++i)
    {
        vec3 L = normalize(lightPositions[i] - WorldPos);
        vec3 H = normalize(V + L);
        float distance    = length(lightPositions[i] - WorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance     = lightColors[i] * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color   = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
```

## The C++ Side — Rendering a Sphere Grid

To showcase PBR, we render a **7×7 grid of spheres**. Roughness increases along the horizontal axis (left = smooth, right = rough) and metallic along the vertical axis (bottom = dielectric, top = metal). Four point lights illuminate the scene.

### Sphere Generation

We generate a UV sphere procedurally. Each vertex has a position, normal, and texture coordinate:

```cpp
#include <vector>
#include <cmath>

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

unsigned int sphereVAO = 0;
unsigned int sphereIndexCount;

void renderSphere()
{
    if (sphereVAO == 0)
    {
        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        const float PI = 3.14159265359f;

        std::vector<glm::vec3> positions;
        std::vector<glm::vec2> uv;
        std::vector<glm::vec3> normals;
        std::vector<unsigned int> indices;

        for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
        {
            for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
            {
                float xSegment = static_cast<float>(x) / X_SEGMENTS;
                float ySegment = static_cast<float>(y) / Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                float yPos = std::cos(ySegment * PI);
                float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                positions.push_back(glm::vec3(xPos, yPos, zPos));
                uv.push_back(glm::vec2(xSegment, ySegment));
                normals.push_back(glm::vec3(xPos, yPos, zPos));
            }
        }

        bool oddRow = false;
        for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
        {
            if (!oddRow)
            {
                for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
                {
                    indices.push_back(y       * (X_SEGMENTS + 1) + x);
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                }
            }
            else
            {
                for (int x = X_SEGMENTS; x >= 0; --x)
                {
                    indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                    indices.push_back(y       * (X_SEGMENTS + 1) + x);
                }
            }
            oddRow = !oddRow;
        }
        sphereIndexCount = static_cast<unsigned int>(indices.size());

        std::vector<float> data;
        for (unsigned int i = 0; i < positions.size(); ++i)
        {
            data.push_back(positions[i].x);
            data.push_back(positions[i].y);
            data.push_back(positions[i].z);
            if (!normals.empty())
            {
                data.push_back(normals[i].x);
                data.push_back(normals[i].y);
                data.push_back(normals[i].z);
            }
            if (!uv.empty())
            {
                data.push_back(uv[i].x);
                data.push_back(uv[i].y);
            }
        }

        unsigned int sphereVBO, sphereEBO;
        glGenVertexArrays(1, &sphereVAO);
        glGenBuffers(1, &sphereVBO);
        glGenBuffers(1, &sphereEBO);

        glBindVertexArray(sphereVAO);
        glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
        glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float),
                     data.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(unsigned int),
                     indices.data(), GL_STATIC_DRAW);

        unsigned int stride = (3 + 3 + 2) * sizeof(float);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                              (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                              (void*)(6 * sizeof(float)));

        glBindVertexArray(0);
    }

    glBindVertexArray(sphereVAO);
    glDrawElements(GL_TRIANGLE_STRIP, sphereIndexCount,
                   GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
```

### Light Setup and Render Loop

```cpp
// Light positions and colors
glm::vec3 lightPositions[] = {
    glm::vec3(-10.0f,  10.0f, 10.0f),
    glm::vec3( 10.0f,  10.0f, 10.0f),
    glm::vec3(-10.0f, -10.0f, 10.0f),
    glm::vec3( 10.0f, -10.0f, 10.0f),
};
glm::vec3 lightColors[] = {
    glm::vec3(300.0f, 300.0f, 300.0f),
    glm::vec3(300.0f, 300.0f, 300.0f),
    glm::vec3(300.0f, 300.0f, 300.0f),
    glm::vec3(300.0f, 300.0f, 300.0f),
};

int nrRows    = 7;
int nrColumns = 7;
float spacing = 2.5f;
```

The light colors are set to 300.0 — far above 1.0. This is intentional. We're working in HDR space, so these physically plausible high-intensity values are tone-mapped down in the shader.

### The Render Loop

```cpp
while (!glfwWindowShouldClose(window))
{
    // Per-frame logic
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    processInput(window);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pbrShader.use();
    glm::mat4 projection = glm::perspective(
        glm::radians(camera.Zoom),
        (float)SCR_WIDTH / (float)SCR_HEIGHT,
        0.1f, 100.0f
    );
    glm::mat4 view = camera.GetViewMatrix();
    pbrShader.setMat4("projection", projection);
    pbrShader.setMat4("view", view);
    pbrShader.setVec3("camPos", camera.Position);

    // Set light uniforms
    for (unsigned int i = 0; i < 4; ++i)
    {
        pbrShader.setVec3("lightPositions[" + std::to_string(i) + "]",
                          lightPositions[i]);
        pbrShader.setVec3("lightColors[" + std::to_string(i) + "]",
                          lightColors[i]);
    }

    // Render 7x7 sphere grid
    glm::mat4 model = glm::mat4(1.0f);
    for (int row = 0; row < nrRows; ++row)
    {
        pbrShader.setFloat("metallic", static_cast<float>(row) / nrRows);
        for (int col = 0; col < nrColumns; ++col)
        {
            // Clamp roughness to [0.05, 1.0] — 0.0 looks incorrect
            // with direct lighting (infinitely small highlight)
            pbrShader.setFloat("roughness",
                glm::clamp(static_cast<float>(col) / nrColumns,
                           0.05f, 1.0f));

            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(
                (col - (nrColumns / 2)) * spacing,
                (row - (nrRows / 2)) * spacing,
                0.0f
            ));
            pbrShader.setMat4("model", model);
            pbrShader.setMat3("normalMatrix",
                glm::transpose(glm::inverse(glm::mat3(model))));

            pbrShader.setVec3("albedo", 0.5f, 0.0f, 0.0f);
            pbrShader.setFloat("ao", 1.0f);

            renderSphere();
        }
    }

    glfwSwapBuffers(window);
    glfwPollEvents();
}
```

```
Sphere Grid Layout (7×7)
════════════════════════

  metallic
  1.0 ─── ● ● ● ● ● ● ●   ← fully metallic
          ● ● ● ● ● ● ●
          ● ● ● ● ● ● ●
          ● ● ● ● ● ● ●
          ● ● ● ● ● ● ●
          ● ● ● ● ● ● ●
  0.0 ─── ● ● ● ● ● ● ●   ← fully dielectric
          │             │
        0.05          1.0
         smooth ──── rough
              roughness →

  ☀ light        ☀ light
  (-10,10,10)    (10,10,10)

  ☀ light        ☀ light
  (-10,-10,10)   (10,-10,10)
```

We clamp roughness to a minimum of 0.05 rather than 0.0 because a perfectly smooth surface produces an infinitely small specular highlight that can cause numerical issues with point lights. (With IBL in later chapters, roughness 0.0 works fine.)

## Textured PBR

Using uniform values is great for demonstrating the effect of each parameter, but real-world PBR materials use **textures** to provide per-pixel variation. A typical PBR material consists of five maps:

| Texture | Type | Description |
|---|---|---|
| **Albedo map** | sRGB (vec3) | Base color — sampled and converted to linear space |
| **Normal map** | Linear (vec3) | Per-pixel normal perturbation in tangent space |
| **Metallic map** | Linear (float) | Per-pixel metallic value (usually black or white) |
| **Roughness map** | Linear (float) | Per-pixel roughness |
| **AO map** | Linear (float) | Pre-baked ambient occlusion |

### Textured PBR Vertex Shader

For normal mapping, we need the TBN matrix. This is the same technique we covered in the Normal Mapping chapter:

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec3 WorldPos;
out vec2 TexCoords;
out vec3 Normal;
out mat3 TBN;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat3 normalMatrix;

void main()
{
    WorldPos = vec3(model * vec4(aPos, 1.0));
    TexCoords = aTexCoords;

    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * aNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);

    TBN = mat3(T, B, N);
    Normal = N;

    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
```

We re-orthogonalize the tangent with the Gram-Schmidt process (`T - dot(T, N) * N`) to handle imprecise mesh tangents.

### Textured PBR Fragment Shader

The fragment shader samples material properties from textures instead of uniforms:

```glsl
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec2 TexCoords;
in vec3 Normal;
in mat3 TBN;

uniform sampler2D albedoMap;
uniform sampler2D normalMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;

uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];

uniform vec3 camPos;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    // Sample material textures
    vec3  albedo    = pow(texture(albedoMap, TexCoords).rgb, vec3(2.2));
    float metallic  = texture(metallicMap, TexCoords).r;
    float roughness = texture(roughnessMap, TexCoords).r;
    float ao        = texture(aoMap, TexCoords).r;

    // Obtain normal from normal map in [0,1] range and transform to [-1,1]
    vec3 N = texture(normalMap, TexCoords).rgb;
    N = N * 2.0 - 1.0;
    N = normalize(TBN * N);

    vec3 V = normalize(camPos - WorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < 4; ++i)
    {
        vec3 L = normalize(lightPositions[i] - WorldPos);
        vec3 H = normalize(V + L);
        float distance    = length(lightPositions[i] - WorldPos);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance     = lightColors[i] * attenuation;

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color   = ambient + Lo;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
```

The critical differences from the uniform version:

1. **Albedo is converted from sRGB to linear** with `pow(texture(...).rgb, vec3(2.2))`. Albedo textures are authored in sRGB space, but all lighting math must happen in linear space.
2. **The normal comes from a normal map** transformed through the TBN matrix, giving us fine surface detail.
3. **Metallic, roughness, and AO are sampled as single channels** from their respective maps.

### Loading and Binding PBR Textures on the C++ Side

```cpp
unsigned int albedoMap    = loadTexture("textures/pbr/rusted_iron/albedo.png");
unsigned int normalMap    = loadTexture("textures/pbr/rusted_iron/normal.png");
unsigned int metallicMap  = loadTexture("textures/pbr/rusted_iron/metallic.png");
unsigned int roughnessMap = loadTexture("textures/pbr/rusted_iron/roughness.png");
unsigned int aoMap        = loadTexture("textures/pbr/rusted_iron/ao.png");

pbrShader.use();
pbrShader.setInt("albedoMap",    0);
pbrShader.setInt("normalMap",    1);
pbrShader.setInt("metallicMap",  2);
pbrShader.setInt("roughnessMap", 3);
pbrShader.setInt("aoMap",        4);

// In the render loop, before drawing:
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, albedoMap);
glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, normalMap);
glActiveTexture(GL_TEXTURE2);
glBindTexture(GL_TEXTURE_2D, metallicMap);
glActiveTexture(GL_TEXTURE3);
glBindTexture(GL_TEXTURE_2D, roughnessMap);
glActiveTexture(GL_TEXTURE4);
glBindTexture(GL_TEXTURE_2D, aoMap);
```

PBR texture sets are widely available. Free resources include [FreePBR](https://freepbr.com) and [Poly Haven](https://polyhaven.com). You can find materials like rusted iron, gold, scratched plastic, stone, and more — each consisting of the five maps described above.

## Understanding the Results

With the sphere grid rendering, you should observe:

- **Bottom-left sphere** (roughness ≈ 0, metallic ≈ 0): A smooth dielectric. Produces a small, bright specular highlight with visible diffuse color underneath.
- **Bottom-right sphere** (roughness ≈ 1, metallic ≈ 0): A rough dielectric. Almost no visible specular highlight; the surface appears matte with only diffuse color.
- **Top-left sphere** (roughness ≈ 0, metallic ≈ 1): A polished metal. Mirror-like specular reflection tinted by the albedo, no diffuse color at all.
- **Top-right sphere** (roughness ≈ 1, metallic ≈ 1): A rough metal. Broad, dim specular highlight with albedo tinting, still no diffuse.

The Fresnel effect is visible on all spheres — look at the edges (grazing angles), where even the dielectric spheres become noticeably more reflective.

## Key Takeaways

- The entire Cook-Torrance BRDF can be implemented in about 50 lines of GLSL through four functions: `DistributionGGX`, `GeometrySchlickGGX`, `GeometrySmith`, and `fresnelSchlick`.
- Point light attenuation uses physically correct inverse-square falloff instead of the ad hoc polynomial from Phong.
- Light colors far exceed 1.0 (e.g., 300.0) because PBR works in HDR space — tone mapping handles the conversion to displayable range.
- Textured PBR replaces uniform parameters with per-pixel texture lookups, adding realistic surface detail. Albedo textures must be converted from sRGB to linear space.
- The `+ 0.0001` epsilon in the denominator is a practical necessity to prevent division by zero at grazing angles.
- Even though the ambient term `vec3(0.03) * albedo * ao` is crude, the scene already looks dramatically more realistic than Phong. In the next two chapters, we'll replace this constant ambient with Image-Based Lighting for truly stunning results.

## Exercises

1. **Experiment with albedo colors:** Change the `albedo` uniform to various colors (blue, green, white, dark grey). Notice how metals tint their specular highlight while dielectrics keep it white.
2. **Add a directional light:** Modify the shader to support a directional light in addition to the four point lights. A directional light has no attenuation and a constant direction.
3. **Visualize individual BRDF components:** Output just the NDF (D), just the Geometry (G), or just the Fresnel (F) as the fragment color. This helps build intuition for what each function contributes.
4. **Implement Oren-Nayar diffuse:** Replace the Lambertian diffuse term (`albedo / PI`) with the Oren-Nayar model, which accounts for rough diffuse surfaces more accurately.
5. **Multiple materials:** Instead of a uniform albedo, assign different albedo and base roughness/metallic values to different spheres to simulate a material showcase (gold, copper, plastic, rubber, etc.).

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: PBR Theory](01_theory.md) | [06. PBR](.) | [Next: IBL — Diffuse Irradiance →](03_ibl_diffuse.md) |
