# IBL: Diffuse Irradiance

[Previous: PBR Lighting](02_lighting.md) | [Next: IBL — Specular →](04_ibl_specular.md)

---

Our PBR shader from the previous chapter produces realistic results with direct light sources, but the ambient term — a flat `vec3(0.03) * albedo * ao` — is embarrassingly crude. In the real world, objects are lit not just by point lights but by the entire environment: the sky, the ground, buildings, clouds. **Image-Based Lighting (IBL)** replaces that constant ambient with lighting sampled from an actual environment map, producing dramatically more realistic results.

IBL is a broad topic, so we split it into two chapters. This chapter handles the **diffuse** part of IBL. The next chapter tackles the harder **specular** part.

## What Is Image-Based Lighting?

With IBL, we treat an HDR environment image as a light source that illuminates the scene from every direction. Instead of a handful of point lights, we have effectively infinite lights — one for every pixel of the environment map.

```
Point Lights vs. IBL
═════════════════════

  Point Lights:               IBL:
  Light comes from            Light comes from EVERY direction
  a few specific points       (the entire environment)

      ☀         ☀            ╭────────────────────────╮
       ╲       ╱             │ ☁    ☀     ☁     ☁    │
        ╲     ╱              │   sky / buildings /    │
         ╲   ╱               │   trees / ground      │
          ╲ ╱                │         ╲ ↓ ╱         │
           ●                 │          ● ←──────────│── every texel
          ╱ ╲                │         ╱ ↑ ╲         │   is a light
         ╱   ╲               │                       │
      ☀         ☀            ╰────────────────────────╯
```

This means the reflectance equation integral — which we previously simplified to a sum over a few lights — now needs to be solved as a proper integral over the entire hemisphere. Computing this integral per-fragment in real time is far too expensive, so we **pre-compute** it into lookup textures.

## Splitting the Reflectance Equation

Recall the reflectance equation:

$$L_o(p, \omega_o) = \int_\Omega (k_D \frac{c}{\pi} + \frac{DFG}{4(\omega_o \cdot n)(\omega_i \cdot n)}) \cdot L_i(p, \omega_i) \cdot (n \cdot \omega_i) \, d\omega_i$$

We can split this into two separate integrals — **diffuse** and **specular**:

$$L_o = \underbrace{\int_\Omega k_D \frac{c}{\pi} \cdot L_i \cdot (n \cdot \omega_i) \, d\omega_i}_{\text{Diffuse IBL}} + \underbrace{\int_\Omega \frac{DFG}{4(\omega_o \cdot n)(\omega_i \cdot n)} \cdot L_i \cdot (n \cdot \omega_i) \, d\omega_i}_{\text{Specular IBL (next chapter)}}$$

The diffuse integral has a very useful property: the Lambertian term `c/π` and `kD` are **constant** with respect to the integration variable \(\omega_i\). We can factor them out:

$$L_{diffuse} = k_D \cdot \frac{c}{\pi} \cdot \int_\Omega L_i(p, \omega_i) \cdot (n \cdot \omega_i) \, d\omega_i$$

The remaining integral — incoming radiance weighted by cosine, integrated over the hemisphere — depends only on the surface normal direction \(n\), not on the view direction. This means we can **pre-compute** it for every possible normal direction and store the result in a cubemap. This pre-computed cubemap is called the **irradiance map**.

## HDR Environment Maps

IBL requires HDR environment data — we need color values above 1.0 to properly represent bright light sources like the sun. The most common format is the **equirectangular** HDR image (`.hdr` files), a single 2D image that maps the entire 360° environment onto a rectangle.

```
Equirectangular Map
════════════════════

  ┌──────────────────────────────────────────────┐
  │             sky / zenith                      │
  │      ☁         ☀ sun         ☁               │  ← top = up
  │                                              │
  │  mountains ──── horizon ──── buildings       │  ← middle = horizon
  │                                              │
  │          ground / nadir                      │  ← bottom = down
  └──────────────────────────────────────────────┘
   0°                                          360°
   ← longitude →
```

