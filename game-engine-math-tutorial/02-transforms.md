# Chapter 2: Transforms

> Based on *Foundations of Game Engine Development, Volume 1: Mathematics* by Eric Lengyel

Transforms are the bread and butter of game engines. Every time a character moves, a camera rotates, or a model is scaled, a transform is being applied. This chapter covers how transforms convert geometric objects between coordinate systems, how to compose them, and the full machinery of rotations, reflections, scales, skews, homogeneous coordinates, and quaternions.

---

## 2.1 Coordinate Spaces

A game engine typically defines several coordinate systems:

- **World space** (global space): a fixed background coordinate system
- **Object space** (local space): each object's own independent coordinate system

When two objects interact, their positions must be expressed in a common coordinate system. This requires *transforming* vectors from one system to another.

### 2.1.1 Transformation Matrices

A position in a coordinate system is a vector from that system's origin. The components tell you how far to travel along each axis. When you transform from coordinate system A to system B, the components must change to account for the new origin and new axis orientations.

The transformation from position **p**_A in coordinate system A to position **p**_B in coordinate system B is:

`p_B = M × p_A + t`

where **M** is a 3×3 matrix that reorients the coordinate axes, and **t** is a 3D translation vector that moves the origin. This is called an **affine transformation**.

The inverse transformation (from B back to A) is:

`p_A = M⁻¹ × (p_B − t)`

The linear part `v_B = M * v_A` replaces the axes of system A with the columns of M in system B. If M = [**a** **b** **c**], then for an arbitrary vector **v**:

`v_B = M × v_A = vₓ × a + v_y × b + v_z × c`

**Worked Example: Affine Transformation**

Suppose system B is rotated 90° about the z-axis relative to system A and translated by **t** = (3, 1, 0).

```
M = |  0  -1   0 |    t = ( 3 )
    |  1   0   0 |        ( 1 )
    |  0   0   1 |        ( 0 )
```

Transform **p**_A = (2, 5, 0):

```
p_B = M × p_A + t

    = |  0  -1   0 | ( 2 )   ( 3 )
      |  1   0   0 | ( 5 ) + ( 1 )
      |  0   0   1 | ( 0 )   ( 0 )

    = ( -5 )   ( 3 )   ( -2 )
      (  2 ) + ( 1 ) = (  3 )
      (  0 )   ( 0 )   (  0 )
```

Inverse:

```
p_A = M⁻¹ × (p_B − t) = Mᵀ × ( -2 - 3 )
                               (  3 - 1 )
                               (  0 - 0 )

    = |  0   1   0 | ( -5 )   ( 2 )
      | -1   0   0 | (  2 ) = ( 5 ) ✓
      |  0   0   1 | (  0 )   ( 0 )
```

---

### 2.1.2 Orthogonal Transforms

An **orthogonal matrix** has columns that are mutually perpendicular unit-length vectors. The following statements are all equivalent:

- **M** is orthogonal
- **M**⁻¹ = **M**ᵀ (inverse equals transpose)
- The columns of **M** are mutually perpendicular unit-length vectors
- The rows of **M** are mutually perpendicular unit-length vectors

**Why M⁻¹ = Mᵀ?** If M = [**a** **b** **c**] where **a**, **b**, **c** are orthonormal:

```
MᵀM = | a·a  a·b  a·c |   | 1  0  0 |
       | b·a  b·b  b·c | = | 0  1  0 | = I
       | c·a  c·b  c·c |   | 0  0  1 |
```

**Key properties of orthogonal transforms:**

1. **Preserve dot products:** (M**a**) · (M**b**) = **a**ᵀMᵀM**b** = **a**ᵀ**b** = **a** · **b**
2. **Preserve lengths:** ‖M**v**‖ = ‖**v**‖ (since ‖**v**‖² = **v** · **v**)
3. **Preserve angles:** the angle between any two vectors is unchanged
4. **Determinant is ±1:** det(M) = +1 for pure rotations, det(M) = −1 when a reflection is included

**Worked Example: Verify Orthogonality**

```
M = | 1   0   0 |
    | 0   0  -1 |
    | 0   1   0 |
```

Columns: **c₀** = (1,0,0), **c₁** = (0,0,1), **c₂** = (0,−1,0).

- ‖**c₀**‖ = 1, ‖**c₁**‖ = 1, ‖**c₂**‖ = 1 ✓
- **c₀** · **c₁** = 0, **c₀** · **c₂** = 0, **c₁** · **c₂** = 0 ✓
- det(M) = 1(0·0 − (−1)·1) − 0 + 0 = 1 → pure rotation ✓

Inverse = transpose:

```
M⁻¹ = Mᵀ = | 1   0   0 |
             | 0   0   1 |
             | 0  -1   0 |
```

Verify: M × Mᵀ = I ✓ (this is a 90° rotation about the x-axis)

---

### 2.1.3 Transform Composition

When a vector **v** needs to be transformed first by **M₁** and then by **M₂**:

`v' = M₂ × (M₁ × v) = (M₂ × M₁) × v = N × v`

where **N** = **M₂M₁** is the combined matrix. This extends to any number of transforms: **N** = **Mₙ** ··· **M₂M₁**.

> **Important:** Matrices are applied right-to-left. **M₁** is applied first, then **M₂**.

**Changing the coordinate system of a transform:**

If a transform **A** works in coordinate system A, and **M** transforms from A to B, then the equivalent transform in system B is:

`B = M × A × M⁻¹`

Think of it as: transform from B to A (via M⁻¹), apply A, then transform back to B (via M).

**Worked Example: Composing Two Rotations**

Rotate 90° about the z-axis, then 90° about the x-axis.

```
M₁ = Rz(90°) = |  0  -1   0 |    M₂ = Rx(90°) = | 1   0   0 |
                |  1   0   0 |                     | 0   0  -1 |
                |  0   0   1 |                     | 0   1   0 |
```

```
N = M₂ × M₁ = | 1   0   0 | |  0  -1   0 |   |  0  -1   0 |
               | 0   0  -1 | |  1   0   0 | = |  0   0  -1 |
               | 0   1   0 | |  0   0   1 |   |  1   0   0 |
```

Apply to **v** = (1, 0, 0):

```
v' = N × v = ( 0 )
             ( 0 )
             ( 1 )
```

The x-axis unit vector ends up pointing along z, which makes sense: first it rotated to y (via Rz), then y rotated to z (via Rx).

**Worked Example: Changing Coordinate System of a Transform**

Suppose transform **A** scales by 2 along the x-axis in system A:

```
A = | 2  0  0 |
    | 0  1  0 |
    | 0  0  1 |
```

And M rotates 90° about the z-axis from A to B:

```
M = |  0  -1   0 |    M⁻¹ = Mᵀ = |  0   1   0 |
    |  1   0   0 |                 | -1   0   0 |
    |  0   0   1 |                 |  0   0   1 |
```

