# Chapter 1: Coordinate Systems

> *"To describe where something is, you first need to agree on how to measure."*

---

## 1. What is a Coordinate System?

A **coordinate system** is a framework for describing positions in space using numbers.

Think of it like a city grid. If someone says *"Meet me at 5th Avenue and 3rd Street,"* you
can find them because you both agree on the same grid of named streets. Without that shared
grid, "5th and 3rd" means nothing.

In games and 3D graphics, every object — characters, cameras, lights, particles — needs a
position. A coordinate system gives us the **shared language** to express those positions
as numbers a computer can work with.

**Why do we need them?**

- To **place** objects in a scene (the tree is at position (10, 0, 5))
- To **move** objects (add velocity to position every frame)
- To **measure** distances (how far is the enemy?)
- To **rotate** and **scale** things (spin a wheel, grow a power-up)
- To **render** the 3D world onto a 2D screen

---

## 2. The 1D Number Line

The simplest coordinate system is a **number line** — a single axis.

```
  -3   -2   -1    0    1    2    3    4
───┼────┼────┼────┼────┼────┼────┼────┼───>
                  ^              ^
                origin        P = 3
```

- The **origin** is the reference point at `0`.
- Positive numbers go right, negative numbers go left.
- A position is described by a single number.

**Distance between two points** on a 1D line:

\[
d = |b - a|
\]

Example: distance from `-2` to `3` is \( |3 - (-2)| = 5 \).

**Game use case:** A simple side-scroller where a character only moves left/right could
track position with a single number.

---

## 3. 2D Cartesian Coordinates

Add a second axis perpendicular to the first and you get a **2D Cartesian plane**.

```
          Y
          ^
          |
     Q2   |   Q1
   (-,+)  |  (+,+)
          |
 ---------+---------> X
          |
     Q3   |   Q4
   (-,-)  |  (+,-)
          |
```

- **X axis** — horizontal (positive to the right)
- **Y axis** — vertical (positive upward)
- **Origin** — the point `(0, 0)` where the axes cross

A point is written as an **ordered pair**: \( P = (x, y) \).

### The Four Quadrants

| Quadrant | X sign | Y sign | Example      |
|----------|--------|--------|--------------|
| Q1       | +      | +      | (3, 4)       |
| Q2       | −      | +      | (−2, 5)      |
| Q3       | −      | −      | (−3, −1)     |
| Q4       | +      | −      | (4, −2)      |

### Example: Placing a Character on a 2D Screen

Imagine a 2D game with a world coordinate system where +Y is up.

```
  Y
  ^
  |
  |       @ Player at (200, 150)
  |
  +-------------------> X
(0,0)
```

Your character's position might be stored as:

```
player.position = (200, 150)
```

To move the player 10 units to the right each frame:

```
player.position.x = player.position.x + 10
```

### Distance in 2D

The **Euclidean distance** between two points \( A = (x_1, y_1) \) and \( B = (x_2, y_2) \):

\[
d = \sqrt{(x_2 - x_1)^2 + (y_2 - y_1)^2}
\]

---

## 4. 3D Cartesian Coordinates

Real game worlds are three-dimensional. We add a **Z axis** perpendicular to both X and Y.

A point is now a **triple**: \( P = (x, y, z) \).

```
        Y (up)
        |
        |
        |
        +---------> X (right)
       /
      /
     Z (towards you)
```

### Left-Handed vs Right-Handed Coordinate Systems

There are **two conventions** for which direction Z points, and they matter a lot.

#### Right-Handed System (OpenGL, Blender, glTF)

Curl the fingers of your **right hand** from X toward Y — your **thumb** points in the +Z
direction (out of the screen, towards you).

```
        Y                    Right Hand Rule:
        ^                    Fingers: X -> Y
        |                    Thumb:   +Z (out of screen)
        |
        +-------> X
       /
      /
     Z  (out of screen, towards you)
```

- +X = right
- +Y = up
- +Z = **towards the viewer** (out of the screen)

#### Left-Handed System (Unity, DirectX, Unreal Engine)

Curl the fingers of your **left hand** from X toward Y — your **thumb** points in the +Z
direction (into the screen, away from you).

