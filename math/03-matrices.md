# Chapter 3: Matrices

> *"A matrix is a machine: feed it a vector, and it spits out a transformed vector."*

---

## 1. What is a Matrix?

A **matrix** is a rectangular grid of numbers arranged in **rows** and **columns**.

A matrix with *m* rows and *n* columns is called an **m × n** matrix (read "m by n").

```
        Column 1  Column 2  Column 3
         ↓         ↓         ↓
Row 1 → [ 1        4         7 ]
Row 2 → [ 2        5         8 ]
Row 3 → [ 3        6         9 ]

This is a 3×3 matrix.
```

### The "Transformer Machine" Analogy

Think of a matrix as a **machine** sitting between input and output:

```
                    +-----------+
  Input Vector ---> |  MATRIX   | ---> Output Vector
   (original)      | (machine)  |      (transformed)
                    +-----------+
```

You feed in a position vector `(3, 4, 0)`, and depending on what numbers are inside the
matrix, you might get back a **rotated** version, a **scaled** version, a **skewed** version,
or some combination.

The matrix **encodes the transformation**. Different numbers inside = different transformation.

---

## 2. Matrix Notation

Individual elements are referenced with **subscripts**: \( M_{ij} \) means the element in
row *i*, column *j*.

\[
M = \begin{bmatrix}
M_{11} & M_{12} & M_{13} \\
M_{21} & M_{22} & M_{23} \\
M_{31} & M_{32} & M_{33}
\end{bmatrix}
\]

### Common Sizes in Games

| Size  | Used For                                                 |
|-------|----------------------------------------------------------|
| 2×2   | 2D rotations, 2D scaling                                 |
| 3×3   | 3D rotations, 3D scaling, 2D transformations with translation |
| 4×4   | Full 3D transformations (rotation + scale + translation)  |

---

## 3. Special Matrices

### Identity Matrix

The identity matrix \( I \) is the "do nothing" matrix. Multiplying any vector or matrix by
\( I \) returns the original unchanged.

\[
I_{2\times2} = \begin{bmatrix} 1 & 0 \\ 0 & 1 \end{bmatrix}
\qquad
I_{3\times3} = \begin{bmatrix} 1 & 0 & 0 \\ 0 & 1 & 0 \\ 0 & 0 & 1 \end{bmatrix}
\qquad
I_{4\times4} = \begin{bmatrix} 1 & 0 & 0 & 0 \\ 0 & 1 & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{bmatrix}
\]

It has **1s on the main diagonal** and **0s everywhere else**.

```
// Multiplying a vector by identity:
I * (3, 4, 5) = (3, 4, 5)       // unchanged!
```

Think of it as multiplying a number by 1 — it's the multiplicative "do nothing."

### Zero Matrix

All elements are 0. Multiplying by it collapses everything to the zero vector. Rarely useful
on its own, but important conceptually.

### Diagonal Matrix

Non-zero values only on the main diagonal:

\[
D = \begin{bmatrix} 2 & 0 & 0 \\ 0 & 3 & 0 \\ 0 & 0 & 0.5 \end{bmatrix}
\]

This is a **scaling matrix** — it scales X by 2, Y by 3, and Z by 0.5:

```
D * (1, 1, 1) = (2, 3, 0.5)
```

### Symmetric Matrix

A matrix where \( M_{ij} = M_{ji} \) (mirrored across the diagonal):

\[
S = \begin{bmatrix} 1 & 2 & 3 \\ 2 & 4 & 5 \\ 3 & 5 & 6 \end{bmatrix}
\]

Symmetric matrices appear in physics (inertia tensors) and in covariance matrices. Their
eigenvalues are always real, which makes them well-behaved numerically.

---

## 4. Matrix Addition and Scalar Multiplication

### Addition

Add corresponding elements. Both matrices must be the same size.

\[
\begin{bmatrix} 1 & 2 \\ 3 & 4 \end{bmatrix}
+
\begin{bmatrix} 5 & 6 \\ 7 & 8 \end{bmatrix}
=
\begin{bmatrix} 6 & 8 \\ 10 & 12 \end{bmatrix}
\]

### Scalar Multiplication

Multiply every element by the scalar:

\[
3 \times \begin{bmatrix} 1 & 2 \\ 3 & 4 \end{bmatrix}
=
\begin{bmatrix} 3 & 6 \\ 9 & 12 \end{bmatrix}
\]

These operations are straightforward but less commonly used in game engines than matrix
multiplication.

---

## 5. Matrix Multiplication

