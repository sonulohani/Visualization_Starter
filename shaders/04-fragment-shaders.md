# 04 — Fragment Shaders & Patterns

This is where the magic happens. The fragment shader decides the **color of every pixel**. We'll build up from simple gradients to shapes, patterns, and basic lighting.

---

## The Template

Every example below uses this skeleton:

```glsl
#ifdef GL_ES
precision mediump float;
#endif

uniform vec2  u_resolution;
uniform float u_time;

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution.xy;  // normalized 0→1
    vec3 color = vec3(0.0);

    // ... your code here ...

    gl_FragColor = vec4(color, 1.0);
}
```

---

## Coordinate Systems

### Raw screen coordinates
```glsl
vec2 pixel = gl_FragCoord.xy;  // (0,0) at bottom-left, in pixels
```

### Normalized (0 → 1)
```glsl
vec2 uv = gl_FragCoord.xy / u_resolution.xy;
```

### Centered (-0.5 → 0.5)
```glsl
vec2 uv = gl_FragCoord.xy / u_resolution.xy - 0.5;
```

### Centered + Aspect-corrected (the gold standard)
```glsl
vec2 uv = gl_FragCoord.xy / u_resolution.xy - 0.5;
uv.x *= u_resolution.x / u_resolution.y;
```

**Always fix the aspect ratio** or your circles will look like ellipses.

---

## Building Blocks: Gradients

### Horizontal gradient
```glsl
color = vec3(uv.x);  // black left, white right
```

### Vertical gradient
```glsl
color = vec3(uv.y);  // black bottom, white top
```

### Diagonal gradient
```glsl
color = vec3((uv.x + uv.y) * 0.5);
```

### Two-color gradient
```glsl
vec3 colorA = vec3(0.1, 0.2, 0.8);  // blue
vec3 colorB = vec3(1.0, 0.4, 0.1);  // orange
color = mix(colorA, colorB, uv.x);
```

---

## Building Blocks: Shapes

### Circle

```glsl
vec2 uv = gl_FragCoord.xy / u_resolution.xy - 0.5;
uv.x *= u_resolution.x / u_resolution.y;

float d = length(uv);                     // distance from center
float circle = smoothstep(0.3, 0.29, d);  // radius 0.3, soft edge
color = vec3(circle);
```

### Ring

```glsl
float ring = smoothstep(0.28, 0.29, d) - smoothstep(0.30, 0.31, d);
color = vec3(ring);
```

### Rectangle

```glsl
vec2 uv = gl_FragCoord.xy / u_resolution.xy - 0.5;
uv.x *= u_resolution.x / u_resolution.y;

vec2 size = vec2(0.3, 0.2);
vec2 edge = smoothstep(size, size - 0.01, abs(uv));
float rect = edge.x * edge.y;
color = vec3(rect);
```

### Rounded Rectangle (SDF approach)

```glsl
float roundedRect(vec2 p, vec2 size, float radius) {
    vec2 d = abs(p) - size + radius;
    return length(max(d, 0.0)) - radius;
}

// In main():
float d = roundedRect(uv, vec2(0.3, 0.2), 0.05);
color = vec3(smoothstep(0.01, 0.0, d));
```

---

## Building Blocks: Patterns

### Stripes

```glsl
float stripe = step(0.5, fract(uv.x * 10.0));  // 10 vertical stripes
color = vec3(stripe);
```

### Grid

```glsl
vec2 grid = step(0.5, fract(uv * 10.0));
color = vec3(grid.x * grid.y);  // checkerboard-ish
```

### Checkerboard

```glsl
vec2 cell = floor(uv * 10.0);
float checker = mod(cell.x + cell.y, 2.0);
color = vec3(checker);
```

### Polka Dots (repeating circles)

```glsl
vec2 cell = fract(uv * 5.0) - 0.5;  // repeat + center each cell
float d = length(cell);
float dot = smoothstep(0.2, 0.19, d);
color = vec3(dot);
```

---

## Animation with `u_time`

