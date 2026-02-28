# Chapter 5: Projections

## 5.1 What is Projection?

A **projection** squashes a 3D world onto a 2D surface — your screen. It answers the fundamental question: *given a point in 3D space, where does it appear on a flat monitor?*

**Analogy:** Imagine holding a flashlight behind a 3D wireframe model. The shadow it casts on the wall is a projection.

```
3D World                          2D Screen

    ╱╲                              /\
   ╱  ╲         ──project──▶      /  \
  ╱ ◆  ╲                        / ◆  \
 ╱______╲                      /______\

   Cube                        Flat image of cube
```

### The Core Problem

Your game world has three dimensions (x, y, z), but your monitor is a flat rectangle with only two (x, y). Projection is the mathematical bridge between them.

There are two major types:

| Type | Far objects | Parallel lines | Use case |
|------|------------|----------------|----------|
| **Orthographic** | Same size as near | Stay parallel | 2D games, CAD, strategy |
| **Perspective** | Appear smaller | Converge to vanishing point | 3D games, first-person |

---

## 5.2 Orthographic Projection

### How It Works

In orthographic (or "parallel") projection, all projection rays are **parallel** to each other and perpendicular to the view plane. There is no foreshortening — a 1-meter box looks the same size whether it's 5 meters or 500 meters away.

```
         Eye (at infinity)
         ↓   ↓   ↓   ↓   ↓   (parallel rays)
         │   │   │   │   │
    ─────┼───┼───┼───┼───┼─────  View Plane (screen)
         │   │   │   │   │
         ■   ●       ▲   ◆     (3D objects at various depths)
```

### Use Cases

- **2D games** — the world is already flat; ortho projection is natural.
- **CAD / architectural software** — true-to-scale measurements.
- **Top-down / side-scrolling** views.
- **Isometric games** — SimCity, Diablo II, classic RTS games.
- **UI overlays / HUD rendering** — drawn without perspective.

### The Orthographic View Volume

The orthographic projection defines a **box** (rectangular parallelepiped) in view space. Everything inside the box is visible; everything outside is clipped.

```
Parameters:
  left (l), right (r)     ─ horizontal bounds
  bottom (b), top (t)     ─ vertical bounds
  near (n), far (f)       ─ depth bounds

        top ──────────────────
       /   /                /|
      /   /                / |
     /   / near           /  |
    /___/________________/   |
    |   |                |   |
    |   |    Visible     |   |  far
    |   |    Volume      |   |
    |   |                |   /
    |   |________________|  /
    |  / bottom          | /
    | /                  |/
    left ──────────── right
```

### Deriving the Orthographic Matrix

The goal is to map this box to the **Normalized Device Coordinate (NDC)** cube \([-1, 1]^3\) (OpenGL convention).

**Step 1: Translate the center of the box to the origin.**

Center = \(\left(\frac{r+l}{2}, \frac{t+b}{2}, \frac{f+n}{2}\right)\)

**Step 2: Scale to fit into \([-1, 1]\) on each axis.**

Width = \(r - l\), Height = \(t - b\), Depth = \(f - n\)

Scale factors: \(\frac{2}{r-l}\), \(\frac{2}{t-b}\), \(\frac{2}{f-n}\)

**Combined matrix (OpenGL, right-handed, looking down -Z):**