### Loading HDR Images with stb_image

The `stbi_loadf` function loads `.hdr` files as floating-point data:

```cpp
#include "stb_image.h"

stbi_set_flip_vertically_on_load(true);
int width, height, nrComponents;
float* data = stbi_loadf("textures/hdr/newport_loft.hdr",
                          &width, &height, &nrComponents, 0);

unsigned int hdrTexture;
if (data)
{
    glGenTextures(1, &hdrTexture);
    glBindTexture(GL_TEXTURE_2D, hdrTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0,
                 GL_RGB, GL_FLOAT, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
}
else
{
    std::cout << "Failed to load HDR image." << std::endl;
}
```

We use `GL_RGB16F` as the internal format to preserve the HDR range. Free HDR environment maps are available from [Poly Haven](https://polyhaven.com/hdris) and [sIBL Archive](http://www.hdrlabs.com/sibl/archive.html).

## Converting Equirectangular to Cubemap

Cubemaps are much more practical for real-time sampling than equirectangular maps (no distortion at the poles, hardware-accelerated sampling). We convert by rendering a unit cube from inside, using a shader that samples the equirectangular map based on the cube face direction.

### Setup: Capture Framebuffer

We need a framebuffer with a renderbuffer for depth, and we'll render to each face of a cubemap:

```cpp
unsigned int captureFBO, captureRBO;
glGenFramebuffers(1, &captureFBO);
glGenRenderbuffers(1, &captureRBO);

glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                          GL_RENDERBUFFER, captureRBO);
```

### Setup: Empty Cubemap

```cpp
unsigned int envCubemap;
glGenTextures(1, &envCubemap);
glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
for (unsigned int i = 0; i < 6; ++i)
{
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
}
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

### Equirectangular-to-Cubemap Vertex Shader

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 WorldPos;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    WorldPos = aPos;
    gl_Position = projection * view * vec4(WorldPos, 1.0);
}
```

### Equirectangular-to-Cubemap Fragment Shader

This shader takes a 3D direction, converts it to equirectangular UV coordinates, and samples the HDR texture:

```glsl
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;

uniform sampler2D equirectangularMap;

const vec2 invAtan = vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = SampleSphericalMap(normalize(WorldPos));
    vec3 color = texture(equirectangularMap, uv).rgb;

    FragColor = vec4(color, 1.0);
}
```

The math: `atan(z, x)` gives the longitude (−π to π), and `asin(y)` gives the latitude (−π/2 to π/2). Multiplying by `invAtan` (= 1/(2π) and 1/π) normalizes to [−0.5, 0.5], then we add 0.5 to get [0, 1] UVs.

### Rendering to the Cubemap Faces

We set up six view matrices looking down each axis from the center of the cube:

```cpp
glm::mat4 captureProjection = glm::perspective(
    glm::radians(90.0f), 1.0f, 0.1f, 10.0f
);
glm::mat4 captureViews[] = {
    glm::lookAt(glm::vec3(0), glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
};

equirectangularToCubemapShader.use();
equirectangularToCubemapShader.setInt("equirectangularMap", 0);
equirectangularToCubemapShader.setMat4("projection", captureProjection);

glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, hdrTexture);

glViewport(0, 0, 512, 512);
glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
for (unsigned int i = 0; i < 6; ++i)
{
    equirectangularToCubemapShader.setMat4("view", captureViews[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                           envCubemap, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderCube();
}
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

The 90° field-of-view with a 1:1 aspect ratio ensures each render covers exactly one cubemap face. After this loop, `envCubemap` contains the full HDR environment as a cubemap.

### The renderCube() Helper

For reference, here's the unit cube used for rendering to cubemap faces and for the skybox:

```cpp
unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;

void renderCube()
{
    if (cubeVAO == 0)
    {
        float vertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
        };

        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);
        glBindVertexArray(cubeVAO);
        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
                     GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}
