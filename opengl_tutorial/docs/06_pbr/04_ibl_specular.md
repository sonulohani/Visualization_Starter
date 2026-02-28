# IBL: Specular

[Previous: IBL — Diffuse Irradiance](03_ibl_diffuse.md)

---

In the previous chapter we solved the diffuse half of the IBL equation by pre-computing an irradiance map. Metals and smooth surfaces still looked lifeless, though, because they depend almost entirely on **specular** reflections — and we had nothing to provide those. This chapter completes the picture.

The specular integral is much harder than the diffuse one. It depends on the **view direction** and the surface **roughness**, making a simple per-normal pre-computation impossible. The solution, pioneered by Epic Games, is the **Split Sum Approximation**: we factor the specular integral into two independent, pre-computable parts. Let's dive in.

## The Specular Challenge

Recall the specular part of the reflectance equation:

$$L_{specular}(p, \omega_o) = \int_\Omega \frac{DFG}{4(\omega_o \cdot n)(\omega_i \cdot n)} \cdot L_i(p, \omega_i) \cdot (n \cdot \omega_i) \, d\omega_i$$

Unlike the diffuse integral (which depends only on \(n\)), this integral depends on:

- **View direction** \(\omega_o\) — the halfway vector and Fresnel change with viewing angle
- **Roughness** — controls the width of the GGX distribution, which determines which incoming directions contribute

Pre-computing this for every combination of (normal, view direction, roughness) would require a 5D lookup table — completely impractical. We need a clever factorization.

## The Split Sum Approximation

Epic Games' approach splits the integral into two independent parts:

$$L_{specular} \approx \underbrace{\left(\int_\Omega L_i(p, \omega_i) \, d\omega_i\right)}_{\text{Pre-filtered environment map}} \cdot \underbrace{\left(\int_\Omega f_r(p, \omega_i, \omega_o) \cdot (n \cdot \omega_i) \, d\omega_i\right)}_{\text{BRDF integration (LUT)}}$$

```
Split Sum Approximation
═══════════════════════

  Specular IBL ≈  Pre-filtered Env Map  ×  BRDF LUT
                  (depends on roughness)    (depends on NdotV, roughness)

  ┌──────────────────────┐    ┌──────────────────────┐
  │ Pre-filtered Map     │    │ BRDF Integration LUT │
  │                      │    │                      │
  │ Mip 0: sharp (r=0)  │    │  NdotV →             │
  │ Mip 1: slightly blur│    │  ┌─────────────────┐ │
  │ Mip 2: more blur    │    │  │                 │ │
  │ Mip 3: very blur    │    │  │    R   G        │ │
  │ Mip 4: max blur(r=1)│    │  │  (scale, bias)  │ │
  │                      │    │  └─────────────────┘ │
  │ Cubemap with mips    │    │  roughness ↓         │
  │ 128×128 per face     │    │  2D texture (512×512)│
  └──────────────────────┘    └──────────────────────┘
```

**Part 1 — Pre-filtered environment map:** For each roughness level, we pre-convolve the environment map with the GGX distribution, storing the result in successive mip levels of a cubemap. Mip 0 is a sharp mirror reflection (roughness = 0), and higher mips are progressively blurrier (higher roughness).

**Part 2 — BRDF integration LUT:** A 2D texture where the inputs are `NdotV` (x-axis) and `roughness` (y-axis), and the output is a `(scale, bias)` pair for the Fresnel term. This is pre-computed once and works for all environments and materials.

At render time, we simply look up both parts and combine them:

```
prefilteredColor = textureLod(prefilterMap, R, roughness * MAX_LOD)
brdf             = texture(brdfLUT, vec2(NdotV, roughness))
specular         = prefilteredColor * (F * brdf.x + brdf.y)
```

## Importance Sampling — Why and How

Both the pre-filtered map and the BRDF LUT require integrating over the hemisphere. Uniform sampling (like we used for the irradiance map) is wasteful for specular — the GGX distribution concentrates energy in a narrow lobe, so most uniform samples would contribute nothing.