### Pulsing

```glsl
float pulse = 0.5 + 0.5 * sin(u_time * 3.0);
color = vec3(pulse, 0.2, 0.5);
```

### Moving wave

```glsl
float wave = sin(uv.x * 20.0 + u_time * 4.0) * 0.05;
float line = smoothstep(0.01, 0.0, abs(uv.y - 0.5 + wave));
color = vec3(0.2, 0.8, 0.4) * line;
```

### Color cycling

```glsl
color = 0.5 + 0.5 * cos(u_time + uv.xyx + vec3(0.0, 2.0, 4.0));
```

This classic one-liner creates beautiful rainbow cycling. It works because `cos` outputs -1→1, we remap to 0→1 with `0.5 + 0.5 * cos(...)`, and the `vec3` offsets shift each color channel.

---

## Polar Coordinates

Convert from cartesian (x,y) to polar (angle, distance) for radial patterns:

```glsl
vec2 uv = gl_FragCoord.xy / u_resolution.xy - 0.5;
uv.x *= u_resolution.x / u_resolution.y;

float angle = atan(uv.y, uv.x);     // -PI → PI
float radius = length(uv);

// Star burst
float star = abs(sin(angle * 5.0));
color = vec3(star * smoothstep(0.5, 0.0, radius));
```

---

## Basic Lighting (Diffuse)

```glsl
// Assume v_normal comes interpolated from vertex shader
// and is per-pixel normal in world space

varying vec3 v_normal;
varying vec3 v_position;

uniform vec3 u_lightDir;  // direction TO the light (normalized)

void main() {
    vec3 normal = normalize(v_normal);

    // Lambertian diffuse: max(dot(N, L), 0)
    float diffuse = max(dot(normal, u_lightDir), 0.0);

    vec3 baseColor = vec3(0.8, 0.2, 0.3);
    vec3 color = baseColor * diffuse;

    gl_FragColor = vec4(color, 1.0);
}
```

### Add ambient so shadows aren't pitch black:

```glsl
float ambient = 0.15;
vec3 color = baseColor * (ambient + diffuse);
```

---

## Signed Distance Functions (SDF) — The Gateway to Advanced Shaders

SDFs return the **distance** from a point to the nearest surface. Negative = inside, positive = outside.

```glsl
// Circle SDF
float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

// Box SDF
float sdBox(vec2 p, vec2 size) {
    vec2 d = abs(p) - size;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

// Usage:
float d = sdCircle(uv, 0.3);
color = vec3(smoothstep(0.01, 0.0, d));  // filled circle
color = vec3(smoothstep(0.02, 0.01, abs(d)));  // outline only
```

### Combining SDFs:

```glsl
float union_sdf(float a, float b) { return min(a, b); }         // merge shapes
float intersect_sdf(float a, float b) { return max(a, b); }     // overlap only
float subtract_sdf(float a, float b) { return max(a, -b); }     // cut b from a
float smooth_union(float a, float b, float k) {                  // soft merge
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}
```

---

## Texture Sampling

```glsl
uniform sampler2D u_texture;
varying vec2 v_uv;

void main() {
    vec4 texColor = texture2D(u_texture, v_uv);
    gl_FragColor = texColor;
}
```

UV coordinates (0→1) map the texture onto the geometry. `v_uv` is set in the vertex shader and interpolated per-pixel.

---

## Summary

| Concept | Key Function |
|---|---|
| Distance to center | `length(uv)` |
| Soft edge | `smoothstep(a, b, d)` |
| Hard edge | `step(edge, d)` |
| Repeat pattern | `fract(uv * N)` |
| Tile index | `floor(uv * N)` |
| Animation | `sin(... + u_time)` |
| Color blend | `mix(colorA, colorB, t)` |
| Polar coords | `atan(y, x)`, `length(p)` |

---

**Previous:** [03 — Vertex Shaders](03-vertex-shaders.md)
**Next:** [05 — Tips & Tricks](05-tips-and-tricks.md)
