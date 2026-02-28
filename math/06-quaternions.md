# Chapter 6: Quaternions

## 6.1 Why Quaternions?

Before we dive in, let's understand the problem quaternions solve.

### Problems with Euler Angles

Euler angles (pitch, yaw, roll) are intuitive but have a critical flaw: **gimbal lock**. When one axis rotates 90°, two axes become aligned, and you lose a degree of freedom. The rotation "jams" and you can't smoothly rotate through certain orientations.

### Problems with Matrices

A 3×3 rotation matrix uses **9 numbers** (or 16 for a 4×4). That's a lot of storage and computation. Worse, after many multiplications, floating-point errors accumulate and the matrix may stop being a proper rotation (it drifts). Re-orthogonalizing a matrix is expensive.

### The Quaternion Solution

Quaternions offer the best of all worlds:

| Property | Euler Angles | Matrices (3×3) | Quaternions |
|----------|-------------|-----------------|-------------|
| Storage | 3 numbers | 9 numbers | 4 numbers |
| Gimbal lock | **Yes** | No | **No** |
| Smooth interpolation | Difficult | Difficult | **Easy (SLERP)** |
| Combining rotations | Awkward | Matrix multiply | Quaternion multiply |
| Normalization cost | N/A | Expensive (Gram-Schmidt) | Cheap (normalize 4 values) |
| Intuitive | Very | Somewhat | Not at first |

Quaternions: **4 numbers, no gimbal lock, smooth interpolation.**

---

## 6.2 A Brief History

In 1843, the Irish mathematician **William Rowan Hamilton** was walking along the Royal Canal in Dublin, struggling to extend complex numbers (2D) into 3D. The breakthrough came suddenly: you need **four** dimensions, not three.

He was so excited that he carved the fundamental formula into the stone of Brougham Bridge:

\[
i^2 = j^2 = k^2 = ijk = -1
\]

This seemingly simple equation defines the entire algebra of quaternions. Hamilton spent the rest of his life studying them. Today, they're indispensable in game engines, robotics, aerospace, and computer graphics.

---

## 6.3 What is a Quaternion?

A quaternion is a number with **one real part** and **three imaginary parts**:

\[
q = w + xi + yj + zk
\]

Or in the compact notation:

\[
q = (w, \mathbf{v}) \quad \text{where } \mathbf{v} = (x, y, z)
\]

- **\( w \)** — the **scalar** (real) part.
- **\( \mathbf{v} = (x, y, z) \)** — the **vector** (imaginary) part.

### The Imaginary Units

The three imaginary units \( i, j, k \) follow Hamilton's rules:

\[
i^2 = j^2 = k^2 = -1
\]
\[
ij = k, \quad ji = -k
\]
\[
jk = i, \quad kj = -i
\]
\[
ki = j, \quad ik = -j
\]

Notice: \( ij \neq ji \) — quaternion multiplication is **not commutative**, just like cross products and matrix multiplication.

### The Rotation Analogy

Think of a unit quaternion as encoding:

- **Which axis** to rotate around → the vector part \( \mathbf{v} \)
- **How much** to rotate → the scalar part \( w \)

Specifically, a rotation of angle \( \theta \) around axis \( \hat{a} \) becomes:

\[
q = \left(\cos\frac{\theta}{2},\; \sin\frac{\theta}{2} \cdot \hat{a}\right)
\]

The \( \theta/2 \) is not a typo — it's a deep mathematical property that makes the rotation formula work.

```
Axis-angle to quaternion:

         Axis â
          ↑    ╲
          |     ╲  θ (rotation angle)
          |      ╲
          ●───────▶

  q = (cos(θ/2), sin(θ/2) * â)
```

---

## 6.4 Unit Quaternions

A **unit quaternion** has magnitude 1:

\[
|q| = \sqrt{w^2 + x^2 + y^2 + z^2} = 1
\]

