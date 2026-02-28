# Chapter 11: Lighting and Shading

> *"Without light, there is no sight. Without shading, there is no shape."*

Everything you see in a 3D scene is ultimately about **light interacting with surfaces**.
This chapter covers the math behind how games simulate that interaction — from the humble
dot product to physically based rendering.

---

## 11.1 Why Lighting Matters

Consider a sphere rendered with a single flat color:

```
        ████████
      ████████████
    ████████████████
    ████████████████        ← No lighting: looks like a circle
    ████████████████
      ████████████
        ████████
```

Now add shading:

```
        ░░▒▒▓▓██
      ░░▒▒▒▒▓▓████
    ░░▒▒▒▒▒▒▓▓██████
    ░░▒▒▒▒▓▓████████        ← With lighting: reads as a sphere
    ░░▒▒▓▓██████████
      ░░▓▓████████
        ████████
```

Lighting provides:

- **Depth perception** — we understand 3D shape from light and shadow.
- **Mood and atmosphere** — warm sunset vs. cold fluorescent.
- **Realism** — materials look like metal, wood, skin, glass.
- **Gameplay cues** — a glowing item, a dark corridor.

---

## 11.2 Normal Vectors

### What is a surface normal?

A **normal vector** is a unit-length vector that points **perpendicular to a surface**.
It tells the renderer which way a surface is "facing."

```
            N (normal)
            ↑
            |
  ──────────┼──────────  ← surface
```

### Face normals vs. vertex normals

**Face normal**: one normal per triangle. Every pixel in that triangle uses the same
normal. Results in **flat shading** — you can see each facet.

**Vertex normal**: one normal per vertex, usually the average of all neighboring face
normals. Normals are interpolated across the triangle, giving **smooth shading**.

```
  Face normals (flat):          Vertex normals (smooth):

    ↑     ↗                       ↑   ↗
   ┌───┐┌───┐                   ┌───┬───┐
   │   ││   │                   │  ╱╲  │
   └───┘└───┘                   └───┴───┘
  Each face has its              Normals blend
  own direction                  across vertices
```

### Computing a face normal

Given triangle vertices \( A \), \( B \), \( C \):

\[
\mathbf{N} = \text{normalize}\bigl((\mathbf{B} - \mathbf{A}) \times (\mathbf{C} - \mathbf{A})\bigr)
\]

```
function faceNormal(A, B, C):
    edge1 = B - A
    edge2 = C - A
    N = cross(edge1, edge2)
    return normalize(N)
```

The cross product gives a vector perpendicular to both edges, and normalizing makes it
unit length. The **winding order** (CW vs. CCW) determines whether the normal points
"outward" or "inward."

### Computing vertex normals

```
function computeVertexNormals(mesh):
    for each vertex v:
        v.normal = (0, 0, 0)

    for each triangle (A, B, C):
        N = faceNormal(A, B, C)
        A.normal += N
        B.normal += N
        C.normal += N

    for each vertex v:
        v.normal = normalize(v.normal)
```

---

## 11.3 Light Types

### Directional light (the sun)

All rays are **parallel**. Defined only by a direction vector. No position, no
attenuation — the sun is so far away that these simplifications hold.

```
    ↓  ↓  ↓  ↓  ↓  ↓  ↓      All rays point the same direction
   ═══════════════════════
          surface
```

```
struct DirectionalLight:
    direction: vec3   // e.g., normalize((-0.5, -1.0, -0.3))
    color:     vec3   // e.g., (1.0, 0.95, 0.8) for warm sunlight
    intensity: float  // e.g., 1.0
```

### Point light (a light bulb)

Radiates in **all directions** from a point. Intensity falls off with distance.

```
              * (light position)
            / | \
          /   |   \
        /     |     \
      ↙       ↓       ↘
   ═══════════════════════
```

```
struct PointLight:
    position:  vec3
    color:     vec3
    intensity: float
    range:     float   // cutoff distance for performance
```

### Spot light (a flashlight)

A point light restricted to a **cone**. Has an inner angle (full brightness) and outer
angle (fades to zero).

```
              * (position)
             /|\
            / | \
           /  |  \     ← outer cone
          / (.|.) \
         /  inner  \
        /   cone    \
       ═══════════════
```