\[
P_{\text{ortho}} = \begin{bmatrix}
\frac{2}{r-l} & 0 & 0 & -\frac{r+l}{r-l} \\
0 & \frac{2}{t-b} & 0 & -\frac{t+b}{t-b} \\
0 & 0 & -\frac{2}{f-n} & -\frac{f+n}{f-n} \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

The negative sign on the Z row flips the Z axis (OpenGL looks down -Z, but NDC Z goes from -1 to 1 front-to-back).

### Step-by-Step Example

Parameters: left = -5, right = 5, bottom = -5, top = 5, near = 1, far = 100.

\[
r - l = 10, \quad t - b = 10, \quad f - n = 99
\]

\[
P_{\text{ortho}} = \begin{bmatrix}
0.2 & 0 & 0 & 0 \\
0 & 0.2 & 0 & 0 \\
0 & 0 & -0.0202 & -1.0202 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

Project a point \( (3, 4, -50) \) (50 units in front of camera):

\[
\begin{bmatrix} 0.2 & 0 & 0 & 0 \\ 0 & 0.2 & 0 & 0 \\ 0 & 0 & -0.0202 & -1.0202 \\ 0 & 0 & 0 & 1 \end{bmatrix}
\begin{bmatrix} 3 \\ 4 \\ -50 \\ 1 \end{bmatrix}
=
\begin{bmatrix} 0.6 \\ 0.8 \\ -0.0101 \\ 1 \end{bmatrix}
\]

NDC coordinates: \( (0.6, 0.8, -0.0101) \) — all within \([-1, 1]\), so the point is visible.

```
Pseudocode:

function orthoMatrix(left, right, bottom, top, near, far):
    m = Matrix4.Zero()
    m[0][0] = 2 / (right - left)
    m[1][1] = 2 / (top - bottom)
    m[2][2] = -2 / (far - near)
    m[0][3] = -(right + left) / (right - left)
    m[1][3] = -(top + bottom) / (top - bottom)
    m[2][3] = -(far + near) / (far - near)
    m[3][3] = 1
    return m
```

---

## 5.3 Perspective Projection

### How It Works

In perspective projection, objects farther from the camera appear **smaller**, just like in real life. Projection rays converge at the **eye point** (camera position).

```
                     ● Far object (appears small)
                    /|
                   / |
                  /  |
Eye ────────────●───── Near plane (screen)
                  \  |
                   \ |
                    \|
                     ● Another far object
```

This creates the illusion of depth that makes 3D games feel immersive.

### The Viewing Frustum

The visible region is a **frustum** — a truncated pyramid. Only geometry inside this shape gets rendered.

```
Side view:

           near                          far
            │                             │
     ┌──────┤                             │
    /│      │                             │
   / │      │         Visible Volume      │
  /  │      │        (frustum shape)      │
 /   │      │                             │
Eye  │      │                             │
 \   │      │                             │
  \  │      │                             │
   \ │      │                             │
    \│      │                             │
     └──────┤                             │
            │                             │

Top view:

              near ───────┐
             /             \
            /               \
    Eye ◄──                  \
            \               /
             \             /
              far ────────┘
```

### Field of View (FOV)

**FOV** is the angle of the camera's cone of vision.

- **Vertical FOV (fovY)**: How much you see from top to bottom. Common values: 60°–90°.
- **Horizontal FOV**: Derived from vertical FOV and aspect ratio.

```
                ┌─────────────┐
               /    fovY       \
              /    ╱    ╲       \
             /    ╱  Eye  ╲      \
            /    ╱──┤θ/2├──╲      \
           /    ╱    near    ╲     \
          └────╱──────────────╲────┘
```

A wider FOV = see more of the world but with more distortion at edges (fisheye effect).

### Aspect Ratio

\[
\text{aspect} = \frac{\text{width}}{\text{height}}
\]

A 1920×1080 screen: aspect = 16/9 ≈ 1.778.

The aspect ratio ensures the image doesn't appear stretched.

### Near and Far Clipping Planes

| Plane | What it does | Too close/far? |
|-------|-------------|-----------------|
| **Near** | Closest distance rendered | Too small → objects clip through camera |
| **Far** | Farthest distance rendered | Too large → z-fighting artifacts |

The ratio \( \frac{f}{n} \) matters enormously for depth precision. A ratio of 1000:1 or less is ideal. Avoid near = 0.001, far = 100000.

### Deriving the Perspective Matrix

We want to map the frustum into the NDC cube \([-1, 1]^3\).

**Key insight:** A point at \( (x, y, z) \) in view space projects to screen position:

\[
x_{\text{proj}} = \frac{x}{-z} \cdot \frac{n}{\text{...}}, \quad y_{\text{proj}} = \frac{y}{-z} \cdot \frac{n}{\text{...}}
\]

The division by \( -z \) is what makes far things small. We can't do division in a matrix multiply, so we use a trick: store \( -z \) in the \( w \) component and divide later (the **perspective divide**).

**Full perspective matrix (OpenGL, symmetric frustum, vertical FOV):**

Let \( f_y = \frac{1}{\tan(\text{fovY}/2)} \) and aspect = width/height.

\[
P_{\text{persp}} = \begin{bmatrix}
\frac{f_y}{\text{aspect}} & 0 & 0 & 0 \\
0 & f_y & 0 & 0 \\
0 & 0 & \frac{n+f}{n-f} & \frac{2nf}{n-f} \\
0 & 0 & -1 & 0
\end{bmatrix}
\]

Where \( n \) = near, \( f \) = far.

**How it works row by row:**

| Row | Purpose |
|-----|---------|
| Row 0 | Scales X by FOV and aspect ratio |
| Row 1 | Scales Y by FOV |
| Row 2 | Maps Z to \([-1, 1]\) non-linearly |
| Row 3 | Copies \(-z\) into \(w\) for perspective divide |

### The Perspective Divide

After multiplying by the projection matrix, the result is in **clip space** with a non-unity \( w \):

\[
\begin{bmatrix} x_c \\ y_c \\ z_c \\ w_c \end{bmatrix} = P \begin{bmatrix} x_e \\ y_e \\ z_e \\ 1 \end{bmatrix}
\]

NDC is obtained by dividing by \( w_c \):

\[
x_{\text{ndc}} = \frac{x_c}{w_c}, \quad y_{\text{ndc}} = \frac{y_c}{w_c}, \quad z_{\text{ndc}} = \frac{z_c}{w_c}
\]

Since \( w_c = -z_e \), this achieves the "far things look smaller" effect.

### Step-by-Step Example

**Setup:** fovY = 90°, aspect = 16/9 ≈ 1.778, near = 1, far = 100.

\( f_y = 1/\tan(45°) = 1.0 \)

\[
P = \begin{bmatrix}
0.5625 & 0 & 0 & 0 \\
0 & 1.0 & 0 & 0 \\
0 & 0 & -1.0202 & -2.0202 \\
0 & 0 & -1 & 0
\end{bmatrix}
\]

**Project point** \( (2, 3, -10) \) (10 units in front of camera):

\[
\begin{bmatrix} 0.5625 & 0 & 0 & 0 \\ 0 & 1 & 0 & 0 \\ 0 & 0 & -1.0202 & -2.0202 \\ 0 & 0 & -1 & 0 \end{bmatrix}
\begin{bmatrix} 2 \\ 3 \\ -10 \\ 1 \end{bmatrix}
=
\begin{bmatrix} 1.125 \\ 3.0 \\ 8.182 \\ 10.0 \end{bmatrix}
\]

**Perspective divide** (divide by w = 10):

\[
\text{NDC} = \left(\frac{1.125}{10},\; \frac{3.0}{10},\; \frac{8.182}{10}\right) = (0.1125,\; 0.3,\; 0.8182)
\]

All values in \([-1, 1]\), so the point is on screen. It appears slightly right of center (x = 0.11) and above center (y = 0.3).

```
Pseudocode:

function perspectiveMatrix(fovY, aspect, near, far):
    fy = 1.0 / tan(fovY / 2)
    m = Matrix4.Zero()
    m[0][0] = fy / aspect
    m[1][1] = fy
    m[2][2] = (near + far) / (near - far)
    m[2][3] = (2 * near * far) / (near - far)
    m[3][2] = -1
    return m

function projectPoint(point, projMatrix):
    clip = projMatrix * vec4(point, 1.0)
    ndc = vec3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w)
    return ndc
```

---

## 5.4 Normalized Device Coordinates (NDC)

After projection and perspective divide, all visible geometry lives in a standardized cube called **Normalized Device Coordinates**.

| API | X range | Y range | Z range |
|-----|---------|---------|---------|
| **OpenGL** | \([-1, 1]\) | \([-1, 1]\) | \([-1, 1]\) |
| **DirectX** | \([-1, 1]\) | \([-1, 1]\) | \([0, 1]\) |
| **Vulkan** | \([-1, 1]\) | \([-1, 1]\) | \([0, 1]\) |

### Why NDC Exists

NDC provides a **resolution-independent** coordinate system. Whether your screen is 640×480 or 3840×2160, the projected geometry is in the same \([-1, 1]\) range. The viewport transform (next section) maps these to actual pixels.

```
NDC Space (OpenGL):

  (-1, 1) ─────────────── (1, 1)
     │                       │
     │       (0, 0)          │
     │         ●             │
     │       center          │
     │                       │
  (-1,-1) ─────────────── (1,-1)
```

Any point outside this range is **clipped** (discarded or trimmed).

---

## 5.5 Viewport Transform

The **viewport transform** maps NDC coordinates to **screen pixel coordinates**.

### Formula

Given a viewport with origin \( (x_0, y_0) \), width \( w \), and height \( h \):

\[
x_{\text{screen}} = \frac{x_{\text{ndc}} + 1}{2} \cdot w + x_0
\]
\[
y_{\text{screen}} = \frac{y_{\text{ndc}} + 1}{2} \cdot h + y_0
\]

This maps \([-1, 1]\) → \([0, \text{width}]\) and \([-1, 1]\) → \([0, \text{height}]\).

### Example

NDC point: \( (0.1125, 0.3) \). Screen resolution: 1920 × 1080. Viewport at (0, 0).

\[
x_{\text{screen}} = \frac{0.1125 + 1}{2} \times 1920 = \frac{1.1125}{2} \times 1920 = 0.55625 \times 1920 = 1068
\]
\[
y_{\text{screen}} = \frac{0.3 + 1}{2} \times 1080 = \frac{1.3}{2} \times 1080 = 0.65 \times 1080 = 702
\]

The 3D point \( (2, 3, -10) \) appears at **pixel (1068, 702)** on a 1080p screen.

```
Pseudocode:

function ndcToScreen(ndc, screenWidth, screenHeight):
    sx = (ndc.x + 1) / 2 * screenWidth
    sy = (ndc.y + 1) / 2 * screenHeight
    return (sx, sy)
```

---

## 5.6 Depth Buffer (Z-Buffer)

### How Depth is Stored

After projection, the Z component of NDC represents **depth**. The GPU stores this in a **depth buffer** (or Z-buffer) — a screen-sized image where each pixel holds the depth of the closest fragment drawn there so far.

When a new fragment arrives, the GPU compares its depth to what's already in the buffer:

- **Closer?** → Draw it, update the buffer.
- **Farther?** → Discard it (something is in front).

```
Scene (side view):            Depth Buffer (per pixel):

  Near                Far
   │    ■      ●       │     Pixel at ● column stores ●'s depth
   │    │      │       │     Pixel at ■ column stores ■'s depth
   │    │      │       │
   camera               
```

### Non-Linear Depth Distribution

In perspective projection, depth values are **not** distributed evenly:

\[
z_{\text{ndc}} = \frac{f + n}{f - n} + \frac{1}{z_e} \cdot \frac{2fn}{f - n}
\]

The \( 1/z_e \) term means depth precision is **concentrated near the near plane** and sparse near the far plane.

```
Depth precision distribution:

Near ├████████████████│         │              │ Far
     ├─── lots of ────┤  some   │   very little │
     │   precision     │         │   precision   │
```

### Z-Fighting

When two surfaces are nearly coplanar and far from the camera, they compete for the same depth buffer value. This causes **z-fighting** — flickering pixels that alternate between the two surfaces.

```
Z-fighting appearance:

  ┌──────────────────────┐
  │  ████░░░████░░░████  │    ← Flickering checkerboard
  │  ░░░████░░░████░░░█  │       pattern where two surfaces
  │  ████░░░████░░░████  │       overlap at similar depth
  └──────────────────────┘
```

**How to mitigate z-fighting:**

1. **Keep the near plane as far as possible** (0.1 instead of 0.001).
2. **Keep the far/near ratio small** (< 1000 ideally).
3. Use **reversed Z** (map near to 1, far to 0) with a floating-point depth buffer for much better precision distribution.
4. Add a small **depth bias** (polygon offset) for overlapping coplanar geometry like decals.

---

## 5.7 Comparing Orthographic vs Perspective

| Feature | Orthographic | Perspective |
|---------|-------------|-------------|
| **Parallel lines** | Stay parallel | Converge at vanishing point |
| **Size vs distance** | Constant | Farther = smaller |
| **Depth cue** | None | Natural depth perception |
| **Matrix bottom row** | \([0, 0, 0, 1]\) | \([0, 0, -1, 0]\) |
| **W after multiply** | 1 (no divide needed) | \(-z_e\) (divide needed) |
| **Best for** | 2D, CAD, UI, isometric | 3D worlds, first-person, immersive |

```
Orthographic view:                 Perspective view:

  ┌──┐  ┌──┐  ┌──┐                  ┌──┐
  │  │  │  │  │  │                ┌─┐│  │┌─┐
  └──┘  └──┘  └──┘               └─┘└──┘└─┘
  near  mid   far                near mid  far

  All boxes same size             Far boxes smaller
```

### When to Use Which

- **Perspective** for any game where 3D immersion matters (FPS, third-person, racing).
- **Orthographic** for 2D games, top-down views, minimaps, UI rendering, level editors, and shadow maps.

---

## 5.8 Isometric Projection

**Isometric projection** is a special case of orthographic projection where the camera is angled so that all three axes are equally foreshortened. This gives a 3D appearance without perspective distortion.

### The Isometric Angle

The camera looks along the direction \( (1, 1, 1) \) (normalized). In practice, the camera is rotated:

1. **45°** around the Y axis.
2. **~35.264°** (arctan(1/√2)) around the X axis.

```
Isometric view:

         /\
        /  \
       / ◆  \
      /______\
     /\      /\
    /  \    /  \
   /    \  /    \
  /______\/______\

Classic isometric tile grid — 2:1 diamond shape.
```

### Setting Up an Isometric Camera

```
Pseudocode:

function setupIsometricCamera():
    viewMatrix = Matrix4.Identity()
    viewMatrix = viewMatrix * Matrix4.RotationX(35.264°)
    viewMatrix = viewMatrix * Matrix4.RotationY(45°)

    // Use orthographic projection (no perspective)
    projMatrix = orthoMatrix(-10, 10, -10, 10, 0.1, 100)

    return projMatrix * viewMatrix
```

Games using isometric projection: classic *Diablo*, *StarCraft*, *SimCity 2000*, *Transistor*.

---

## 5.9 Full Pipeline Example

Let's trace a single 3D point through the entire rendering pipeline, from its local model space to a screen pixel.

### Setup

- **Model point**: \( P_{\text{local}} = (1, 2, 0) \) — a vertex on a character model.
- **Model transform (World)**: Translate by \( (10, 0, -20) \).
- **View (Camera)**: Camera at \( (0, 5, 0) \), looking down the -Z axis (for simplicity, view = translation by \( (0, -5, 0) \)).
- **Projection**: Perspective, fovY = 90°, aspect = 16/9, near = 1, far = 100.
- **Screen**: 1920 × 1080.

### Step 1: Model → World

\[
P_{\text{world}} = M_{\text{model}} \cdot P_{\text{local}}
\]

\[
\begin{bmatrix} 1&0&0&10 \\ 0&1&0&0 \\ 0&0&1&-20 \\ 0&0&0&1 \end{bmatrix}
\begin{bmatrix} 1\\2\\0\\1 \end{bmatrix}
=
\begin{bmatrix} 11\\2\\-20\\1 \end{bmatrix}
\]

World position: \( (11, 2, -20) \).

### Step 2: World → View (Camera Space)

\[
P_{\text{view}} = M_{\text{view}} \cdot P_{\text{world}}
\]

\[
\begin{bmatrix} 1&0&0&0 \\ 0&1&0&-5 \\ 0&0&1&0 \\ 0&0&0&1 \end{bmatrix}
\begin{bmatrix} 11\\2\\-20\\1 \end{bmatrix}
=
\begin{bmatrix} 11\\-3\\-20\\1 \end{bmatrix}
\]

View-space position: \( (11, -3, -20) \) — the point is 20 units in front of the camera.

### Step 3: View → Clip Space (Projection)

Using our perspective matrix (fovY=90°, aspect=1.778):

\[
P_{\text{clip}} = M_{\text{proj}} \cdot P_{\text{view}}
\]

\[
\begin{bmatrix} 0.5625&0&0&0 \\ 0&1&0&0 \\ 0&0&-1.0202&-2.0202 \\ 0&0&-1&0 \end{bmatrix}
\begin{bmatrix} 11\\-3\\-20\\1 \end{bmatrix}
=
\begin{bmatrix} 6.1875\\-3\\18.384\\20 \end{bmatrix}
\]

### Step 4: Clip → NDC (Perspective Divide)

\[
P_{\text{ndc}} = \left(\frac{6.1875}{20},\; \frac{-3}{20},\; \frac{18.384}{20}\right) = (0.309,\; -0.15,\; 0.919)
\]

All components in \([-1, 1]\) — the point is visible!

- \( x = 0.309 \): slightly right of center.
- \( y = -0.15 \): slightly below center.
- \( z = 0.919 \): fairly far (close to the far plane in NDC).

### Step 5: NDC → Screen Pixels (Viewport Transform)

\[
x_{\text{screen}} = \frac{0.309 + 1}{2} \times 1920 = 0.6545 \times 1920 \approx 1257
\]
\[
y_{\text{screen}} = \frac{-0.15 + 1}{2} \times 1080 = 0.425 \times 1080 \approx 459
\]

**Final pixel position: (1257, 459)** on a 1920×1080 display.

```
Pseudocode (full pipeline):

function renderVertex(localPos, modelMatrix, viewMatrix, projMatrix, screenW, screenH):
    // Model → World
    world = modelMatrix * vec4(localPos, 1.0)

    // World → View
    view = viewMatrix * world

    // View → Clip
    clip = projMatrix * view

    // Clip → NDC (perspective divide)
    ndc = vec3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w)

    // NDC → Screen
    screenX = (ndc.x + 1) / 2 * screenW
    screenY = (ndc.y + 1) / 2 * screenH

    return (screenX, screenY, ndc.z)  // ndc.z goes to depth buffer
```

### The Complete Pipeline Diagram

```
┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐     ┌──────────┐
│  Model   │     │  World   │     │  View    │     │  Clip    │     │  NDC     │
│  Space   │────▶│  Space   │────▶│  Space   │────▶│  Space   │────▶│  Space   │
│ (local)  │ M   │ (global) │ V   │ (camera) │ P   │ (w≠1)   │ ÷w  │ [-1,1]  │
└──────────┘     └──────────┘     └──────────┘     └──────────┘     └──────────┘
                                                                          │
                                                                     Viewport
                                                                          │
                                                                          ▼
                                                                    ┌──────────┐
                                                                    │  Screen  │
                                                                    │  Pixels  │
                                                                    └──────────┘
```

### Summary of Transforms

| Stage | Transform | Matrix | Purpose |
|-------|-----------|--------|---------|
| Model → World | Model matrix | \( M \) | Position object in world |
| World → View | View matrix | \( V \) | Reframe relative to camera |
| View → Clip | Projection matrix | \( P \) | Apply perspective / ortho |
| Clip → NDC | Perspective divide | \( \div w \) | Normalize coordinates |
| NDC → Screen | Viewport transform | Linear map | Map to pixel coordinates |

The combined Model-View-Projection matrix (\( \text{MVP} = P \cdot V \cdot M \)) is often precomputed and uploaded to the GPU once per object, since it transforms vertices directly from local space to clip space in a single matrix multiply.

---

## 5.10 Quick Reference

### Orthographic Matrix (OpenGL)

\[
\begin{bmatrix}
\frac{2}{r-l} & 0 & 0 & -\frac{r+l}{r-l} \\
0 & \frac{2}{t-b} & 0 & -\frac{t+b}{t-b} \\
0 & 0 & -\frac{2}{f-n} & -\frac{f+n}{f-n} \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

### Perspective Matrix (OpenGL, symmetric)

\[
\begin{bmatrix}
\frac{f_y}{a} & 0 & 0 & 0 \\
0 & f_y & 0 & 0 \\
0 & 0 & \frac{n+f}{n-f} & \frac{2nf}{n-f} \\
0 & 0 & -1 & 0
\end{bmatrix}
\]

Where \( f_y = 1/\tan(\text{fovY}/2) \) and \( a = \text{aspect ratio} \).

### Viewport Transform

\[
x_s = \frac{x_{\text{ndc}}+1}{2} \cdot w + x_0, \quad y_s = \frac{y_{\text{ndc}}+1}{2} \cdot h + y_0
\]

### Key Takeaways

- **Orthographic** = parallel rays, no size change with distance, box-shaped volume.
- **Perspective** = converging rays, distant objects shrink, frustum-shaped volume.
- The **perspective divide** (\( \div w \)) is the magic that makes far things small.
- **Depth precision** is non-linear in perspective — keep the near/far ratio tight.
- The **MVP matrix** takes a vertex from local space all the way to clip space.
- **Isometric** is just orthographic with the camera tilted at the "magic angle."