```
B = M × A × M⁻¹
```

First: M × A:

```
M × A = |  0  -1   0 |
        |  2   0   0 |
        |  0   0   1 |
```

Then: (M × A) × M⁻¹:

```
B = |  0  -1   0 | |  0   1   0 |   | 1  0  0 |
    |  2   0   0 | | -1   0   0 | = | 0  2  0 |
    |  0   0   1 | |  0   0   1 |   | 0  0  1 |
```

Result: **B** scales by 2 along the y-axis. Since the coordinate system was rotated 90° about z, the x-axis in A became the y-axis in B, so the scale "followed" the axis.

---

## 2.2 Rotations

A rotation occurs in a plane. In 3D, one axis remains fixed (the axis of rotation), and the rotation happens in the plane perpendicular to it.

**Convention (right-hand rule):** A positive angle produces a counterclockwise rotation when the axis of rotation points toward the viewer. Curl the fingers of your right hand in the direction of rotation, and your thumb points along the axis.

### 2.2.1 Rotation About a Coordinate Axis

#### Deriving the Z-Axis Rotation

Consider rotating **v** = vₓ**i** + v_y**j** + v_z**k** through angle θ about the z-axis. The z-component doesn't change. We need to see what happens to the x-y components.

**Step 1:** The vector vₓ**i** is rotated through angle θ. The result is a new vector of the same length vₓ, decomposed into components along **i** and **j**:

`vₓ × i  →(θ)→  vₓ·cosθ × i + vₓ·sinθ × j`

**Step 2:** The vector v_y**j** is also rotated through angle θ. Since **j** is 90° ahead of **i**, the result uses **j** and −**i**:

`v_y × j  →(θ)→  −v_y·sinθ × i + v_y·cosθ × j`

**Step 3:** Combine both rotated components plus the unchanged v_z**k**:

`v' = (vₓ·cosθ − v_y·sinθ) × i + (vₓ·sinθ + v_y·cosθ) × j + v_z × k`

Written as a matrix-vector product:

```
v' = | cosθ  -sinθ  0 |
     | sinθ   cosθ  0 | × v
     |  0      0    1 |
```

#### All Three Rotation Matrices

```
              | 1    0      0   |
Mrot,x(θ) =  | 0   cosθ  -sinθ |
              | 0   sinθ   cosθ |

              |  cosθ  0  sinθ |
Mrot,y(θ) =  |   0    1   0   |
              | -sinθ  0  cosθ |

              | cosθ  -sinθ  0 |
Mrot,z(θ) =  | sinθ   cosθ  0 |
              |  0      0    1 |
```

> **Note:** For the y-axis rotation, the negated sine is in the *lower-left* corner, not upper-right. The negated sine always appears one row below and one column to the left (with wraparound) of the 1 on the diagonal.

**C++ Implementation:**

```cpp
Matrix3D MakeRotationX(float t) {
    float c = cos(t);
    float s = sin(t);
    return Matrix3D(1.0f, 0.0f, 0.0f,
                    0.0f,    c,   -s,
                    0.0f,    s,    c);
}

Matrix3D MakeRotationY(float t) {
    float c = cos(t);
    float s = sin(t);
    return Matrix3D(   c, 0.0f,    s,
                    0.0f, 1.0f, 0.0f,
                      -s, 0.0f,    c);
}

Matrix3D MakeRotationZ(float t) {
    float c = cos(t);
    float s = sin(t);
    return Matrix3D(   c,   -s, 0.0f,
                       s,    c, 0.0f,
                    0.0f, 0.0f, 1.0f);
}
```

#### Numerical Examples

**Example 1: Rotate (1, 0, 0) by 30° about the z-axis**

cos 30° = √3/2 ≈ 0.866, sin 30° = 1/2 = 0.5

```
v' = | 0.866  -0.5  0 | ( 1 )   ( 0.866 )
     | 0.5   0.866  0 | ( 0 ) = ( 0.5   )
     | 0      0     1 | ( 0 )   ( 0     )
```

The unit x-vector tilts 30° toward the y-axis. Its length is still 1: √(0.866² + 0.5²) = √(0.75 + 0.25) = 1 ✓

**Example 2: Rotate (1, 1, 0) by 45° about the z-axis**

cos 45° = sin 45° = √2/2 ≈ 0.7071

```
v' = | 0.7071  -0.7071  0 | ( 1 )   ( 0.7071 − 0.7071 )   ( 0      )
     | 0.7071   0.7071  0 | ( 1 ) = ( 0.7071 + 0.7071 ) = ( 1.4142 )
     | 0        0       1 | ( 0 )   ( 0                )   ( 0      )
```

The vector (1, 1, 0) was at 45° from the x-axis. After a 45° rotation, it's now at 90° — pointing straight up along y. Length: √(1² + 1²) = √2 ≈ 1.4142 ✓

**Example 3: Rotate (0, 1, 0) by 90° about the x-axis**

cos 90° = 0, sin 90° = 1

```
v' = | 1  0   0 | ( 0 )   ( 0 )
     | 0  0  -1 | ( 1 ) = ( 0 )
     | 0  1   0 | ( 0 )   ( 1 )
```

The y-axis unit vector rotates to become the z-axis unit vector, as expected from a 90° rotation about x.

**Example 4: Rotate (1, 0, 0) by 90° about the y-axis**

```
v' = |  0  0  1 | ( 1 )   (  0 )
     |  0  1  0 | ( 0 ) = (  0 )
     | -1  0  0 | ( 0 )   ( -1 )
```

The x-axis vector goes to −z after a 90° rotation about y. Right-hand rule: thumb along +y, fingers curl from +x toward −z ✓

---

### 2.2.2 Rotation About an Arbitrary Axis

Given a unit vector **a** defining the axis and an angle θ, we decompose any vector **v** into:

- **v**∥ₐ = (**v** · **a**)**a** — the component parallel to **a** (unchanged)
- **v**⊥ₐ = **v** − (**v** · **a**)**a** — the component perpendicular to **a** (rotated)

The rotated vector is:

`v' = v_parallel + v_perp × cosθ + (a × v) × sinθ`

Expanding the projection/rejection:

`v' = v·cosθ + (v · a)·a·(1 − cosθ) + (a × v)·sinθ`

**The full rotation matrix** (using c = cos θ, s = sin θ):

```
Mrot(θ, a) =

| c + (1−c)aₓ²        (1−c)aₓa_y − s·a_z   (1−c)aₓa_z + s·a_y |
| (1−c)aₓa_y + s·a_z  c + (1−c)a_y²         (1−c)a_ya_z − s·aₓ |
| (1−c)aₓa_z − s·a_y  (1−c)a_ya_z + s·aₓ   c + (1−c)a_z²       |
```

**C++ Implementation:**