```
struct SpotLight:
    position:    vec3
    direction:   vec3
    color:       vec3
    intensity:   float
    innerAngle:  float  // radians — full brightness inside this
    outerAngle:  float  // radians — fades to zero at this boundary
```

The spotlight factor is computed as:

\[
\text{spot} = \text{clamp}\!\left(
  \frac{\cos\theta - \cos\theta_{outer}}{\cos\theta_{inner} - \cos\theta_{outer}},\;0,\;1
\right)
\]

where \( \theta \) is the angle between the spot direction and the vector to the surface point.

### Ambient light

A constant amount of light everywhere. Simulates the indirect light bouncing around a
scene that would be too expensive to compute accurately.

\[
I_{ambient} = k_a \cdot I_a
\]

It's a hack, but an effective one. Without ambient light, surfaces facing away from all
lights would be pitch black.

### Area lights (brief mention)

Real-world lights have **area** (a window, a fluorescent tube). Area lights produce
soft shadows and are important for realism. They're expensive to compute — usually
approximated with multiple point lights or specialized techniques.

---

## 11.4 The Phong Reflection Model

The **Phong model** (Bui Tuong Phong, 1975) decomposes surface reflection into three
components:

```
  Total = Ambient + Diffuse + Specular

  ┌──────────┐   ┌──────────┐   ┌──────────┐
  │          │   │  ░░░░░░  │   │     ·    │
  │  uniform │ + │ ░░░░░░░░ │ + │    · ·   │  =  final shading
  │  color   │   │░░░░░░░░░░│   │     ·    │
  │          │   │  ░░░░░░  │   │          │
  └──────────┘   └──────────┘   └──────────┘
    Ambient        Diffuse        Specular
```

### Ambient component

\[
I_{ambient} = k_a \cdot I_a
\]

- \( k_a \): material's ambient reflectivity (0–1)
- \( I_a \): ambient light intensity

This is the "base glow" that prevents absolute darkness.

### Diffuse component — Lambert's cosine law

A perfectly matte surface scatters light equally in all directions. The brightness
depends only on the angle between the surface normal and the light direction:

\[
I_{diffuse} = k_d \cdot \max(\mathbf{N} \cdot \mathbf{L},\; 0) \cdot I_d
\]

- \( \mathbf{N} \): unit surface normal
- \( \mathbf{L} \): unit vector **from** the surface point **toward** the light
- \( k_d \): material's diffuse reflectivity
- \( I_d \): diffuse light color/intensity

The `max(..., 0)` clamps negative values — light hitting the back of a surface
contributes nothing.

```
        L (to light)
         ↗
        /  θ
       / ←── angle between N and L
      /
  ───N──────── surface

  Brightness = cos(θ) = dot(N, L)
  θ = 0°  → cos = 1.0  → fully lit (face-on)
  θ = 90° → cos = 0.0  → edge-on, no light
```

### Specular component

Shiny surfaces have a bright **highlight** where light reflects toward the viewer:

\[
I_{specular} = k_s \cdot \bigl[\max(\mathbf{R} \cdot \mathbf{V},\; 0)\bigr]^{n} \cdot I_s
\]

- \( \mathbf{R} = 2(\mathbf{N} \cdot \mathbf{L})\mathbf{N} - \mathbf{L} \): the reflection of the light vector around the normal
- \( \mathbf{V} \): unit vector from the surface point toward the camera/viewer
- \( n \): **shininess** exponent
- \( k_s \): material's specular reflectivity

```
          L        N        R
           ↘       ↑       ↗
            ↘      |      ↗
             ↘  θ  |  θ  ↗        V (to viewer)
              ↘    |    ↗          ↗
   ════════════════════════════
                surface

  R = reflect(-L, N)
  Specular = pow(max(dot(R, V), 0), shininess)
```

The shininess exponent controls highlight size:

| Shininess \( n \) | Appearance              |
|-------------------|-------------------------|
| 1–10              | Very broad, matte-ish   |
| 32                | Plastic-like            |
| 128               | Glossy                  |
| 512+              | Mirror-like pinpoint    |

### Total Phong illumination

\[
I = k_a I_a + k_d \max(\mathbf{N} \cdot \mathbf{L}, 0) I_d + k_s \bigl[\max(\mathbf{R} \cdot \mathbf{V}, 0)\bigr]^n I_s
\]

### Step-by-step example with numbers