This is the **core operation** — combining two transformations into one.

### The Rule: Row × Column

To compute element \( C_{ij} \) of the product \( C = A \times B \):

Take **row i of A** and **column j of B**, multiply corresponding elements, and sum them.

\[
C_{ij} = \sum_{k=1}^{n} A_{ik} \cdot B_{kj}
\]

### Dimension Requirement

\[
\underbrace{A}_{m \times n} \times \underbrace{B}_{n \times p} = \underbrace{C}_{m \times p}
\]

The **inner dimensions must match** (both are *n*). The result has the **outer dimensions**.

```
(2×3) × (3×2) = (2×2)   ✓
(2×3) × (2×3) = ???      ✗  (3 ≠ 2, can't multiply)
```

### Step-by-Step Example: 2×2 × 2×2

\[
\begin{bmatrix} a & b \\ c & d \end{bmatrix}
\times
\begin{bmatrix} e & f \\ g & h \end{bmatrix}
=
\begin{bmatrix}
ae + bg & af + bh \\
ce + dg & cf + dh
\end{bmatrix}
\]

With actual numbers:

\[
\begin{bmatrix} 1 & 2 \\ 3 & 4 \end{bmatrix}
\times
\begin{bmatrix} 5 & 6 \\ 7 & 8 \end{bmatrix}
=
\begin{bmatrix}
1 \cdot 5 + 2 \cdot 7 & 1 \cdot 6 + 2 \cdot 8 \\
3 \cdot 5 + 4 \cdot 7 & 3 \cdot 6 + 4 \cdot 8
\end{bmatrix}
=
\begin{bmatrix}
19 & 22 \\
43 & 50
\end{bmatrix}
\]

### Order Matters! (AB ≠ BA)

Matrix multiplication is **not commutative** in general:

```
A × B ≠ B × A     (usually!)
```

This is critically important in game math. "Rotate then translate" produces a different
result than "translate then rotate."

```
Rotate then Translate:              Translate then Rotate:

    *                                      *
   /|                                    / |
  / |  rotate first...                 /   | ...different result!
 *--+  ...then translate right        *----+

         ≠
```

### Example: Combining Rotation and Scale

Instead of applying rotation and scale as two separate operations each frame, you can
**pre-multiply** them into a single matrix:

```
combinedMatrix = rotationMatrix * scaleMatrix

// Now use combinedMatrix to transform vertices in one step:
transformedVertex = combinedMatrix * vertex
```

This is faster because one matrix multiply per vertex is cheaper than two.

---

## 6. Matrix-Vector Multiplication

This is how you actually **transform** a point or vector — multiply a matrix by a vector.

A vector is treated as a **column matrix** (n×1):

\[
\begin{bmatrix} a & b \\ c & d \end{bmatrix}
\times
\begin{bmatrix} x \\ y \end{bmatrix}
=
\begin{bmatrix} ax + by \\ cx + dy \end{bmatrix}
\]

### Step-by-Step Example: Rotating a 2D Point by 90°

A 2D rotation matrix for angle \( \theta \):

\[
R(\theta) = \begin{bmatrix} \cos\theta & -\sin\theta \\ \sin\theta & \cos\theta \end{bmatrix}
\]

For \( \theta = 90° \) (i.e., \( \pi/2 \) radians):

\[
\cos(90°) = 0, \quad \sin(90°) = 1
\]

\[
R(90°) = \begin{bmatrix} 0 & -1 \\ 1 & 0 \end{bmatrix}
\]

Now transform the point \( P = (1, 0) \):

\[
R \times P =
\begin{bmatrix} 0 & -1 \\ 1 & 0 \end{bmatrix}
\begin{bmatrix} 1 \\ 0 \end{bmatrix}
=
\begin{bmatrix} 0 \cdot 1 + (-1) \cdot 0 \\ 1 \cdot 1 + 0 \cdot 0 \end{bmatrix}
=
\begin{bmatrix} 0 \\ 1 \end{bmatrix}
\]

The point moved from `(1, 0)` to `(0, 1)` — a 90° counter-clockwise rotation:

```
     Y
     ^
     |
(0,1)* <--- rotated here
     |
     +---*-------> X
        (1,0) original
```

### Pseudocode: Transforming All Vertices of a Mesh

```
function transformMesh(mesh, matrix):
    for each vertex in mesh.vertices:
        vertex.position = matrix * vertex.position
```

---

## 7. Transpose

The **transpose** of a matrix \( M \), written \( M^T \), flips it across the main diagonal
— rows become columns and columns become rows.

