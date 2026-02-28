# Chapter 4: Transformations

## 4.1 What is a Transformation?

A **transformation** changes an object's position, orientation, or size in space. Every time you move a character, spin a camera, or scale a power-up in a game, you're applying a transformation.

**Analogy:** Imagine toys on a table.

- **Translation** — sliding a toy car across the table without rotating it.
- **Rotation** — spinning a top in place.
- **Scaling** — inflating a balloon so it gets bigger (or deflating it).

In math terms, a transformation is a **function** that takes a point (or vector) as input and produces a new point as output:

\[
P' = T(P)
\]

Where \( P \) is the original point and \( P' \) is the transformed point.

### Why Matrices?

We represent most transformations as **matrices** because:

1. A single matrix can encode translation, rotation, and scaling.
2. We can **combine** multiple transformations into one matrix by multiplying them.
3. GPUs are built to multiply matrices at blistering speed.

```
Original Point P ──▶ [ Transformation Matrix M ] ──▶ Transformed Point P'
```

---

## 4.2 Translation (Moving)

Translation moves every point of an object by the same offset.

### 2D Translation

Given a point \( (x, y) \) and an offset \( (t_x, t_y) \):

\[
x' = x + t_x \\
y' = y + t_y
\]

```
    Before          After (tx=3, ty=2)

      y                  y
      |                  |
      |  ●(1,1)         |        ●(4,3)
      |                  |
      +------x           +------x
```

### 3D Translation

\[
x' = x + t_x, \quad y' = y + t_y, \quad z' = z + t_z
\]

### The Problem with 3×3 Matrices

A 3×3 matrix can encode rotation and scaling, but **not** translation. Matrix multiplication is a *linear* operation — it can only scale and rotate vectors that pass through the origin. Translation is an *affine* operation; it shifts everything by a constant.

### Homogeneous Coordinates and 4×4 Matrices

The trick is to add a **fourth coordinate** \( w \). A 3D point \( (x, y, z) \) becomes \( (x, y, z, 1) \). This lets us sneak translation into a matrix multiplication.

**Translation matrix (4×4):**

\[
T = \begin{bmatrix}
1 & 0 & 0 & t_x \\
0 & 1 & 0 & t_y \\
0 & 0 & 1 & t_z \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

**Applying it:**

\[
\begin{bmatrix}
1 & 0 & 0 & t_x \\
0 & 1 & 0 & t_y \\
0 & 0 & 1 & t_z \\
0 & 0 & 0 & 1
\end{bmatrix}
\begin{bmatrix} x \\ y \\ z \\ 1 \end{bmatrix}
=
\begin{bmatrix} x + t_x \\ y + t_y \\ z + t_z \\ 1 \end{bmatrix}
\]

### Step-by-Step Example: Moving a Character

Move a character from \( (0, 0, 0) \) to \( (5, 0, 3) \):

\[
T = \begin{bmatrix}
1 & 0 & 0 & 5 \\
0 & 1 & 0 & 0 \\
0 & 0 & 1 & 3 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

Multiply:

\[
\begin{bmatrix}
1 & 0 & 0 & 5 \\
0 & 1 & 0 & 0 \\
0 & 0 & 1 & 3 \\
0 & 0 & 0 & 1
\end{bmatrix}
\begin{bmatrix} 0 \\ 0 \\ 0 \\ 1 \end{bmatrix}
=
\begin{bmatrix} 0+0+0+5 \\ 0+0+0+0 \\ 0+0+0+3 \\ 0+0+0+1 \end{bmatrix}
=
\begin{bmatrix} 5 \\ 0 \\ 3 \\ 1 \end{bmatrix}
\]

The character is now at \( (5, 0, 3) \).

```
Pseudocode:

function translate(point, tx, ty, tz):
    return (point.x + tx, point.y + ty, point.z + tz)

// Usage: move NPC to the right and forward
npcPosition = translate(npcPosition, 5, 0, 3)
```

---

## 4.3 Scaling

Scaling changes the **size** of an object.

### Uniform vs Non-Uniform Scaling

| Type | Description | Example |
|------|-------------|---------|
| **Uniform** | Same factor on all axes | Making a sphere bigger: scale by 2 in x, y, and z |
| **Non-uniform** | Different factors per axis | Stretching a character to be taller but not wider |

### Scaling Matrix (3×3)

\[
S = \begin{bmatrix}
s_x & 0 & 0 \\
0 & s_y & 0 \\
0 & 0 & s_z
\end{bmatrix}
\]

### Scaling Matrix (4×4, Homogeneous)

\[
S = \begin{bmatrix}
s_x & 0 & 0 & 0 \\
0 & s_y & 0 & 0 \\
0 & 0 & s_z & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

### Scaling About the Origin

When you apply the scaling matrix directly, it scales relative to the **origin** \( (0,0,0) \). Points move closer to or farther from the origin.

```
Before (scale = 1):         After (scale = 2):

   y                           y
   |                           |
   |  ● (1,1)                 |         ● (2,2)
   |                           |
   +------x                    +------x

   The point (1,1) moved to (2,2) — farther from origin.
```

### Scaling About an Arbitrary Point

To scale around a center point \( C \) that isn't the origin:

1. Translate so that \( C \) is at the origin.
2. Apply scaling.
3. Translate back.

\[
M = T(C) \cdot S \cdot T(-C)
\]

### Example: Power-Up Growing to 2× Size

A power-up gem sits at position \( (10, 5, 0) \) and we want it to pulse to 2× size around its own center.

```
Pseudocode:

function scaleAroundPoint(vertices, center, scaleFactor):
    for each vertex v in vertices:
        // Step 1: translate to origin
        v = v - center
        // Step 2: scale
        v = v * scaleFactor
        // Step 3: translate back
        v = v + center
    return vertices

gemCenter = (10, 5, 0)
gemVertices = scaleAroundPoint(gemVertices, gemCenter, 2.0)
```

### Negative Scaling = Mirroring / Reflection

If you use a **negative** scale factor on one axis, the object gets flipped (mirrored) across the plane perpendicular to that axis.

\[
S = \begin{bmatrix}
-1 & 0 & 0 \\
0 & 1 & 0 \\
0 & 0 & 1
\end{bmatrix}
\]

This mirrors across the YZ-plane (flips left/right along X).

```
Before:         After (sx = -1):

  ►               ◄
 /|               |\
/ |               | \

(Object is mirrored horizontally)
```

Use case: flip a 2D sprite to face the other direction without needing a separate art asset.

---

## 4.4 Rotation in 2D

### Derivation from the Unit Circle

Consider a point at distance \( r \) from the origin, at angle \( \alpha \). Its coordinates are:

\[
x = r \cos(\alpha), \quad y = r \sin(\alpha)
\]

After rotating by angle \( \theta \), the new angle is \( \alpha + \theta \):

\[
x' = r \cos(\alpha + \theta) = r \cos\alpha \cos\theta - r \sin\alpha \sin\theta = x \cos\theta - y \sin\theta
\]
\[
y' = r \sin(\alpha + \theta) = r \cos\alpha \sin\theta + r \sin\alpha \cos\theta = x \sin\theta + y \cos\theta
\]

### 2D Rotation Matrix

\[
R(\theta) = \begin{bmatrix}
\cos\theta & -\sin\theta \\
\sin\theta & \cos\theta
\end{bmatrix}
\]

**Convention:** positive \( \theta \) = **counter-clockwise** rotation.

```
        y
        |     ● P' (rotated CCW)
        |    /
        |   /  θ
        |  / ·
        | /·
        +·-----------x
                ● P (original)
```

### Step-by-Step Example: Rotating 45°

Rotate \( P = (1, 0) \) by \( 45° \):

\( \cos 45° = \sin 45° = \frac{\sqrt{2}}{2} \approx 0.707 \)

\[
\begin{bmatrix}
0.707 & -0.707 \\
0.707 & 0.707
\end{bmatrix}
\begin{bmatrix} 1 \\ 0 \end{bmatrix}
=
\begin{bmatrix} 0.707 \\ 0.707 \end{bmatrix}
\]

The point moved from \( (1, 0) \) to approximately \( (0.707, 0.707) \) — 45° counter-clockwise on the unit circle.

---

## 4.5 Rotation in 3D

In 3D, a rotation always happens **around an axis**. The three fundamental rotations are around X, Y, and Z.

### Rotation Around the X-Axis

The X coordinate stays fixed; Y and Z rotate.

\[
R_x(\theta) = \begin{bmatrix}
1 & 0 & 0 \\
0 & \cos\theta & -\sin\theta \\
0 & \sin\theta & \cos\theta
\end{bmatrix}
\]

```
       Y
       |
       |   (Y-Z plane rotates)
       |  /
       | /
       +------X  (X stays fixed)
      /
     Z
```

### Rotation Around the Y-Axis

\[
R_y(\theta) = \begin{bmatrix}
\cos\theta & 0 & \sin\theta \\
0 & 1 & 0 \\
-\sin\theta & 0 & \cos\theta
\end{bmatrix}
\]

### Rotation Around the Z-Axis

\[
R_z(\theta) = \begin{bmatrix}
\cos\theta & -\sin\theta & 0 \\
\sin\theta & \cos\theta & 0 \\
0 & 0 & 1
\end{bmatrix}
\]

### Euler Angles: Pitch, Yaw, Roll

Any 3D orientation can be described by three sequential rotations:

| Name | Axis | Motion |
|------|------|--------|
| **Pitch** | X-axis | Nodding head up/down |
| **Yaw** | Y-axis | Shaking head left/right |
| **Roll** | Z-axis | Tilting head ear-to-shoulder |

```
        Yaw (Y)
         |
         |    Roll (Z)
         |   /
         |  /
         | /
         +------Pitch (X)
        /
       /

   Imagine an airplane:
     Pitch = nose up/down
     Yaw   = nose left/right
     Roll  = wings tilt
```

The combined rotation is:

\[
R = R_y(\text{yaw}) \cdot R_x(\text{pitch}) \cdot R_z(\text{roll})
\]

(The exact order depends on the convention your engine uses.)

### Gimbal Lock

**Gimbal lock** occurs when two rotation axes become aligned, causing a loss of one degree of freedom.

**Simple example:** Set pitch to 90° (look straight up). Now yaw and roll both rotate around the same world axis — you've "lost" an axis of control.

```
Normal (3 independent axes):       Gimbal Lock (pitch = 90°):

   Y ──┐                              Y and Z collapse
   |   |                              into the same axis!
   |   Z                                  |
   |  /                                   |
   | /                                    |
   +──── X                                +──── X

   3 degrees of freedom               2 degrees of freedom
```

This is why flight simulators and games often use **quaternions** instead of Euler angles (covered in Chapter 6).

### Rotation About an Arbitrary Axis (Rodrigues' Formula)

To rotate a vector \( \mathbf{v} \) by angle \( \theta \) around a unit axis \( \mathbf{k} \):

\[
\mathbf{v'} = \mathbf{v} \cos\theta + (\mathbf{k} \times \mathbf{v}) \sin\theta + \mathbf{k}(\mathbf{k} \cdot \mathbf{v})(1 - \cos\theta)
\]

**Breakdown:**

- \( \mathbf{v} \cos\theta \) — shrink the original by the cosine.
- \( (\mathbf{k} \times \mathbf{v}) \sin\theta \) — the perpendicular swing component.
- \( \mathbf{k}(\mathbf{k} \cdot \mathbf{v})(1 - \cos\theta) \) — the component along the axis stays put.

```
Pseudocode:

function rodriguesRotate(v, axis, angleDeg):
    theta = degreesToRadians(angleDeg)
    k = normalize(axis)
    cosT = cos(theta)
    sinT = sin(theta)

    return v * cosT + cross(k, v) * sinT + k * dot(k, v) * (1 - cosT)
```

---

## 4.6 Combining Transformations

### Order Matters!

Matrix multiplication is **not commutative**: \( A \cdot B \neq B \cdot A \).

**The standard order is Scale → Rotate → Translate (SRT):**

\[
M = T \cdot R \cdot S
\]

Why this order?

1. **Scale** the object in its own local space.
2. **Rotate** it to face the right direction.
3. **Translate** it to the correct world position.

```
          Local Space            World Space

  S: [tiny tree] ──scale──▶ [big tree]
  R: [big tree]  ──rotate──▶ [big tree, tilted]
  T: [tilted]    ──move────▶ [tilted tree at (10,0,5)]
```

If you translated first and *then* rotated, the rotation would swing the entire object around the world origin — sending it on an arc instead of spinning in place!

### Example: Placing a Tree in a Game World

- Scale the tree model to 3× size.
- Rotate it 30° around Y (yaw).
- Place it at position \( (10, 0, 5) \).

**Step 1: Scale Matrix**

\[
S = \begin{bmatrix}
3 & 0 & 0 & 0 \\
0 & 3 & 0 & 0 \\
0 & 0 & 3 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

**Step 2: Rotation Matrix** (Y-axis, 30°; \( \cos 30° \approx 0.866 \), \( \sin 30° = 0.5 \))

\[
R = \begin{bmatrix}
0.866 & 0 & 0.5 & 0 \\
0 & 1 & 0 & 0 \\
-0.5 & 0 & 0.866 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

**Step 3: Translation Matrix**

\[
T = \begin{bmatrix}
1 & 0 & 0 & 10 \\
0 & 1 & 0 & 0 \\
0 & 0 & 1 & 5 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

**Combined:** \( M = T \cdot R \cdot S \)

First, \( R \cdot S \):

\[
R \cdot S = \begin{bmatrix}
0.866 \cdot 3 & 0 & 0.5 \cdot 3 & 0 \\
0 & 3 & 0 & 0 \\
-0.5 \cdot 3 & 0 & 0.866 \cdot 3 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
=
\begin{bmatrix}
2.598 & 0 & 1.5 & 0 \\
0 & 3 & 0 & 0 \\
-1.5 & 0 & 2.598 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

Then, \( T \cdot (R \cdot S) \):

\[
M = \begin{bmatrix}
2.598 & 0 & 1.5 & 10 \\
0 & 3 & 0 & 0 \\
-1.5 & 0 & 2.598 & 5 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

Applying to the tree's origin \( (0, 0, 0, 1) \):

\[
M \begin{bmatrix} 0 \\ 0 \\ 0 \\ 1 \end{bmatrix} = \begin{bmatrix} 10 \\ 0 \\ 5 \\ 1 \end{bmatrix}
\]

The tree's center is at world position \( (10, 0, 5) \), scaled 3× and rotated 30°.

```
Pseudocode:

treeTransform = Matrix4.Translation(10, 0, 5)
              * Matrix4.RotationY(30°)
              * Matrix4.Scale(3, 3, 3)

for each vertex v in treeMesh:
    worldV = treeTransform * v
```

---

## 4.7 Inverse Transformations

The **inverse** of a transformation undoes it. If \( M \) moves a point from A to B, then \( M^{-1} \) moves it from B back to A.

\[
M \cdot M^{-1} = I \quad \text{(identity matrix)}
\]

### Inverse of Translation

Just negate the offsets:

\[
T^{-1}(t_x, t_y, t_z) = T(-t_x, -t_y, -t_z)
\]

### Inverse of Scaling

Use the reciprocal of each scale factor:

\[
S^{-1}(s_x, s_y, s_z) = S\!\left(\frac{1}{s_x}, \frac{1}{s_y}, \frac{1}{s_z}\right)
\]

### Inverse of Rotation

The inverse of a rotation is the **transpose** of the rotation matrix (because rotation matrices are orthogonal):

\[
R^{-1} = R^T
\]

Or simply rotate by \( -\theta \).

### Inverse of a Combined Transform

If \( M = T \cdot R \cdot S \), then:

\[
M^{-1} = S^{-1} \cdot R^{-1} \cdot T^{-1}
\]

Note the **reversed order** — just like taking off clothes in reverse order of putting them on.

```
Pseudocode:

// Undo a transformation
function invertTransform(translation, rotation, scale):
    invT = Matrix4.Translation(-translation)
    invR = transpose(rotation)
    invS = Matrix4.Scale(1/scale.x, 1/scale.y, 1/scale.z)
    return invS * invR * invT
```

---

## 4.8 Affine Transformations

An **affine transformation** is any transformation that preserves:

1. **Straight lines** — lines remain lines (they don't bend).
2. **Parallelism** — parallel lines stay parallel.
3. **Ratios of distances** along a line — the midpoint of a segment stays the midpoint.

It does **not** necessarily preserve:

- Angles (shearing changes angles)
- Distances (scaling changes distances)

All the transformations we've discussed — translation, rotation, scaling, shearing — are affine.

A **non-affine** transformation (like perspective projection) can make parallel lines converge.

```
Affine (parallelism preserved):     Non-affine (lines converge):

   ┌──────────┐                      /\
   │          │                     /  \
   │          │    ──shear──▶      /    \
   │          │                   /      \
   └──────────┘                  /________\

   Rectangle → Parallelogram     Rectangle → Trapezoid
```

The general 4×4 affine matrix has the form:

\[
\begin{bmatrix}
a & b & c & t_x \\
d & e & f & t_y \\
g & h & i & t_z \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

The bottom row is always \( [0, 0, 0, 1] \) for affine transforms.

---

## 4.9 Shearing

A **shear** transformation shifts coordinates proportionally to another coordinate. It slants or skews an object.

### 2D Shear (X-direction)

\[
\begin{bmatrix}
1 & sh_x \\
0 & 1
\end{bmatrix}
\begin{bmatrix} x \\ y \end{bmatrix}
=
\begin{bmatrix} x + sh_x \cdot y \\ y \end{bmatrix}
\]

The X coordinate is shifted by an amount proportional to Y.

```
Before:          After (shear X by 0.5):

┌──────┐          ╱──────╱
│      │         ╱      ╱
│      │        ╱      ╱
│      │       ╱      ╱
└──────┘      ╱──────╱
```

### 3D Shear Matrix (4×4, shearing X by Y and Z)

\[
\begin{bmatrix}
1 & sh_{xy} & sh_{xz} & 0 \\
sh_{yx} & 1 & sh_{yz} & 0 \\
sh_{zx} & sh_{zy} & 1 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

### Use Cases

- **Italic text rendering**: shear characters along X.
- **Wind effects**: shear grass or trees to simulate bending.
- **Squash and stretch**: cartoon-style animation deformation.
- **Shadow projection** on a flat ground plane.

```
Pseudocode:

function shearX2D(point, factor):
    return (point.x + factor * point.y, point.y)

// Italic effect: shear each glyph
for each character glyph:
    for each vertex v in glyph:
        v = shearX2D(v, 0.2)
```

---

## 4.10 Pivot Points

By default, rotation and scaling happen **around the origin**. But in a game, you almost never want that. A door should rotate around its **hinge**, not the center of the world.

### The Pattern: Translate → Transform → Translate Back

To rotate (or scale) around an arbitrary point \( P \):

1. **Translate** so that \( P \) moves to the origin: \( T(-P) \)
2. **Apply** the rotation (or scale): \( R \)
3. **Translate back**: \( T(P) \)

\[
M = T(P) \cdot R \cdot T(-P)
\]

### Example: Opening a Door

A door's hinge is at \( (2, 0, 0) \). We want to rotate the door 90° around Y (swing open).

```
Top-down view:

  Before:                After 90° around hinge:

     Hinge                   Hinge
      ●━━━━━━━                ●
      (2,0,0)                 ┃
                              ┃
                              ┃
                              ┃
                              ┃
```

**Step 1:** Translate hinge to origin: \( T(-2, 0, 0) \)

**Step 2:** Rotate 90° around Y: \( R_y(90°) \)

**Step 3:** Translate back: \( T(2, 0, 0) \)

\[
M_{\text{door}} = T(2,0,0) \cdot R_y(90°) \cdot T(-2,0,0)
\]

```
Pseudocode:

hingePos = (2, 0, 0)
doorOpenAngle = 90

doorMatrix = Matrix4.Translation(hingePos)
           * Matrix4.RotationY(doorOpenAngle)
           * Matrix4.Translation(-hingePos)

for each vertex v in doorMesh:
    v = doorMatrix * v
```

---

## 4.11 Hierarchy and Scene Graphs

In a game, objects are often organized in a **parent-child hierarchy**. A character's hand is attached to the forearm, which is attached to the upper arm, which is attached to the torso.

### Parent-Child Relationships

Each object stores a **local transform** (position, rotation, scale relative to its parent). The **world transform** is computed by chaining all parent transforms:

\[
\text{WorldMatrix}_{\text{child}} = \text{WorldMatrix}_{\text{parent}} \times \text{LocalMatrix}_{\text{child}}
\]

```
Scene Graph (tree structure):

              [World Root]
                  │
           ┌──────┴──────┐
        [Car Body]    [Street Light]
           │
     ┌─────┼─────┐
  [Wheel1] [Wheel2] [Wheel3] [Wheel4]

When the car body moves, all wheels move with it automatically.
```

### Example: Character Skeleton

```
         [Body]  (root, at world position)
           │
    ┌──────┼──────┐
 [L_Arm]       [R_Arm]
    │              │
 [L_Hand]      [R_Hand]
    │              │
 [Sword]       [Shield]

WorldMatrix(Sword) = WorldMatrix(Body)
                   × LocalMatrix(L_Arm)
                   × LocalMatrix(L_Hand)
                   × LocalMatrix(Sword)
```

Moving the body automatically repositions the arms, hands, and items.

```
Pseudocode:

class SceneNode:
    localTransform = Matrix4.Identity()
    parent = null
    children = []

    function getWorldTransform():
        if parent == null:
            return localTransform
        else:
            return parent.getWorldTransform() * localTransform

// Usage
body.localTransform = Matrix4.Translation(5, 0, 0)
leftArm.localTransform = Matrix4.Translation(0, 1, 0) * Matrix4.RotationZ(-30°)

// Sword follows the entire chain
swordWorldPos = sword.getWorldTransform() * swordLocalVertex
```

### Practical Considerations

- **Caching**: Recompute world matrices only when a node (or its ancestor) changes — use a "dirty" flag.
- **Order**: Traverse the tree top-down (parent before children) so parent world matrices are ready.
- **Detaching**: To drop an item from a character's hand, compute its current world transform, detach it, and set that as its new local transform (it's now a root-level object).

---

## 4.12 Summary and Cheat Sheet

### Transformation Matrices (4×4, Homogeneous)

**Translation:**

\[
T(t_x, t_y, t_z) = \begin{bmatrix}
1 & 0 & 0 & t_x \\
0 & 1 & 0 & t_y \\
0 & 0 & 1 & t_z \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

**Scaling:**

\[
S(s_x, s_y, s_z) = \begin{bmatrix}
s_x & 0 & 0 & 0 \\
0 & s_y & 0 & 0 \\
0 & 0 & s_z & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

**Rotation around X:**

\[
R_x(\theta) = \begin{bmatrix}
1 & 0 & 0 & 0 \\
0 & \cos\theta & -\sin\theta & 0 \\
0 & \sin\theta & \cos\theta & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

**Rotation around Y:**

\[
R_y(\theta) = \begin{bmatrix}
\cos\theta & 0 & \sin\theta & 0 \\
0 & 1 & 0 & 0 \\
-\sin\theta & 0 & \cos\theta & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

**Rotation around Z:**

\[
R_z(\theta) = \begin{bmatrix}
\cos\theta & -\sin\theta & 0 & 0 \\
\sin\theta & \cos\theta & 0 & 0 \\
0 & 0 & 1 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

**Shear (general 3D):**

\[
H = \begin{bmatrix}
1 & sh_{xy} & sh_{xz} & 0 \\
sh_{yx} & 1 & sh_{yz} & 0 \\
sh_{zx} & sh_{zy} & 1 & 0 \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

### Quick Reference

| Operation | Inverse | Notes |
|-----------|---------|-------|
| Translate by \( (t_x, t_y, t_z) \) | Translate by \( (-t_x, -t_y, -t_z) \) | Just negate |
| Scale by \( (s_x, s_y, s_z) \) | Scale by \( (1/s_x, 1/s_y, 1/s_z) \) | Reciprocals |
| Rotate by \( \theta \) | Rotate by \( -\theta \) or transpose | Orthogonal matrix |
| Combined \( T \cdot R \cdot S \) | \( S^{-1} \cdot R^{-1} \cdot T^{-1} \) | Reverse order |

### The Golden Rule

**SRT order: Scale first, then Rotate, then Translate.**

Compose as: \( M = T \cdot R \cdot S \)

(Rightmost matrix is applied first when multiplying by a column vector.)

### Key Takeaways

- Use **4×4 homogeneous matrices** to unify all transformations.
- **Order matters** — always think about what space you're in.
- **Pivot points** — translate to origin, transform, translate back.
- **Scene graphs** — chain parent transforms to get world positions.
- **Euler angles** work but beware of **gimbal lock** — quaternions are safer (see Chapter 6).