```cpp
Matrix3D MakeRotation(float t, const Vector3D& a) {
    float c = cos(t);
    float s = sin(t);
    float d = 1.0f - c;

    float x = a.x * d;
    float y = a.y * d;
    float z = a.z * d;

    float axay = x * a.y;
    float axaz = x * a.z;
    float ayaz = y * a.z;

    return Matrix3D(c + x * a.x,  axay - s * a.z,  axaz + s * a.y,
                    axay + s * a.z,  c + y * a.y,  ayaz - s * a.x,
                    axaz - s * a.y,  ayaz + s * a.x,  c + z * a.z);
}
```

**Worked Example: Rotate (1, 0, 0) by 90° about the axis (1/√3, 1/√3, 1/√3)**

This axis points equally in all three directions. θ = 90°, so c = cos 90° = 0, s = sin 90° = 1.

With aₓ = a_y = a_z = 1/√3:

- aₓ² = a_y² = a_z² = 1/3
- aₓa_y = aₓa_z = a_ya_z = 1/3
- (1 − c) = 1

```
M = | 0 + 1/3        1/3 − 1/√3     1/3 + 1/√3  |
    | 1/3 + 1/√3     0 + 1/3        1/3 − 1/√3   |
    | 1/3 − 1/√3     1/3 + 1/√3     0 + 1/3       |
```

Numerically (1/3 ≈ 0.333, 1/√3 ≈ 0.577):

```
M ≈ |  0.333  -0.244   0.911 |
    |  0.911   0.333  -0.244 |
    | -0.244   0.911   0.333 |
```

Apply to **v** = (1, 0, 0):

```
v' = ( 0.333  )     ( 1/3             )
     ( 0.911  )  ≈  ( 1/3 + 1/√3      )
     ( -0.244 )     ( 1/3 − 1/√3      )
```

Check: ‖**v**'‖ = √(0.333² + 0.911² + 0.244²) = √(0.111 + 0.830 + 0.060) = √1.001 ≈ 1 ✓

**Sanity check:** A 120° rotation about (1,1,1)/√3 would cycle the axes: x→y→z→x. At 90° we get something in between — the x-axis vector picks up a large y-component and smaller z-component. This matches the result.

**Another check:** The axis itself is invariant. Let **v** = (1/√3, 1/√3, 1/√3):

```
v' = M × v = ( (0.333 + (−0.244) + 0.911)/√3 )   ( 1/√3 )
             ( (0.911 + 0.333 + (−0.244))/√3  ) = ( 1/√3 ) ✓
             ( ((−0.244) + 0.911 + 0.333)/√3  )   ( 1/√3 )
```

Each row sums to 1, confirming that the axis vector is unchanged.

---

## 2.3 Reflections

A reflection through a plane perpendicular to unit vector **a** negates the component of a vector parallel to **a** and preserves the perpendicular component.

`v' = v_perp_a − v_parallel_a`

**Reflection matrix:**

```
Mreflect(a) = I − 2(a ⊗ a)

= | 1 − 2aₓ²     −2aₓa_y     −2aₓa_z  |
  | −2aₓa_y     1 − 2a_y²     −2a_ya_z  |
  | −2aₓa_z     −2a_ya_z     1 − 2a_z²  |
```

where **a** ⊗ **a** is the outer product (a 3×3 matrix where entry (i,j) = aᵢaⱼ).

**Properties:**
- **det(Mreflect) = −1** always (reflections reverse handedness)
- **M²reflect = I** (reflecting twice returns to the original — an involution)
- The determinant is −1 because Mreflect = I − 2(**a** ⊗ **a**), and det(I + **a** ⊗ **b**) = 1 + **a** · **b**, so det(I − 2**a** ⊗ **a**) = 1 + (−2**a**) · **a** = 1 − 2 = −1.

**C++ Implementation:**

```cpp
Matrix3D MakeReflection(const Vector3D& a) {
    float x = a.x * -2.0f;
    float y = a.y * -2.0f;
    float z = a.z * -2.0f;
    float axay = x * a.y;
    float axaz = x * a.z;
    float ayaz = y * a.z;

    return Matrix3D(x * a.x + 1.0f, axay,           axaz,
                    axay,           y * a.y + 1.0f,  ayaz,
                    axaz,           ayaz,            z * a.z + 1.0f);
}
```

**Involution** (negate the perpendicular component, preserve the parallel):

`v' = v_parallel_a − v_perp_a`

```
Minvol(a) = 2(a ⊗ a) − I

= | 2aₓ² − 1     2aₓa_y      2aₓa_z   |
  | 2aₓa_y      2a_y² − 1     2a_ya_z   |
  | 2aₓa_z      2a_ya_z      2a_z² − 1  |
```

This is just −Mreflect. In 3D, an involution composes two reflections, equivalent to a 180° rotation about **a**. Its determinant is +1 (in odd dimensions).

**Worked Example: Reflect (3, 4, 0) through the plane perpendicular to (0, 1, 0)**

Here **a** = (0, 1, 0), so we're reflecting through the xz-plane.

```
M = | 1 − 0    0     0   |   | 1   0   0 |
    |  0     1 − 2    0   | = | 0  -1   0 |
    |  0       0    1 − 0 |   | 0   0   1 |
```

```
v' = M × v = (  3 )
             ( -4 )
             (  0 )
```

The y-component was flipped. Length preserved: √(9 + 16) = 5 for both ✓

**Worked Example: Reflect (1, 1, 0) through the plane perpendicular to (1/√2, 1/√2, 0)**

**a** = (1/√2, 1/√2, 0), so aₓ² = a_y² = 1/2, aₓa_y = 1/2.

```
M = | 1 − 1    −1     0 |   |  0  -1   0 |
    |  −1    1 − 1     0 | = | -1   0   0 |
    |   0      0      1 |   |  0   0   1 |
```

```
v' = |  0  -1   0 | ( 1 )   ( -1 )
     | -1   0   0 | ( 1 ) = ( -1 )
     |  0   0   1 | ( 0 )   (  0 )
```

The vector (1, 1, 0) is parallel to **a** = (1/√2, 1/√2, 0), so its reflection is completely negated. This makes sense: reflecting a vector that lies entirely along the normal direction flips it 180°.

det(M) = 0·0 − (−1)(−1) + 0 = −1 ✓

**Worked Example: Verify M² = I**

Using the same M:

```
M² = |  0  -1   0 | |  0  -1   0 |   | 1  0  0 |
     | -1   0   0 | | -1   0   0 | = | 0  1  0 | = I ✓
     |  0   0   1 | |  0   0   1 |   | 0  0  1 |
```

---

## 2.4 Scales

### Uniform Scale

Multiply all components by the same factor *s*:

```
v' = s × v = | s  0  0 |
             | 0  s  0 | × v
             | 0  0  s |
```

### Non-Uniform Scale Along Axes

Different scale factors along x, y, z:

```
Mscale = | sₓ   0    0  |
         |  0   s_y   0  |
         |  0    0   s_z |
```