**Importance sampling** generates samples that follow the GGX distribution, concentrating our computational budget where it matters most. This dramatically reduces the number of samples needed for convergence.

### The Hammersley Sequence

We need a low-discrepancy sequence to generate well-distributed sample points. The **Hammersley sequence** is a quasi-random sequence that provides good coverage with relatively few samples:

```glsl
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}
```

`RadicalInverse_VdC` computes the Van der Corput sequence by reversing the bits of the integer `i`. Combined with the linear sequence `i/N`, this produces 2D points that are well-distributed across the unit square.

```
Hammersley Sequence (16 samples)
════════════════════════════════

  1.0 ┌──────────────────┐
      │  ·     ·    ·   ·│   Points are spread
      │    ·      ·    · │   evenly across the
      │ ·    ·   ·   ·   │   unit square — no
      │    ·  ·     ·    │   clumping, no gaps
  0.0 └──────────────────┘
      0.0               1.0
```

### GGX Importance Sampling

Given a Hammersley sample point `(u, v)`, we generate a halfway vector that follows the GGX distribution:

```glsl
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    // Spherical to Cartesian (halfway vector in tangent space)
    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // Tangent-space to world-space
    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}
```

The key line is the `cosTheta` computation. It inverts the GGX CDF (cumulative distribution function) to transform the uniform random variable `Xi.y` into a cosTheta that follows the GGX distribution. When roughness is low, samples cluster near the normal (tight specular lobe). When roughness is high, samples spread out (wide lobe).

## Part 1: Pre-filtered Environment Map

### Creating the Pre-filter Cubemap with Mip Levels

We create a cubemap with mipmaps. Each mip level corresponds to a different roughness value:

```cpp
unsigned int prefilterMap;
glGenTextures(1, &prefilterMap);
glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
for (unsigned int i = 0; i < 6; ++i)
{
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
}
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
```

We use `GL_LINEAR_MIPMAP_LINEAR` for trilinear filtering between mip levels, giving smooth transitions between roughness values. The base resolution is 128×128 per face — higher than the irradiance map because specular reflections retain more detail.

### Pre-filter Fragment Shader

```glsl
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;

uniform samplerCube environmentMap;
uniform float roughness;

const float PI = 3.14159265359;

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

void main()
{
    vec3 N = normalize(WorldPos);
    // Assume view direction equals the reflection direction equals the normal.
    // This is a simplification that avoids the view-direction dependency.
    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    vec3  prefilteredColor = vec3(0.0);
    float totalWeight      = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0)
        {
            // Sample from the environment's mip level to reduce
            // bright-dot artifacts from sharp HDR features
            float D     = DistributionGGX(N, H, roughness);
            float NdotH = max(dot(N, H), 0.0);
            float HdotV = max(dot(H, V), 0.0);
            float pdf   = D * NdotH / (4.0 * HdotV) + 0.0001;

            float resolution = 512.0; // resolution of source cubemap
            float saTexel    = 4.0 * PI / (6.0 * resolution * resolution);
            float saSample   = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);

            float mipLevel = roughness == 0.0
                           ? 0.0
                           : 0.5 * log2(saSample / saTexel);

            prefilteredColor += textureLod(environmentMap, L, mipLevel).rgb
                              * NdotL;
            totalWeight      += NdotL;
        }
    }
    prefilteredColor = prefilteredColor / totalWeight;

    FragColor = vec4(prefilteredColor, 1.0);
}
```

We also need the `DistributionGGX` function in this shader (same as before):

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

Key implementation details:

- **`V = R = N` assumption:** We assume the view direction equals the normal. This loses some accuracy (no view-dependent parallax in reflections) but makes pre-computation possible. The BRDF LUT compensates for this at render time.
- **Mip-level sampling of the source cubemap:** Instead of sampling the source at mip 0, we compute the appropriate mip level based on the PDF. This prevents bright specular dots from isolated bright pixels (like the sun in an HDR map).
- **NdotL weighting:** We weight each sample by `NdotL` and divide by the total weight at the end. This produces cleaner results than uniform weighting.