\[
M = \begin{bmatrix} 1 & 2 & 3 \\ 4 & 5 & 6 \end{bmatrix}
\qquad
M^T = \begin{bmatrix} 1 & 4 \\ 2 & 5 \\ 3 & 6 \end{bmatrix}
\]

A 2×3 matrix becomes a 3×2 matrix.

### Properties

- \( (M^T)^T = M \)
- \( (AB)^T = B^T A^T \) (note the reversed order)
- If \( M = M^T \), the matrix is **symmetric**

### When It's Useful

- **Converting row-major to column-major** format (or vice versa) is equivalent to
  transposing. If you pass a matrix from a row-major API to a column-major API without
  transposing, your transforms will be wrong.
- **Orthogonal matrices** have a special property: \( M^{-1} = M^T \). This means the
  inverse of a rotation matrix is just its transpose — very cheap to compute.

---

## 8. Determinant

The **determinant** of a square matrix is a single scalar that encodes important geometric
information.

### Geometric Meaning

The determinant tells you the **scaling factor** that the matrix applies to area (2D) or
volume (3D):

- \( \det(M) = 1 \) → preserves area/volume (e.g., pure rotation)
- \( \det(M) = 2 \) → doubles area/volume
- \( \det(M) = 0 \) → **collapses** a dimension (the matrix is singular)
- \( \det(M) < 0 \) → includes a **reflection** (flips orientation)

```
Original          det = 2             det = 0           det = -1
  +--+            +------+            +                  +--+
  |  |            |      |            |\                 |  | (mirrored)
  +--+            |      |           collapsed           +--+
 area=1           +------+           to a line
                  area=2             area=0
```

### 2×2 Determinant

\[
\det \begin{bmatrix} a & b \\ c & d \end{bmatrix} = ad - bc
\]

Example:

\[
\det \begin{bmatrix} 3 & 1 \\ 2 & 4 \end{bmatrix} = 3 \cdot 4 - 1 \cdot 2 = 10
\]

### 3×3 Determinant (Cofactor Expansion)

Expand along the first row:

\[
\det \begin{bmatrix} a & b & c \\ d & e & f \\ g & h & i \end{bmatrix}
= a(ei - fh) - b(di - fg) + c(dh - eg)
\]

A helpful mnemonic — the **Rule of Sarrus**:

```
 a  b  c | a  b
 d  e  f | d  e
 g  h  i | g  h

 Positive diagonals (↘): aei + bfg + cdh
 Negative diagonals (↙): ceg + bdi + afh

 det = (aei + bfg + cdh) - (ceg + bdi + afh)
```

### What det = 0 Means

If \( \det(M) = 0 \), the matrix is **singular** (non-invertible). It squishes space into
a lower dimension. You **cannot undo** this transformation — information is permanently lost.

In game terms: if a scale matrix has a 0 on any diagonal element, it flattens geometry into
a plane (or a line, or a point). You can't recover the original shape.

---

## 9. Inverse Matrix

The **inverse** of matrix \( M \), written \( M^{-1} \), is the matrix that **undoes** the
transformation:

\[
M \times M^{-1} = M^{-1} \times M = I
\]

### When Does It Exist?

Only when \( \det(M) \neq 0 \). If the determinant is zero, the matrix is singular and has
no inverse.

### 2×2 Inverse Formula

\[
\begin{bmatrix} a & b \\ c & d \end{bmatrix}^{-1}
=
\frac{1}{ad - bc}
\begin{bmatrix} d & -b \\ -c & a \end{bmatrix}
\]

Example:

\[
M = \begin{bmatrix} 4 & 7 \\ 2 & 6 \end{bmatrix}
\]

\[
\det(M) = 4 \cdot 6 - 7 \cdot 2 = 24 - 14 = 10
\]

\[
M^{-1} = \frac{1}{10} \begin{bmatrix} 6 & -7 \\ -2 & 4 \end{bmatrix}
= \begin{bmatrix} 0.6 & -0.7 \\ -0.2 & 0.4 \end{bmatrix}
\]

Verify: \( M \times M^{-1} \) should give the identity:

\[
\begin{bmatrix} 4 & 7 \\ 2 & 6 \end{bmatrix}
\begin{bmatrix} 0.6 & -0.7 \\ -0.2 & 0.4 \end{bmatrix}
=
\begin{bmatrix}
4(0.6)+7(-0.2) & 4(-0.7)+7(0.4) \\
2(0.6)+6(-0.2) & 2(-0.7)+6(0.4)
\end{bmatrix}
=
\begin{bmatrix} 1 & 0 \\ 0 & 1 \end{bmatrix}
\; ✓
\]