```cpp
Matrix3D MakeScale(float sx, float sy, float sz) {
    return Matrix3D(sx, 0.0f, 0.0f,
                    0.0f, sy, 0.0f,
                    0.0f, 0.0f, sz);
}
```

### Scale Along an Arbitrary Direction

To scale by factor *s* along direction **a** (unit vector) while preserving size in perpendicular directions, decompose **v** into parallel and perpendicular components and scale only the parallel part:

`v' = s × v_parallel_a + v_perp_a`

```
Mscale(s, a) =

| (s−1)aₓ² + 1    (s−1)aₓa_y      (s−1)aₓa_z   |
| (s−1)aₓa_y      (s−1)a_y² + 1    (s−1)a_ya_z   |
| (s−1)aₓa_z      (s−1)a_ya_z      (s−1)a_z² + 1 |
```

This can also be written as: M = I + (s − 1)(a ⊗ a)

```cpp
Matrix3D MakeScale(float s, const Vector3D& a) {
    float d = s - 1.0f;
    float x = a.x * d;
    float y = a.y * d;
    float z = a.z * d;
    float axay = x * a.y;
    float axaz = x * a.z;
    float ayaz = y * a.z;

    return Matrix3D(x * a.x + 1.0f, axay,           axaz,
                    axay,           y * a.y + 1.0f,  ayaz,
                    axaz,           ayaz,            z * a.z + 1.0f);
}
```

**Worked Example: Uniform Scale**

Scale (2, 3, 1) by factor 4:

```
v' = 4 × ( 2 ) = (  8 )
         ( 3 )   ( 12 )
         ( 1 )   (  4 )
```

**Worked Example: Non-Uniform Scale**

Scale (2, 3, 1) with sₓ = 2, s_y = 0.5, s_z = 3:

```
v' = | 2    0    0 | ( 2 )   (  4  )
     | 0   0.5   0 | ( 3 ) = ( 1.5 )
     | 0    0    3 | ( 1 )   (  3  )
```

**Worked Example: Scale by 3 Along Direction (0, 1/√2, 1/√2)**

Scale **v** = (1, 2, 0) by s = 3 along **a** = (0, 1/√2, 1/√2).

With s − 1 = 2, aₓ = 0, a_y = a_z = 1/√2:

```
M = | 0 + 1      0         0      |   | 1  0  0 |
    |   0     2(1/2) + 1  2(1/2)  | = | 0  2  1 |
    |   0      2(1/2)    2(1/2)+1 |   | 0  1  2 |
```

```
v' = | 1  0  0 | ( 1 )   ( 1 )
     | 0  2  1 | ( 2 ) = ( 4 )
     | 0  1  2 | ( 0 )   ( 2 )
```

Verify: **v**∥ = (**v** · **a**)**a** = (0 + 2/√2 + 0)(0, 1/√2, 1/√2) = √2(0, 1/√2, 1/√2) = (0, 1, 1).
Scaled parallel: 3(0, 1, 1) = (0, 3, 3).
Perpendicular: **v** − **v**∥ = (1, 1, −1).
Result: (0, 3, 3) + (1, 1, −1) = (1, 4, 2) ✓

---

## 2.5 Skews

A **skew** (shear) slides an object along one direction based on how far it extends in a perpendicular direction.

Given unit vectors **a** (skew direction) and **b** (measurement direction, perpendicular to **a**), and skew angle θ:

`v' = v + a × (b · v) × tanθ`

The factor **b** · **v** measures how far **v** extends along **b**, and tan θ converts that distance to the amount of skew along **a**.

**Skew matrix:**

`Mskew(θ, a, b) = I + tanθ × (a ⊗ b)`

```
= | aₓbₓ·tanθ + 1    aₓb_y·tanθ       aₓb_z·tanθ      |
  | a_ybₓ·tanθ       a_yb_y·tanθ + 1   a_yb_z·tanθ      |
  | a_zbₓ·tanθ       a_zb_y·tanθ       a_zb_z·tanθ + 1  |
```

**Volume preservation:** Since **a** · **b** = 0 (they are perpendicular), det(Mskew) = 1 + tan θ(**a** · **b**) = 1. Skews preserve volume!

When **a** and **b** are aligned to coordinate axes, the matrix simplifies greatly. For example, **a** = (1, 0, 0), **b** = (0, 1, 0):

```
Mskew(θ, i, j) = | 1  tanθ  0 |
                  | 0   1    0 |
                  | 0   0    1 |
```

```cpp
Matrix3D MakeSkew(float t, const Vector3D& a, const Vector3D& b) {
    t = tan(t);
    float x = a.x * t;
    float y = a.y * t;
    float z = a.z * t;

    return Matrix3D(x * b.x + 1.0f, x * b.y,        x * b.z,
                    y * b.x,        y * b.y + 1.0f,  y * b.z,
                    z * b.x,        z * b.y,         z * b.z + 1.0f);
}
```

**Worked Example: Skew Along x Based on y, θ = 45°**

**a** = (1,0,0), **b** = (0,1,0), tan 45° = 1.

```
M = | 1  1  0 |
    | 0  1  0 |
    | 0  0  1 |
```

Apply to **v** = (2, 3, 1):

```
v' = | 1  1  0 | ( 2 )   ( 2 + 3 )   ( 5 )
     | 0  1  0 | ( 3 ) = (   3   ) = ( 3 )
     | 0  0  1 | ( 1 )   (   1   )   ( 1 )
```

The x-component increased by 3 (the y-value times tan 45° = 1), while y and z are unchanged.

det(M) = 1 ✓ (volume preserved)

**Worked Example: Skew Along y Based on z, θ = 30°**

**a** = (0,1,0), **b** = (0,0,1), tan 30° ≈ 0.577.

```
M = | 1    0     0   |
    | 0    1   0.577 |
    | 0    0     1   |
```

Apply to **v** = (1, 0, 4):

```
v' = (         1         )   (   1   )
     ( 0 + 4 × 0.577     ) = ( 2.309 )
     (         4         )   (   4   )
```

---

## 2.6 Homogeneous Coordinates

Until now, all transforms have been centered at the origin. To handle *translations* (moving objects to new positions), we extend to **4D homogeneous coordinates**.

### The w Coordinate

Append a fourth component *w* to every vector:

| Type | 4D Representation | w value |
|------|-------------------|---------|
| **Direction vector** | (vₓ, v_y, v_z, **0**) | 0 |
| **Position vector (point)** | (pₓ, p_y, p_z, **1**) | 1 |
| **Point at infinity** | (x, y, z, **0**) | 0 |

For a general homogeneous vector (x, y, z, w) with w ≠ 0, the corresponding 3D point is (x/w, y/w, z/w). Any nonzero scalar multiple of a 4D vector represents the same 3D point.

