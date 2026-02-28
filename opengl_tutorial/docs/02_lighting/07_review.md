# Lighting — Review

Congratulations! You've completed the entire Lighting section. Let's consolidate everything we've learned, review the key concepts, discuss common pitfalls, and give you a comprehensive exercise to test your knowledge.

## Summary of the Phong Lighting Model

The Phong model approximates real-world lighting with three additive components:

```
  Final Color = Ambient + Diffuse + Specular

  ┌────────────┬───────────────────────────────────────────────────┐
  │ Component  │ What It Simulates                                │
  ├────────────┼───────────────────────────────────────────────────┤
  │ Ambient    │ Indirect light bouncing around the environment.  │
  │            │ A constant base illumination so nothing is       │
  │            │ completely black.                                │
  ├────────────┼───────────────────────────────────────────────────┤
  │ Diffuse    │ Direct light hitting a surface. Brightness       │
  │            │ depends on the angle between the surface normal  │
  │            │ and the light direction (dot product).           │
  ├────────────┼───────────────────────────────────────────────────┤
  │ Specular   │ The shiny highlight on reflective surfaces.      │
  │            │ Depends on the viewer's angle relative to the    │
  │            │ reflected light ray. Controlled by a shininess   │
  │            │ exponent.                                        │
  └────────────┴───────────────────────────────────────────────────┘
```

### The Core Equations

```glsl
// Ambient
vec3 ambient = light.ambient * material.ambient;

// Diffuse
vec3 norm     = normalize(Normal);
vec3 lightDir = normalize(lightPos - FragPos);
float diff    = max(dot(norm, lightDir), 0.0);
vec3 diffuse  = light.diffuse * diff * material.diffuse;

// Specular
vec3 viewDir    = normalize(viewPos - FragPos);
vec3 reflectDir = reflect(-lightDir, norm);
float spec      = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
vec3 specular   = light.specular * spec * material.specular;

// Final
vec3 result = ambient + diffuse + specular;
```

## Materials and Lighting Maps

### Materials Evolution

We progressed through three stages of material representation:

```
  Stage 1: Hardcoded values     Stage 2: Material struct     Stage 3: Lighting maps
  ────────────────────────      ────────────────────────     ────────────────────────
  float ambientStrength = 0.1;  struct Material {            struct Material {
  float specStrength = 0.5;         vec3 ambient;                sampler2D diffuse;
  float shininess = 32;             vec3 diffuse;                sampler2D specular;
                                    vec3 specular;               float shininess;
                                    float shininess;         };
                                };
                                                             Per-fragment material
  One color for entire object   Per-object properties        properties via textures
```

**Lighting maps** give us per-fragment control:
- **Diffuse map**: Defines the surface color at each point (a regular texture).
- **Specular map**: Defines how shiny each point is (grayscale texture).
- **Emission map** (optional): Defines self-illuminating areas.

## Light Types Comparison

| Property | Directional Light | Point Light | Spotlight |
|----------|------------------|-------------|-----------|
| **Real-world analog** | Sun, moon | Light bulb, candle | Flashlight, desk lamp |
| **Has position?** | No | Yes | Yes |
| **Has direction?** | Yes (uniform) | No (radiates all directions) | Yes (cone direction) |
| **Attenuation?** | No (infinite distance) | Yes (`1/(Kc + Kl*d + Kq*d²)`) | Yes |
| **Cutoff angle?** | No | No | Yes (inner + outer) |
| **Rays** | Parallel | Radial from point | Conical |
| **Use case** | Outdoor sun/moon | Indoor lamps, torches | Flashlights, stage lights |

```
  Visual Comparison
  ─────────────────

  Directional          Point              Spotlight
  ↓ ↓ ↓ ↓ ↓ ↓         ╱│╲                  │
  ↓ ↓ ↓ ↓ ↓ ↓        ╱ │ ╲                ╱│╲
  ↓ ↓ ↓ ↓ ↓ ↓      ←── ● ──→            ╱ │ ╲
  ↓ ↓ ↓ ↓ ↓ ↓        ╲ │ ╱             ╱  ↓  ╲
  ─────────────        ╲│╱             ───────────
  parallel rays      all directions     cone of light
```

## Architecture of the Lighting System

Here's how all the pieces fit together in the final `multiple_lights` shader:

```
  Shader Architecture
  ───────────────────

  Vertex Shader
  ┌──────────────────────────────────┐
  │ Input: aPos, aNormal, aTexCoords │
  │                                  │
  │ • FragPos = model * aPos         │
  │ • Normal  = normalMatrix * aNor  │
  │ • TexCoords = aTexCoords         │
  │ • gl_Position = MVP * aPos       │
  └──────────────┬───────────────────┘
                 │ (interpolated)
                 ▼
  Fragment Shader
  ┌──────────────────────────────────┐
  │ Uniforms:                        │
  │   Material (diffuse, specular,   │
  │            shininess)            │
  │   DirLight, PointLight[N],       │
  │   SpotLight, viewPos             │
  │                                  │
  │ ┌──────────────────────────────┐ │
  │ │ CalcDirLight()               │ │
  │ │   → ambient + diffuse + spec │──┐
  │ └──────────────────────────────┘ │ │
  │ ┌──────────────────────────────┐ │ │
  │ │ CalcPointLight() × N         │ │ │  result += each
  │ │   → amb + diff + spec + atten│──┤
  │ └──────────────────────────────┘ │ │
  │ ┌──────────────────────────────┐ │ │
  │ │ CalcSpotLight()              │ │ │
  │ │   → amb + diff + spec + att  │──┘
  │ │     + intensity              │ │
  │ └──────────────────────────────┘ │
  │                                  │
  │ FragColor = vec4(result, 1.0)    │
  └──────────────────────────────────┘
```