### Larger Matrices

For 3×3 and 4×4 matrices, computing inverses analytically is tedious. In practice, game
engines use optimized numerical methods (Gauss-Jordan elimination, or exploit structure
like orthogonality).

### Example: World Space → Local Space

If the **model matrix** \( M \) transforms from local space to world space:

\[
\vec{p}_{world} = M \times \vec{p}_{local}
\]

Then the **inverse model matrix** goes the other way:

\[
\vec{p}_{local} = M^{-1} \times \vec{p}_{world}
\]

This is how engines figure out *"where on the mesh did the player click?"* — they transform
the click ray from world space back into the model's local space, then do a local intersection
test.

---

## 10. Orthogonal Matrices

A matrix \( M \) is **orthogonal** if its columns (or rows) are all:
- **Unit length** (normalized)
- **Mutually perpendicular** (orthogonal to each other)

This means:

\[
M^T M = I \quad \Longrightarrow \quad M^{-1} = M^T
\]

The inverse equals the transpose — no expensive inversion needed.

### Why Rotation Matrices Are Orthogonal

A rotation doesn't stretch or squash space. It just re-aims the axes. The columns of a
rotation matrix *are* the new axis directions, and they remain unit-length and perpendicular.

```
Original axes:         After rotation:
  Y ^                   Y' ^  /
    |                      | / X'
    |                      |/
    +---> X                +
```

The columns of the rotation matrix are the new X', Y', Z' axis directions, and they form
an orthonormal basis.

**Key properties of orthogonal matrices:**
- \( \det(M) = \pm 1 \)
- If \( \det(M) = +1 \): proper rotation (no reflection)
- If \( \det(M) = -1 \): rotation + reflection (improper rotation)
- Preserve lengths and angles

### In Practice

```
// Inverting a rotation matrix is FREE — just transpose:
inverseRotation = transpose(rotationMatrix)

// Compare with a general 4×4 inverse — much more expensive
```

---

## 11. Homogeneous Coordinates

### The Problem: Translation Can't Be Done with 3×3 Matrices

A 3×3 matrix can rotate and scale a 3D vector, but it **cannot translate** (move) it.
Why? Because matrix multiplication always maps the origin to the origin:

\[
M \times \vec{0} = \vec{0} \quad \text{(for any 3×3 matrix)}
\]

Translation needs to *add* an offset, and pure matrix multiplication only does *multiply and
add between components*.

### The Solution: Add a Fourth Dimension

We embed our 3D point into **4D** by adding a `w` component:

\[
\text{Point:} \quad (x, y, z) \rightarrow (x, y, z, 1)
\]
\[
\text{Direction vector:} \quad (x, y, z) \rightarrow (x, y, z, 0)
\]

Now a 4×4 matrix can encode **rotation, scale, AND translation** all at once:

\[
\begin{bmatrix}
r_{11} & r_{12} & r_{13} & t_x \\
r_{21} & r_{22} & r_{23} & t_y \\
r_{31} & r_{32} & r_{33} & t_z \\
0      & 0      & 0      & 1
\end{bmatrix}
\times
\begin{bmatrix} x \\ y \\ z \\ 1 \end{bmatrix}
=
\begin{bmatrix}
r_{11}x + r_{12}y + r_{13}z + t_x \\
r_{21}x + r_{22}y + r_{23}z + t_y \\
r_{31}x + r_{32}y + r_{33}z + t_z \\
1
\end{bmatrix}
\]

Look at the result — the upper-left 3×3 block handles rotation/scale, and the right column
\( (t_x, t_y, t_z) \) adds translation. With `w = 1`, the translation is applied. With
`w = 0`, it's ignored (which is exactly what you want for direction vectors — they shouldn't
be translated).

### The Structure of a 4×4 Transformation Matrix

```
+---------------------+-------+
|                     |       |
|   3×3 Rotation/     |  Tx   |
|   Scale block       |  Ty   |
|                     |  Tz   |
+---------------------+-------+
|   0     0     0     |   1   |
+---------------------+-------+
```

### Converting Back from Homogeneous Coordinates

After perspective projection, `w` may not be 1. To convert back to 3D, **divide by w**:

\[
(x, y, z, w) \rightarrow \left(\frac{x}{w}, \frac{y}{w}, \frac{z}{w}\right)
\]