If w = 0, the vector represents a direction, or equivalently a "point at infinity" — useful for things like the sun's direction.

### 4×4 Transformation Matrix

The affine transformation **p**_B = M**p**_A + **t** can be expressed as a single 4×4 matrix:

```
H = | M₀₀  M₀₁  M₀₂  tₓ |
    | M₁₀  M₁₁  M₁₂  t_y |
    | M₂₀  M₂₁  M₂₂  t_z |
    |  0    0    0    1   |
```

The upper-left 3×3 block is M, the rightmost column (top 3 entries) is the translation **t**, and the bottom row is always [0 0 0 1].

**Key behavior:**

- **Position vectors** (w = 1): Transformed by both M and the translation **t**. The fourth row [0 0 0 1] preserves w = 1 in the result.
- **Direction vectors** (w = 0): Only transformed by M. The translation has no effect because the fourth column is multiplied by w = 0. Directions carry no position information.

**Composition:** Multiplying matrices of this form always produces another matrix of this form (the fourth row stays [0 0 0 1]).

**Meaning of columns:** The first three columns are the object's local x, y, z axes expressed in the global coordinate system. The fourth column is the object's local origin expressed in global coordinates.

### Inverse of a 4×4 Transform

```
H⁻¹ = | M⁻¹       −M⁻¹ × t |
       | 0  0  0       1     |
```

This inverts the rotation/scale via M⁻¹ and reverses the translation by computing −M⁻¹**t**.

### Point3D and Transform4D Data Structures

```cpp
struct Point3D : Vector3D {
    Point3D() = default;
    Point3D(float a, float b, float c) : Vector3D(a, b, c) {}
};

// Point + Direction = Point
inline Point3D operator+(const Point3D& a, const Vector3D& b) {
    return Point3D(a.x + b.x, a.y + b.y, a.z + b.z);
}

// Point - Point = Direction
inline Vector3D operator-(const Point3D& a, const Point3D& b) {
    return Vector3D(a.x - b.x, a.y - b.y, a.z - b.z);
}
```

The `Transform4D` structure inherits from `Matrix4D` but assumes the fourth row is always [0 0 0 1]:

```cpp
struct Transform4D : Matrix4D {
    // Constructors set fourth row to [0 0 0 1]
    Transform4D(const Vector3D& a, const Vector3D& b,
                const Vector3D& c, const Point3D& p);

    // Transform a direction vector (w = 0): only upper-left 3x3
    friend Vector3D operator*(const Transform4D& H, const Vector3D& v);

    // Transform a position vector (w = 1): includes translation
    friend Point3D operator*(const Transform4D& H, const Point3D& p);
};

// Direction: only upper-left 3x3 block
Vector3D operator*(const Transform4D& H, const Vector3D& v) {
    return Vector3D(
        H(0,0)*v.x + H(0,1)*v.y + H(0,2)*v.z,
        H(1,0)*v.x + H(1,1)*v.y + H(1,2)*v.z,
        H(2,0)*v.x + H(2,1)*v.y + H(2,2)*v.z);
}

// Point: upper-left 3x3 block + translation column
Point3D operator*(const Transform4D& H, const Point3D& p) {
    return Point3D(
        H(0,0)*p.x + H(0,1)*p.y + H(0,2)*p.z + H(0,3),
        H(1,0)*p.x + H(1,1)*p.y + H(1,2)*p.z + H(1,3),
        H(2,0)*p.x + H(2,1)*p.y + H(2,2)*p.z + H(2,3));
}
```

### Worked Examples

**Example 1: Transform a Point**

Rotation = 90° about z-axis, Translation = (10, 5, 0).

```
H = |  0  -1   0  10 |
    |  1   0   0   5 |
    |  0   0   1   0 |
    |  0   0   0   1 |
```

Transform point **p** = (3, 4, 0) (w = 1):

```
H × ( 3 )   ( 0·3 + (−1)·4 + 0·0 + 10·1 )   (  6 )
    ( 4 ) = ( 1·3 +   0·4  + 0·0 +  5·1 ) = (  8 )
    ( 0 )   ( 0·3 +   0·4  + 1·0 +  0·1 )   (  0 )
    ( 1 )   (                  1          )   (  1 )
```

Result: point (6, 8, 0). The point was rotated AND translated.

**Example 2: Transform a Direction (Same Matrix)**

Transform direction **v** = (3, 4, 0) (w = 0):

```
H × ( 3 )   ( 0·3 + (−1)·4 )   ( -4 )
    ( 4 ) = ( 1·3 +   0·4  ) = (  3 )
    ( 0 )   (      0        )   (  0 )
    ( 0 )   (      0        )   (  0 )
```

Result: direction (−4, 3, 0). The direction was *only rotated* — the translation had no effect.

**Example 3: Computing H⁻¹**

```
M = |  0  -1   0 |      M⁻¹ = Mᵀ = |  0   1   0 |      t = ( 10 )
    |  1   0   0 |                   | -1   0   0 |          (  5 )
    |  0   0   1 |                   |  0   0   1 |          (  0 )
```

```
−M⁻¹ × t = − |  0   1   0 | ( 10 )     ( 5  )   ( -5 )
              | -1   0   0 | (  5 ) = − (-10 ) = ( 10 )
              |  0   0   1 | (  0 )     ( 0  )   (  0 )
```

```
H⁻¹ = |  0   1   0  -5 |
       | -1   0   0  10 |
       |  0   0   1   0 |
       |  0   0   0   1 |
```

Verify: H⁻¹ × (6, 8, 0, 1)ᵀ should give back (3, 4, 0, 1)ᵀ:

```
H⁻¹ × ( 6 )   ( 0·6 + 1·8 + 0 − 5  )   ( 3 )
       ( 8 ) = ( −1·6 + 0·8 + 0 + 10 ) = ( 4 ) ✓
       ( 0 )   ( 0 + 0 + 0 + 0       )   ( 0 )
       ( 1 )   (         1            )   ( 1 )
```

**Example 4: Point + Direction, Point − Point**

Point **p** = (3, 4, 0) as 4D: (3, 4, 0, 1)
Direction **v** = (1, −1, 0) as 4D: (1, −1, 0, 0)

**p** + **v** = (4, 3, 0, **1**) → a new point ✓

Point **q** = (7, 2, 0) as 4D: (7, 2, 0, 1)
**q** − **p** = (4, −2, 0, **0**) → a direction vector ✓

---

## 2.7 Quaternions

Quaternions were discovered by William Rowan Hamilton in 1843. They provide a compact, numerically stable way to represent and compose rotations.

### 2.7.1 Quaternion Fundamentals

A quaternion **q** has four components:

`q = x·i + y·j + z·k + w`

where i, j, k are imaginary units and w is the real (scalar) part. We write **q** = **v** + *s*, where **v** = (x, y, z) is the **vector part** and *s* = w is the **scalar part**.

#### Multiplication Rules