Setup:
- Surface point \( P = (1, 0, 0) \), normal \( \mathbf{N} = (0, 1, 0) \)
- Light position \( L_{pos} = (1, 5, 0) \), white light \( I = (1, 1, 1) \)
- Camera position \( C = (1, 3, 4) \)
- Material: \( k_a = 0.1 \), \( k_d = 0.7 \), \( k_s = 0.5 \), \( n = 32 \)

**Step 1: Compute L (to-light vector)**

\[
\mathbf{L} = \text{normalize}(L_{pos} - P) = \text{normalize}((0, 5, 0)) = (0, 1, 0)
\]

**Step 2: Diffuse**

\[
\mathbf{N} \cdot \mathbf{L} = (0)(0) + (1)(1) + (0)(0) = 1.0
\]
\[
I_{diffuse} = 0.7 \times 1.0 \times (1,1,1) = (0.7, 0.7, 0.7)
\]

**Step 3: Compute R (reflection)**

\[
\mathbf{R} = 2(1.0)(0,1,0) - (0,1,0) = (0,1,0)
\]

**Step 4: Compute V (to-viewer vector)**

\[
\mathbf{V} = \text{normalize}(C - P) = \text{normalize}((0, 3, 4)) = (0, 0.6, 0.8)
\]

**Step 5: Specular**

\[
\mathbf{R} \cdot \mathbf{V} = (0)(0) + (1)(0.6) + (0)(0.8) = 0.6
\]
\[
I_{specular} = 0.5 \times 0.6^{32} \times (1,1,1) \approx 0.5 \times 0.00013 \approx (0.000065, 0.000065, 0.000065)
\]

**Step 6: Ambient**

\[
I_{ambient} = 0.1 \times (1,1,1) = (0.1, 0.1, 0.1)
\]

**Step 7: Total**

\[
I \approx (0.1, 0.1, 0.1) + (0.7, 0.7, 0.7) + (0.000065 \ldots) \approx (0.8, 0.8, 0.8)
\]

The surface is almost fully lit because the normal points straight at the light. The
specular highlight is nearly invisible because the viewer isn't aligned with the
reflection direction.

### Pseudocode

```
function phongShading(P, N, lightPos, lightColor, camPos, material):
    // Ambient
    ambient = material.ka * lightColor

    // Diffuse
    L = normalize(lightPos - P)
    diff = max(dot(N, L), 0.0)
    diffuse = material.kd * diff * lightColor

    // Specular
    R = reflect(-L, N)       // reflect: R = 2*dot(N,L)*N - L
    V = normalize(camPos - P)
    spec = pow(max(dot(R, V), 0.0), material.shininess)
    specular = material.ks * spec * lightColor

    return ambient + diffuse + specular
```

---

## 11.5 Blinn-Phong Model

Jim Blinn's modification (1977) replaces the reflection vector \( \mathbf{R} \) with the
**half-vector** \( \mathbf{H} \):

\[
\mathbf{H} = \text{normalize}(\mathbf{L} + \mathbf{V})
\]