This **perspective divide** is what creates the foreshortening effect — distant objects
appear smaller because they have a larger `w`.

### Pure Translation Matrix

\[
T = \begin{bmatrix}
1 & 0 & 0 & t_x \\
0 & 1 & 0 & t_y \\
0 & 0 & 1 & t_z \\
0 & 0 & 0 & 1
\end{bmatrix}
\]

```
T * (x, y, z, 1) = (x + tx, y + ty, z + tz, 1)
```

### Pure Scale Matrix (4×4)

\[
S = \begin{bmatrix}
s_x & 0   & 0   & 0 \\
0   & s_y & 0   & 0 \\
0   & 0   & s_z & 0 \\
0   & 0   & 0   & 1
\end{bmatrix}
\]

### Rotation Around the Y Axis (4×4)

\[
R_y(\theta) = \begin{bmatrix}
\cos\theta  & 0 & \sin\theta & 0 \\
0           & 1 & 0          & 0 \\
-\sin\theta & 0 & \cos\theta & 0 \\
0           & 0 & 0          & 1
\end{bmatrix}
\]

---

## 12. Row-Major vs Column-Major

This is one of the most confusing topics in graphics programming because it affects
**memory layout** and **multiplication order**.

### Memory Layout

A matrix lives in memory as a flat array of 16 floats (for 4×4). But which order?

Consider this matrix:

\[
M = \begin{bmatrix} 1 & 2 & 3 & 4 \\ 5 & 6 & 7 & 8 \\ 9 & 10 & 11 & 12 \\ 13 & 14 & 15 & 16 \end{bmatrix}
\]

**Row-major** (DirectX, C/C++ 2D arrays):

```
memory: [1, 2, 3, 4,  5, 6, 7, 8,  9, 10, 11, 12,  13, 14, 15, 16]
         ----row 1---  ----row 2---  ----row 3-----  ----row 4------
```

Row 1 elements are stored contiguously, then row 2, etc.

**Column-major** (OpenGL, GLSL, glm):

```
memory: [1, 5, 9, 13,  2, 6, 10, 14,  3, 7, 11, 15,  4, 8, 12, 16]
         ----col 1----  ----col 2----  ----col 3-----  ----col 4----
```

Column 1 elements are stored contiguously, then column 2, etc.

**The mathematical matrix is the same** — only the memory layout differs. Transposing the
matrix converts between the two layouts.

### Multiplication Order

The convention pairs with vector multiplication order:

| Convention    | Vector Type | Multiply As              | Matrix Chain               |
|---------------|-------------|--------------------------|----------------------------|
| Column-major  | Column vec  | \( M \times \vec{v} \)   | \( P \times V \times M \times \vec{v} \) |
| Row-major     | Row vec     | \( \vec{v} \times M \)   | \( \vec{v} \times M \times V \times P \) |

In column-major (OpenGL), transformations are applied **right-to-left**:

```
result = Projection * View * Model * vertex
         --------applied last--------first-
```

In row-major (DirectX), they're applied **left-to-right**:

```
result = vertex * Model * View * Projection
         --------applied first--------last-
```

Both produce the same final result — the math is equivalent, just written differently.

### Practical Tip

Know which convention your engine/API uses and be consistent. If you're mixing libraries
(e.g., using glm with a DirectX backend), you'll need to transpose matrices at the boundary.

---

## 13. The Rendering Pipeline Matrix Chain

Every vertex in a 3D scene goes through a chain of matrix transformations to end up as a
pixel on screen:

```
                  Model           View          Projection
  Local Space ──────────> World ────────> Camera ──────────> Clip Space
   (mesh)        Matrix   Space   Matrix   Space    Matrix
                                                        │
                                                        │ ÷ w
                                                        v
                                                    NDC Space
                                                        │
                                                        │ Viewport
                                                        v
                                                   Screen Space
                                                    (pixels)
```

### Model Matrix (Local → World)

Encodes the object's **position**, **rotation**, and **scale** in the world.

```
ModelMatrix = Translation * Rotation * Scale
```

Each object in the scene has its own model matrix.

### View Matrix (World → Camera)

Positions everything relative to the camera. It's the **inverse of the camera's world
transform**:

```
ViewMatrix = inverse(cameraWorldTransform)
```

If the camera moves right, the view matrix effectively moves the whole world left.

### Projection Matrix (Camera → Clip)

Defines the camera's **field of view** and maps the 3D view frustum to the clip-space cube.

**Perspective projection** — objects farther away appear smaller (realistic):