Only unit quaternions represent rotations. Non-unit quaternions would also scale the object.

### Axis-Angle to Quaternion

To create a quaternion from an axis \( \hat{a} = (a_x, a_y, a_z) \) and angle \( \theta \):

\[
q = \left(\cos\frac{\theta}{2},\; \sin\frac{\theta}{2} \cdot a_x,\; \sin\frac{\theta}{2} \cdot a_y,\; \sin\frac{\theta}{2} \cdot a_z\right)
\]

### Example: 90° Rotation Around the Y-Axis

\( \theta = 90° \), \( \hat{a} = (0, 1, 0) \)

\[
\cos(45°) = \frac{\sqrt{2}}{2} \approx 0.707
\]
\[
\sin(45°) = \frac{\sqrt{2}}{2} \approx 0.707
\]

\[
q = (0.707,\; 0,\; 0.707,\; 0)
\]

Let's verify: \( |q| = \sqrt{0.707^2 + 0 + 0.707^2 + 0} = \sqrt{0.5 + 0.5} = 1 \). Unit quaternion confirmed.

```
Pseudocode:

function axisAngleToQuat(axis, angleDeg):
    theta = degreesToRadians(angleDeg)
    halfAngle = theta / 2
    s = sin(halfAngle)
    return Quaternion(
        w = cos(halfAngle),
        x = axis.x * s,
        y = axis.y * s,
        z = axis.z * s
    )

// 90° around Y
q = axisAngleToQuat((0, 1, 0), 90)
// q ≈ (0.707, 0, 0.707, 0)
```

### Special Quaternions

| Quaternion | Represents |
|-----------|------------|
| \( (1, 0, 0, 0) \) | **Identity** — no rotation |
| \( (0, 1, 0, 0) \) | 180° around X |
| \( (0, 0, 1, 0) \) | 180° around Y |
| \( (0, 0, 0, 1) \) | 180° around Z |

---

## 6.5 Quaternion Operations

### Quaternion Addition

\[
q_1 + q_2 = (w_1 + w_2,\; x_1 + x_2,\; y_1 + y_2,\; z_1 + z_2)
\]

Rarely used for rotations directly, but needed for interpolation (see SLERP/NLERP).

### Quaternion Multiplication (Hamilton Product)

This is the **core operation** for combining rotations.

Given \( q_1 = (w_1, \mathbf{v_1}) \) and \( q_2 = (w_2, \mathbf{v_2}) \):

\[
q_1 q_2 = \big(w_1 w_2 - \mathbf{v_1} \cdot \mathbf{v_2},\;\; w_1 \mathbf{v_2} + w_2 \mathbf{v_1} + \mathbf{v_1} \times \mathbf{v_2}\big)
\]

**Expanded component form:**

\[
q_1 q_2 = \begin{pmatrix}
w_1 w_2 - x_1 x_2 - y_1 y_2 - z_1 z_2 \\
w_1 x_2 + x_1 w_2 + y_1 z_2 - z_1 y_2 \\
w_1 y_2 - x_1 z_2 + y_1 w_2 + z_1 x_2 \\
w_1 z_2 + x_1 y_2 - y_1 x_2 + z_1 w_2
\end{pmatrix}
\]

### Step-by-Step Multiplication Example

Let \( q_1 = (0.707, 0, 0.707, 0) \) (90° around Y) and \( q_2 = (0.707, 0.707, 0, 0) \) (90° around X).

**Scalar part:**

\[
w = 0.707 \times 0.707 - (0 \times 0.707 + 0.707 \times 0 + 0 \times 0) = 0.5 - 0 = 0.5
\]

**X component:**

\[
x = 0.707 \times 0.707 + 0 \times 0.707 + 0.707 \times 0 - 0 \times 0 = 0.5
\]

**Y component:**

\[
y = 0.707 \times 0 - 0 \times 0 + 0.707 \times 0.707 + 0 \times 0.707 = 0.5
\]