Hamilton's fundamental equation:

`i² = j² = k² = ijk = −1`

More explicitly:

| Product | Result |
|---------|--------|
| **ij** | **k** |
| **ji** | −**k** |
| **jk** | **i** |
| **kj** | −**i** |
| **ki** | **j** |
| **ik** | −**j** |

> **Quaternion multiplication is NOT commutative!** Reversing the order of two imaginary units negates the result.

#### General Product Formula

For **q₁** = x₁i + y₁j + z₁k + w₁ and **q₂** = x₂i + y₂j + z₂k + w₂:

```
q₁q₂ = (x₁w₂ + y₁z₂ − z₁y₂ + w₁x₂) i
      + (y₁w₂ + z₁x₂ + w₁y₂ − x₁z₂) j
      + (z₁w₂ + w₁z₂ + x₁y₂ − y₁x₂) k
      + (w₁w₂ − x₁x₂ − y₁y₂ − z₁z₂)
```

#### Product Using Vector/Scalar Parts

Using **q₁** = **v₁** + s₁ and **q₂** = **v₂** + s₂:

```
q₁q₂ = (s₁·v₂ + s₂·v₁ + v₁ × v₂)  ← vector part
      + (s₁·s₂ − v₁ · v₂)           ← scalar part
```

The cross product **v₁** × **v₂** is the *only* non-commutative piece. Two quaternions commute only if their vector parts are parallel (making the cross product zero).

Reversing the order changes the product by twice the cross product:

`q₁q₂ − q₂q₁ = 2(v₁ × v₂)`

#### Conjugate

`q* = −v + s`

Negate the vector part, keep the scalar part. Analogous to complex conjugation.

#### Magnitude

`‖q‖ = √(x² + y² + z² + w²)`

Note: **qq*** = **q*q** = ‖**q**‖² (always a non-negative real number).

#### Inverse

`q⁻¹ = q* / ‖q‖² = (−v + s) / (x² + y² + z² + w²)`

For a **unit quaternion** (‖**q**‖ = 1): **q**⁻¹ = **q***

```cpp
struct Quaternion {
    float x, y, z, w;

    Quaternion() = default;
    Quaternion(float a, float b, float c, float s)
        : x(a), y(b), z(c), w(s) {}

    Quaternion(const Vector3D& v, float s)
        : x(v.x), y(v.y), z(v.z), w(s) {}

    const Vector3D& GetVectorPart() const {
        return reinterpret_cast<const Vector3D&>(x);
    }
};

Quaternion operator*(const Quaternion& q1, const Quaternion& q2) {
    return Quaternion(
        q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y,
        q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x,
        q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w,
        q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z);
}
```

#### Numerical Example: Quaternion Multiplication

Let **q₁** = 1**i** + 2**j** + 3**k** + 4 (v₁ = (1,2,3), s₁ = 4)
and **q₂** = 5**i** + 6**j** + 7**k** + 8 (v₂ = (5,6,7), s₂ = 8).

Using the vector/scalar formula:

**Vector part** = s₁**v₂** + s₂**v₁** + **v₁** × **v₂**

- s₁**v₂** = 4(5, 6, 7) = (20, 24, 28)
- s₂**v₁** = 8(1, 2, 3) = (8, 16, 24)
- **v₁** × **v₂** = (2·7 − 3·6, 3·5 − 1·7, 1·6 − 2·5) = (14 − 18, 15 − 7, 6 − 10) = (−4, 8, −4)
- Sum: (20 + 8 − 4, 24 + 16 + 8, 28 + 24 − 4) = **(24, 48, 48)**

**Scalar part** = s₁s₂ − **v₁** · **v₂** = 4·8 − (1·5 + 2·6 + 3·7) = 32 − (5 + 12 + 21) = 32 − 38 = **−6**

`q₁q₂ = 24i + 48j + 48k + (−6)`

Let's verify using the component formula:

- x: 1·8 + 2·7 − 3·6 + 4·5 = 8 + 14 − 18 + 20 = **24** ✓
- y: 2·8 + 3·5 + 4·6 − 1·7 = 16 + 15 + 24 − 7 = **48** ✓
- z: 3·8 + 4·7 + 1·6 − 2·5 = 24 + 28 + 6 − 10 = **48** ✓
- w: 4·8 − 1·5 − 2·6 − 3·7 = 32 − 5 − 12 − 21 = **−6** ✓

**Numerical Example: Conjugate and Inverse**

**q** = 1**i** + 2**j** + 3**k** + 4

- **q*** = −1**i** − 2**j** − 3**k** + 4
- ‖**q**‖² = 1 + 4 + 9 + 16 = 30
- ‖**q**‖ = √30 ≈ 5.477
- **q**⁻¹ = (−1, −2, −3, 4) / 30 = (−1/30, −2/30, −3/30, 4/30) ≈ (−0.033, −0.067, −0.100, 0.133)

Verify: **qq**⁻¹ should equal 1. Using our product formula with **q** and **q***/ 30:

**qq*** = (1² + 2² + 3² + 4²) = 30 (a pure real quaternion)
30 / 30 = 1 ✓

---

### 2.7.2 Rotations With Quaternions

#### The Sandwich Product

Given a unit quaternion **q** and a 3D vector **v** (treated as the pure quaternion vₓi + v_yj + v_zk), the rotation is:

`v' = q × v × q*`

This is called the **sandwich product**. The magnitude of **q** doesn't matter (it cancels between **q** and **q**⁻¹), but using unit quaternions simplifies computation since **q**⁻¹ = **q***.

For unit **q** = **b** + c (vector part **b**, scalar part c), the sandwich product expands to:

`q × v × q* = (c² − b²)·v + 2·(v · b)·b + 2c·(b × v)`

```cpp
Vector3D Transform(const Vector3D& v, const Quaternion& q) {
    const Vector3D& b = q.GetVectorPart();
    float b2 = b.x*b.x + b.y*b.y + b.z*b.z;
    return v * (q.w*q.w - b2)
         + b * (Dot(v, b) * 2.0f)
         + Cross(b, v) * (q.w * 2.0f);
}
```

#### Quaternion for a Given Rotation

The quaternion that rotates through angle θ about unit axis **a** is:

`q = sin(θ/2) × a + cos(θ/2)`

Note the **half angle** — this is not a typo! The quaternion uses θ/2 internally. The sandwich product effectively doubles the angle.

#### Composing Rotations

To first rotate by **q₁**, then by **q₂**:

`v'' = q₂ × (q₁ × v × q₁*) × q₂* = (q₂q₁) × v × (q₂q₁)*`

The combined rotation is **q₂q₁** — just multiply the quaternions! This costs 16 multiplies + 12 adds, compared to 27 multiplies + 18 adds for 3×3 matrix multiplication.

#### q and −q Represent the Same Rotation