### Rendering the Pre-filtered Map

We render to each mip level of each face:

```cpp
prefilterShader.use();
prefilterShader.setInt("environmentMap", 0);
prefilterShader.setMat4("projection", captureProjection);

glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
unsigned int maxMipLevels = 5;
for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
{
    // Resize framebuffer according to mip-level size
    unsigned int mipWidth  = static_cast<unsigned int>(128 * std::pow(0.5, mip));
    unsigned int mipHeight = static_cast<unsigned int>(128 * std::pow(0.5, mip));
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          mipWidth, mipHeight);
    glViewport(0, 0, mipWidth, mipHeight);

    float roughness = static_cast<float>(mip) / static_cast<float>(maxMipLevels - 1);
    prefilterShader.setFloat("roughness", roughness);

    for (unsigned int i = 0; i < 6; ++i)
    {
        prefilterShader.setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                               prefilterMap, mip);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderCube();
    }
}
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

This produces 5 mip levels: 128×128 (roughness 0.0), 64×64 (roughness 0.25), 32×32 (roughness 0.5), 16×16 (roughness 0.75), and 8×8 (roughness 1.0).

```
Pre-filtered Map Mip Levels
════════════════════════════

  Mip 0 (128×128)  Mip 1 (64×64)   Mip 2 (32×32)
  roughness = 0.0  roughness = 0.25 roughness = 0.5
  ┌────────────┐   ┌──────────┐    ┌────────┐
  │ Sharp      │   │ Slightly │    │ Blurry │
  │ mirror     │   │ blurred  │    │ soft   │
  │ reflection │   │          │    │        │
  └────────────┘   └──────────┘    └────────┘

  Mip 3 (16×16)    Mip 4 (8×8)
  roughness = 0.75  roughness = 1.0
  ┌──────┐          ┌────┐
  │ Very │          │Max │
  │ blur │          │blur│
  └──────┘          └────┘
```

## Part 2: BRDF Integration LUT

The second part of the split sum is a 2D lookup texture that captures the integral of the BRDF itself (without the environment). For a given `(NdotV, roughness)`, it outputs two values:

- **scale** (red channel): how much F0 is scaled
- **bias** (green channel): how much is added to F0

These are used as: `F0 * scale + bias`.

### BRDF Integration Shader

We render a fullscreen quad with this fragment shader:

```glsl
#version 330 core
out vec2 FragColor;

in vec2 TexCoords;

const float PI = 3.14159265359;

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;

    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    // NOTE: k for IBL differs from direct lighting!
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec2 IntegrateBRDF(float NdotV, float roughness)
{
    vec3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;

    vec3 N = vec3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L  = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0);
        float NdotH = max(H.z, 0.0);
        float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0)
        {
            float G     = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);
            float Fc    = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return vec2(A, B);
}

void main()
{
    vec2 integratedBRDF = IntegrateBRDF(TexCoords.x, TexCoords.y);
    FragColor = integratedBRDF;
}
```

Important: the geometry function here uses `k = roughness² / 2` (the **IBL variant**), not `(roughness + 1)² / 8` which is for direct lighting.

The integration works with the Fresnel equation factored as:

```
F = F0 × (1 - (1-VdotH)^5) + 1 × (1-VdotH)^5
  = F0 × A                  + B
```

So the output `(A, B)` gives us the scale and bias to apply to any F0 value at render time.

### Creating and Rendering the BRDF LUT

```cpp
unsigned int brdfLUTTexture;
glGenTextures(1, &brdfLUTTexture);

glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0,
             GL_RG, GL_FLOAT, 0);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D, brdfLUTTexture, 0);

glViewport(0, 0, 512, 512);
brdfShader.use();
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
renderQuad();

glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

We use `GL_RG16F` — only two channels needed (scale and bias). The `renderQuad()` function draws a fullscreen quad:

```cpp
unsigned int quadVAO = 0;
unsigned int quadVBO;

void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
            // positions        // texcoords
            -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };

        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices),
                     quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                              5 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindVertexArray(0);
    }

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}
```

