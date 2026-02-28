# 05 — Tips, Tricks & Common Recipes

A collection of practical techniques, performance tips, and patterns that will save you hours of debugging.

---

## Debugging Techniques

### 1. Visualize Any Value as Color

This is THE most important shader debugging skill:

```glsl
// See a float value (0→1 range)
gl_FragColor = vec4(vec3(myValue), 1.0);  // white = 1, black = 0

// See a float that can be negative
gl_FragColor = vec4(myValue, -myValue, 0.0, 1.0);  // red = positive, green = negative

// See a vec2 (like UV coordinates)
gl_FragColor = vec4(uv, 0.0, 1.0);  // RG channels show x,y

// See a normal (remap -1→1 to 0→1)
gl_FragColor = vec4(normal * 0.5 + 0.5, 1.0);

// See if a value is NaN (NaN ≠ NaN is true)
if (myValue != myValue) gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0); // magenta = NaN!
```

### 2. Isolate by Region

Show your value only in one half of the screen:

```glsl
if (uv.x > 0.5) {
    color = vec3(debugValue);
} else {
    color = finalColor;
}
```

### 3. Number Lines (contour visualization)

```glsl
// Show contour lines of a distance field
float lines = fract(d * 20.0);
color = vec3(smoothstep(0.05, 0.0, lines));
```

---

## Performance Tips

### 1. Avoid Branching — Use Math Instead

```glsl
// ❌ SLOW: GPU must evaluate both branches
if (x > 0.5) {
    color = red;
} else {
    color = blue;
}

// ✅ FAST: branchless
color = mix(blue, red, step(0.5, x));
```

### 2. Move Computation to the Vertex Shader

Vertex shaders run per-vertex (3 times per triangle), fragment shaders run per-pixel (thousands per triangle). If a value only varies linearly across a surface, compute it in the vertex shader and let the GPU interpolate.

### 3. Avoid `pow()` When Possible

```glsl
// ❌ pow is expensive
float x2 = pow(x, 2.0);

// ✅ just multiply
float x2 = x * x;
float x3 = x * x * x;
```

### 4. Use `inversesqrt()` Instead of `1.0 / sqrt()`

```glsl
// ❌ two operations
float inv = 1.0 / sqrt(x);

// ✅ one built-in
float inv = inversesqrt(x);
```

### 5. Swizzle Instead of Constructing

```glsl
// ❌ constructs a new vec3
vec3 gray = vec3(color.r, color.r, color.r);

// ✅ swizzle (free)
vec3 gray = color.rrr;
```

### 6. `smoothstep` Over Custom Curves

`smoothstep` is hardware-optimized on most GPUs. Prefer it over custom polynomial interpolation.

### 7. Minimize Texture Reads

Each `texture2D()` call is expensive. Cache the result:

```glsl
// ❌ reads texture twice
float r = texture2D(tex, uv).r;
float g = texture2D(tex, uv).g;

// ✅ read once
vec4 sample = texture2D(tex, uv);
float r = sample.r;
float g = sample.g;
```

---

## Common Recipes

### Remap a Range

```glsl
// Map value from [inMin, inMax] → [outMin, outMax]
float remap(float value, float inMin, float inMax, float outMin, float outMax) {
    return outMin + (value - inMin) * (outMax - outMin) / (inMax - inMin);
}
```

### Rotate UV Coordinates

```glsl
vec2 rotate(vec2 uv, float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, -s, s, c) * uv;
}

// Usage: rotate around center
vec2 centered = uv - 0.5;
centered = rotate(centered, u_time);
vec2 rotated = centered + 0.5;
```

### Tiling / Repeat

```glsl
vec2 tile(vec2 uv, float count) {
    return fract(uv * count);
}

// Usage: 5x5 grid of your pattern
vec2 tiled = tile(uv, 5.0);
```

### Simple Noise (Hash-Based)

```glsl
float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}
```

### Value Noise

```glsl
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);  // smoothstep interpolation

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
```

### Vignette Effect

```glsl
float vignette(vec2 uv, float intensity) {
    uv = uv * 2.0 - 1.0;  // center
    return 1.0 - dot(uv, uv) * intensity;
}

// Usage:
color *= vignette(uv, 0.5);
```

### Grayscale (Perceptual)

```glsl
float gray = dot(color, vec3(0.2126, 0.7152, 0.0722));
// These weights match human perception (we're most sensitive to green)
```

### Screen-Space Grid Overlay

```glsl
float grid(vec2 uv, float spacing, float thickness) {
    vec2 g = abs(fract(uv * spacing) - 0.5);
    float line = min(g.x, g.y);
    return 1.0 - smoothstep(0.0, thickness, line);
}
```

### Glow / Bloom (Fake)

```glsl
// Inverse distance creates a glow effect
float glow = 0.02 / length(uv);  // bright at center, fades out
color += vec3(0.3, 0.6, 1.0) * glow;
```

### Film Grain

```glsl
float grain = hash(uv + u_time) * 0.1;  // subtle noise
color += grain - 0.05;  // center around zero to avoid brightness shift
```

---

## Useful Constants

```glsl
#define PI      3.14159265359
#define TWO_PI  6.28318530718
#define HALF_PI 1.57079632679
#define E       2.71828182846
```

---

## Pattern of Patterns: The Shader Development Loop

1. **Setup coordinates** — normalize, center, fix aspect ratio
2. **Define your SDF or pattern** — distance field, noise, coordinates
3. **Shape it** — `smoothstep`, `step`, `fract`, math
4. **Color it** — `mix` between colors based on your shape
5. **Animate** — add `u_time` to things
6. **Polish** — vignette, glow, color grading

This loop works for everything from a simple circle to a complex fractal.

---

## The "I'm Stuck" Checklist

When your shader shows **all black** or looks wrong:

- [ ] Are your UVs normalized? (`/ u_resolution.xy`)
- [ ] Did you output to `gl_FragColor`?
- [ ] Is `alpha` set to `1.0`? (transparent = invisible)
- [ ] Is your value in 0→1 range? (negative → black, >1 → clipped white)
- [ ] Output just `vec4(1.0, 0.0, 0.0, 1.0)` — if not red, the shader isn't compiling
- [ ] Check the console/log for GLSL compile errors
- [ ] Are your uniform names spelled exactly right?
- [ ] `1` vs `1.0` — type mismatch?
- [ ] `sin()`/`cos()` return values in -1→1, not 0→1 — need to remap?

---

## Recommended Learning Path

```
Day 1:  Gradient, circle, rectangle (basic shapes)
Day 2:  Stripes, checkerboard, polka dots (patterns with fract/floor)
Day 3:  Animation with sin/cos and u_time
Day 4:  Polar coordinates, starburst patterns
Day 5:  SDFs — combine, subtract, smooth union
Day 6:  Noise functions, organic textures
Day 7:  Basic lighting (diffuse + ambient)
Week 2: Post-processing effects (blur, vignette, color grading)
Week 3: Raymarching (3D SDFs rendered in fragment shader)
```

---

## Books & Resources

| Resource | Link | Best For |
|---|---|---|
| The Book of Shaders | thebookofshaders.com | Best beginner guide, interactive |
| Shadertoy | shadertoy.com | Browse & learn from others |
| Inigo Quilez's site | iquilezles.org | SDF bible, math reference |
| GPU Gems (NVIDIA) | developer.nvidia.com | Deep dives, free online |
| Learn OpenGL | learnopengl.com | Full pipeline understanding |

---

**Previous:** [04 — Fragment Shaders & Patterns](04-fragment-shaders.md)
**Back to:** [README](README.md)