Negating both **b** and c in the sandwich product cancels out (each term has two factors of **q**). Geometrically, −**q** corresponds to a full 2π rotation combined with **q**'s rotation, which gives the same net result.

This can be exploited for storage: always ensure w ≥ 0 (negate **q** if needed), then only store (x, y, z) and recover w = √(1 − x² − y² − z²).

#### Advantages of Quaternions Over Matrices

| Property | Quaternion | 3×3 Matrix |
|----------|-----------|------------|
| Storage | 4 floats | 9 floats |
| Composition cost | 16 mul + 12 add | 27 mul + 18 add |
| Interpolation | Smooth (SLERP) | Problematic |
| Normalization | Normalize 4 components | Re-orthogonalize 9 entries |

#### Converting Quaternion to 3×3 Matrix

For **q** = xi + yj + zk + w:

```
Mrot(q) =

| 1 − 2(y² + z²)    2(xy − wz)       2(xz + wy)    |
| 2(xy + wz)        1 − 2(x² + z²)    2(yz − wx)    |
| 2(xz − wy)        2(yz + wx)       1 − 2(x² + y²) |
```

```cpp
Matrix3D Quaternion::GetRotationMatrix() {
    float x2 = x * x, y2 = y * y, z2 = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;

    return Matrix3D(
        1.0f - 2.0f*(y2 + z2),  2.0f*(xy - wz),        2.0f*(xz + wy),
        2.0f*(xy + wz),         1.0f - 2.0f*(x2 + z2),  2.0f*(yz - wx),
        2.0f*(xz - wy),         2.0f*(yz + wx),         1.0f - 2.0f*(x2 + y2));
}
```

#### Converting 3×3 Matrix to Quaternion

From the matrix entries, we use these relationships:

| Equation | Source |
|----------|--------|
| M₂₁ − M₁₂ = 4wx | off-diagonal differences |
| M₀₂ − M₂₀ = 4wy | |
| M₁₀ − M₀₁ = 4wz | |
| M₀₀ + M₁₁ + M₂₂ = 3 − 4(x² + y² + z²) | diagonal sum |

**Algorithm (handling numerical stability):**

1. Compute sum = M₀₀ + M₁₁ + M₂₂
2. If sum > 0: w is the largest component. Compute w = √(sum + 1) / 2, then x, y, z via the off-diagonal differences divided by 4w.
3. Otherwise, find the largest diagonal entry and compute the corresponding component first:
   - If M₀₀ is largest: x = √(M₀₀ − M₁₁ − M₂₂ + 1) / 2, then compute y, z, w
   - If M₁₁ is largest: y = √(M₁₁ − M₀₀ − M₂₂ + 1) / 2, then compute x, z, w
   - If M₂₂ is largest: z = √(M₂₂ − M₀₀ − M₁₁ + 1) / 2, then compute x, y, w

This avoids dividing by a very small number.

```cpp
void Quaternion::SetRotationMatrix(const Matrix3D& m) {
    float m00 = m(0,0), m11 = m(1,1), m22 = m(2,2);
    float sum = m00 + m11 + m22;

    if (sum > 0.0f) {
        w = sqrt(sum + 1.0f) * 0.5f;
        float f = 0.25f / w;
        x = (m(2,1) - m(1,2)) * f;
        y = (m(0,2) - m(2,0)) * f;
        z = (m(1,0) - m(0,1)) * f;
    } else if (m00 > m11 && m00 > m22) {
        x = sqrt(m00 - m11 - m22 + 1.0f) * 0.5f;
        float f = 0.25f / x;
        y = (m(1,0) + m(0,1)) * f;
        z = (m(0,2) + m(2,0)) * f;
        w = (m(2,1) - m(1,2)) * f;
    } else if (m11 > m22) {
        y = sqrt(m11 - m00 - m22 + 1.0f) * 0.5f;
        float f = 0.25f / y;
        x = (m(1,0) + m(0,1)) * f;
        z = (m(2,1) + m(1,2)) * f;
        w = (m(0,2) - m(2,0)) * f;
    } else {
        z = sqrt(m22 - m00 - m11 + 1.0f) * 0.5f;
        float f = 0.25f / z;
        x = (m(0,2) + m(2,0)) * f;
        y = (m(2,1) + m(1,2)) * f;
        w = (m(1,0) - m(0,1)) * f;
    }
}
```

---

### Quaternion Worked Examples

**Example 1: Create a Quaternion for 90° Rotation About the Z-Axis**

θ = 90°, **a** = (0, 0, 1)

`q = sin(45°) × (0, 0, 1) + cos(45°) = (√2/2) × (0, 0, 1) + √2/2`

`q = (0, 0, 0.7071, 0.7071)`

Verify: ‖**q**‖ = √(0 + 0 + 0.5 + 0.5) = 1 ✓

**Example 2: Apply That Quaternion to v = (1, 0, 0)**

Using the formula: `v' = (c² − b²)·v + 2·(v · b)·b + 2c·(b × v)`

- **b** = (0, 0, 0.7071), c = 0.7071
- c² = 0.5, b² = 0.5
- (c² − b²) = 0 → first term is **0**
- **v** · **b** = 0 → second term is **0**
- **b** × **v** = (0, 0, 0.7071) × (1, 0, 0) = (0·0 − 0.7071·0, 0.7071·1 − 0·0, 0·0 − 0·1) = (0, 0.7071, 0)
- 2c(**b** × **v**) = 2 · 0.7071 · (0, 0.7071, 0) = (0, 1, 0)

`v' = (0, 0, 0) + (0, 0, 0) + (0, 1, 0) = (0, 1, 0)`

A 90° rotation about z takes the x-axis to the y-axis ✓

**Example 3: Compose Two 90° Rotations (z then x)**

**q₁** = 90° about z: (0, 0, sin 45°, cos 45°) = (0, 0, 0.7071, 0.7071)
**q₂** = 90° about x: (sin 45°, 0, 0, cos 45°) = (0.7071, 0, 0, 0.7071)

Combined = **q₂q₁**:

Using the product formula:
- x: 0.7071·0.7071 + 0·0 + 0·0.7071 − 0·0 = 0.5
- y: 0·0.7071 + 0·0 + 0.7071·0 + 0.7071·0.7071 = 0.5
- z: 0·0.7071 + 0.7071·0.7071 + 0.7071·0 − 0·0 = 0.5
- w: 0.7071·0.7071 − 0.7071·0 − 0·0.7071 − 0·0 = 0.5

`q_combined = (0.5, 0.5, 0.5, 0.5)`

‖**q**_combined‖ = √(0.25 × 4) = 1 ✓

Let's verify by applying to **v** = (1, 0, 0):

