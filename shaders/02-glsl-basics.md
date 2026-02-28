# 02 — GLSL Basics

GLSL (OpenGL Shading Language) is the language used to write shaders for OpenGL and WebGL. This covers the essentials you need to start writing shaders.

---

## Data Types

### Scalars

```glsl
float x = 1.0;   // Always use decimal! 1.0 not 1
int   i = 5;
bool  b = true;
```

> **Rule:** Floats MUST have a decimal point. `1` is an int, `1.0` is a float. Mixing them causes errors.

### Vectors (The Star of the Show)

```glsl
vec2 position = vec2(0.5, 0.8);        // 2 components: x, y
vec3 color    = vec3(1.0, 0.0, 0.5);   // 3 components: r, g, b  (or x, y, z)
vec4 pixel    = vec4(1.0, 0.0, 0.5, 1.0); // 4 components: r, g, b, a
```

### Accessing Vector Components

You have **three naming sets** (they're aliases):

```glsl
vec4 v = vec4(1.0, 2.0, 3.0, 4.0);

v.x, v.y, v.z, v.w   // position style
v.r, v.g, v.b, v.a   // color style
v.s, v.t, v.p, v.q   // texture style

// They're the same thing:
v.x == v.r == v.s  // all refer to the first component
```

### Swizzling (Super Power!)

You can rearrange and repeat components freely:

```glsl
vec3 color = vec3(1.0, 0.5, 0.0); // orange

color.rr   → vec2(1.0, 1.0)       // repeat red twice
color.bgr  → vec3(0.0, 0.5, 1.0)  // reverse order
color.rrr  → vec3(1.0, 1.0, 1.0)  // grayscale from red channel
```

### Matrices

```glsl
mat2 m2;  // 2×2 matrix
mat3 m3;  // 3×3 matrix
mat4 m4;  // 4×4 matrix (used for transforms)
```

---

## Variable Qualifiers

These control how data flows in and out of shaders:

| Qualifier | Direction | Description |
|---|---|---|
| `uniform` | CPU → GPU (read-only) | Same value for every pixel/vertex |
| `attribute` | CPU → Vertex Shader | Per-vertex data (position, normal, UV) |
| `varying` | Vertex → Fragment | Interpolated across the triangle |
| `const` | — | Compile-time constant |

```glsl
uniform float u_time;          // elapsed time (same for all pixels)
uniform vec2  u_resolution;    // canvas size (same for all pixels)
uniform vec2  u_mouse;         // mouse position

attribute vec3 a_position;     // vertex position (vertex shader only)

varying vec2 v_uv;             // passed from vertex → fragment shader
```

> **Note:** In modern GLSL (3.0+), `attribute` → `in`, `varying` → `out`/`in`. But we'll use the classic names since most tutorials and Shadertoy use them.

---

## Essential Built-in Functions

These are **the core toolkit**. Memorize what they do:

### Interpolation & Clamping

```glsl
mix(a, b, t)       // Linear interpolation: a*(1-t) + b*t
                    // t=0 → a, t=1 → b, t=0.5 → halfway

clamp(x, min, max) // Constrains x between min and max

step(edge, x)      // Returns 0.0 if x < edge, 1.0 if x >= edge
                    // Hard cutoff — like a binary switch

smoothstep(e0, e1, x)  // Smooth transition from 0 to 1 between e0 and e1
                        // The most useful function in all of shading
```

**Visual comparison:**
```
step(0.5, x):         smoothstep(0.3, 0.7, x):
  1 |    ┌──────        1 |       ╱──────
    |    │                 |     ╱
  0 |────┘               0 |──╱
    +---------x           +-----------x
       0.5                  0.3  0.7
```

### Math

```glsl
abs(x)              // Absolute value
mod(x, y)           // Modulo (remainder): great for repeating patterns
fract(x)            // Fractional part: x - floor(x)  → always 0.0 to 1.0
floor(x)            // Round down
ceil(x)             // Round up
sign(x)             // Returns -1, 0, or 1
min(a, b)           // Smaller of two values
max(a, b)           // Larger of two values
pow(x, n)           // x to the power of n
sqrt(x)             // Square root
```

### Trigonometry

```glsl
sin(x)              // Sine (-1 to 1), for waves and oscillation
cos(x)              // Cosine
atan(y, x)          // Angle of a point (used for polar coordinates)
radians(deg)        // Degrees to radians
```

### Vector Operations

```glsl
length(v)           // Magnitude of vector: sqrt(x² + y²)
distance(a, b)      // Distance between two points: length(a - b)
normalize(v)        // Unit vector (length = 1) in same direction
dot(a, b)           // Dot product (how aligned two vectors are)
cross(a, b)         // Cross product (3D only, gives perpendicular vector)
reflect(I, N)       // Reflect vector I around normal N
```

---

## Operator Overloading on Vectors

Math operators work **component-wise** on vectors:

```glsl
vec2 a = vec2(1.0, 2.0);
vec2 b = vec2(3.0, 4.0);

a + b  → vec2(4.0, 6.0)      // component-wise add
a * b  → vec2(3.0, 8.0)      // component-wise multiply
a * 2.0 → vec2(2.0, 4.0)     // scalar multiply
```

This is incredibly useful and saves you from writing loops.

---

## The Fragment Shader Structure

Every fragment shader looks like this:

```glsl
#ifdef GL_ES
precision mediump float;
#endif

// Uniforms (data from CPU)
uniform vec2  u_resolution;
uniform float u_time;

void main() {
    // 1. Normalize coordinates
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;

    // 2. Do your math / compute color
    vec3 color = vec3(0.0);

    // 3. Output
    gl_FragColor = vec4(color, 1.0);
}
```

### Key built-in variables:

| Variable | Type | Description |
|---|---|---|
| `gl_FragCoord` | `vec4` | Pixel coordinates in screen space |
| `gl_FragColor` | `vec4` | **Output** — the final pixel color |

---

## Type Constructors & Casting

GLSL is **strict** about types:

```glsl
// Constructing vectors
vec3(1.0)           → vec3(1.0, 1.0, 1.0)   // shorthand: fill all
vec4(color, 1.0)    → vec4(r, g, b, 1.0)     // extend a vec3
vec2(pixel.xy)      → extract first 2 components of a vec4

// Casting
float f = float(5);     // int → float
int   i = int(3.7);     // float → int (truncates)
```

---

## Putting It Together: Example

```glsl
#ifdef GL_ES
precision mediump float;
#endif

uniform vec2  u_resolution;
uniform float u_time;

void main() {
    // Normalize to 0→1
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;

    // Center the origin: -0.5 → 0.5
    uv -= 0.5;

    // Fix aspect ratio
    uv.x *= u_resolution.x / u_resolution.y;

    // Distance from center
    float d = length(uv);

    // Animated pulsing circle
    float radius = 0.3 + 0.05 * sin(u_time * 3.0);
    float circle = smoothstep(radius, radius - 0.01, d);

    // Color it
    vec3 color = vec3(0.2, 0.6, 1.0) * circle;

    gl_FragColor = vec4(color, 1.0);
}
```

**What this does:** Draws a smooth blue circle in the center that pulses with time.

---

## Common Beginner Mistakes

| Mistake | Fix |
|---|---|
| Writing `1` instead of `1.0` | GLSL floats need the decimal |
| Forgetting `precision mediump float;` | Required for WebGL/ES |
| Not normalizing coordinates | Always divide `gl_FragCoord.xy` by `u_resolution.xy` |
| UVs going 0→1 when you need centered | Subtract `0.5` to get -0.5→0.5 |
| Forgetting aspect ratio correction | Multiply `uv.x` by `resolution.x / resolution.y` |

---

**Previous:** [01 — Introduction & Mindset](01-introduction-and-mindset.md)
**Next:** [03 — Vertex Shaders](03-vertex-shaders.md)
