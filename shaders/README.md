# Shader Tutorials

A beginner-friendly guide to writing GPU shaders with GLSL. No prior graphics programming needed.

---

## Tutorials

| # | Topic | What You'll Learn |
|---|---|---|
| 01 | [Introduction & Mindset](01-introduction-and-mindset.md) | What shaders are, how to think about them, the 7 mental shifts |
| 02 | [GLSL Basics](02-glsl-basics.md) | Data types, vectors, swizzling, built-in functions, qualifiers |
| 03 | [Vertex Shaders](03-vertex-shaders.md) | Transforms, matrices, passing data to fragments |
| 04 | [Fragment Shaders & Patterns](04-fragment-shaders.md) | Shapes, patterns, SDFs, animation, basic lighting |
| 05 | [Tips & Tricks](05-tips-and-tricks.md) | Debugging, performance, recipes, learning path |

## Example Shaders

Ready-to-run `.frag` files in the [examples/](examples/) folder. Paste into [Shadertoy](https://shadertoy.com) or use the **glsl-canvas** VS Code extension for live preview.

| File | Description |
|---|---|
| [01-gradient.frag](examples/01-gradient.frag) | Red/green gradient — the "hello world" |
| [02-circle.frag](examples/02-circle.frag) | Pulsing blue circle with smoothstep |
| [03-checkerboard.frag](examples/03-checkerboard.frag) | Classic checkerboard with floor/mod |
| [04-wave.frag](examples/04-wave.frag) | Animated sine wave line |
| [05-rainbow.frag](examples/05-rainbow.frag) | Rainbow color cycling (one-liner trick) |
| [06-sdf-shapes.frag](examples/06-sdf-shapes.frag) | Circle + box with smooth union SDF |
| [07-glow.frag](examples/07-glow.frag) | Three orbiting glow lights |

## Quick Start

1. Go to [shadertoy.com](https://shadertoy.com) → "New Shader"
2. Replace the code with any example `.frag` file
3. Replace `u_resolution` with `iResolution` and `u_time` with `iTime` (Shadertoy's names)
4. Hit **Play**

Or install the **glsl-canvas** VS Code extension and open any `.frag` file for instant local preview.

## Cheat Sheet

```
Think PER-PIXEL, not per-frame
Coordinates: gl_FragCoord.xy / u_resolution.xy → normalize to 0–1
Center: uv -= 0.5
Aspect fix: uv.x *= u_resolution.x / u_resolution.y
Circle: smoothstep(r, r-0.01, length(uv))
Pattern: fract(uv * N) for tiling
Animate: add u_time to sin/cos arguments
Debug: output value as color → vec4(vec3(val), 1.0)
Blend: mix(colorA, colorB, t)
Hard cut: step(edge, value)
Soft cut: smoothstep(edge0, edge1, value)
```