```

## The Irradiance Map (Diffuse Convolution)

Now for the key step. We need to pre-compute, for every possible surface normal direction, the integral of all incoming light weighted by cosine. This is a **convolution** of the environment cubemap.

The result is a small cubemap (32×32 per face is sufficient — the diffuse integral is very smooth) called the **irradiance map**:

```
Environment Cubemap → Irradiance Map
═════════════════════════════════════

  ┌──────────────┐          ┌──────────────┐
  │ Sharp detail │   blur   │ Smooth color │
  │ clouds, sun, │ ───────► │ average of   │
  │ buildings    │ convolve │ hemisphere   │
  │ 512×512/face │          │ 32×32/face   │
  └──────────────┘          └──────────────┘
  Environment map           Irradiance map
```

### Creating the Irradiance Cubemap

```cpp
unsigned int irradianceMap;
glGenTextures(1, &irradianceMap);
glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
for (unsigned int i = 0; i < 6; ++i)
{
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
}
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

32×32 per face is enough because the irradiance map is an extremely blurred version of the environment — all high-frequency detail is averaged out.

### Irradiance Convolution Shader

The vertex shader is the same as the equirectangular-to-cubemap shader. The fragment shader performs the hemisphere convolution:

```glsl
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;

uniform samplerCube environmentMap;

const float PI = 3.14159265359;

void main()
{
    // The world-space direction of this fragment is the normal
    // for which we're computing irradiance
    vec3 N = normalize(WorldPos);

    vec3 irradiance = vec3(0.0);

    // Build a tangent-space basis around N
    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;

    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // Spherical to Cartesian (in tangent space)
            vec3 tangentSample = vec3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta)
            );
            // Tangent space to world space
            vec3 sampleVec = tangentSample.x * right
                           + tangentSample.y * up
                           + tangentSample.z * N;

            irradiance += texture(environmentMap, sampleVec).rgb
                        * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));

    FragColor = vec4(irradiance, 1.0);
}
```

Let's break down what's happening:

1. **Build a tangent-space basis** around the normal direction \(N\). This lets us express sample directions relative to \(N\).
2. **Iterate over the hemisphere** using spherical coordinates: `phi` (azimuth, 0 to 2π) and `theta` (polar, 0 to π/2).
3. **Convert each sample** from tangent space to world space and look up the environment map.
4. **Weight by `cos(theta) * sin(theta)`**: `cos(theta)` is Lambert's cosine law (light hitting at a steeper angle contributes less), and `sin(theta)` accounts for the fact that there are more directions near the equator of the hemisphere than near the pole (the solid angle differential in spherical coordinates).
5. **Normalize** by dividing by the number of samples and multiplying by π.

```
Hemisphere Sampling
═══════════════════

          N (surface normal)
          ↑  ╱  theta = 0 (pole)
          │ ╱
          │╱  theta = π/4
    ──────●──────  theta = π/2 (equator)
          │
      phi goes around (0 to 2π)

  For each (phi, theta) pair, we sample the environment map
  and weight by cos(theta) × sin(theta).
```

### Rendering the Irradiance Map

```cpp
glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

irradianceShader.use();
irradianceShader.setInt("environmentMap", 0);
irradianceShader.setMat4("projection", captureProjection);

glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

glViewport(0, 0, 32, 32);
glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
for (unsigned int i = 0; i < 6; ++i)
{
    irradianceShader.setMat4("view", captureViews[i]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                           irradianceMap, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderCube();
}
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

This is the same render-to-cubemap-face technique we used for the equirectangular conversion, just at a smaller resolution (32×32) and with the convolution shader.

## Using the Irradiance Map for Diffuse Ambient

Now we modify our PBR shader to use the irradiance map instead of the constant `vec3(0.03)` ambient.

### Modified Fresnel for IBL

When we compute kS for ambient lighting, we need a version of the Fresnel equation that accounts for roughness. The standard Schlick approximation doesn't consider roughness, which causes rough surfaces to show excessive Fresnel reflection at grazing angles. The fix:

```glsl
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
             * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
