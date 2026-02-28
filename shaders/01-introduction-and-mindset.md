# 01 — Introduction to Shaders & The Shader Mindset

## What Is a Shader?

A **shader** is a small program that runs on the **GPU** (Graphics Processing Unit) instead of the CPU. Its job is to decide:

- **Where** a vertex (point) ends up on the screen → *Vertex Shader*
- **What color** each pixel should be → *Fragment (Pixel) Shader*

That's it. Everything you see rendered in a game, a 3D app, or even a fancy website background boils down to those two questions.

---

## Why Should You Learn Shaders?

| Reason | Benefit |
|---|---|
| Full creative control | Paint pixels exactly how you want |
| Performance | GPU runs thousands of threads in parallel |
| Portability | GLSL / HLSL / WGSL work across platforms |
| Demystification | You stop being afraid of the graphics pipeline |

---

## The Shader Mindset (Read This First!)

Writing shaders is **fundamentally different** from writing normal code. Before you write a single line, internalize these mental shifts:

### 1. Think Per-Pixel, Not Per-Frame

In normal code you think: *"loop over all pixels and color them."*

In shaders you think: **"I am ONE pixel. What color should I be?"**

Your fragment shader runs **in parallel** for every pixel on screen. You never loop over pixels yourself — the GPU does that for you.

### 2. There Is No State

Shaders are **stateless**. Each pixel knows nothing about its neighbors. You cannot say *"if the pixel to my left is red, make me blue."* You only have:

- Your own coordinates (`gl_FragCoord`, UV, etc.)
- Uniforms (global read-only data sent from the CPU)
- Textures you can sample

If you need to know what happened last frame, you must explicitly pass that info via a texture.

### 3. Everything Is Math

There are no objects, no classes, no OOP. Shaders are pure **mathematical functions**:

```
input coordinates → do math → output color
```

The better your intuition with:
- **Vectors and dot products**
- **`sin`, `cos`, `fract`, `mod`, `step`, `smoothstep`, `mix`**
- **Distance functions**

…the more powerful your shaders become.

### 4. Embrace Floating Point (0.0 to 1.0)

Colors are `vec4(r, g, b, a)` where each channel is **0.0 – 1.0** (not 0–255). Coordinates are usually **normalized** too:

```glsl
vec2 uv = gl_FragCoord.xy / u_resolution.xy; // 0.0 → 1.0
```

Get comfortable thinking in the **unit square** (0→1 on both axes).

### 5. Debug with Color

There's no `print()` in a shader. To debug, **output values as colors**:

```glsl
// Want to see what uv.x looks like?
gl_FragColor = vec4(uv.x, 0.0, 0.0, 1.0);
// Bright red = high value, black = low value
```

This is your `console.log`. Embrace it.

### 6. Start Simple, Layer Complexity

The best shaders are built by stacking simple operations:

1. Get normalized coordinates
2. Apply one transform (e.g., center the origin)
3. Compute one distance / pattern
4. Map it to a color

Don't try to write a raymarcher on day one. Start with a gradient. Then a circle. Then animate it.

### 7. GPU Branching Is Expensive

`if/else` on the GPU isn't free like on the CPU. Prefer **math-based selection**:

```glsl
// ❌ Branchy
if (d < 0.5) color = red; else color = blue;

// ✅ Branchless (preferred)
color = mix(blue, red, step(0.5, d));
```

`step()`, `smoothstep()`, and `mix()` are your best friends.

---

## The Graphics Pipeline (Simplified)

```
CPU (your app code)
  │
  ▼
┌──────────────────┐
│  Vertex Shader   │  → Transforms each vertex position
└──────────────────┘
  │
  ▼
┌──────────────────┐
│  Rasterization   │  → GPU figures out which pixels are inside each triangle
└──────────────────┘
  │
  ▼
┌──────────────────┐
│ Fragment Shader   │  → Colors each pixel
└──────────────────┘
  │
  ▼
  Screen
```

For now, **fragment shaders** are where the fun is. That's what we'll focus on.

---

## Where to Practice

| Tool | URL | Notes |
|---|---|---|
| **Shadertoy** | shadertoy.com | Best playground, huge community |
| **The Book of Shaders** | thebookofshaders.com | Best beginner tutorial |
| **GLSL Sandbox** | glslsandbox.com | Quick experiments |
| **VS Code + glsl-canvas** | VS Code extension | Local, real-time preview |

---

## Your First Shader (Just Read, Don't Stress)

```glsl
// A fragment shader that makes a red-to-black horizontal gradient
#ifdef GL_ES
precision mediump float;
#endif

uniform vec2 u_resolution; // canvas size in pixels

void main() {
    // Step 1: Normalize coordinates to 0.0 → 1.0
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;

    // Step 2: Use the x-coordinate as red intensity
    vec3 color = vec3(uv.x, 0.0, 0.0);

    // Step 3: Output the color
    gl_FragColor = vec4(color, 1.0);
}
```

**What it does:** Every pixel asks "how far right am I?" and makes itself that much red.

---

## Key Takeaways

- A shader = a function that runs per-pixel (or per-vertex) on the GPU
- Think **"I am one pixel, what color am I?"**
- No loops over pixels, no state, no print — just math
- Colors and coordinates live in the **0.0–1.0** range
- Debug by outputting values as colors
- Start dead simple and layer up

---

**Next:** [02 — GLSL Basics](02-glsl-basics.md)