```
Frustum (perspective):

      near plane         far plane
        +---+           +----------+
       /     \         /            \
      /       \       /              \
     /  eye    \     /                \
    /     *     \   /                  \
     \       /       \              /
      \     /         \            /
       +---+           +----------+
```

**Orthographic projection** — no size change with distance (used in 2D games, CAD, UI):

```
Frustum (orthographic):

    +--------+--------+
    |        |        |
    |  near  |  far   |
    |  plane |  plane |
    +--------+--------+
    (same size everywhere)
```

### The Combined MVP Matrix

Many engines pre-multiply the three matrices into a single **MVP** (Model-View-Projection)
matrix:

```
MVP = Projection * View * Model

// In the vertex shader:
gl_Position = MVP * vec4(vertexPosition, 1.0);
```

This single multiplication per vertex replaces three separate multiplications — a meaningful
performance win for complex meshes.

---

## 14. Summary and Cheat Sheet

### Key Matrix Types

| Matrix           | Size  | What It Does                              |
|------------------|-------|-------------------------------------------|
| Identity         | n×n   | Does nothing (like multiplying by 1)       |
| Scale            | 4×4   | Stretches/shrinks along axes              |
| Rotation         | 4×4   | Rotates around an axis                    |
| Translation      | 4×4   | Moves position (requires homogeneous coords)|
| Model            | 4×4   | Local → World (combines S, R, T)          |
| View             | 4×4   | World → Camera                            |
| Projection       | 4×4   | Camera → Clip space                       |
| MVP              | 4×4   | All three combined                        |

### Key Operations

| Operation               | Result            | Key Rule                                  |
|-------------------------|-------------------|-------------------------------------------|
| \( A + B \)             | Matrix (same size)| Element-wise, same dimensions required     |
| \( k \cdot A \)         | Matrix            | Multiply every element by k                |
| \( A \times B \)        | Matrix            | Row-by-column; inner dimensions must match |
| \( M \times \vec{v} \)  | Vector            | Transform a point/vector                   |
| \( M^T \)               | Matrix            | Flip rows ↔ columns                       |
| \( \det(M) \)           | Scalar            | Area/volume scale factor                   |
| \( M^{-1} \)            | Matrix            | Undoes the transformation; needs det ≠ 0   |

### Key Formulas

**2D Rotation Matrix:**

\[
R(\theta) = \begin{bmatrix} \cos\theta & -\sin\theta \\ \sin\theta & \cos\theta \end{bmatrix}
\]

**2×2 Determinant:**

\[
\det \begin{bmatrix} a & b \\ c & d \end{bmatrix} = ad - bc
\]

**2×2 Inverse:**

\[
\begin{bmatrix} a & b \\ c & d \end{bmatrix}^{-1}
= \frac{1}{ad-bc} \begin{bmatrix} d & -b \\ -c & a \end{bmatrix}
\]

**Orthogonal Matrix Property:**

\[
M^{-1} = M^T \quad \text{(inverse is free!)}
\]

**Homogeneous Point:**

\[
(x, y, z) \rightarrow (x, y, z, 1) \quad \text{and back:} \quad (x, y, z, w) \rightarrow (x/w, y/w, z/w)
\]

### Mental Model Cheat Sheet

```
Matrix multiplication order (column-major / OpenGL):

  gl_Position = Projection * View * Model * vec4(pos, 1.0)
                ----------   ----   -----
                applied       |    applied
                 last         |     first
                              |
                         applied second

Read right to left:
  1. Model:      put the object in the world
  2. View:       see it from the camera's perspective
  3. Projection: squash 3D into 2D with perspective
```

### Common Pitfalls

| Pitfall                                | Solution                                    |
|----------------------------------------|---------------------------------------------|
| Multiplying matrices in wrong order    | Remember: order matters! Draw it out.        |
| Mixing row-major / column-major        | Know your API and transpose at boundaries   |
| Forgetting w=1 for points             | Points get (x,y,z,1); directions get (x,y,z,0)|
| Trying to invert a singular matrix    | Check determinant first                      |
| Accumulated floating-point drift       | Re-orthogonalize rotation matrices periodically|
| Translating with 3×3 instead of 4×4  | Translation requires homogeneous coordinates |

---

*With coordinate systems, vectors, and matrices under your belt, you have the fundamental
tools for almost all 3D game math. Next chapters will build on these foundations to cover
transformations (rotation, scale, translation in depth), quaternions, and the full
rendering pipeline.*
