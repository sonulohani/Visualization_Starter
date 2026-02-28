# Chapter 12: Camera Math

> *"There is no camera. There is only the illusion of one — created by moving the
> entire world in the opposite direction."*

In a 3D engine, the "camera" is a mathematical construct: a coordinate transformation
that defines **what** you see and **how** it's projected onto your screen. This chapter
covers every piece of math behind it.

---

## 12.1 What the Camera Really Is

There is no physical camera object in your scene. What we call a "camera" is a pair of
matrices:

1. **View matrix** — transforms the world so the camera is at the origin, looking down
   an axis.
2. **Projection matrix** — maps the 3D view into the 2D screen (covered in earlier
   chapters on projection).

The key insight:

> **Moving the camera left is the same as moving the entire world right.**

```
  "Camera moves left"            "World moves right"

   World stays:    Cam ←          World →      Cam stays:
   ┌───┐                                       ┌───┐
   │ A │    👁→                   👁             │ A │  →
   └───┘                                       └───┘

  Same result on screen!
```

This means the view matrix is the **inverse** of the camera's own world transform.

---

## 12.2 The View Matrix

If the camera has a world-space transform matrix (its position and orientation in the
scene), the view matrix is simply:

\[
\mathbf{V} = \mathbf{M}_{camera}^{-1}
\]

Applying the view matrix to any point transforms it from **world space** to
**camera/view space** — a coordinate system where:

- The camera is at the origin \( (0, 0, 0) \).
- The camera looks down the negative-Z axis (OpenGL convention) or positive-Z (DirectX).
- "Up" is the Y axis, "right" is the X axis.

```
  World space:                  View/camera space:

       ↑ Y                          ↑ Y
       |    📦                       |
       | /                           | /
  ─────📷──── X                ──────O──── X
      /                             /|
     Z                             Z  (camera at origin,
                                       box is now relative to camera)
```

### Why the inverse?

The camera's world matrix says "camera is here, facing this way." We need the opposite:
"transform everything so the camera's position becomes the origin." Inverse does exactly
that.

For a rotation-translation matrix (no scale), the inverse is cheap:

\[
\mathbf{M} = \begin{bmatrix} R & \mathbf{t} \\ 0 & 1 \end{bmatrix}
\quad \Rightarrow \quad
\mathbf{M}^{-1} = \begin{bmatrix} R^T & -R^T \mathbf{t} \\ 0 & 1 \end{bmatrix}
\]

The rotation part just transposes; no general matrix inversion needed.

---

## 12.3 The Look-At Matrix

The most common way to build a view matrix: specify **where** the camera is, **what**
it's looking at, and **which way is up**.

### Inputs

- \( \mathbf{eye} \): camera position
- \( \mathbf{target} \): point the camera looks at
- \( \mathbf{worldUp} \): the world's up direction, usually \( (0, 1, 0) \)

### Step-by-step construction

**Step 1**: Compute the forward vector (from camera toward target):

\[
\mathbf{f} = \text{normalize}(\mathbf{target} - \mathbf{eye})
\]

**Step 2**: Compute the right vector (perpendicular to forward and world up):

\[
\mathbf{r} = \text{normalize}(\mathbf{f} \times \mathbf{worldUp})
\]

**Step 3**: Compute the true up vector (perpendicular to right and forward):

\[
\mathbf{u} = \mathbf{r} \times \mathbf{f}
\]

**Step 4**: Assemble the view matrix (OpenGL convention, camera looks down -Z):

