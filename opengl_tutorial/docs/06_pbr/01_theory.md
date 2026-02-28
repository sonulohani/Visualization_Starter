# PBR Theory

[Previous: SSAO](../05_advanced_lighting/10_ssao.md) | [Next: PBR Lighting →](02_lighting.md)

---

Everything we've rendered so far — from Phong shading through shadow mapping, normal mapping, HDR, and SSAO — has relied on empirical lighting models. They look plausible, but they're hand-tuned approximations that don't follow the actual physics of light. In this chapter we introduce **Physically Based Rendering (PBR)**, a lighting approach grounded in physical laws that delivers dramatically more realistic results with fewer ad hoc parameters.

This chapter is **theory only**. We won't write any code yet — the next chapter translates every concept here into working GLSL and C++. Take your time with this material; understanding the math will make the implementation feel natural.

## What Is Physically Based Rendering?

Physically Based Rendering is a collection of rendering techniques that model the interaction of light and surfaces using equations derived from physics. Instead of inventing arbitrary "ambient strength" or "specular power" knobs, PBR works with measurable material properties that behave correctly under any lighting condition.

A lighting model is considered "physically based" when it satisfies **three conditions**:

1. **Based on the microfacet surface model** — surfaces are modeled as collections of tiny, perfectly reflective mirrors.
2. **Energy conserving** — outgoing light energy never exceeds incoming light energy.
3. **Uses a physically based BRDF** — the function that describes how light reflects off a surface is derived from physical principles, not guesswork.

We'll explore each of these in depth throughout this chapter.

## Why PBR Over Phong/Blinn-Phong?

Our old Phong and Blinn-Phong models have served us well, but they fall short in several ways:

| | Phong / Blinn-Phong | PBR (Cook-Torrance) |
|---|---|---|
| **Energy conservation** | No — specular can add energy | Yes — outgoing ≤ incoming |
| **Fresnel effect** | Not modeled | Built into the BRDF |
| **Material consistency** | Looks different under different lighting | Consistent across all environments |
| **Artist workflow** | Tweaking `shininess`, `specularStrength` | Intuitive: `roughness`, `metallic` |
| **Physical realism** | Approximation of observation | Derived from physics |
| **Metal/dielectric distinction** | Manual, error-prone | Handled by the `metallic` parameter |

With PBR, an artist specifies a material once and it looks correct whether it's lit by a single point light, a sunset, or a fluorescent office — because the math enforces physical plausibility.

## The Microfacet Model

At a macroscopic level, a surface may look smooth. But at a microscopic level, every surface is composed of tiny, perfectly flat, mirror-like facets called **microfacets**. Each microfacet reflects light in a single direction determined by its own surface normal.

```
Smooth Surface (low roughness)
═══════════════════════════════════
  ↗ ↗ ↗ ↗ ↗ ↗ ↗ ↗ ↗ ↗ ↗ ↗ ↗      ← microfacet normals mostly aligned
─────────────────────────────────      with the macro surface normal
  Most microfacets point the same direction → sharp, mirror-like reflection


Rough Surface (high roughness)
═══════════════════════════════════
  ↗ ↖ ↑ ↗ ↙ ↗ ↖ ↑ ↗ ↖ ↗ ↑ ↙      ← microfacet normals point randomly
/\/\  /\/\/\  /\  /\/\/\  /\/\
  Microfacets point in many directions → light scatters, appears diffuse/blurry
```

The key insight: **roughness** controls how chaotically the microfacet normals are oriented relative to the overall surface normal.

- A **perfectly smooth** surface (roughness = 0) has all microfacets aligned → perfect mirror reflection.
- A **perfectly rough** surface (roughness = 1) has microfacets pointing in every direction → light scatters uniformly.

We never render individual microfacets. Instead, we use **statistical functions** that describe what fraction of microfacets are oriented in a particular direction. This is where the Normal Distribution Function (NDF) comes in — we'll define it precisely later.

## Energy Conservation

In the real world, a surface cannot reflect more light than it receives. This seems obvious, but Phong shading routinely violates it — a bright specular highlight can add energy that wasn't in the incoming light.

When light hits a surface, it splits into two parts:

```
         Incoming Light
              │
              ▼
    ┌─────────────────────┐
    │       Surface       │
    └──────┬────┬─────────┘
           │    │
     ┌─────┘    └─────┐
     ▼                ▼
 Reflected        Refracted
 (Specular)       (enters surface)
     kS               kD
                       │
                       ▼
               ┌───────────────┐
               │  Dielectric:  │  Scattered internally, exits as diffuse color
               │  Metal:       │  Absorbed entirely — no diffuse
               └───────────────┘
```