## Common Issues and Debugging Tips

### 1. Everything Is Black

**Possible causes:**
- Light ambient/diffuse/specular set to `(0, 0, 0)`.
- Forgot to set uniform values (they default to 0).
- Shader compilation failed (check `glGetShaderInfoLog`).
- Wrong VAO bound when drawing.

**Fix:** Start simple. Set `FragColor = vec4(1.0, 0.0, 0.0, 1.0)` to verify the geometry renders. Then add lighting back piece by piece.

### 2. Lighting Looks Wrong / Too Dark on One Side

**Possible causes:**
- Normal vectors point in the wrong direction or aren't normalized.
- Forgot `normalize(Normal)` in the fragment shader (interpolation can de-normalize).
- Model matrix has non-uniform scaling but you didn't use the normal matrix.

**Debug:** Output the normal as a color: `FragColor = vec4(Normal * 0.5 + 0.5, 1.0)`. Each face of a cube should be a distinct solid color.

### 3. Specular Highlight Is in the Wrong Place

**Possible causes:**
- `viewPos` uniform not set or set to `(0,0,0)`.
- Using `-lightDir` instead of `lightDir` (or vice versa) in `reflect()`.
- Computing `FragPos` in the wrong coordinate space.

**Debug:** Output the specular component alone: `FragColor = vec4(vec3(spec), 1.0)`.

### 4. Spotlight Doesn't Work (Everything Lit or Everything Dark)

**Possible causes:**
- Passing the cutoff angle in degrees instead of as a cosine.
- `light.direction` not normalized.
- Inner cutoff is larger than outer cutoff (they should be: inner < outer in angle, but inner > outer in cosine).

**Fix:** Remember, we compare cosines. `cos(12.5°) > cos(17.5°)`, so the `cutOff` value should be greater than `outerCutOff`.

### 5. Attenuation Too Strong or Too Weak

**Possible causes:**
- Using attenuation values for the wrong distance range.
- Scene scale doesn't match the attenuation curve (e.g., light at distance 100 with attenuation values meant for distance 20).

**Fix:** Refer to the attenuation table in Chapter 5 and choose values that match your scene's scale.

### 6. Texture Uniforms Not Working

**Possible causes:**
- Forgot to call `glActiveTexture(GL_TEXTUREn)` before binding.
- Sampler uniform not set to the correct texture unit number.
- Texture not loaded correctly (check `stbi_load` return value).

**Debug:** Set `FragColor = texture(material.diffuse, TexCoords)` to verify textures are bound correctly.

## Chapter-by-Chapter Recap

| Chapter | Key Concept | What You Built |
|---------|-------------|----------------|
| 1. Colors | RGB model, `lightColor * objectColor` | Coral cube + white lamp |
| 2. Basic Lighting | Phong model: ambient + diffuse + specular | Shaded cube with highlight |
| 3. Materials | Material/Light structs, real-world values | Gold/silver/emerald cubes |
| 4. Lighting Maps | Diffuse/specular/emission textures | Textured container with selective shine |
| 5. Light Casters | Directional, point (attenuation), spot (cutoff) | Sun, bulb, flashlight |
| 6. Multiple Lights | Combining all types, shader functions | Rich multi-light scene |

## Final Exercise: Build Your Own Lit Scene

Create a comprehensive scene that demonstrates everything from this section. Here are the requirements:

### Requirements

1. **At least 10 objects** with at least 2 different materials/textures.
2. **1 directional light** simulating the sun or moon.
3. **At least 4 point lights** with different colors, positioned strategically.
4. **1 spotlight** (flashlight or fixed stage light).
5. **Interactive controls:**
   - `WASD` + mouse for camera movement (you already have this).
   - `F` to toggle the flashlight on/off.
   - `1-4` to toggle individual point lights.
   - `+`/`-` to increase/decrease the flashlight cone angle.

### Suggested Approach

```
Step 1: Set up the scene geometry (cubes at various positions/rotations)
Step 2: Load and bind diffuse + specular maps
Step 3: Implement the multiple-lights shader from Chapter 6
Step 4: Set up the directional light (moonlight: dim blue-white)
Step 5: Add 4 point lights with distinct colors
Step 6: Add the flashlight spotlight
Step 7: Add toggle controls
Step 8: Polish — adjust colors, attenuation, and positions until it looks good
```

### Bonus Challenges

- **Day/night cycle:** Animate the directional light direction and color over time. Transition from warm sunrise to bright noon to orange sunset to dark night with moonlight.
- **Flickering torch:** Make one of the point lights flicker like a torch by rapidly varying its diffuse intensity with a random-ish function.
- **Colored shadows:** With colored point lights, objects between the light and a surface will cast "shadows" (areas that only receive other lights). This emerges naturally from the additive model.
- **Performance profiling:** Add 50 or 100 point lights. At what point does the frame rate drop? This leads into the motivation for **deferred rendering** (an advanced topic).

## What's Next

With lighting mastered, you now have the tools to create visually rich 3D scenes. The next major section is **Model Loading**, where we'll learn to:

- Load 3D models created in tools like Blender (`.obj`, `.fbx`, etc.)
- Use the Assimp library for model importing
- Apply the lighting techniques from this section to complex 3D meshes
- Render objects with multiple meshes and textures

The combination of proper lighting and detailed 3D models is what transforms a simple OpenGL demo into a convincing 3D application.

---

| [← Multiple Lights](06_multiple_lights.md) | [Next: Model Loading →](../03_model_loading/) |
|:---|---:|