```
BRDF LUT — What It Looks Like
══════════════════════════════

  roughness
  1.0 ┌─────────────────────┐
      │  dark-red    orange  │
      │                      │
      │                      │
      │                      │
      │  bright-red  red     │
  0.0 └─────────────────────┘
      0.0    NdotV →      1.0

  Red channel  (scale): high at low roughness + high NdotV
  Green channel (bias): high at high roughness + low NdotV
```

The BRDF LUT only needs to be generated once. It is independent of the environment and works for all PBR materials.

## Combining Everything — The Final PBR Shader

Now we have all the pieces: the irradiance map (diffuse IBL), the pre-filtered map (specular IBL), and the BRDF LUT. Here is the complete fragment shader:

```glsl
#version 330 core
out vec4 FragColor;

in vec3 WorldPos;
in vec3 Normal;
in vec2 TexCoords;

// Material
uniform vec3  albedo;
uniform float metallic;
uniform float roughness;
uniform float ao;

// IBL
uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D   brdfLUT;

// Lights
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

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
             * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    vec3 N = normalize(Normal);
    vec3 V = normalize(camPos - WorldPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // ========== Direct Lighting ==========
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

    // ========== Ambient Lighting (IBL) ==========
    vec3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    // Diffuse IBL
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuse    = irradiance * albedo;

    // Specular IBL
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilterMap, R,
                                       roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    vec3 ambient = (kD * diffuse + specular) * ao;

    vec3 color = ambient + Lo;

    // HDR tone mapping + gamma correction
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
```

Let's trace the specular IBL computation:

1. **Compute reflection vector** `R = reflect(-V, N)` — the direction of mirror reflection.
2. **Sample the pre-filtered map** at `R` using the roughness-scaled mip level. Smooth surfaces (low roughness) sample from the sharp mip 0; rough surfaces sample from the blurry higher mips.
3. **Sample the BRDF LUT** with `(NdotV, roughness)` to get the scale and bias for the Fresnel term.
4. **Combine:** `prefilteredColor * (F * brdf.x + brdf.y)` — the pre-filtered color modulated by the view-dependent BRDF response.
5. **Add diffuse and specular IBL together** as the ambient term: `(kD * diffuse + specular) * ao`.

## Complete C++ Setup

Here's the full initialization and render loop with all IBL components:

```cpp
// ==================== Initialization ====================

// 1. Load HDR environment map
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

// 2. Setup capture FBO
unsigned int captureFBO, captureRBO;
glGenFramebuffers(1, &captureFBO);
glGenRenderbuffers(1, &captureRBO);
glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                          GL_RENDERBUFFER, captureRBO);

// 3. Setup capture view matrices
glm::mat4 captureProjection = glm::perspective(
    glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
glm::mat4 captureViews[] = {
    glm::lookAt(glm::vec3(0), glm::vec3( 1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 1, 0), glm::vec3(0, 0, 1)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0,-1, 0), glm::vec3(0, 0,-1)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 0, 1), glm::vec3(0,-1, 0)),
    glm::lookAt(glm::vec3(0), glm::vec3( 0, 0,-1), glm::vec3(0,-1, 0)),
};

// 4. Convert equirectangular to cubemap
unsigned int envCubemap;
glGenTextures(1, &envCubemap);
glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
for (unsigned int i = 0; i < 6; ++i)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                 512, 512, 0, GL_RGB, GL_FLOAT, nullptr);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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

// Generate mipmaps for the environment cubemap (used by pre-filter)
glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

// 5. Create irradiance map
unsigned int irradianceMap;
glGenTextures(1, &irradianceMap);
glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
for (unsigned int i = 0; i < 6; ++i)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

irradianceShader.use();
irradianceShader.setInt("environmentMap", 0);
irradianceShader.setMat4("projection", captureProjection);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

glViewport(0, 0, 32, 32);
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

// 6. Create pre-filtered environment map
unsigned int prefilterMap;
glGenTextures(1, &prefilterMap);
glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
for (unsigned int i = 0; i < 6; ++i)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

prefilterShader.use();
prefilterShader.setInt("environmentMap", 0);
prefilterShader.setMat4("projection", captureProjection);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);

glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
unsigned int maxMipLevels = 5;
for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
{
    unsigned int mipWidth  = static_cast<unsigned int>(
        128 * std::pow(0.5, mip));
    unsigned int mipHeight = mipWidth;
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          mipWidth, mipHeight);
    glViewport(0, 0, mipWidth, mipHeight);

    float roughness = static_cast<float>(mip)
                    / static_cast<float>(maxMipLevels - 1);
    prefilterShader.setFloat("roughness", roughness);

    for (unsigned int i = 0; i < 6; ++i)
    {
        prefilterShader.setMat4("view", captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                               prefilterMap, mip);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderCube();
    }
}
glBindFramebuffer(GL_FRAMEBUFFER, 0);

// 7. Generate BRDF LUT
unsigned int brdfLUTTexture;
glGenTextures(1, &brdfLUTTexture);
glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0,
             GL_RG, GL_FLOAT, 0);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                       GL_TEXTURE_2D, brdfLUTTexture, 0);
glViewport(0, 0, 512, 512);
brdfShader.use();
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
renderQuad();
glBindFramebuffer(GL_FRAMEBUFFER, 0);

// Restore viewport
glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
```

