# 03 — Vertex Shaders

The vertex shader runs **once per vertex** and decides **where** that vertex appears on screen. It runs before the fragment shader.

---

## What Does a Vertex Shader Do?

```
Input: vertex data (position, normal, UV, color, ...)
  │
  ▼
Vertex Shader: transforms position from 3D world → 2D screen
  │
  ▼
Output: screen-space position + data to pass to fragment shader
```

Its primary job: **multiply the vertex position by transformation matrices**.

---

## The Simplest Vertex Shader

```glsl
attribute vec3 a_position;   // vertex position from your mesh

uniform mat4 u_modelViewProjection; // combined transform matrix

void main() {
    gl_Position = u_modelViewProjection * vec4(a_position, 1.0);
}
```

**That's it.** Take the position, multiply by the MVP matrix, assign to `gl_Position`.

### Key built-in output:

| Variable | Type | Description |
|---|---|---|
| `gl_Position` | `vec4` | **Required output** — clip-space position |
| `gl_PointSize` | `float` | Size of point (when drawing `GL_POINTS`) |

---

## The Three Transformation Matrices

Vertex transformations are done with three matrices, usually combined:

```
Model Matrix → World Matrix → View Matrix → Projection Matrix
  (object       (where in      (camera        (perspective
   space)         world)         position)      or ortho)
```

### In practice:

```glsl
uniform mat4 u_model;       // object → world
uniform mat4 u_view;        // world → camera
uniform mat4 u_projection;  // camera → clip space

void main() {
    gl_Position = u_projection * u_view * u_model * vec4(a_position, 1.0);
}
```

Most frameworks give you a pre-combined **MVP** (Model-View-Projection) matrix so you only need one multiply.

---

## Passing Data to the Fragment Shader

The vertex shader can send data downstream using `varying` (or `out` in GLSL 3.0+):

```glsl
// ---- VERTEX SHADER ----
attribute vec3 a_position;
attribute vec2 a_uv;

uniform mat4 u_mvp;

varying vec2 v_uv;  // → goes to fragment shader

void main() {
    v_uv = a_uv;    // pass UV coordinates through
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
```

```glsl
// ---- FRAGMENT SHADER ----
precision mediump float;

varying vec2 v_uv;  // ← received from vertex shader (interpolated!)

void main() {
    gl_FragColor = vec4(v_uv, 0.0, 1.0); // visualize UVs as colors
}
```

### How `varying` works:

The GPU **interpolates** varying values across the triangle. If vertex A has `v_uv = (0,0)` and vertex B has `v_uv = (1,1)`, a pixel halfway between them gets `v_uv = (0.5, 0.5)`.

```
Vertex A (UV: 0,0) ────────── Vertex B (UV: 1,0)
         ╲                    ╱
          ╲    pixel here    ╱
           ╲   UV ≈ 0.5,0.3╱
            ╲              ╱
             ╲            ╱
         Vertex C (UV: 0,1)
```

---

## Common Vertex Shader Techniques

### 1. Vertex Displacement (Wavy Surface)

```glsl
attribute vec3 a_position;
uniform mat4 u_mvp;
uniform float u_time;

void main() {
    vec3 pos = a_position;

    // Displace y based on x position and time → wave effect
    pos.y += sin(pos.x * 4.0 + u_time * 2.0) * 0.1;

    gl_Position = u_mvp * vec4(pos, 1.0);
}
```

### 2. Billboarding (Always Face Camera)

```glsl
// Make a quad always face the camera by zeroing out the rotation
// part of the view matrix
mat4 billboard = u_view;
billboard[0] = vec4(1.0, 0.0, 0.0, 0.0);
billboard[1] = vec4(0.0, 1.0, 0.0, 0.0);
billboard[2] = vec4(0.0, 0.0, 1.0, 0.0);

gl_Position = u_projection * billboard * u_model * vec4(a_position, 1.0);
```

### 3. Scaling a Point Sprite

```glsl
uniform float u_pointSize;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    gl_PointSize = u_pointSize / gl_Position.w;  // perspective scaling
}
```

---

## Vertex vs Fragment Shader Cheat Sheet

| Aspect | Vertex Shader | Fragment Shader |
|---|---|---|
| Runs per… | Vertex (3 per triangle) | Pixel (thousands per triangle) |
| Primary output | `gl_Position` (where) | `gl_FragColor` (color) |
| Input from CPU | `attribute` | — |
| Input from vertex | — | `varying` (interpolated) |
| Typical work | Transform, deform mesh | Color, lighting, textures |
| Complexity budget | Cheaper (fewer invocations) | More expensive (many pixels) |

**Pro tip:** Move expensive calculations to the vertex shader when possible. It runs far fewer times than the fragment shader.

---

## Normals and Lighting (Preview)

For lighting you need to transform **normals** too, but with the **inverse-transpose** of the model matrix (to handle non-uniform scaling):

```glsl
attribute vec3 a_normal;
uniform mat3 u_normalMatrix;  // inverse-transpose of model matrix

varying vec3 v_normal;

void main() {
    v_normal = normalize(u_normalMatrix * a_normal);
    gl_Position = u_mvp * vec4(a_position, 1.0);
}
```

We'll use this in the fragment shader tutorial for lighting.

---

**Previous:** [02 — GLSL Basics](02-glsl-basics.md)
**Next:** [04 — Fragment Shaders & Patterns](04-fragment-shaders.md)