**Z component:**

\[
z = 0.707 \times 0 + 0 \times 0 - 0.707 \times 0.707 + 0 \times 0.707 = -0.5
\]

Result: \( q_1 q_2 = (0.5, 0.5, 0.5, -0.5) \)

Verify magnitude: \( \sqrt{0.25 + 0.25 + 0.25 + 0.25} = 1 \). Still a unit quaternion.

```
Pseudocode:

function quatMultiply(q1, q2):
    return Quaternion(
        w = q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z,
        x = q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y,
        y = q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x,
        z = q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w
    )
```

### Conjugate

The conjugate negates the vector part:

\[
q^* = (w, -x, -y, -z) = (w, -\mathbf{v})
\]

For a unit quaternion, the conjugate represents the **reverse rotation**.

### Inverse

\[
q^{-1} = \frac{q^*}{|q|^2}
\]

For unit quaternions (\( |q| = 1 \)):

\[
q^{-1} = q^*
\]

This is extremely cheap — just negate three components!

### Norm (Magnitude)

\[
|q| = \sqrt{w^2 + x^2 + y^2 + z^2}
\]

For rotation quaternions, always keep \( |q| = 1 \) by normalizing after arithmetic:

\[
\hat{q} = \frac{q}{|q|}
\]

---

## 6.6 Rotating a Vector with a Quaternion

To rotate a 3D vector \( \mathbf{v} \) by a unit quaternion \( q \):