### The Render Loop

```cpp
while (!glfwWindowShouldClose(window))
{
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    processInput(window);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    pbrShader.use();
    glm::mat4 projection = glm::perspective(
        glm::radians(camera.Zoom),
        (float)SCR_WIDTH / (float)SCR_HEIGHT,
        0.1f, 100.0f);
    glm::mat4 view = camera.GetViewMatrix();
    pbrShader.setMat4("projection", projection);
    pbrShader.setMat4("view", view);
    pbrShader.setVec3("camPos", camera.Position);

    // Bind IBL textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, brdfLUTTexture);

    pbrShader.setInt("irradianceMap", 0);
    pbrShader.setInt("prefilterMap",  1);
    pbrShader.setInt("brdfLUT",       2);

    // Set light uniforms
    for (unsigned int i = 0; i < 4; ++i)
    {
        pbrShader.setVec3("lightPositions[" + std::to_string(i) + "]",
                          lightPositions[i]);
        pbrShader.setVec3("lightColors[" + std::to_string(i) + "]",
                          lightColors[i]);
    }

    // Render sphere grid
    for (int row = 0; row < nrRows; ++row)
    {
        pbrShader.setFloat("metallic", static_cast<float>(row) / nrRows);
        for (int col = 0; col < nrColumns; ++col)
        {
            pbrShader.setFloat("roughness",
                glm::clamp(static_cast<float>(col) / nrColumns,
                           0.05f, 1.0f));

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

    // Render skybox
    glDepthFunc(GL_LEQUAL);
    backgroundShader.use();
    backgroundShader.setMat4("projection", projection);
    backgroundShader.setMat4("view", view);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, envCubemap);
    renderCube();
    glDepthFunc(GL_LESS);

    glfwSwapBuffers(window);
    glfwPollEvents();
}
```

## The Result

With full IBL (diffuse + specular), the scene transforms dramatically:

- **Smooth metallic spheres** now reflect the environment like mirrors — you can see the sky, buildings, and ground in their surface.
- **Rough metallic spheres** show blurred reflections that fade smoothly with increasing roughness.
- **Smooth dielectrics** have subtle environmental reflections at their edges (Fresnel effect) while retaining their diffuse color.
- **Rough dielectrics** appear matte with soft ambient lighting from the environment.
- The spheres look like they **belong** in the environment. The lighting is coherent, natural, and physically plausible.