- **Reflected light (specular, kS):** Bounces directly off the surface. This is the sharp highlight you see on glossy objects.
- **Refracted light:** Enters the surface. For **dielectrics** (non-metals like plastic, wood, stone), the light scatters internally and exits as **diffuse color (kD)**. For **metals**, the refracted light is **absorbed** — metals have no diffuse component, only specular (often tinted by their albedo).

The energy conservation constraint is simple:

```
kS + kD ≤ 1.0
```

In practice, we compute kS from the Fresnel equation and set:

```
kD = 1.0 - kS
```

This guarantees we never emit more light than we received. For metals, we further multiply kD by `(1.0 - metallic)` to eliminate their diffuse contribution.

### Metals vs. Dielectrics

This distinction is fundamental in PBR:

| Property | Dielectric (non-metal) | Metal |
|---|---|---|
| Diffuse | Yes — refracted light scatters back out | No — refracted light is absorbed |
| Specular | Yes — weak, uncolored (white/grey) | Yes — strong, tinted by albedo |
| F0 (base reflectivity) | ~0.04 (low) | 0.5 – 1.0 (high, colored) |
| Examples | Plastic, wood, stone, skin, fabric | Gold, silver, copper, iron, aluminum |

A `metallic` parameter of 0.0 or 1.0 handles pure dielectrics and metals. Values in between can represent dusty or oxidized metals, though in practice most materials are clearly one or the other.

## Radiance, Irradiance, and Radiant Flux

Before we can write the rendering equation, we need precise vocabulary for describing light quantities:

**Radiant flux (Φ)** is the total energy emitted by a light source per unit time, measured in watts (W). Think of it as "how much light" a source produces in total.

**Solid angle (ω)** describes a direction in 3D space, or more precisely, a "cone" of directions. It's the 3D equivalent of a 2D angle, measured in steradians (sr). A full sphere subtends 4π steradians.

**Radiant intensity (I)** is the radiant flux per solid angle: how much energy is emitted in a specific direction. For a point light that radiates equally in all directions:

```
I = Φ / (4π)
```

**Radiance (L)** is the amount of light arriving at (or leaving) a point from a specific direction, per unit area, per solid angle. It's the quantity our rendering equation computes. Radiance is what determines the color of a pixel.

**Irradiance (E)** is the total radiance arriving at a point from all directions over a hemisphere. It's what determines how "lit" a point is overall.

For our direct-lighting PBR shader, we compute radiance from each light individually and sum them — the summation replaces the integral over the hemisphere.

## The Reflectance Equation

The rendering equation, simplified for direct reflectance, is:

$$L_o(p, \omega_o) = \int_\Omega f_r(p, \omega_i, \omega_o) \cdot L_i(p, \omega_i) \cdot (n \cdot \omega_i) \, d\omega_i$$

Let's decode every symbol:

| Symbol | Meaning |
|---|---|
| \(L_o(p, \omega_o)\) | **Outgoing radiance** from point \(p\) in direction \(\omega_o\) — this is what we compute per fragment |
| \(\Omega\) | The hemisphere oriented around the surface normal at \(p\) |
| \(f_r\) | The **BRDF** — describes how much incoming light from \(\omega_i\) is reflected toward \(\omega_o\) |
| \(L_i(p, \omega_i)\) | **Incoming radiance** at \(p\) from direction \(\omega_i\) |
| \(n \cdot \omega_i\) | **Cosine term** (Lambert's cosine law) — light hitting at a glancing angle contributes less |
| \(d\omega_i\) | Differential solid angle — we integrate over all incoming directions |

```
              n (surface normal)
              ↑
              │       Ω (hemisphere)
          ────┼──── ╱ ╲
        ╱     │   ╱     ╲
      ╱       │  ╱  all   ╲
    ╱    ωi ──┤╱  incoming  ╲
    ╲    ↗    p    light     ╱
      ╲      ╱│╲  directions╱
        ╲  ╱  │  ╲       ╱
          ────┼────╲   ╱
              │     ωo → (view direction)
              │
```

The integral says: "for every possible incoming light direction over the hemisphere, compute how much of that light gets reflected toward the viewer, weight it by the angle of incidence, and sum it all up."

For **direct lighting** (point lights, directional lights), each light illuminates from exactly one direction, so the integral collapses to a finite sum:

$$L_o(p, \omega_o) = \sum_{i=0}^{n} f_r(p, \omega_i, \omega_o) \cdot L_i(p, \omega_i) \cdot (n \cdot \omega_i)$$

This is what we'll implement in the next chapter.

## The Cook-Torrance BRDF

The BRDF is the heart of PBR. We use the **Cook-Torrance** model, which splits the BRDF into a diffuse and specular part:

$$f_r = k_D \cdot f_{Lambert} + k_S \cdot f_{CookTorrance}$$

where:

$$f_{Lambert} = \frac{c}{\pi}$$

\(c\) is the surface albedo (base color). The division by π normalizes the diffuse term for energy conservation.

$$f_{CookTorrance} = \frac{DFG}{4(\omega_o \cdot n)(\omega_i \cdot n)}$$

The three functions in the numerator — **D**, **F**, and **G** — each model a different physical phenomenon:

| Function | Name | What it models |
|---|---|---|
| **D** | Normal Distribution Function (NDF) | How many microfacets are aligned with the halfway vector |
| **F** | Fresnel equation | Ratio of reflected vs. refracted light at different angles |
| **G** | Geometry function | Self-shadowing between microfacets |

```
Cook-Torrance BRDF — Conceptual Breakdown
════════════════════════════════════════════

                        D · F · G
  f_CookTorrance = ─────────────────────
                    4 · (ωo·n) · (ωi·n)

  ┌──────────────────┐
  │  D (NDF)         │  "How many microfacets reflect light toward me?"
  │  Distribution of │  High D → bright specular highlight
  │  microfacet      │  Controlled by roughness
  │  orientations    │
  └──────────────────┘

  ┌──────────────────┐
  │  F (Fresnel)     │  "How much light is reflected vs. refracted?"
  │  Angle-dependent │  At steep angles → more reflection
  │  reflectivity    │  Controlled by F0 (base reflectivity)
  └──────────────────┘

  ┌──────────────────┐
  │  G (Geometry)    │  "How much light is blocked by other microfacets?"
  │  Self-shadowing  │  Rough surfaces → more self-shadowing
  │  of microfacets  │  Reduces brightness at grazing angles
  └──────────────────┘
```

The denominator \(4(\omega_o \cdot n)(\omega_i \cdot n)\) is a normalization factor that accounts for the geometry of the reflection configuration. Let's now examine each function.

## D — Normal Distribution Function (Trowbridge-Reitz GGX)

The NDF statistically approximates the percentage of microfacets whose normals are aligned with the **halfway vector** \(h\) between the light direction and the view direction. Only microfacets whose normals align with \(h\) can reflect light from the light source toward the viewer.

We use the **Trowbridge-Reitz GGX** distribution, the industry standard:

$$D_{GGX}(n, h, \alpha) = \frac{\alpha^2}{\pi \left( (n \cdot h)^2 (\alpha^2 - 1) + 1 \right)^2}$$

where:
- \(n\) is the surface normal
- \(h\) is the halfway vector: \(h = \text{normalize}(\omega_i + \omega_o)\)
- \(\alpha = \text{roughness}^2\) (squaring the roughness gives more perceptually linear control)

```
NDF Response for Different Roughness Values
════════════════════════════════════════════

  D(n·h)
  │
  │  ╱╲  roughness = 0.1 (very smooth)
  │ ╱  ╲    → tall, narrow peak
  │╱    ╲   → tight specular highlight
  │      ╲
  │   ╱──────╲   roughness = 0.5
  │  ╱        ╲    → medium peak
  │ ╱          ╲   → wider highlight
  │╱            ╲
  │╱──────────────╲─  roughness = 1.0
  │                    → very wide, low peak
  │                    → broad, dim highlight
  └────────────────────── n·h
  0                     1.0
```

At low roughness, the NDF produces a sharp spike — only microfacets very closely aligned with \(h\) contribute. This creates a small, bright specular highlight. At high roughness, the distribution spreads out, producing a larger, dimmer highlight.

## F — Fresnel Equation (Schlick's Approximation)

The Fresnel equations describe how the ratio of reflected to refracted light changes with the viewing angle. You've seen this in everyday life: a lake looks transparent when you look straight down (minimal reflection), but becomes a mirror when you look at it from a low angle (maximal reflection).

The full Fresnel equations are expensive to compute, so we use **Schlick's approximation**:

$$F_{Schlick}(h, v, F_0) = F_0 + (1 - F_0)(1 - (h \cdot v))^5$$

where:
- \(F_0\) is the **base reflectivity** — the surface's reflectance when looking straight at it (0° angle)
- \(h\) is the halfway vector
- \(v\) is the view direction

As the angle between \(h\) and \(v\) increases (grazing angles), the Fresnel term approaches 1.0 — nearly total reflection. This is the **Fresnel effect**.

### Base Reflectivity (F0) Values

The F0 value differs dramatically between dielectrics and metals:

| Material | F0 | Type |
|---|---|---|
| Water | (0.02, 0.02, 0.02) | Dielectric |
| Plastic / Glass | (0.04, 0.04, 0.04) | Dielectric |
| Diamond | (0.17, 0.17, 0.17) | Dielectric |
| Iron | (0.56, 0.57, 0.58) | Metal |
| Copper | (0.95, 0.64, 0.54) | Metal |
| Gold | (1.00, 0.71, 0.29) | Metal |
| Silver | (0.95, 0.93, 0.88) | Metal |
| Aluminum | (0.91, 0.92, 0.92) | Metal |

Notice that dielectrics have a low, achromatic (grey) F0 around 0.04, while metals have high, often colored F0 values (gold reflects more red and green than blue, giving it that characteristic warm color).

In our PBR shader, we compute F0 as:

```
F0 = mix(vec3(0.04), albedo, metallic)
```

For dielectrics (metallic = 0), F0 is 0.04. For metals (metallic = 1), F0 is the albedo color itself, since metal reflections are tinted by the metal's color.

## G — Geometry Function (Smith's Method with Schlick-GGX)

Even after accounting for how many microfacets are aligned correctly (D) and how much light gets reflected (F), some of the reflected light never reaches the viewer because it's **blocked by neighboring microfacets**. This blocking takes two forms:

```
Geometry Obstruction (incoming light blocked)
═════════════════════════════════════════════

        Light ↘
                ╲
    ┌───┐        ╲ blocked!
    │   │ ◄──────╳─────── microfacet
    │   │        ╱
    └───┘      ╱
    blocker  ╱
           surface


Geometry Shadowing (outgoing light blocked)
═══════════════════════════════════════════

    microfacet ──────╳──────► blocked!
                    ╱        ╲
                  ╱    ┌───┐  ╲ Eye
                ╱      │   │
              surface  │   │
                       └───┘
                       blocker
```

We model both effects with **Smith's method**, which combines two evaluations of the **Schlick-GGX** function — one for the light direction, one for the view direction:

$$G_{SchlickGGX}(n, v, k) = \frac{n \cdot v}{(n \cdot v)(1 - k) + k}$$

$$G_{Smith}(n, v, l, k) = G_{SchlickGGX}(n, v, k) \cdot G_{SchlickGGX}(n, l, k)$$

The parameter \(k\) depends on whether we're doing direct or image-based lighting:

| Lighting type | k |
|---|---|
| Direct lighting | \(k = \frac{(\text{roughness} + 1)^2}{8}\) |
| IBL (image-based) | \(k = \frac{\text{roughness}^2}{2}\) |

The geometry function returns a value between 0 and 1:
- **1.0**: no self-shadowing — all light gets through
- **0.0**: complete self-shadowing — all light is blocked

Rough surfaces have more extreme microfacet angles, so they produce more self-shadowing. This naturally dims rough surfaces at grazing angles, which matches real-world observation.

```
Geometry Function Response
══════════════════════════

  G(n·v)
  1.0 ─────────────╮
                    │╲  roughness = 0.1
                    │ ╲    → almost no shadowing
                    │  ╲
  0.5               │   ╲
                    │╲   ╲
                    │ ╲ roughness = 0.5
                    │  ╲ ╲
                    │   ╲ ╲  roughness = 1.0
                    │    ╲╲  → significant shadowing at grazing angles
  0.0 ─────────────┴──────── n·v
                   1.0     0.0
                 (head-on)  (grazing)
```

## Putting It All Together — The Complete PBR Pipeline

Let's trace the full path of a single fragment computation:

```
PBR Fragment Pipeline
═════════════════════

  Inputs:                     Computation:
  ┌───────────────┐
  │ albedo (vec3) │──┐
  │ metallic      │──┤        ┌───────────────────────────────┐
  │ roughness     │──┼───────►│  For each light:              │
  │ ao            │──┤        │                               │
  │ N (normal)    │──┤        │  1. Calculate L, V, H         │
  │ V (view dir)  │──┘        │  2. Radiance = color × atten  │
                              │  3. D = GGX NDF(N, H, α)     │
                              │  4. F = Fresnel(H, V, F0)    │
                              │  5. G = Smith(N, V, L, k)    │
                              │                               │
                              │  specular = DFG / 4·NV·NL    │
                              │  kD = (1-F) × (1-metallic)   │
                              │  Lo += (kD×albedo/π           │
                              │        + specular)            │
                              │        × radiance × NdotL    │
                              └───────────┬───────────────────┘
                                          │
                                          ▼
                              ┌───────────────────────┐
                              │ ambient = 0.03 × alb  │
                              │          × ao         │
                              │ color = ambient + Lo   │
                              └───────────┬───────────┘
                                          │
                                          ▼
                              ┌───────────────────────┐
                              │ HDR Tone Mapping      │
                              │ Gamma Correction      │
                              │ → FragColor           │
                              └───────────────────────┘
```

## PBR Material Parameters

PBR distills material appearance down to a small set of intuitive, physically meaningful parameters:

### Albedo (vec3)

The base color of the surface. For dielectrics, this is the diffuse color. For metals, it defines the tint of the specular reflection. Albedo values should be in sRGB space and are typically sourced from a texture.

### Metallic (float, 0.0 – 1.0)

Whether the surface is a metal (1.0) or a dielectric (0.0). Most materials are one or the other; intermediate values are used sparingly for transitions like rust or dust on metal.

### Roughness (float, 0.0 – 1.0)

How rough the surface is at a microscopic level. 0.0 produces a perfect mirror; 1.0 produces a completely diffuse-looking surface. This single parameter controls the width of specular highlights and the sharpness of reflections.

### Ambient Occlusion (float, 0.0 – 1.0)

A per-pixel darkening factor representing how much ambient/indirect light reaches that point. Crevices, corners, and enclosed spaces have lower AO values. This is usually baked into a texture or computed with SSAO (which we covered in the previous section).

```
PBR Material Examples
═════════════════════

  Material        Albedo          Metallic   Roughness
  ──────────────  ──────────────  ─────────  ─────────
  Polished Gold   (1.0, 0.7, 0.3)   1.0       0.1
  Rough Iron      (0.6, 0.6, 0.6)   1.0       0.8
  Smooth Plastic  (0.8, 0.2, 0.2)   0.0       0.2
  Rough Wood      (0.6, 0.4, 0.2)   0.0       0.9
  Ceramic Tile    (0.9, 0.9, 0.85)  0.0       0.3
```

## Summary

Let's recap the core concepts of PBR before we implement them:

1. **Microfacet model:** Surfaces are composed of tiny mirrors. The roughness parameter controls how chaotically oriented they are. Statistical functions (D, G) describe their collective behavior.

2. **Energy conservation:** Outgoing light never exceeds incoming light. `kS + kD ≤ 1.0`. Metals absorb all refracted light (no diffuse). Dielectrics have both diffuse and specular.

3. **Cook-Torrance BRDF:** Combines a Lambertian diffuse term with a microfacet specular term (DFG / 4·NV·NL).

4. **Normal Distribution Function (D):** Trowbridge-Reitz GGX — determines how many microfacets are aligned with the halfway vector. Controlled by roughness.

5. **Fresnel equation (F):** Schlick's approximation — surfaces become more reflective at grazing angles. F0 is 0.04 for dielectrics, albedo-colored for metals.

6. **Geometry function (G):** Smith's method with Schlick-GGX — accounts for microfacet self-shadowing. Rough surfaces have more occlusion.

7. **Material parameters:** Just four values — albedo, metallic, roughness, AO — replace the half-dozen ad hoc knobs of Phong shading.

## Key Takeaways

- PBR produces more realistic and consistent results than empirical models like Phong because it is grounded in physical laws of light transport.
- The three pillars of PBR are the microfacet model, energy conservation, and a physically based BRDF.
- The Cook-Torrance BRDF uses three functions — NDF (D), Fresnel (F), and Geometry (G) — each modeling a distinct physical phenomenon.
- Roughness is the single most impactful parameter, controlling highlight size, reflection sharpness, and self-shadowing simultaneously.
- Metals and dielectrics are treated differently: metals have no diffuse component and their specular reflections are tinted by their albedo.
- For direct lighting, the hemisphere integral simplifies to a sum over light sources, making PBR practical for real-time rendering.

## Next Steps

In the next chapter, we take everything we've learned here and translate it into a working OpenGL implementation. We'll write the Cook-Torrance BRDF in GLSL, render a grid of spheres with varying roughness and metallic values, and see PBR in action.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: SSAO](../05_advanced_lighting/10_ssao.md) | [06. PBR](.) | [Next: PBR Lighting →](02_lighting.md) |