- **b** = (0.5, 0.5, 0.5), c = 0.5
- c² = 0.25, b² = 0.75
- (c² − b²) = −0.5, so first term = −0.5(1, 0, 0) = (−0.5, 0, 0)
- **v** · **b** = 0.5, so second term = 2 · 0.5 · (0.5, 0.5, 0.5) = (0.5, 0.5, 0.5)
- **b** × **v** = (0.5, 0.5, 0.5) × (1, 0, 0) = (0.5·0 − 0.5·0, 0.5·1 − 0.5·0, 0.5·0 − 0.5·1) = (0, 0.5, −0.5)
- 2c(**b** × **v**) = 2 · 0.5 · (0, 0.5, −0.5) = (0, 0.5, −0.5)

`v' = (−0.5, 0, 0) + (0.5, 0.5, 0.5) + (0, 0.5, −0.5) = (0, 1, 0)`

Wait, let's also verify with the matrix approach. Rotate (1,0,0) by 90° about z → (0,1,0). Then rotate (0,1,0) by 90° about x → (0,0,1).

But we got (0, 1, 0)... Let me recheck the quaternion product. Actually, let me redo this more carefully.

**q₂q₁** product components (using q1 = (0, 0, s, c) and q2 = (s, 0, 0, c) with s = c = 1/√2 ≈ 0.7071):

- x: q2.w·q1.x + q2.x·q1.w + q2.y·q1.z − q2.z·q1.y = 0.7071·0 + 0.7071·0.7071 + 0·0.7071 − 0·0 = 0.5
- y: q2.w·q1.y − q2.x·q1.z + q2.y·q1.w + q2.z·q1.x = 0.7071·0 − 0.7071·0.7071 + 0·0.7071 + 0·0 = −0.5
- z: q2.w·q1.z + q2.x·q1.y − q2.y·q1.x + q2.z·q1.w = 0.7071·0.7071 + 0.7071·0 − 0·0 + 0·0.7071 = 0.5
- w: q2.w·q1.w − q2.x·q1.x − q2.y·q1.y − q2.z·q1.z = 0.7071·0.7071 − 0.7071·0 − 0·0 − 0·0.7071 = 0.5

`q_combined = (0.5, −0.5, 0.5, 0.5)`

Now apply to **v** = (1, 0, 0):

- **b** = (0.5, −0.5, 0.5), c = 0.5
- b² = 0.25 + 0.25 + 0.25 = 0.75
- (c² − b²) = 0.25 − 0.75 = −0.5
- First term: −0.5 · (1, 0, 0) = (−0.5, 0, 0)
- **v** · **b** = 0.5
- Second term: 2 · 0.5 · (0.5, −0.5, 0.5) = (0.5, −0.5, 0.5)
- **b** × **v** = (−0.5·0 − 0.5·0, 0.5·1 − 0.5·0, 0.5·0 − (−0.5)·1) = (0, 0.5, 0.5)
- Third term: 2 · 0.5 · (0, 0.5, 0.5) = (0, 0.5, 0.5)

`v' = (−0.5, 0, 0) + (0.5, −0.5, 0.5) + (0, 0.5, 0.5) = (0, 0, 1)`

The x-axis unit vector ends up pointing along z ✓ — matching the matrix composition result from Section 2.1.3!

**Example 4: Convert a Quaternion to Matrix and Back**

Take **q** = (0, 0, 0.7071, 0.7071) (90° about z).

- x = 0, y = 0, z = 0.7071, w = 0.7071
- x² = 0, y² = 0, z² = 0.5
- xy = 0, xz = 0, yz = 0
- wx = 0, wy = 0, wz = 0.5

```
M = | 1 − 2(0 + 0.5)    2(0 − 0.5)      2(0 + 0)    |
    | 2(0 + 0.5)        1 − 2(0 + 0.5)    2(0 − 0)    |
    | 2(0 − 0)          2(0 + 0)          1 − 2(0 + 0) |

  = |  0  -1   0 |
    |  1   0   0 |
    |  0   0   1 |
```

This is exactly the 90° z-rotation matrix ✓

Now convert back. Sum = M₀₀ + M₁₁ + M₂₂ = 0 + 0 + 1 = 1 > 0.

- w = √(1 + 1) / 2 = √2 / 2 = 0.7071
- f = 0.25 / 0.7071 = 0.3536
- x = (M₂₁ − M₁₂) · f = (0 − 0) · 0.3536 = 0
- y = (M₀₂ − M₂₀) · f = (0 − 0) · 0.3536 = 0
- z = (M₁₀ − M₀₁) · f = (1 − (−1)) · 0.3536 = 2 · 0.3536 = 0.7071

Recovered: **q** = (0, 0, 0.7071, 0.7071) ✓

**Example 5: 180° Rotation About the Y-Axis**

θ = 180°, **a** = (0, 1, 0)

`q = sin(90°) × (0, 1, 0) + cos(90°) = 1 · (0, 1, 0) + 0 = (0, 1, 0, 0)`

This is a pure imaginary quaternion! Apply to **v** = (1, 0, 0):

- **b** = (0, 1, 0), c = 0
- c² − b² = 0 − 1 = −1, first term: −1 · (1, 0, 0) = (−1, 0, 0)
- **v** · **b** = 0, second term: 0
- **b** × **v** = (0, 1, 0) × (1, 0, 0) = (0, 0, −1), third term: 2 · 0 · (0, 0, −1) = 0

`v' = (−1, 0, 0)`

The x-axis vector is reversed, as expected for a 180° rotation about y ✓

Note that −**q** = (0, −1, 0, 0) gives the same result: (−1)² = 1 appears in each term.

---

## Summary

| Transform | Matrix Form | Properties |
|-----------|-------------|------------|
| **Rotation (axis-aligned)** | See Mrotx, Mroty, Mrotz | Orthogonal, det = +1 |
| **Rotation (arbitrary axis)** | Equation 2.21 | Orthogonal, det = +1 |
| **Reflection** | I − 2(a ⊗ a) | Orthogonal, det = −1, M² = I |
| **Involution** | 2(a ⊗ a) − I | Orthogonal, det = +1 (3D), M² = I |
| **Uniform Scale** | sI | det = s³ |
| **Non-uniform Scale** | diag(sₓ, s_y, s_z) | det = sₓs_ys_z |
| **Arbitrary Scale** | I + (s−1)(a ⊗ a) | det = s |
| **Skew** | I + tan θ(a ⊗ b) | det = 1 (volume-preserving) |
| **Quaternion rotation** | v' = qvq* | ‖q‖ = 1, compact, composable |

**Key implementation tips for game engines:**

1. Use orthogonal matrices whenever possible — the inverse is just the transpose (free!).
2. Store rotations as quaternions for interpolation and composition; convert to matrices for rendering.
3. Use 4×4 Transform4D matrices for combined rotation + translation transforms.
4. When converting matrix → quaternion, always use the numerically stable algorithm that avoids dividing by small values.
5. Remember: q and −q represent the same rotation. Normalize quaternions periodically to prevent drift.

---

*Next chapter: [Chapter 3 — Geometry](./03-geometry.md)*