```
Full IBL Result — Sphere Appearance
═══════════════════════════════════

  metallic
  1.0 ─── ◉ ◉ ◎ ○ ○ ○ ○   ← mirror → blurred reflections
          ◉ ◉ ◎ ○ ○ ○ ○
          ◉ ◉ ◎ ○ ○ ○ ○
          ◉ ◉ ◎ ○ ○ ○ ○
          ● ● ● ● ● ● ●
          ● ● ● ● ● ● ●
  0.0 ─── ● ● ● ● ● ● ●   ← subtle Fresnel → matte
          │             │
        0.05          1.0
         smooth ──── rough

  ◉ = mirror-like (env visible)
  ◎ = blurred reflection
  ○ = very diffuse reflection
  ● = diffuse color with subtle edge reflections
```

## The Complete Pre-computation Pipeline Summary

```
Full IBL Setup — One-Time Pre-computation
═════════════════════════════════════════

  HDR image (.hdr)
       │
       ▼
  ┌────────────────────────┐
  │ Equirectangular → Cube │  512×512/face, GL_RGB16F
  └──────────┬─────────────┘
             │
     ┌───────┼───────┐
     ▼       ▼       ▼
  ┌──────┐ ┌──────┐ ┌──────┐
  │Irradi│ │Pre-  │ │BRDF  │
  │ance  │ │filter│ │LUT   │
  │Map   │ │Map   │ │      │
  │32×32 │ │128×  │ │512×  │
  │/face │ │128/  │ │512   │
  │      │ │face  │ │RG16F │
  │Diff. │ │5 mips│ │      │
  │convo │ │GGX   │ │ Once │
  │lution│ │import│ │ for  │
  │      │ │samp. │ │ all  │
  └──┬───┘ └──┬───┘ └──┬───┘
     │        │        │
     └────────┼────────┘
              │
              ▼
     ┌────────────────┐
     │  Render loop:  │
     │  sample all 3  │
     │  in PBR shader │
     └────────────────┘
```

## Key Takeaways

- The **Split Sum Approximation** factors the specular IBL integral into two pre-computable parts: a pre-filtered environment map and a BRDF integration LUT.
- The **pre-filtered environment map** is a cubemap where each mip level stores the environment convolved with the GGX distribution at a specific roughness. `textureLod` with `roughness * MAX_LOD` selects the right blur level.
- The **BRDF LUT** is a 2D texture mapping `(NdotV, roughness)` to Fresnel scale and bias factors `(A, B)`. It's environment-independent and only needs to be computed once.
- **Importance sampling** (Hammersley + GGX) focuses samples where the BRDF has the most energy, dramatically improving convergence for both pre-filtering and BRDF integration.
- The **geometry function for IBL** uses `k = roughness²/2` instead of `(roughness+1)²/8` used for direct lighting.
- The final specular IBL is computed as: `prefilteredColor * (F * brdf.x + brdf.y)`.
- Combined with the irradiance map for diffuse, this gives us complete image-based lighting where objects naturally reflect and are lit by their environment.
- All the heavy computation happens once at initialization. At render time, the shader just performs a few texture lookups — making full IBL practical at 60+ FPS.

## Exercises

1. **Swap environments:** Try the same sphere grid with drastically different HDR maps (sunny outdoor, dark indoor, studio). Notice how the materials remain physically consistent while the lighting changes.
2. **Visualize the pre-filtered map:** Render the pre-filtered cubemap as the skybox at different mip levels (use a uniform to control the LOD). You should see the environment progressively blur.
3. **Visualize the BRDF LUT:** Render the BRDF LUT as a fullscreen quad. The red channel should be high at low roughness and the green channel high at high roughness for low NdotV values.
4. **Increase sample count:** Change `SAMPLE_COUNT` in the pre-filter shader from 1024 to 4096. Is the difference visible? What about reducing to 256?
5. **Use textured PBR with IBL:** Combine the textured PBR shader from the previous chapter with IBL. Load a rusted iron or gold material and see how the normal map interacts with environment reflections.
6. **Multiple objects:** Load a 3D model (from the Model Loading chapters) and render it with full IBL PBR. Compare the realism to the same model rendered with Phong shading.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: IBL — Diffuse Irradiance](03_ibl_diffuse.md) | [06. PBR](.) | |