```

This replaces the `(1 - F0)` maximum with `max(1 - roughness, F0)`, so rough surfaces don't become overly reflective at the edges.

### Updated PBR Fragment Shader with Diffuse IBL

Here is the complete updated fragment shader (changes from the previous chapter are in the ambient calculation at the bottom):

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

uniform samplerCube irradianceMap;

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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
             * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // --- Direct lighting (same as before) ---
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
        float denominator = 4.0 * max(dot(N, V), 0.0)
                          * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // --- Ambient lighting with diffuse IBL ---
    vec3 kS = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse    = irradiance * albedo;
    vec3 ambient    = (kD * diffuse) * ao;

    vec3 color = ambient + Lo;

    // HDR tone mapping + gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
```

The ambient section is where the magic happens:

1. Compute `kS` using `fresnelSchlickRoughness` — this tells us how much light is reflected (and therefore not available for diffuse).
2. `kD = 1.0 - kS` scaled by `(1 - metallic)` — the diffuse fraction.
3. Sample the irradiance map using the surface normal `N` — this gives us the pre-computed diffuse irradiance from that direction.
4. Multiply by albedo to get the diffuse color contribution.
5. Apply ambient occlusion.

## Rendering the HDR Skybox

We should also render the environment as a background. This is essentially a cubemap skybox, but we need to render it in HDR and then tone-map.

### Skybox Vertex Shader

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 WorldPos;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    WorldPos = aPos;

    mat4 rotView = mat4(mat3(view));
    vec4 clipPos = projection * rotView * vec4(WorldPos, 1.0);

    gl_Position = clipPos.xyww;
}
```

We set `gl_Position = clipPos.xyww` so that the depth is always 1.0 (maximum depth), ensuring the skybox is rendered behind everything else. The `mat3(view)` trick removes the translation component so the skybox doesn't move with the camera.

### Skybox Fragment Shader

```glsl
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;

uniform samplerCube environmentMap;

void main()
{
    vec3 envColor = texture(environmentMap, WorldPos).rgb;

    // HDR tone mapping
    envColor = envColor / (envColor + vec3(1.0));
    // Gamma correction
    envColor = pow(envColor, vec3(1.0 / 2.2));

    FragColor = vec4(envColor, 1.0);
}
```

### Rendering the Skybox

```cpp
backgroundShader.use();
backgroundShader.setMat4("projection", projection);
backgroundShader.setMat4("view", view);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
renderCube();
```

Remember to set the depth function to `GL_LEQUAL` before rendering the skybox (and reset it after):

```cpp
glDepthFunc(GL_LEQUAL);
backgroundShader.use();
// ... set uniforms, bind cubemap, renderCube() ...
glDepthFunc(GL_LESS);
```

## The Complete Rendering Pipeline

Here's the full sequence from startup to frame rendering:

```
IBL Diffuse Pipeline — Initialization
══════════════════════════════════════

  1. Load HDR equirectangular image
          │
          ▼
  2. Convert to cubemap (512×512 per face)
     ┌──────────┐
     │ Render   │  equirectangular → cubemap shader
     │ cube     │  render to FBO, 6 faces
     │ faces    │
     └────┬─────┘
          │
          ▼
  3. Convolve cubemap → irradiance map (32×32 per face)
     ┌──────────┐
     │ Render   │  convolution shader
     │ cube     │  hemisphere integral per texel
     │ faces    │
     └────┬─────┘
          │
          ▼
  4. Ready for rendering!


IBL Diffuse Pipeline — Per Frame
════════════════════════════════

  ┌───────────────────────────────────┐
  │ For each object:                  │
  │  • Direct lighting (point lights) │
  │  • Ambient = kD × irradiance × alb│
  │            × ao                   │
  │  • Tone map + gamma              │
  └───────────────┬───────────────────┘
                  │
                  ▼
  ┌───────────────────────────────────┐
  │ Render HDR skybox                 │
  │ (environment cubemap, depth=1.0)  │
  └───────────────────────────────────┘