```
        Y                    Left Hand Rule:
        ^                    Fingers: X -> Y
        |                    Thumb:   +Z (into screen)
        |
        +-------> X
         \
          \
           Z  (into screen, away from you)
```

- +X = right
- +Y = up
- +Z = **into the screen** (away from you, "forward into the scene")

### Which Engines/APIs Use Which?

| System / Engine   | Handedness    | +Z Direction      |
|-------------------|---------------|-------------------|
| OpenGL            | Right-handed  | Out of screen     |
| Vulkan            | Right-handed* | Out of screen     |
| DirectX           | Left-handed   | Into screen       |
| Unity             | Left-handed   | Into screen       |
| Unreal Engine     | Left-handed   | Into screen (Z-up)|
| Blender           | Right-handed  | Out of screen     |
| glTF format       | Right-handed  | Out of screen     |

> *Vulkan flips the Y-axis in clip space compared to OpenGL, but world-space is still
> conventionally right-handed.

**Key takeaway:** When importing assets between tools, mismatched handedness is one of the
most common sources of "everything is mirrored" bugs.

---

## 5. Screen Space

**Screen space** (or **window space**) is the 2D coordinate system of the display.

```
  (0,0)────────────────────────> X (pixels)
    │
    │     +-----------------------+
    │     |                       |
    │     |    Game Window        |
    │     |       800 x 600      |
    │     |                       |
    │     |          * (400,300)  |
    │     |                       |
    │     +-----------------------+
    v
    Y (pixels, going DOWN)
```

**Key differences from math-class coordinates:**

- **Origin is at the top-left corner** (not center or bottom-left).
- **Y increases downward** (not upward).
- Units are **pixels**.

### Example: Mouse Click

A player clicks at pixel `(400, 300)` on an `800×600` window:

```
mouse.x = 400   // halfway across horizontally
mouse.y = 300   // halfway down vertically
```

To convert a screen-space click into the game world, you need to *unproject* it — more on
that in the transformation chapters.

---

## 6. World Space

**World space** is the single, global coordinate system for the entire scene. Every object
in the game has a world-space position.

```
World Space
============
                * Streetlight (10, 3, -5)

  * Tree (-4, 0, 2)
                             * Player (0, 0, 0)

          * Rock (6, 0, 8)
```

- It has a fixed origin (the "center of the world").
- All objects share this frame of reference.
- Distances and directions between objects are measured here.

**Example:** If the player is at `(0, 0, 0)` and an enemy is at `(10, 0, 5)`, the distance
between them is:

```
dx = 10 - 0 = 10
dy = 0 - 0  = 0
dz = 5 - 0  = 5

distance = sqrt(10² + 0² + 5²) = sqrt(125) ≈ 11.18
```

---

## 7. Object / Local Space

Every object has its own private coordinate system called **local space** (or **object
space** or **model space**).

- The object's **own origin** is typically its center or base.
- Positions of sub-parts (vertices, attachment points) are given relative to that origin.
- When the object moves or rotates in the world, the local space moves with it.

```
Local space of a Sword:

        (0, 1.2, 0) -- Tip
            |
            |  blade
            |
        (0, 0.3, 0)
            |  handle
            |
        (0, 0, 0) -- Origin (base of handle)
```

### Example: Sword Attached to a Character's Hand

The sword tip is at `(0, 1.2, 0)` **in the sword's local space**. When the sword is
parented to the character's hand bone, the engine:

1. Applies the sword's local-to-hand transform
2. Then the hand-to-arm, arm-to-body transforms (skeleton hierarchy)
3. Then the character's local-to-world transform

The final **world-space** position of the sword tip depends on the entire chain, but the
*local* position `(0, 1.2, 0)` never changes — that's the beauty of local space.

---

## 8. Camera / View Space

**View space** (or **camera space** or **eye space**) re-expresses the world from the
camera's point of view.

```
Camera View Space:

        Y (up)
        ^
        |        * Object
        |       /
        |      /
        +----/---------> X (right)
       /
      /
     Z (into the scene, or out — depends on convention)

    Camera is at the origin, looking along -Z (OpenGL)
    or +Z (DirectX).
```

**How it works:**