\[
\mathbf{V} = \begin{bmatrix}
r_x & r_y & r_z & -\mathbf{r} \cdot \mathbf{eye} \\
u_x & u_y & u_z & -\mathbf{u} \cdot \mathbf{eye} \\
-f_x & -f_y & -f_z & \mathbf{f} \cdot \mathbf{eye} \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

The top-left 3x3 is the transposed rotation (camera axes as rows). The rightmost column
handles translation.

### Pseudocode

```
function lookAt(eye, target, worldUp):
    f = normalize(target - eye)         // forward
    r = normalize(cross(f, worldUp))    // right
    u = cross(r, f)                     // up (recomputed to be orthogonal)

    return mat4(
         r.x,   r.y,   r.z,  -dot(r, eye),
         u.x,   u.y,   u.z,  -dot(u, eye),
        -f.x,  -f.y,  -f.z,   dot(f, eye),
         0,     0,     0,      1
    )
```

### Example: camera at (0, 5, 10) looking at the origin

```
eye    = (0, 5, 10)
target = (0, 0, 0)
worldUp = (0, 1, 0)

f = normalize((0,0,0) - (0,5,10)) = normalize((0,-5,-10))
  = (0, -0.4472, -0.8944)

r = normalize(f × worldUp)
  = normalize((-0.4472)(0) - (-0.8944)(0),
              (-0.8944)(0) - (0)(0),
              (0)(0) - (-0.4472)(0))

  Hmm, let's compute carefully:
  f × (0,1,0) = (f.y*0 - f.z*1, f.z*0 - f.x*0, f.x*1 - f.y*0)
               = (-(-0.8944), 0, 0 - (-0.4472))
    Wait — let me use the standard formula:
  f × worldUp = (f.y*up.z - f.z*up.y,  f.z*up.x - f.x*up.z,  f.x*up.y - f.y*up.x)
               = (-0.4472*0 - (-0.8944)*1,  (-0.8944)*0 - 0*0,  0*1 - (-0.4472)*0)
               = (0.8944, 0, 0)

  r = normalize((0.8944, 0, 0)) = (1, 0, 0)

u = r × f = (0*(-0.8944) - 0*(-0.4472),
             0*0 - 1*(-0.8944),
             1*(-0.4472) - 0*0)
           = (0, 0.8944, -0.4472)

View matrix:
┌                                              ┐
│  1       0        0       -dot(r, eye)       │     r = (1,0,0), eye = (0,5,10)
│  0       0.8944  -0.4472  -dot(u, eye)       │     → -dot(r,eye) = 0
│  0       0.4472   0.8944   dot(f, eye)       │     → -dot(u,eye) = -(0+4.472-4.472) = 0
│  0       0        0        1                 │     →  dot(f,eye) = 0-2.236-8.944 = -11.18
└                                              ┘

V = | 1   0       0       0      |
    | 0   0.8944 -0.4472  0      |
    | 0   0.4472  0.8944 -11.18  |
    | 0   0       0       1      |
```

Multiplying any world point by this matrix expresses it in camera space, where the
camera is at the origin looking down -Z.

---

## 12.4 Camera Types

### First-person camera (FPS)

The camera **is** the player's eyes. Controlled by mouse look (yaw/pitch) and WASD
movement. The simplest and most common camera type.

### Third-person camera (orbit)

Orbits around a character or target point. The player sees their avatar. Controlled by
yaw, pitch, and zoom distance.

```
        . · * Camera
      ·    /
    ·    / distance
  ·   /
  🧑 (target)
```

### Top-down / isometric

Camera is above and slightly angled. Common in strategy games and ARPGs (Diablo,
StarCraft). Often uses orthographic projection.

```
       📷
       ↓ (looking down at an angle)
    ┌──────────┐
    │  game    │
    │  world   │
    └──────────┘
```

### Fly-through camera (6DOF)

Full freedom: pitch, yaw, **and roll**, plus movement in any direction. Used in space
games and level editors. Uses a full orientation quaternion or matrix — no gimbal lock
issues.

### Side-scroller camera (2D)

Follows the player along one axis. Often adds **parallax** layers (backgrounds scroll
slower than foreground) for depth.

```
  Background layer (slow scroll):   ░░░ mountains ░░░
  Midground layer (medium):         ▒▒▒ trees ▒▒▒
  Foreground layer (1:1):           ███ player ███
```

---

## 12.5 FPS Camera Implementation

### The state

An FPS camera needs just a few variables:

```
struct FPSCamera:
    position: vec3
    yaw:      float   // rotation around Y axis (left/right), in radians
    pitch:    float   // rotation around X axis (up/down), in radians
```

### Computing the forward vector

From yaw and pitch, we derive the direction the camera is looking:

\[
\text{forward} = \begin{pmatrix}
\cos(\text{pitch}) \cdot \sin(\text{yaw}) \\
\sin(\text{pitch}) \\
-\cos(\text{pitch}) \cdot \cos(\text{yaw})
\end{pmatrix}
\]

(The negative Z is because in OpenGL convention, the camera looks down -Z.)

```
  Top-down view (Y is up, looking from above):

              -Z (forward at yaw=0)
               ↑
               |
       -X ─────┼───── +X
               |
               ↓
              +Z

  yaw rotates around Y:
    yaw = 0    → looking along -Z
    yaw = π/2  → looking along +X
    yaw = π    → looking along +Z
```

### Mouse input

```
function handleMouseInput(camera, deltaX, deltaY, sensitivity):
    camera.yaw   += deltaX * sensitivity
    camera.pitch -= deltaY * sensitivity   // inverted: mouse up → look up

    // Clamp pitch to prevent flipping
    maxPitch = radians(89.0)
    camera.pitch = clamp(camera.pitch, -maxPitch, maxPitch)
```

Why clamp at 89 instead of 90? At exactly 90 degrees, the forward vector becomes
parallel to the up vector, and the cross product used to compute the right vector
degenerates (produces a zero-length vector). Clamping just before 90 avoids this.

### Movement

```
function handleMovement(camera, input, speed, dt):
    forward = computeForward(camera.yaw, camera.pitch)
    right   = normalize(cross(forward, vec3(0, 1, 0)))

    moveDir = vec3(0, 0, 0)
    if input.forward:  moveDir += forward
    if input.backward: moveDir -= forward
    if input.left:     moveDir -= right
    if input.right:    moveDir += right

    if length(moveDir) > 0:
        moveDir = normalize(moveDir)

    camera.position += moveDir * speed * dt
```

### Building the view matrix

```
function getViewMatrix(camera):
    forward = computeForward(camera.yaw, camera.pitch)
    target  = camera.position + forward
    return lookAt(camera.position, target, vec3(0, 1, 0))
```

### Complete FPS camera pseudocode

```
struct FPSCamera:
    position  = vec3(0, 1.7, 0)  // eye height
    yaw       = 0.0
    pitch     = 0.0
    speed     = 5.0              // m/s
    sensitivity = 0.002          // radians per pixel

function update(camera, mouseDeltas, input, dt):
    // Mouse look
    camera.yaw   += mouseDeltas.x * camera.sensitivity
    camera.pitch -= mouseDeltas.y * camera.sensitivity
    camera.pitch  = clamp(camera.pitch, radians(-89), radians(89))

    // Movement
    fwd   = computeForward(camera.yaw, camera.pitch)
    right = normalize(cross(fwd, vec3(0,1,0)))
    move  = vec3(0)
    if input.W: move += fwd
    if input.S: move -= fwd
    if input.A: move -= right
    if input.D: move += right
    if length(move) > 0:
        move = normalize(move)
    camera.position += move * camera.speed * dt

function computeForward(yaw, pitch):
    return vec3(
        cos(pitch) * sin(yaw),
        sin(pitch),
       -cos(pitch) * cos(yaw)
    )

function viewMatrix(camera):
    target = camera.position + computeForward(camera.yaw, camera.pitch)
    return lookAt(camera.position, target, vec3(0, 1, 0))
```

---

## 12.6 Third-Person (Orbit) Camera

### Spherical coordinates

The orbit camera is positioned using **spherical coordinates** around a target:

```
                  Y (up)
                  ↑
                  |    . Camera
                  |  ╱
                  |╱  distance (r)
   Target ────────O─────── X
                ╱ |
              ╱   |
            Z     |
                  pitch (elevation angle from horizontal)

  yaw = rotation around Y axis
  pitch = elevation angle
  distance = radius
```

### Converting spherical to Cartesian

\[
\text{cameraPos} = \text{target} + \begin{pmatrix}
r \cdot \cos(\text{pitch}) \cdot \sin(\text{yaw}) \\
r \cdot \sin(\text{pitch}) \\
r \cdot \cos(\text{pitch}) \cdot \cos(\text{yaw})
\end{pmatrix}
\]

The camera always **looks at the target**, so the view matrix is:

```
viewMatrix = lookAt(cameraPos, target, worldUp)
```

### Smooth follow

When the target moves (a character running), the camera shouldn't snap instantly.
**Lerp** (linear interpolation) or **smooth damp** creates fluid following:

```
function smoothFollow(current, target, smoothTime, dt):
    // Exponential smoothing (simple and effective)
    t = 1.0 - pow(0.001, dt / smoothTime)
    return lerp(current, target, t)
```

`smoothTime` controls how quickly the camera catches up. 0.1 = snappy, 0.5 = lazy.

### Zoom (scroll wheel)

```
function handleZoom(camera, scrollDelta, minDist, maxDist):
    camera.distance -= scrollDelta * zoomSpeed
    camera.distance = clamp(camera.distance, minDist, maxDist)
```

### Collision avoidance

The camera can clip through walls. A common fix: cast a ray from the target to the
desired camera position. If it hits geometry, move the camera to the hit point.

```
function avoidCollision(target, desiredPos, minDist):
    dir = desiredPos - target
    dist = length(dir)
    hit = raycast(target, normalize(dir), dist)
    if hit:
        return hit.point - normalize(dir) * 0.1   // small offset from wall
    return desiredPos
```

### Complete orbit camera pseudocode

```
struct OrbitCamera:
    target    = vec3(0)
    yaw       = 0.0
    pitch     = radians(20)   // slightly elevated
    distance  = 10.0
    smoothTime = 0.2

function update(camera, playerPos, mouseDeltas, scrollDelta, dt):
    // Orbit controls
    camera.yaw   += mouseDeltas.x * sensitivity
    camera.pitch -= mouseDeltas.y * sensitivity
    camera.pitch  = clamp(camera.pitch, radians(-80), radians(80))

    // Zoom
    camera.distance -= scrollDelta * zoomSpeed
    camera.distance  = clamp(camera.distance, 2.0, 50.0)

    // Smooth follow target
    camera.target = smoothFollow(camera.target, playerPos, camera.smoothTime, dt)

    // Compute camera position from spherical coords
    offset = vec3(
        camera.distance * cos(camera.pitch) * sin(camera.yaw),
        camera.distance * sin(camera.pitch),
        camera.distance * cos(camera.pitch) * cos(camera.yaw)
    )
    desiredPos = camera.target + offset
    finalPos   = avoidCollision(camera.target, desiredPos, 1.0)

    return lookAt(finalPos, camera.target, vec3(0, 1, 0))
```

---

## 12.7 The View Frustum

### What it is

The **view frustum** is the volume of space visible to the camera — a truncated pyramid
(for perspective projection) bounded by six planes.

```
         Near plane
          ┌───┐
         ╱     ╲
        ╱       ╲
       ╱         ╲
      ╱           ╲
     ╱    Visible   ╲
    ╱     Volume     ╲
   ╱                   ╲
  └─────────────────────┘
         Far plane

  Side view:

  Eye ─── near ═══════════ far
           │                 │
           │   frustum       │
           │                 │
```

### The six planes

| Plane    | Clips geometry that is...         |
|----------|-----------------------------------|
| **Near** | Too close to the camera           |
| **Far**  | Too far from the camera           |
| **Left** | Off the left edge of the screen   |
| **Right**| Off the right edge of the screen  |
| **Top**  | Above the top of the screen       |
| **Bottom** | Below the bottom of the screen  |

Each plane is defined by a normal vector and a distance from the origin:

\[
\mathbf{n} \cdot \mathbf{p} + d = 0
\]

A point is **inside** the frustum if it's on the positive (inner) side of all six planes.

### Extracting frustum planes from the VP matrix

Given the combined View-Projection matrix \( \mathbf{M} \) (rows \( m_0, m_1, m_2, m_3 \)):

\[
\text{Left:}   \quad m_3 + m_0
\]
\[
\text{Right:}  \quad m_3 - m_0
\]
\[
\text{Bottom:} \quad m_3 + m_1
\]
\[
\text{Top:}    \quad m_3 - m_1
\]
\[
\text{Near:}   \quad m_3 + m_2
\]
\[
\text{Far:}    \quad m_3 - m_2
\]

Each result gives \( (A, B, C, D) \) where the plane equation is \( Ax + By + Cz + D = 0 \).
Normalize by dividing by \( \sqrt{A^2 + B^2 + C^2} \).

```
function extractFrustumPlanes(VP):
    planes = array of 6 Planes

    // VP is a 4x4 matrix; m[row] gives a vec4
    planes[LEFT]   = VP.row(3) + VP.row(0)
    planes[RIGHT]  = VP.row(3) - VP.row(0)
    planes[BOTTOM] = VP.row(3) + VP.row(1)
    planes[TOP]    = VP.row(3) - VP.row(1)
    planes[NEAR]   = VP.row(3) + VP.row(2)
    planes[FAR]    = VP.row(3) - VP.row(2)

    for each plane p in planes:
        len = length(p.normal)       // (A, B, C)
        p.normal /= len
        p.distance /= len            // D

    return planes
```

---

## 12.8 Frustum Culling

### Why cull?

Rendering objects the camera can't see wastes GPU time. In a scene with 10,000 trees,
maybe only 500 are visible. Frustum culling skips the other 9,500.

### Point vs. frustum

A point \( \mathbf{p} \) is inside the frustum if it's on the positive side of all six
planes:

```
function isPointInFrustum(point, planes):
    for each plane in planes:
        if dot(plane.normal, point) + plane.distance < 0:
            return false   // outside this plane
    return true
```

### Sphere vs. frustum

A sphere (center \( \mathbf{c} \), radius \( r \)) is outside if it's entirely behind
any plane:

```
function isSphereInFrustum(center, radius, planes):
    for each plane in planes:
        dist = dot(plane.normal, center) + plane.distance
        if dist < -radius:
            return false   // entirely behind this plane
    return true
```

This is the most common culling test — fast and works well for most objects using a
bounding sphere.

### AABB vs. frustum

An Axis-Aligned Bounding Box test checks the "most positive" corner relative to each
plane:

```
function isAABBInFrustum(aabbMin, aabbMax, planes):
    for each plane in planes:
        // Find the corner most aligned with the plane normal
        px = plane.normal.x >= 0 ? aabbMax.x : aabbMin.x
        py = plane.normal.y >= 0 ? aabbMax.y : aabbMin.y
        pz = plane.normal.z >= 0 ? aabbMax.z : aabbMin.z
        positiveCorner = vec3(px, py, pz)

        if dot(plane.normal, positiveCorner) + plane.distance < 0:
            return false
    return true
```

### Example: forest scene

```
Scene: 10,000 trees spread across the world

  ┌──────────────────────────────────────────┐
  │  🌲 🌲 🌲 🌲 🌲 🌲 🌲 🌲 🌲 🌲 🌲 🌲       │
  │  🌲 🌲 ┌──── frustum ────┐ 🌲 🌲 🌲      │
  │  🌲 🌲 │ 🌲 🌲 🌲 🌲 🌲  │ 🌲 🌲        │
  │  🌲    │   500 visible   │   🌲          │
  │  🌲    │ 🌲 🌲 🌲 🌲 🌲  │              │
  │        └─────📷──────────┘               │
  │  🌲 🌲 🌲 🌲      🌲 🌲 🌲 🌲 🌲         │
  └──────────────────────────────────────────┘

  9,500 trees culled → massive performance gain
```

---

## 12.9 Screen-to-World (Unprojection / Ray Casting)

### The problem

The player clicks at screen position \( (x, y) \). You need to know **what 3D point or
object** they clicked on. This requires **reversing** the entire rendering pipeline.

### The rendering pipeline (forward)

```
World space → View space → Clip space → NDC → Screen space
    M_view      M_proj       /w          viewport
```

### The reverse pipeline

```
Screen space → NDC → Clip space → View space → World space
   viewport     ×2-1     ×w        M_proj⁻¹     M_view⁻¹
```

### Step by step

**Step 1: Screen to NDC**

\[
ndc_x = \frac{2 \cdot \text{screen}_x}{\text{width}} - 1
\]
\[
ndc_y = 1 - \frac{2 \cdot \text{screen}_y}{\text{height}}
\]

(Y is flipped because screen Y goes down, NDC Y goes up.)

**Step 2: NDC to clip space**

For a ray, we create two points — one on the near plane and one on the far plane:

\[
\text{nearClip} = (ndc_x, ndc_y, -1, 1)
\]
\[
\text{farClip}  = (ndc_x, ndc_y,  1, 1)
\]

**Step 3: Clip to world space**

Multiply by the inverse of the ViewProjection matrix:

\[
\mathbf{p}_{world} = (\mathbf{V} \cdot \mathbf{P})^{-1} \cdot \mathbf{p}_{clip}
\]

Then perform the perspective divide: divide x, y, z by w.

**Step 4: Build the ray**

```
rayOrigin    = nearPoint (in world space, after divide)
rayDirection = normalize(farPoint - nearPoint)
```

### Pseudocode

```
function screenToWorldRay(screenX, screenY, screenW, screenH, viewMatrix, projMatrix):
    // Screen → NDC
    ndcX =  (2.0 * screenX / screenW) - 1.0
    ndcY = -(2.0 * screenY / screenH) + 1.0

    // NDC → clip (near and far)
    nearClip = vec4(ndcX, ndcY, -1.0, 1.0)
    farClip  = vec4(ndcX, ndcY,  1.0, 1.0)

    // Clip → world
    invVP = inverse(projMatrix * viewMatrix)
    nearWorld = invVP * nearClip
    farWorld  = invVP * farClip

    // Perspective divide
    nearWorld /= nearWorld.w
    farWorld  /= farWorld.w

    // Build ray
    origin    = nearWorld.xyz
    direction = normalize(farWorld.xyz - nearWorld.xyz)
    return Ray(origin, direction)
```

### Example: clicking on a 3D object

```
Player clicks at screen (400, 300) on a 800×600 window.

1. NDC: x = (800/800) - 1 = 0,  y = -(600/600) + 1 = 0
   → center of screen (makes sense!)

2. Near clip: (0, 0, -1, 1),  Far clip: (0, 0, 1, 1)

3. Transform through inverse VP → world-space near and far points

4. Ray from near to far: this ray goes straight forward from the camera

5. Test this ray against all clickable objects (bounding spheres, meshes)
   → return the closest hit
```

```
function pickObject(screenX, screenY, camera, objects):
    ray = screenToWorldRay(screenX, screenY, ...)
    closestHit = null
    closestDist = INFINITY

    for each obj in objects:
        t = raySphereIntersect(ray, obj.boundingSphere)
        if t >= 0 and t < closestDist:
            closestDist = t
            closestHit = obj

    return closestHit
```

---

## 12.10 Camera Shake

### The concept

Camera shake adds visceral impact to explosions, hits, and earthquakes. It works by
adding small **random offsets** to the camera's position and/or rotation.

### Implementation approaches

**Approach 1: Random offset with decay**

```
struct CameraShake:
    trauma:  float = 0.0     // 0 = no shake, 1 = maximum shake
    decay:   float = 2.0     // how fast trauma fades (per second)

function addTrauma(shake, amount):
    shake.trauma = min(shake.trauma + amount, 1.0)

function getShakeOffset(shake, dt):
    shake.trauma = max(shake.trauma - shake.decay * dt, 0.0)

    // Use trauma² for more dramatic falloff
    intensity = shake.trauma * shake.trauma

    offsetX = randomRange(-1, 1) * maxOffset * intensity
    offsetY = randomRange(-1, 1) * maxOffset * intensity
    roll    = randomRange(-1, 1) * maxRoll   * intensity

    return (offsetX, offsetY, roll)
```

**Approach 2: Perlin noise (smoother)**

Instead of pure random values, sample Perlin noise at increasing time offsets. This
produces smooth, organic-feeling shake rather than jittery randomness.

```
function getShakeOffset(shake, time, dt):
    shake.trauma = max(shake.trauma - shake.decay * dt, 0.0)
    intensity = shake.trauma * shake.trauma

    offsetX = perlinNoise(time * frequency + seed1) * maxOffset * intensity
    offsetY = perlinNoise(time * frequency + seed2) * maxOffset * intensity
    roll    = perlinNoise(time * frequency + seed3) * maxRoll   * intensity

    return (offsetX, offsetY, roll)
```

### Example: explosion shake

```
// When an explosion happens nearby:
distance = length(explosionPos - camera.position)
maxRange = 50.0
falloff  = 1.0 - clamp(distance / maxRange, 0, 1)
camera.shake.addTrauma(0.8 * falloff)   // closer = more shake

// Each frame:
offset = camera.shake.getShakeOffset(dt)
finalCameraPos = camera.position + vec3(offset.x, offset.y, 0)
finalCameraRoll = camera.roll + offset.roll
```

```
  Normal view:       During shake:

  ┌──────────┐      ╱──────────╲
  │          │      │  shifted  │
  │  scene   │      │  + tilted │
  │          │      │  scene    │
  └──────────┘      ╲──────────╱
```

---

## 12.11 Cinematic Camera Techniques

### Dolly zoom (Vertigo effect)

The camera moves **backward** while simultaneously **decreasing** the FOV (zooming in),
or vice versa. The subject stays the same size on screen, but the perspective distortion
changes dramatically — background appears to stretch or compress.

```
  Wide FOV + close:              Narrow FOV + far:

   ╱    Subject    ╲             │  Subject  │
  ╱  (large bg)    ╲            │ (small bg) │
 ╱                    ╲          │             │
╱ Background: HUGE      ╲       │ Bg: small   │
```

```
function dollyZoom(camera, targetWidth, targetDist, t):
    // Keep the target's apparent width constant on screen
    // width = 2 * distance * tan(fov/2)
    // → fov = 2 * atan(width / (2 * distance))

    currentDist = lerp(startDist, endDist, t)
    camera.fov  = 2.0 * atan(targetWidth / (2.0 * currentDist))
    camera.position = target - camera.forward * currentDist
```

### Smooth path following with splines

For cinematic sequences, cameras can follow a **Catmull-Rom spline** through control
points:

```
Control points:  P0 ──── P1 ──── P2 ──── P3

Catmull-Rom gives a smooth curve that passes through P1 and P2,
using P0 and P3 as tangent guides.

function catmullRom(P0, P1, P2, P3, t):
    t2 = t * t
    t3 = t2 * t
    return 0.5 * (
        (2*P1) +
        (-P0 + P2) * t +
        (2*P0 - 5*P1 + 4*P2 - P3) * t2 +
        (-P0 + 3*P1 - 3*P2 + P3) * t3
    )
```

To follow a full path of \( N \) points, compute which segment \( t \) falls in and
evaluate the local parameter:

```
function evaluatePath(points, t):
    // t in [0, 1] across entire path
    n = length(points) - 3    // number of segments (need 4 points per segment)
    segment = floor(t * n)
    localT  = (t * n) - segment
    segment = clamp(segment, 0, n - 1)

    return catmullRom(
        points[segment],
        points[segment + 1],
        points[segment + 2],
        points[segment + 3],
        localT
    )
```

### Easing between camera positions

When cutting between two camera positions, an abrupt jump is jarring. Smooth the
transition with an easing function:

```
function easeInOutCubic(t):
    if t < 0.5:
        return 4 * t * t * t
    else:
        return 1 - pow(-2 * t + 2, 3) / 2

function transitionCamera(camA, camB, duration, elapsed):
    t = clamp(elapsed / duration, 0, 1)
    t = easeInOutCubic(t)

    result.position = lerp(camA.position, camB.position, t)
    result.rotation = slerp(camA.rotation, camB.rotation, t)  // spherical lerp for rotations
    result.fov      = lerp(camA.fov, camB.fov, t)
    return result
```

---

## 12.12 Summary and Camera Implementation Checklist

### Key formulas

| Concept               | Formula                                                        |
|-----------------------|----------------------------------------------------------------|
| View matrix           | \( \mathbf{V} = \mathbf{M}_{camera}^{-1} \)                  |
| Forward from angles   | \( (\cos p \sin y,\; \sin p,\; -\cos p \cos y) \)             |
| Spherical to Cartesian| \( (r\cos p\sin y,\; r\sin p,\; r\cos p\cos y) \)             |
| Frustum plane (left)  | \( m_3 + m_0 \) of the VP matrix                              |
| Screen → NDC          | \( ndc = 2 \cdot screen / resolution - 1 \)                   |
| Unproject             | \( \mathbf{p}_{world} = (VP)^{-1} \cdot \mathbf{p}_{clip} \) |
| Dolly zoom FOV        | \( fov = 2\arctan(w / 2d) \)                                  |

### Implementation checklist

When building a camera system, make sure you handle:

- [ ] **View matrix** — use `lookAt` or manual construction from yaw/pitch
- [ ] **Pitch clamping** — prevent the camera from flipping upside down
- [ ] **Mouse sensitivity** — allow user configuration; typically 0.001–0.005 rad/px
- [ ] **Smooth follow** — lerp/smoothdamp for third-person; instant for FPS
- [ ] **Collision avoidance** — raycast to prevent clipping through walls
- [ ] **Frustum culling** — extract planes, test bounding volumes before rendering
- [ ] **Unprojection** — for mouse picking and UI-to-world interaction
- [ ] **Camera shake** — trauma-based system with Perlin noise
- [ ] **Fixed vs. variable update** — camera can run at render rate (not locked to physics)
- [ ] **Multiple camera modes** — ability to switch between FPS, orbit, cinematic
- [ ] **Smooth transitions** — ease between cameras using lerp/slerp with easing curves
- [ ] **Input dead zone** — small mouse movements ignored to reduce jitter
- [ ] **FOV setting** — user-configurable field of view (common range: 60–120 degrees)

---

*Previous: [Lighting and Shading](11-lighting-and-shading.md)*