\[
I_{specular} = k_s \cdot \bigl[\max(\mathbf{N} \cdot \mathbf{H},\; 0)\bigr]^{n'} \cdot I_s
\]

### Why Blinn-Phong is preferred

1. **Faster**: computing \( \mathbf{H} \) avoids the full reflection calculation.
2. **More stable**: Phong's \( \mathbf{R} \cdot \mathbf{V} \) can produce artifacts at
   grazing angles. Blinn-Phong degrades more gracefully.
3. **Standard in OpenGL**: The fixed-function pipeline used Blinn-Phong.

Note: the shininess exponent \( n' \) for Blinn-Phong is typically **2–4× higher** than
the equivalent Phong exponent to produce a similar highlight size.

```
function blinnPhongSpecular(N, L, V, shininess):
    H = normalize(L + V)
    return pow(max(dot(N, H), 0.0), shininess)
```

---

## 11.6 Gouraud vs. Phong Shading

These are **shading interpolation methods**, not reflection models (confusingly, "Phong"
names both).

### Gouraud shading

1. Compute lighting at each **vertex**.
2. **Interpolate colors** across the triangle.

Fast, but specular highlights can be missed if they fall between vertices.

```
Vertex A: color = bright      ┌─── Bright ────┐
Vertex B: color = medium      │   interpolated │
Vertex C: color = dark        │     colors     │
                              └────── Dark ────┘
```

### Phong shading (per-pixel)

1. Interpolate **normals** across the triangle.
2. Compute lighting at each **pixel** (fragment).

More expensive, but catches highlights that Gouraud misses. This is the standard
approach in modern GPUs.

```
Vertex A: normal = N_a        ┌── interpolated ──┐
Vertex B: normal = N_b        │    normals per    │
Vertex C: normal = N_c        │  pixel → lighting │
                              │   computed here   │
                              └───────────────────┘
```

### Visual comparison

```
  Gouraud (low poly sphere):     Phong (low poly sphere):

    ┌───────┐                      ┌───────┐
    │ █▓▒░  │                      │ ●·    │  ← specular highlight
    │ ██▓▒░ │                      │ █▓▒░  │     visible per-pixel
    │ █▓▒░  │                      │ █▓▒░  │
    └───────┘                      └───────┘
  Highlights may be               Highlights appear
  missed or smeared               correctly everywhere
```

---

## 11.7 Attenuation

Point lights and spot lights lose intensity with distance. The standard attenuation
formula:

\[
\text{atten} = \frac{1}{k_c + k_l \cdot d + k_q \cdot d^2}
\]

where:
- \( d \) = distance from light to surface point
- \( k_c \) = constant term (usually 1.0, prevents division by zero)
- \( k_l \) = linear term
- \( k_q \) = quadratic term

| Terms               | Falloff behavior                          |
|---------------------|-------------------------------------------|
| Only constant       | No falloff (directional-like)             |
| Linear dominant     | Gradual falloff, large radius             |
| Quadratic dominant  | Rapid falloff, tight radius (most realistic) |

Physically, light follows the **inverse-square law** (\( 1/d^2 \)), which corresponds
to setting \( k_c = 0, k_l = 0, k_q = 1 \). Games often add the constant and linear
terms for artistic control.

```
function attenuation(d, kc, kl, kq):
    return 1.0 / (kc + kl * d + kq * d * d)
```

### Example values for different ranges

| Range (units) | \( k_c \) | \( k_l \) | \( k_q \)  |
|---------------|-----------|-----------|------------|
| 7             | 1.0       | 0.7       | 1.8        |
| 20            | 1.0       | 0.22      | 0.20       |
| 50            | 1.0       | 0.09      | 0.032      |
| 100           | 1.0       | 0.045     | 0.0075     |

---

## 11.8 Normal Mapping

### The problem

Adding geometric detail (bumps, grooves, scratches) by increasing polygon count is
expensive. Normal mapping **fakes** the detail by perturbing the surface normal at each
pixel.

```
  Without normal map:         With normal map:

   Flat surface → flat N       Flat surface, but N varies per pixel
   ─────────────────           ─N↗──N↑──N↖──N↑──N↗──
                               Looks bumpy despite flat geometry!
```

### How a normal map works

A normal map is a texture where each pixel's **RGB** values encode a normal direction:

\[
R \to X, \quad G \to Y, \quad B \to Z
\]

Values are stored in [0, 1] and remapped to [-1, 1]:

\[
\mathbf{N}_{map} = \text{texture}(u, v) \times 2.0 - 1.0
\]

The blueish tint of a normal map comes from most normals pointing "up" in tangent space:
\( (0, 0, 1) \) maps to RGB \( (0.5, 0.5, 1.0) \) — that distinctive purple-blue.

### Tangent space and the TBN matrix

Normal maps are stored in **tangent space** — a coordinate system local to each triangle.
To use them, we need the **TBN matrix** (Tangent, Bitangent, Normal) that transforms
from tangent space to world space:

\[
\text{TBN} = \begin{bmatrix} T_x & B_x & N_x \\ T_y & B_y & N_y \\ T_z & B_z & N_z \end{bmatrix}
\]

The world-space perturbed normal is:

\[
\mathbf{N}_{world} = \text{normalize}(\text{TBN} \cdot \mathbf{N}_{map})
\]

```
function applyNormalMap(normalMapSample, T, B, N):
    // Remap from [0,1] to [-1,1]
    tangentNormal = normalMapSample * 2.0 - vec3(1.0)

    // Transform to world space
    TBN = mat3(T, B, N)
    worldNormal = normalize(TBN * tangentNormal)
    return worldNormal
```

---

## 11.9 Physically Based Rendering (PBR) — Overview

PBR attempts to model light interaction as real physics does, producing materials that
look correct under **any** lighting condition.

### Microfacet theory

Surfaces are modeled as a collection of tiny, perfectly reflective mirrors (microfacets)
oriented in slightly different directions:

```
  Smooth surface (low roughness):    Rough surface (high roughness):

   ___________________________        /\/\  /\  /\/\/\  /\  /\/\
                                     Many orientations → blurry reflection
   Most microfacets aligned →
   sharp, mirror-like reflection
```

### Key material properties

| Property      | Range  | Meaning                                           |
|---------------|--------|---------------------------------------------------|
| **Albedo**    | RGB    | Base color (no lighting baked in)                  |
| **Metallic**  | 0–1    | 0 = dielectric (plastic, wood), 1 = metal (gold, chrome) |
| **Roughness** | 0–1    | 0 = mirror smooth, 1 = completely rough/matte      |

### Energy conservation

A surface **cannot reflect more light than it receives**. As specular reflection
increases, diffuse reflection must decrease. PBR shaders enforce this.

### Cook-Torrance BRDF (conceptual overview)

The specular part of a PBR shader uses the Cook-Torrance model:

\[
f_{spec} = \frac{D \cdot F \cdot G}{4 \, (\mathbf{N} \cdot \mathbf{L}) \, (\mathbf{N} \cdot \mathbf{V})}
\]

where:
- **D** — Normal Distribution Function (GGX/Trowbridge-Reitz): probability of microfacets being aligned with the half-vector
- **F** — Fresnel term (Schlick approximation): how much light reflects vs. refracts
- **G** — Geometry/shadowing term (Smith): how much microfacets block each other

The **diffuse** part often uses a Lambertian model:

\[
f_{diff} = \frac{\text{albedo}}{\pi}
\]

The total rendering equation (for a single light):

\[
L_{out} = \bigl(k_d \cdot f_{diff} + f_{spec}\bigr) \cdot L_{in} \cdot (\mathbf{N} \cdot \mathbf{L})
\]

where \( k_d = 1 - F \) (energy conservation: what isn't reflected is diffused).

### Fresnel effect

Surfaces reflect **more** light at **grazing angles** (looking along the surface). Even
a wooden table becomes mirror-like when viewed at a shallow angle. The Schlick
approximation:

\[
F = F_0 + (1 - F_0)(1 - \cos\theta)^5
\]

where \( F_0 \) is the reflectance at normal incidence:
- Dielectrics: \( F_0 \approx 0.04 \)
- Metals: \( F_0 = \) the albedo color (gold, copper, etc.)

```
function fresnelSchlick(cosTheta, F0):
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0)
```

---

## 11.10 Shadows (Brief Math Overview)

### Shadow mapping

The most common real-time shadow technique:

1. **Render the scene from the light's perspective** into a **depth buffer** (shadow map).
2. For each pixel in the main camera view, transform its position into the light's space.
3. Compare its depth to the shadow map: if it's **farther** than the stored depth, it's
   in shadow.

```
  Light's view:                Main camera view:

   ┌─────────────┐             ┌─────────────┐
   │  depth map  │             │  Pixel P:    │
   │  (z-values) │             │  depth in    │
   │             │             │  light space │
   └─────────────┘             │  = 8.3      │
                               │  shadow map  │
                               │  = 5.1      │
                               │  8.3 > 5.1  │
                               │  → IN SHADOW │
                               └─────────────┘
```

### Shadow bias

Without a bias, surfaces can shadow **themselves** (shadow acne) due to depth precision
limits. Adding a small bias offsets the depth comparison:

```
bias = max(0.05 * (1.0 - dot(N, L)), 0.005)
shadow = (currentDepth - bias) > shadowMapDepth ? 0.0 : 1.0
```

---

## 11.11 Ambient Occlusion

**Ambient Occlusion (AO)** darkens areas where ambient light has difficulty reaching —
crevices, corners, and where objects sit on surfaces.

```
  Without AO:                  With AO:

   ┌────────────┐              ┌────────────┐
   │            │              │▓▓          │
   │   ┌──┐    │              │▓▓ ┌──┐    │
   │   │  │    │              │▓▓ │  │▓▓  │
   │   └──┘    │              │▓▓ └──┘▓▓  │
   │            │              │▓▓▓▓▓▓▓▓▓▓│  ← darker at base
   └────────────┘              └────────────┘
```

**Screen-Space Ambient Occlusion (SSAO)**: for each pixel, sample nearby depth values.
If many samples are closer to the camera (i.e., the point is occluded by nearby
geometry), darken that pixel. Cheap approximation that adds significant visual depth.

---

## 11.12 HDR and Tone Mapping

### The problem

Real-world luminance varies enormously — from 0.001 cd/m² (moonlight) to 100,000 cd/m²
(direct sunlight). A monitor can only display 0–255 per channel (LDR). Without special
handling, bright areas clip to white and detail is lost.

### HDR rendering

Render to floating-point buffers that can store values > 1.0. Perform all lighting
calculations in HDR, then **tone map** to the displayable [0, 1] range as a final step.

### Reinhard tone mapping (simple)

\[
L_{out} = \frac{L_{in}}{1 + L_{in}}
\]

Maps all positive values to [0, 1), compressing brights while preserving darks.

### ACES (Academy Color Encoding System)

A more cinematic tone curve used in many modern games:

```
function acesToneMap(x):
    a = 2.51
    b = 0.03
    c = 2.43
    d = 0.59
    e = 0.14
    return clamp((x*(a*x+b)) / (x*(c*x+d)+e), 0.0, 1.0)
```

### Gamma correction

Monitors don't display brightness linearly. The standard sRGB gamma is approximately
\( \gamma = 2.2 \). Lighting calculations should happen in **linear space**; the final
output is converted to sRGB:

\[
\text{sRGB} = \text{linear}^{1/2.2}
\]

---

## 11.13 Summary and Shading Model Comparison

### Comparison table

| Feature           | Flat         | Gouraud      | Phong (per-pixel) | Blinn-Phong  | PBR (Cook-Torrance)    |
|-------------------|--------------|--------------|--------------------|--------------|-----------------------|
| Normal source     | Per face     | Per vertex   | Interpolated       | Interpolated | Interpolated + maps   |
| Lighting computed | Per face     | Per vertex   | Per pixel          | Per pixel    | Per pixel             |
| Specular quality  | None/poor    | Often missed | Good               | Good         | Excellent             |
| Physical accuracy | None         | Low          | Low                | Low          | High                  |
| Cost              | Cheapest     | Cheap        | Moderate           | Moderate     | Expensive             |
| Use case          | Retro/debug  | Legacy HW    | General 3D         | General 3D   | Modern AAA/indie      |

### Key formulas at a glance

| Concept                 | Formula                                                         |
|-------------------------|-----------------------------------------------------------------|
| Face normal             | \( \mathbf{N} = \text{normalize}((\mathbf{B}-\mathbf{A}) \times (\mathbf{C}-\mathbf{A})) \) |
| Lambertian diffuse      | \( k_d \cdot \max(\mathbf{N} \cdot \mathbf{L}, 0) \)          |
| Phong specular          | \( k_s \cdot [\max(\mathbf{R} \cdot \mathbf{V}, 0)]^n \)      |
| Blinn-Phong specular    | \( k_s \cdot [\max(\mathbf{N} \cdot \mathbf{H}, 0)]^{n'} \)   |
| Attenuation             | \( 1 / (k_c + k_l d + k_q d^2) \)                             |
| Fresnel (Schlick)       | \( F_0 + (1 - F_0)(1 - \cos\theta)^5 \)                       |
| Normal from map         | \( \text{TBN} \cdot (\text{sample} \times 2 - 1) \)           |
| Reinhard tone mapping   | \( L / (1 + L) \)                                              |

### Practical implementation checklist

1. Start with **ambient + diffuse** (Lambert) to get basic shape.
2. Add **Blinn-Phong specular** for shininess.
3. Add **attenuation** so lights have range.
4. Add **normal mapping** for surface detail.
5. Implement **shadow mapping** for directional lights.
6. Move to **PBR** when you need material variety (metal, wood, plastic all look right).
7. Add **HDR + tone mapping** last for visual polish.

---

*Previous: [Physics Math](10-physics-math.md) | Next: [Camera Math](12-camera-math.md)*