1. Take every object's world-space position.
2. Subtract the camera's world position (translate so camera is at origin).
3. Rotate so the camera's forward direction aligns with the Z axis.

This is done by multiplying world positions by the **View Matrix** (the inverse of the
camera's world transform).

**Why we need it:** The rendering pipeline needs to know where things are *relative to the
camera*, not the world origin. Something directly in front of the camera should be "straight
ahead" in view space, regardless of where the camera is in the world.

---

## 9. Clip Space and NDC (Normalized Device Coordinates)

After view space, positions go through the **projection matrix** to land in **clip space**,
and then after perspective divide, into **NDC**.

### Clip Space

The projection matrix (perspective or orthographic) transforms view-space coordinates into
a **clip-space** coordinate with four components: \( (x, y, z, w) \).

This is where **frustum culling** happens — anything outside the clip volume is discarded.

### NDC (Normalized Device Coordinates)

Divide by `w` to get NDC:

\[
\text{NDC}_x = \frac{x}{w}, \quad \text{NDC}_y = \frac{y}{w}, \quad \text{NDC}_z = \frac{z}{w}
\]

In NDC, visible geometry falls within a standard cube:

| API    | X range   | Y range   | Z range     |
|--------|-----------|-----------|-------------|
| OpenGL | [−1, +1]  | [−1, +1]  | [−1, +1]    |
| DirectX| [−1, +1]  | [−1, +1]  | [ 0, +1]    |
| Vulkan | [−1, +1]  | [−1, +1]  | [ 0, +1]    |

```
NDC Cube (OpenGL):

            +1 Y
             |
    (-1,+1)  |  (+1,+1)
       +-----+-----+
       |     |     |
  -1 --+-----+-----+-- +1 X
       |     |     |
       +-----+-----+
    (-1,-1)  |  (+1,-1)
             |
            -1 Y
```

**After NDC**, the GPU maps these to actual screen pixels via the **viewport transform**.

### The Full Pipeline (Preview)

```
Local Space
    │  (Model Matrix)
    v
World Space
    │  (View Matrix)
    v
View/Camera Space
    │  (Projection Matrix)
    v
Clip Space
    │  (Perspective Divide: ÷ w)
    v
NDC (Normalized Device Coordinates)
    │  (Viewport Transform)
    v
Screen Space (pixels)
```

---

## 10. Polar and Spherical Coordinates

Not every problem is best solved with Cartesian (x, y, z). Sometimes angles and distances
are more natural.

### 2D Polar Coordinates

A point in 2D can be described as:

\[
P = (r, \theta)
\]

where:
- \( r \) = distance from the origin (radius)
- \( \theta \) = angle from the positive X-axis (in radians)

```
          Y
          ^
          |   * P
          |  /
          | / r
          |/θ
          +---------> X
```

#### Conversion: Polar → Cartesian

\[
x = r \cos(\theta)
\]
\[
y = r \sin(\theta)
\]

#### Conversion: Cartesian → Polar

\[
r = \sqrt{x^2 + y^2}
\]
\[
\theta = \text{atan2}(y, x)
\]

> Always use `atan2(y, x)` instead of `atan(y/x)` — it handles all four quadrants and
> avoids division by zero.

### 3D Spherical Coordinates

Extend polar coordinates to 3D:

\[
P = (r, \theta, \varphi)
\]

where:
- \( r \) = distance from origin
- \( \theta \) = **azimuthal angle** (rotation around the Y-axis, "longitude")
- \( \varphi \) = **polar angle** (angle down from the Y-axis, "latitude from pole")

> Note: conventions for \(\theta\) and \(\varphi\) vary between physics and math textbooks.
> The one above is common in graphics.

#### Conversion: Spherical → Cartesian

\[
x = r \sin(\varphi) \cos(\theta)
\]
\[
y = r \cos(\varphi)
\]
\[
z = r \sin(\varphi) \sin(\theta)
\]

#### Conversion: Cartesian → Spherical

\[
r = \sqrt{x^2 + y^2 + z^2}
\]
\[
\theta = \text{atan2}(z, x)
\]
\[
\varphi = \arccos\left(\frac{y}{r}\right)
\]

### Example: Making an Object Orbit Using Polar Coordinates

Suppose you want a shield to orbit around the player in 2D:

```
orbitRadius = 3.0
orbitSpeed  = 2.0      // radians per second
angle       = 0.0

function update(deltaTime):
    angle = angle + orbitSpeed * deltaTime

    shield.x = player.x + orbitRadius * cos(angle)
    shield.y = player.y + orbitRadius * sin(angle)
```

Increasing `angle` every frame traces a circle. Changing `orbitRadius` over time creates a
spiral. This is much simpler than computing circular motion with Cartesian math directly.

### Example: A 3D Orbit Camera

An orbit camera lets the player rotate around a target. Spherical coordinates are perfect:

```
target    = (0, 1, 0)    // look-at point
distance  = 10.0
azimuth   = 0.0          // horizontal angle (theta)
elevation = 0.5          // vertical angle (phi), 0 = top, pi/2 = horizon

function update(mouseX, mouseY):
    azimuth   = azimuth   + mouseX * sensitivity
    elevation = clamp(elevation + mouseY * sensitivity, 0.1, pi - 0.1)

    camera.x = target.x + distance * sin(elevation) * cos(azimuth)
    camera.y = target.y + distance * cos(elevation)
    camera.z = target.z + distance * sin(elevation) * sin(azimuth)

    camera.lookAt(target)
```

---

## 11. Coordinate Space Transformations (Overview)

Converting a point from one coordinate space to another is one of the most fundamental
operations in game math. The tool for the job is a **transformation matrix**.

| From → To           | Matrix Used        |
|----------------------|--------------------|
| Local → World        | Model Matrix       |
| World → View         | View Matrix        |
| View → Clip          | Projection Matrix  |
| Clip → NDC           | Perspective Divide |
| NDC → Screen         | Viewport Transform |

And in reverse:

| From → To           | Matrix Used                   |
|----------------------|-------------------------------|
| Screen → NDC         | Inverse Viewport Transform    |
| NDC → View           | Inverse Projection            |
| View → World         | Inverse View (= Camera Xform) |
| World → Local        | Inverse Model                 |

The key insight: **each space is just a different "point of view," and matrices let you
translate between them.**

We'll cover the math behind each of these matrices in the chapters on
[Transformations](#) and [Matrices](#).

---

## 12. Summary Table

| Space                  | Dimensions | Origin                      | Typical Use                          |
|------------------------|------------|-----------------------------|--------------------------------------|
| 1D Number Line         | 1          | 0 on the line               | Simple slider or health bar          |
| 2D Cartesian           | 2          | (0, 0)                      | 2D games, UI layout                  |
| 3D Cartesian           | 3          | (0, 0, 0)                   | 3D game worlds                       |
| Screen Space           | 2          | Top-left pixel (0, 0)       | Final rendered image, UI, mouse      |
| World Space            | 3          | Scene origin                | Global positions of all objects      |
| Local / Object Space   | 3          | Object's own center/base    | Mesh vertices, attachment points     |
| View / Camera Space    | 3          | Camera position             | Rendering: what the camera sees      |
| Clip Space             | 4 (x,y,z,w)| After projection           | Frustum culling                      |
| NDC                    | 3          | Center of screen (0,0,0)    | Normalized, API-independent coords   |
| 2D Polar               | 2          | (0, 0)                      | Orbits, radial effects               |
| 3D Spherical           | 3          | (0, 0, 0)                   | Orbit cameras, planet coordinates    |

---

## Key Takeaways

1. **A coordinate system is just a convention** — a shared agreement about where "zero" is
   and which directions are positive.
2. **Different spaces exist for convenience.** Local space makes modeling easy; world space
   makes gameplay logic easy; view/clip/NDC spaces make rendering easy.
3. **Left-handed vs right-handed** is an arbitrary choice, but mixing them up causes painful
   mirroring bugs.
4. **Screen space has Y pointing down** — always remember this when converting from world
   coordinates.
5. **Polar/spherical coordinates** are your best friend for anything involving circles,
   orbits, or angles.
6. **Matrices convert between spaces** — understanding the full chain from local to screen
   is essential for any graphics programmer.

---

*Next up: [Chapter 2 — Vectors](02-vectors.md), the fundamental building block for
describing directions, velocities, and forces.*