```

## Binding Everything in C++

Here's how the uniforms and textures are set up in the render loop:

```cpp
// Restore full viewport after pre-computation
glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);

// In the render loop:
pbrShader.use();
pbrShader.setMat4("projection", projection);
pbrShader.setMat4("view", view);
pbrShader.setVec3("camPos", camera.Position);

// Bind irradiance map
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
pbrShader.setInt("irradianceMap", 0);

// Set light uniforms
for (unsigned int i = 0; i < 4; ++i)
{
    pbrShader.setVec3("lightPositions[" + std::to_string(i) + "]",
                      lightPositions[i]);
    pbrShader.setVec3("lightColors[" + std::to_string(i) + "]",
                      lightColors[i]);
}

// Render sphere grid (same as before)
for (int row = 0; row < nrRows; ++row)
{
    pbrShader.setFloat("metallic", static_cast<float>(row) / nrRows);
    for (int col = 0; col < nrColumns; ++col)
    {
        pbrShader.setFloat("roughness",
            glm::clamp(static_cast<float>(col) / nrColumns, 0.05f, 1.0f));

        glm::mat4 model = glm::mat4(1.0f);
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

// Render skybox last
glDepthFunc(GL_LEQUAL);
backgroundShader.use();
backgroundShader.setMat4("projection", projection);
backgroundShader.setMat4("view", view);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
renderCube();
glDepthFunc(GL_LESS);
```

## Results

Even with only diffuse IBL (no specular reflections yet), the improvement over the constant ambient is dramatic. The spheres are now lit by the environment — a sphere facing the bright part of the sky receives more ambient light than one facing the dark ground. Dielectric spheres pick up subtle color from the environment, and the overall lighting feels natural and grounded in the scene.

However, you'll notice the metallic spheres look dull. That's because metals have no diffuse component (`kD ≈ 0`), and we haven't implemented specular IBL yet. The metallic spheres are essentially receiving zero ambient light. We fix this in the next chapter.

## Key Takeaways

- IBL uses an HDR environment map as a light source, producing vastly more realistic ambient lighting than a constant value.
- The diffuse part of the reflectance equation depends only on the surface normal, not the view direction. This lets us pre-compute it into an **irradiance map** — a small, blurred cubemap.
- The pipeline involves three conversion steps: HDR equirectangular → cubemap → irradiance map. All use the same render-to-cubemap-face technique.
- The irradiance convolution integrates incoming light over the hemisphere, weighted by cosine (Lambert's law) and the solid angle differential (sin θ).
- `fresnelSchlickRoughness` is needed for IBL's ambient Fresnel to prevent excessive edge reflection on rough surfaces.
- The HDR environment is displayed as a skybox using the `clipPos.xyww` depth trick.
- Metallic objects still appear dark because they need specular IBL, which we implement in the next chapter.

## Exercises

1. **Try different HDR environments:** Download various `.hdr` files (indoor, outdoor, studio) and observe how the diffuse lighting changes. The irradiance map captures the dominant color and direction of the environment.
2. **Visualize the irradiance map:** Render the irradiance cubemap as the skybox instead of the environment cubemap. You should see a very blurry version of the environment.
3. **Increase/decrease convolution resolution:** Try 16×16 and 64×64 per face. Is there a visible quality difference? The diffuse integral is so smooth that 32×32 is typically sufficient.
4. **Remove direct lights:** Set the light colors to `vec3(0.0)` and rely only on IBL diffuse. This shows how much the environment contributes.
5. **Rotate the environment:** Apply a rotation to the cubemap sampling direction in the PBR shader. Watch how the lighting on the spheres shifts as the "sun" in the HDR map moves.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: PBR Lighting](02_lighting.md) | [06. PBR](.) | [Next: IBL — Specular →](04_ibl_specular.md) |