1. Treat \( \mathbf{v} \) as a **pure quaternion**: \( p = (0, \mathbf{v}) = (0, v_x, v_y, v_z) \).
2. Compute: \( p' = q \cdot p \cdot q^{-1} \)
3. Extract the vector part of \( p' \): that's your rotated vector.

\[
\mathbf{v'} = \text{vec}(q \cdot p \cdot q^*)
\]

(Using \( q^* \) since \( q \) is unit.)

### Step-by-Step Example

Rotate \( \mathbf{v} = (1, 0, 0) \) by 90° around the Y-axis.

**Quaternion:** \( q = (0.707, 0, 0.707, 0) \)

**Pure quaternion:** \( p = (0, 1, 0, 0) \)

**Step 1: Compute \( q \cdot p \):**

Using the multiplication formula:

\[
w = 0.707 \times 0 - (0 \times 1 + 0.707 \times 0 + 0 \times 0) = 0 - 0 = 0
\]
\[
x = 0.707 \times 1 + 0 \times 0 + 0.707 \times 0 - 0 \times 0 = 0.707
\]
\[
y = 0.707 \times 0 - 0 \times 0 + 0.707 \times 0 + 0 \times 1 = 0
\]
\[
z = 0.707 \times 0 + 0 \times 0 - 0.707 \times 1 + 0 \times 0 = -0.707
\]

So \( q \cdot p = (0, 0.707, 0, -0.707) \).

**Step 2: Compute \( (q \cdot p) \cdot q^* \):**

\( q^* = (0.707, 0, -0.707, 0) \)

\[
w = 0 \times 0.707 - (0.707 \times 0 + 0 \times (-0.707) + (-0.707) \times 0) = 0
\]
\[
x = 0 \times 0 + 0.707 \times 0.707 + 0 \times 0 - (-0.707)(-0.707) = 0.5 - 0.5 = 0
\]
\[
y = 0 \times (-0.707) - 0.707 \times 0 + 0 \times 0.707 + (-0.707) \times 0 = 0
\]
\[
z = 0 \times 0 + 0.707 \times (-0.707) - 0 \times 0.707 + (-0.707)(0.707) = -0.5 - 0.5 = -1
\]

Result: \( p' = (0, 0, 0, -1) \)

The rotated vector is \( \mathbf{v'} = (0, 0, -1) \).

**Verification:** rotating \( (1, 0, 0) \) by 90° around Y should point it down the -Z axis. That's \( (0, 0, -1) \). Correct!

```
           Before:     After 90° around Y:

           Y               Y
           |               |
           |               |
           +---● X         +------X
          /   (1,0,0)     /
         Z                ● Z = (0,0,-1)
```

```
Pseudocode:

function rotateVector(v, q):
    p = Quaternion(0, v.x, v.y, v.z)
    qConj = Quaternion(q.w, -q.x, -q.y, -q.z)
    result = quatMultiply(quatMultiply(q, p), qConj)
    return Vector3(result.x, result.y, result.z)

// Optimized version (avoids full double-multiply):
function rotateVectorFast(v, q):
    t = 2.0 * cross(q.xyz, v)
    return v + q.w * t + cross(q.xyz, t)
```

The optimized version uses the identity expansion to avoid constructing intermediate quaternions — it's what production engines actually use.

---

## 6.7 Combining Rotations

To apply rotation \( q_1 \) **then** rotation \( q_2 \):

\[
q_{\text{total}} = q_2 \cdot q_1
\]

The order is right-to-left, just like matrices: \( q_1 \) is applied first.

**Example:** First pitch up 30°, then yaw left 45°:

```
Pseudocode:

qPitch = axisAngleToQuat((1,0,0), 30)   // 30° around X
qYaw   = axisAngleToQuat((0,1,0), 45)   // 45° around Y

// Apply pitch first, then yaw
qTotal = quatMultiply(qYaw, qPitch)

// Rotate the forward vector
forward = rotateVector((0, 0, -1), qTotal)
```

### Relative vs Absolute Rotations

- **Absolute** (world axes): left-multiply. \( q_{\text{new}} = q_{\text{delta}} \cdot q_{\text{current}} \)
- **Relative** (local axes): right-multiply. \( q_{\text{new}} = q_{\text{current}} \cdot q_{\text{delta}} \)

Think: FPS mouselook uses local pitch (nod head up/down relative to where you're facing) but world yaw (turn left/right around the world's up axis).

---

## 6.8 Quaternion ↔ Matrix Conversion

### Quaternion to Rotation Matrix (3×3)

Given unit quaternion \( q = (w, x, y, z) \):

\[
R = \begin{bmatrix}
1 - 2(y^2 + z^2) & 2(xy - wz) & 2(xz + wy) \\
2(xy + wz) & 1 - 2(x^2 + z^2) & 2(yz - wx) \\
2(xz - wy) & 2(yz + wx) & 1 - 2(x^2 + y^2)
\end{bmatrix}
\]

```
Pseudocode:

function quatToMatrix(q):
    xx = q.x * q.x;  yy = q.y * q.y;  zz = q.z * q.z
    xy = q.x * q.y;  xz = q.x * q.z;  yz = q.y * q.z
    wx = q.w * q.x;  wy = q.w * q.y;  wz = q.w * q.z

    return Matrix3(
        1-2*(yy+zz),  2*(xy-wz),    2*(xz+wy),
        2*(xy+wz),    1-2*(xx+zz),  2*(yz-wx),
        2*(xz-wy),    2*(yz+wx),    1-2*(xx+yy)
    )
```

### Rotation Matrix to Quaternion

Given rotation matrix \( R \), extract the quaternion using Shepperd's method (handles all cases, avoids division by near-zero):

```
Pseudocode:

function matrixToQuat(m):
    trace = m[0][0] + m[1][1] + m[2][2]

    if trace > 0:
        s = 0.5 / sqrt(trace + 1.0)
        w = 0.25 / s
        x = (m[2][1] - m[1][2]) * s
        y = (m[0][2] - m[2][0]) * s
        z = (m[1][0] - m[0][1]) * s
    elif m[0][0] > m[1][1] and m[0][0] > m[2][2]:
        s = 2.0 * sqrt(1.0 + m[0][0] - m[1][1] - m[2][2])
        w = (m[2][1] - m[1][2]) / s
        x = 0.25 * s
        y = (m[0][1] + m[1][0]) / s
        z = (m[0][2] + m[2][0]) / s
    elif m[1][1] > m[2][2]:
        s = 2.0 * sqrt(1.0 + m[1][1] - m[0][0] - m[2][2])
        w = (m[0][2] - m[2][0]) / s
        x = (m[0][1] + m[1][0]) / s
        y = 0.25 * s
        z = (m[1][2] + m[2][1]) / s
    else:
        s = 2.0 * sqrt(1.0 + m[2][2] - m[0][0] - m[1][1])
        w = (m[1][0] - m[0][1]) / s
        x = (m[0][2] + m[2][0]) / s
        y = (m[1][2] + m[2][1]) / s
        z = 0.25 * s

    return normalize(Quaternion(w, x, y, z))
```

### When to Use Which Representation

| Task | Best representation |
|------|-------------------|
| Storing orientation | Quaternion (4 floats, compact) |
| Interpolating rotations | Quaternion (SLERP) |
| Transforming many vertices | Matrix (GPU-friendly) |
| Combining with translation/scale | 4×4 Matrix |
| Human-readable editing | Euler angles |

Typical engine pattern: store as quaternion, convert to matrix for rendering.

---

## 6.9 SLERP (Spherical Linear Interpolation)

### What is SLERP?

SLERP smoothly interpolates between two rotations \( q_1 \) and \( q_2 \) along the shortest arc on the 4D unit sphere. The parameter \( t \) goes from 0 to 1:

- \( t = 0 \) → \( q_1 \)
- \( t = 1 \) → \( q_2 \)
- \( t = 0.5 \) → halfway rotation

```
Imagine the surface of a 4D sphere:

  q1 ●                           ● q2
      \                         /
       \    SLERP path         /
        \  (great circle)     /
         ●───────●───────────●
        t=0    t=0.5        t=1

  SLERP follows the curved surface (constant angular speed).
  LERP would cut a straight chord through the sphere (non-constant speed).
```

### Why SLERP Matters

If you linearly interpolate (LERP) quaternion components, the result:

1. Doesn't stay on the unit sphere (needs normalization).
2. Moves at **non-constant angular speed** (accelerates in the middle, decelerates at the ends).

SLERP maintains **constant angular velocity** — the rotation appears smooth and even.

### SLERP Formula

\[
\text{slerp}(q_1, q_2, t) = q_1 \frac{\sin((1-t)\Omega)}{\sin\Omega} + q_2 \frac{\sin(t\Omega)}{\sin\Omega}
\]

Where \( \Omega \) is the angle between \( q_1 \) and \( q_2 \):

\[
\cos\Omega = q_1 \cdot q_2 = w_1 w_2 + x_1 x_2 + y_1 y_2 + z_1 z_2
\]

```
Pseudocode:

function slerp(q1, q2, t):
    // Compute cosine of angle between quaternions
    dot = q1.w*q2.w + q1.x*q2.x + q1.y*q2.y + q1.z*q2.z

    // If dot is negative, negate one quaternion to take the short path
    if dot < 0:
        q2 = -q2      // negate all components
        dot = -dot

    // If quaternions are very close, fall back to LERP to avoid division by zero
    if dot > 0.9995:
        result = q1 + t * (q2 - q1)
        return normalize(result)

    // SLERP
    omega = acos(dot)
    sinOmega = sin(omega)

    scale1 = sin((1 - t) * omega) / sinOmega
    scale2 = sin(t * omega) / sinOmega

    return Quaternion(
        scale1 * q1.w + scale2 * q2.w,
        scale1 * q1.x + scale2 * q2.x,
        scale1 * q1.y + scale2 * q2.y,
        scale1 * q1.z + scale2 * q2.z
    )
```

### Example: Smooth Camera Rotation

A camera needs to smoothly rotate from facing North to facing East (90° around Y) over 2 seconds.

```
Pseudocode:

qStart = axisAngleToQuat((0,1,0), 0)    // (1, 0, 0, 0) — identity
qEnd   = axisAngleToQuat((0,1,0), 90)   // (0.707, 0, 0.707, 0)
duration = 2.0

function updateCamera(elapsedTime):
    t = clamp(elapsedTime / duration, 0, 1)
    camera.rotation = slerp(qStart, qEnd, t)
```

At \( t = 0.5 \) (1 second in), the camera faces 45° — exactly halfway, at constant speed.

### LERP vs SLERP Comparison

```
Angular velocity over time:

  LERP:                          SLERP:

  speed                          speed
    |   ╱╲                         |  ──────────
    |  ╱  ╲                        | (constant)
    | ╱    ╲                       |
    |╱      ╲                      |
    +────────── t                  +────────── t
   0    0.5    1                  0    0.5    1

  LERP speeds up in the          SLERP moves at a
  middle, slows at ends           perfectly even rate
```

---

## 6.10 NLERP (Normalized Linear Interpolation)

**NLERP** is a cheaper alternative to SLERP:

1. Linearly interpolate the quaternion components.
2. Normalize the result.

\[
\text{nlerp}(q_1, q_2, t) = \text{normalize}\big((1-t) \cdot q_1 + t \cdot q_2\big)
\]

```
Pseudocode:

function nlerp(q1, q2, t):
    // Ensure shortest path
    dot = q1.w*q2.w + q1.x*q2.x + q1.y*q2.y + q1.z*q2.z
    if dot < 0:
        q2 = -q2

    result = Quaternion(
        (1-t)*q1.w + t*q2.w,
        (1-t)*q1.x + t*q2.x,
        (1-t)*q1.y + t*q2.y,
        (1-t)*q1.z + t*q2.z
    )
    return normalize(result)
```

### NLERP vs SLERP

| Property | NLERP | SLERP |
|----------|-------|-------|
| Speed | Faster (no trig) | Slower |
| Constant angular velocity | No (but close for small angles) | Yes |
| Stays on unit sphere | After normalize | Naturally |
| Torque-minimal | Approximately | Exactly |
| When to use | Small angle differences, performance-critical paths | Large angle differences, animations where smoothness matters |

In practice, many game engines use **NLERP** for most cases (it's nearly indistinguishable for small rotations) and reserve **SLERP** for cinematics or large-angle blends.

---

## 6.11 Common Pitfalls

### 1. Double Cover: q and -q Are the Same Rotation

Every rotation is represented by **two** quaternions: \( q \) and \( -q \). Negating all four components gives the same rotation (it goes "the long way around" the 4D sphere, but lands in the same place).

This matters for interpolation: always check that \( q_1 \cdot q_2 \geq 0 \) before interpolating. If the dot product is negative, negate one quaternion to take the **shorter path**.

```
Without the check:              With the check:

  q1 ●────────────────● q2      q1 ●───● -q2
       (the long way)                (short way)

  The camera spins 270°          The camera spins 90°
  instead of 90°!                as expected.
```

### 2. Always Normalize After Arithmetic

After multiplying or adding quaternions, floating-point drift can push the magnitude away from 1.0. A non-unit quaternion will cause skewed or scaled rotations.

```
Pseudocode:

// After any quaternion arithmetic:
q = normalize(q)

function normalize(q):
    len = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z)
    return Quaternion(q.w/len, q.x/len, q.y/len, q.z/len)
```

Re-normalizing a quaternion is cheap (one `sqrt` and four divides), unlike re-orthogonalizing a matrix.

### 3. Quaternion Multiplication is NOT Commutative

\[
q_1 \cdot q_2 \neq q_2 \cdot q_1 \quad \text{(in general)}
\]

"Pitch then yaw" gives a different result from "yaw then pitch." Always think about the order.

### 4. Don't Interpolate Euler Angles

It's tempting to LERP Euler angles, but this produces erratic, wobbly motion and can hit gimbal lock. Always convert to quaternions, SLERP/NLERP, then convert back if needed.

### 5. Beware of Near-Zero Angles

When two quaternions are nearly identical (\( \cos\Omega \approx 1 \)), SLERP's \( \sin\Omega \) approaches zero, causing numerical instability. Fall back to NLERP in this case (most implementations do this automatically with a threshold like 0.9995).

---

## 6.12 Practical Usage

### Storing Rotations in Game Engines

Most engines store orientation as a quaternion internally:

- **Unity**: `Transform.rotation` is a `Quaternion`.
- **Unreal Engine**: `FQuat` for rotations, `FRotator` for Euler display.
- **Godot**: `Quaternion` type, with `Basis` for the matrix form.

The editor may display Euler angles for convenience, but the underlying data is a quaternion.

### Smooth Camera Follow

A third-person camera that smoothly follows a character's rotation:

```
Pseudocode:

function updateCameraFollow(camera, target, deltaTime):
    desiredRotation = target.rotation
    smoothSpeed = 5.0

    // Smoothly interpolate camera rotation toward target
    t = clamp(smoothSpeed * deltaTime, 0, 1)
    camera.rotation = slerp(camera.rotation, desiredRotation, t)

    // Position camera behind the target
    offset = rotateVector((0, 2, -5), camera.rotation)
    camera.position = target.position + offset
```

### First-Person Mouselook

```
Pseudocode:

function handleMouseLook(camera, deltaX, deltaY, sensitivity):
    yawAngle   = -deltaX * sensitivity
    pitchAngle = -deltaY * sensitivity

    // Yaw: rotate around WORLD up axis (prevents roll)
    qYaw = axisAngleToQuat((0, 1, 0), yawAngle)
    camera.rotation = quatMultiply(qYaw, camera.rotation)

    // Pitch: rotate around LOCAL right axis
    rightAxis = rotateVector((1, 0, 0), camera.rotation)
    qPitch = axisAngleToQuat(rightAxis, pitchAngle)
    camera.rotation = quatMultiply(qPitch, camera.rotation)

    camera.rotation = normalize(camera.rotation)
```

### Animation Blending

Blending between two animation poses at each bone:

```
Pseudocode:

function blendPoses(poseA, poseB, blendFactor):
    blendedPose = new Pose()

    for each bone in skeleton:
        // Blend position (simple LERP is fine for translation)
        blendedPose[bone].position = lerp(
            poseA[bone].position,
            poseB[bone].position,
            blendFactor
        )

        // Blend rotation (SLERP for smooth, artifact-free blending)
        blendedPose[bone].rotation = slerp(
            poseA[bone].rotation,
            poseB[bone].rotation,
            blendFactor
        )

    return blendedPose

// Blend 70% idle + 30% walk
currentPose = blendPoses(idlePose, walkPose, 0.3)
```

### Look-At Rotation

Compute a quaternion that rotates the forward direction \( (0,0,-1) \) to look at a target:

```
Pseudocode:

function lookAtQuat(from, to, worldUp = (0,1,0)):
    direction = normalize(to - from)
    forward = (0, 0, -1)

    dot = dot(forward, direction)

    if dot ≈ -1:    // looking exactly backward
        return axisAngleToQuat(worldUp, 180°)

    if dot ≈ 1:     // already looking at target
        return Quaternion(1, 0, 0, 0)

    axis = normalize(cross(forward, direction))
    angle = acos(dot)
    return axisAngleToQuat(axis, degrees(angle))
```

---

## 6.13 Summary and Cheat Sheet

### Creating Quaternions

| From | Formula |
|------|---------|
| Identity (no rotation) | \( q = (1, 0, 0, 0) \) |
| Axis + angle | \( q = (\cos\frac{\theta}{2},\; \sin\frac{\theta}{2} \cdot \hat{a}) \) |
| Rotation matrix | Use Shepperd's method (see §6.8) |

### Core Operations

| Operation | Formula | Notes |
|-----------|---------|-------|
| Multiply | \( q_1 q_2 = (w_1 w_2 - \mathbf{v_1}\!\cdot\!\mathbf{v_2},\; w_1\mathbf{v_2} + w_2\mathbf{v_1} + \mathbf{v_1}\!\times\!\mathbf{v_2}) \) | Combines rotations |
| Conjugate | \( q^* = (w, -\mathbf{v}) \) | Reverse rotation (if unit) |
| Inverse | \( q^{-1} = q^*/\|q\|^2 \) | For unit: \( q^{-1} = q^* \) |
| Rotate vector | \( \mathbf{v'} = q \mathbf{v} q^* \) | Treat \( \mathbf{v} \) as pure quat |
| Normalize | \( \hat{q} = q/\|q\| \) | Always do after arithmetic |

### Interpolation

| Method | Formula | Constant speed? | Cost |
|--------|---------|-----------------|------|
| SLERP | \( q_1 \frac{\sin((1-t)\Omega)}{\sin\Omega} + q_2 \frac{\sin(t\Omega)}{\sin\Omega} \) | Yes | 2× sin, 1× acos |
| NLERP | \( \text{normalize}((1-t)q_1 + t \cdot q_2) \) | Approximately | 1× sqrt |

### Key Rules

1. **Unit quaternions only** for rotations (\( |q| = 1 \)).
2. \( q \) and \( -q \) are the **same rotation** — check dot product before interpolating.
3. Multiplication is **not commutative** — order matters.
4. **Normalize** after arithmetic to prevent drift.
5. **SLERP** for smooth, constant-speed interpolation. **NLERP** when performance matters.
6. Store as quaternion, convert to matrix for the GPU.

### From Euler Angles to Quaternion

\[
q = q_{\text{yaw}} \cdot q_{\text{pitch}} \cdot q_{\text{roll}}
\]

Where each is constructed from axis-angle:

\[
q_{\text{yaw}} = (\cos\frac{\psi}{2},\; 0,\; \sin\frac{\psi}{2},\; 0)
\]
\[
q_{\text{pitch}} = (\cos\frac{\phi}{2},\; \sin\frac{\phi}{2},\; 0,\; 0)
\]
\[
q_{\text{roll}} = (\cos\frac{\rho}{2},\; 0,\; 0,\; \sin\frac{\rho}{2})
\]

### Quaternion to Euler Angles

```
Pseudocode:

function quatToEuler(q):
    // Roll (Z)
    sinr = 2 * (q.w * q.x + q.y * q.z)
    cosr = 1 - 2 * (q.x * q.x + q.y * q.y)
    roll = atan2(sinr, cosr)

    // Pitch (X)
    sinp = 2 * (q.w * q.y - q.z * q.x)
    if abs(sinp) >= 1:
        pitch = copysign(PI/2, sinp)   // clamp at ±90°
    else:
        pitch = asin(sinp)

    // Yaw (Y)
    siny = 2 * (q.w * q.z + q.x * q.y)
    cosy = 1 - 2 * (q.y * q.y + q.z * q.z)
    yaw = atan2(siny, cosy)

    return (pitch, yaw, roll)
```

### The Big Picture

```
Game rotation workflow:

  Designer input              Internal storage           GPU rendering
  (Euler angles) ──convert──▶ (Quaternion) ──convert──▶ (4×4 Matrix)

     Human-                    Compact,                  Hardware-
     friendly                  interpolatable,           optimized
                               no gimbal lock
```

Quaternions are the backbone of 3D rotation in modern game engines. They may seem abstract at first, but once you understand the axis-angle encoding and the handful of operations above, they become a powerful and intuitive tool.
