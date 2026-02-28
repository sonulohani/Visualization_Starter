# Chapter 3: Geometry

> **Game Engine Mathematics Tutorial** — Based on *Foundations of Game Engine Development, Volume 1: Mathematics* by Eric Lengyel

Geometry is the backbone of everything a game engine does. It defines the world, describes characters, tells the GPU how to render, determines visibility, and detects collisions. This chapter covers the essential geometric primitives — triangle meshes, normal vectors, lines, rays, planes, and Plücker coordinates — with thorough worked examples and C++ code.

---

## 3.1 Triangle Meshes

Almost everything drawn by a game engine is made of **triangles**. A triangle mesh is a collection of triangles that together model the surface of a solid object.

### Vertex Positions and Index Lists

A triangle mesh stores:

1. A **vertex list**: an array of 3D points (positions).
2. An **index list**: an array of integer triplets — each triplet names the three vertices of one triangle.

```cpp
struct Mesh {
    std::vector<Point3D> vertices;
    std::vector<int>     indices;  // every 3 consecutive ints = one triangle
};
```

### Example: A Box (8 Vertices, 12 Triangles)

A unit box centered at the origin has 8 corners:

| Index | Position |
|-------|----------|
| 0 | (-1, -1, -1) |
| 1 | ( 1, -1, -1) |
| 2 | ( 1,  1, -1) |
| 3 | (-1,  1, -1) |
| 4 | (-1, -1,  1) |
| 5 | ( 1, -1,  1) |
| 6 | ( 1,  1,  1) |
| 7 | (-1,  1,  1) |

Each of the 6 rectangular faces is split into 2 triangles, giving **12 triangles total**. One possible triangulation (counterclockwise winding when viewed from outside):

| Triangle | Vertex Indices | Face |
|----------|---------------|------|
| 0  | 0, 1, 2 | Front (-z) |
| 1  | 0, 2, 3 | Front (-z) |
| 2  | 5, 4, 7 | Back (+z) |
| 3  | 5, 7, 6 | Back (+z) |
| 4  | 4, 0, 3 | Left (-x) |
| 5  | 4, 3, 7 | Left (-x) |
| 6  | 1, 5, 6 | Right (+x) |
| 7  | 1, 6, 2 | Right (+x) |
| 8  | 3, 2, 6 | Top (+y) |
| 9  | 3, 6, 7 | Top (+y) |
| 10 | 4, 5, 1 | Bottom (-y) |
| 11 | 4, 1, 0 | Bottom (-y) |

### Shared Vertices

Each vertex is shared by multiple triangles. Vertex 0 at (-1, -1, -1) appears in triangles 0, 1, 4, 5, 10, 11 — six triangles meeting at one corner. This sharing is the whole point of the index list: we store each position once and reference it many times.

### Closed Meshes and the Euler Formula

A mesh is **closed** if every edge is used by exactly **two** triangles. A closed mesh satisfies the **Euler formula**:

```
V - E + F = 2
```

where `V` = number of vertices, `E` = number of edges, `F` = number of faces (triangles).

### Example: Verify Euler's Formula for the Box

For the triangulated box above:

- **V = 8** (the 8 corner vertices)
- **F = 12** (the 12 triangles)
- **E = ?** — Each triangle has 3 edges, giving 12 × 3 = 36 edge-uses. Since every edge is shared by exactly 2 triangles in a closed mesh: **E = 36 / 2 = 18**

Check: `V - E + F = 8 - 18 + 12 = 2` ✓

For comparison, the ideal (non-triangulated) box has V = 8, E = 12, F = 6, and 8 - 12 + 6 = 2 as well.

### Winding Direction

The **winding direction** determines which side of a triangle is the "front." We use the **counterclockwise (CCW) convention** with the **right-hand rule**: when viewing a triangle from its front side, the vertices appear in counterclockwise order. Curling the fingers of the right hand in the CCW direction, the thumb points outward (the normal direction).

For the lower-left triangle in Figure 3.1 (vertices 0, 1, 4), the counterclockwise orderings are: (0, 1, 4), (1, 4, 0), or (4, 0, 1). All three are cyclic permutations and define the same winding.

---

## 3.2 Normal Vectors

A **normal vector** is perpendicular to a surface at a given point. Normals are essential for shading, collision detection, and physics.

### 3.2.1 Calculating Normal Vectors

#### Method 1: From an Implicit Surface (Gradient)

If a surface is defined implicitly by `f(p) = 0`, the normal at point **p** is the gradient:

```
n = ∇f(p) = ( ∂f/∂x,  ∂f/∂y,  ∂f/∂z )
```

**Example: Normal on an Ellipsoid**

Consider the ellipsoid:

```
f(p) = x² + y²/4 + z² - 1 = 0
```

The gradient is:

```
∇f = ( 2x,  y/2,  2z )
```

The point `p = (1/√6, 1, 1/√6)` lies on the surface. Verify:

```
1/6 + 1/4 + 1/6 = 2/12 + 3/12 + 2/12 = 7/12 ≠ 1
```

Let's use a point that actually lies on the surface. Try `p = (0.5, 1.0, 0.5)`:

```
0.25 + 1/4 + 0.25 = 0.25 + 0.25 + 0.25 = 0.75 ≠ 1
```

Try `p = (1/√2, 0, 1/√2)`:

```
1/2 + 0 + 1/2 = 1  ✓
```

The normal at this point is:

```
n = ( 2 · 1/√2,  0/2,  2 · 1/√2 ) = ( √2,  0,  √2 )
```

Normalized: `‖n‖ = √(2 + 0 + 2) = 2`, so `n̂ = (1/√2, 0, 1/√2) ≈ (0.707, 0, 0.707)`.

The normal points radially outward, which is geometrically correct for an ellipsoid.

#### Method 2: From a Triangle Face (Cross Product)

For a triangle with CCW-wound vertices **p₀**, **p₁**, **p₂**, the outward-facing normal is:

```
n = (p₁ - p₀) × (p₂ - p₀)
```

**Example: Triangle Face Normal**

Let `p₀ = (0, 0, 0)`, `p₁ = (1, 0, 0)`, `p₂ = (0, 1, 0)`.

```
e₁ = p₁ - p₀ = (1, 0, 0)
e₂ = p₂ - p₀ = (0, 1, 0)

n = e₁ × e₂ = | i  j  k |
               | 1  0  0 |
               | 0  1  0 |

  = (0·0 - 0·1,  0·0 - 1·0,  1·1 - 0·0) = (0, 0, 1)
```

The normal points in the +z direction — the triangle lies in the xy-plane and faces "up."

**Example: A More Interesting Triangle**

Let `p₀ = (1, 0, 0)`, `p₁ = (0, 2, 0)`, `p₂ = (0, 0, 3)`.

```
e₁ = (-1, 2, 0),   e₂ = (-1, 0, 3)

n = e₁ × e₂ = (2·3 - 0·0,  0·(-1) - (-1)·3,  (-1)·0 - 2·(-1)) = (6, 3, 2)
```

`‖n‖ = √(36 + 9 + 4) = 7`, so the unit normal is `n̂ = (6/7, 3/7, 2/7)`.

```cpp
Vector3D FaceNormal(const Point3D& p0, const Point3D& p1, const Point3D& p2) {
    Vector3D e1 = p1 - p0;
    Vector3D e2 = p2 - p0;
    return Normalize(Cross(e1, e2));
}
```

#### Per-Vertex Normals

For smooth shading, compute a **per-vertex normal** by averaging the face normals of all triangles sharing that vertex:

```
n_vertex = Normalize( Σ nᵢ )     (sum over adjacent faces i)
```

The average can optionally be weighted by triangle area or angle at that vertex.

#### Hard Edges

For sharp edges (like the edge of a cube), **duplicate the vertex** at that position and assign different normals to each copy. The two copies share the same position but carry different normal vectors, producing a hard lighting discontinuity.

### 3.2.2 Transforming Normal Vectors

#### The Problem with Naive Transformation

If you transform a model by matrix **M**, every point **p** becomes **Mp** and every tangent **t** becomes **Mt**. It seems natural to also compute **Mn** for normals — but this is **wrong** whenever **M** contains non-uniform scaling.

**Why it breaks**: Consider a triangle with a normal `n = (b, a, 0)` (a row vector) and a scale matrix that doubles the x-axis:

```
      | 2  0  0 |
M  =  | 0  1  0 |
      | 0  0  1 |
```

Naively: `Mn = (2b, a, 0)`. This stretched vector is **no longer perpendicular** to the transformed surface.

#### Why Normals Are Different

Normal vectors (gradients) measure **reciprocal distances** — the components `∂f/∂x`, `∂f/∂y`, `∂f/∂z` have distances in the denominator. This is fundamentally different from ordinary vectors that measure distances directly. A normal should be treated as a **row vector**, not a column vector.

#### Correct Transform for Gradient Normals

For a normal computed via gradient, treat **n** as a row vector and transform:

```
n_B = n_A · M⁻¹
```

#### Correct Transform for Cross-Product Normals

For a normal computed as a cross product of tangent vectors:

```
n_B = n_A · adj(M) = det(M) · n_A · M⁻¹
```

The two formulas differ only by the factor det(**M**). Since normals are almost always re-normalized, this factor usually doesn't matter — except when **M** contains a reflection (det < 0), which flips the normal direction.

#### Verification: Perpendicularity is Preserved

If `n_A · t_A = 0`, then:

```
n_B · t_B = (n_A · M⁻¹)(M · t_A) = n_A · (M⁻¹ · M) · t_A = n_A · I · t_A = n_A · t_A = 0  ✓
```

#### For Orthogonal M

When **M** is orthogonal (rotations, reflections without scale), `M⁻¹ = Mᵀ`, so `n_A · M⁻¹ = n_A · Mᵀ`, which is the same as `M · n_A` if we treat **n** as a column vector. In this case, normals transform identically to regular vectors.

#### Numerical Example: Transform Under Non-Uniform Scale

**Setup**: A surface has normal `n_A = (1, 1, 0)` (row vector). Transform by:

```
      | 3  0  0 |
M  =  | 0  1  0 |
      | 0  0  2 |
```

**Step 1**: Compute `M⁻¹`:

```
        | 1/3  0    0   |
M⁻¹ =  | 0    1    0   |
        | 0    0   1/2  |
```

**Step 2**: Transform the normal:

```
                     | 1/3  0    0   |
n_B = (1, 1, 0)  ·  | 0    1    0   |  = (1/3,  1,  0)
                     | 0    0   1/2  |
```

**Step 3**: Verify with a tangent vector. Take `t_A = (1, -1, 0)`. Check: `n_A · t_A = 1 - 1 + 0 = 0` ✓

Transform the tangent: `t_B = M · t_A = (3, -1, 0)`.

Check perpendicularity: `n_B · t_B = (1/3)(3) + (1)(-1) + 0 = 1 - 1 = 0` ✓

**Wrong answer** (naive): `M · n_A = (3, 1, 0)`. Check: `(3)(3) + (1)(-1) = 9 - 1 = 8 ≠ 0` ✗

```cpp
Vector3D TransformNormal(const Vector3D& n, const Transform4D& H_inverse) {
    return Vector3D(
        n.x * H_inverse(0,0) + n.y * H_inverse(1,0) + n.z * H_inverse(2,0),
        n.x * H_inverse(0,1) + n.y * H_inverse(1,1) + n.z * H_inverse(2,1),
        n.x * H_inverse(0,2) + n.y * H_inverse(1,2) + n.z * H_inverse(2,2));
}
```

---

## 3.3 Lines and Rays

Lines and rays are everywhere in game engines: raycasting for mouse picking, line-of-sight checks, bullet trajectories, rendering ray marching, etc.

### 3.3.1 Parametric Lines

A line through point **p** with direction **v** is written:

```
L(t) = p + t·v
```

- **Line**: `t` ranges over all real numbers (`-∞ < t < ∞`)
- **Ray**: `t ≥ 0` (starts at **p**, extends in direction **v**)
- **Segment**: `0 ≤ t ≤ 1` (from **p** to **p + v**)

This is equivalent to the two-point form:

```
L(t) = (1 - t)·p₁ + t·p₂
```

where `v = p₂ - p₁` and `p = p₁`. At `t = 0` you get `p₁`, at `t = 1` you get `p₂`.

**Example**: Line from `p = (1, 2, 3)` in direction `v = (2, -1, 0)`:

| t    | L(t)         |
|------|--------------|
| 0    | (1, 2, 3)    |
| 1    | (3, 1, 3)    |
| -1   | (-1, 3, 3)   |
| 0.5  | (2, 1.5, 3)  |

### 3.3.2 Distance Between a Point and a Line

Given a line `L(t) = p + t·v` and a point **q**, define `u = q - p`.

**Formula 1** (via Pythagorean theorem):

```
d = √( u² - (u · v)² / v² )
```

This subtracts the squared projection from the squared hypotenuse. It can suffer from floating-point cancellation when **p** and **q** are far apart.

**Formula 2** (more numerically stable, via parallelogram area):

```
d = ‖u × v‖ / ‖v‖
```

The cross product gives the area of the parallelogram spanned by **u** and **v**; dividing by the base length **‖v‖** gives the height, which is the distance.

#### Worked Example

Find the distance from point `q = (3, 4, 1)` to the line through `p = (1, 0, 0)` with direction `v = (1, 1, 0)`.

**Step 1**: `u = q - p = (2, 4, 1)`

**Step 2**: Cross product:

```
u × v = | i  j  k |
        | 2  4  1 |
        | 1  1  0 |

  = (4·0 - 1·1,  1·1 - 2·0,  2·1 - 4·1) = (-1,  1,  -2)
```

**Step 3**: Magnitudes:

```
‖u × v‖ = √(1 + 1 + 4) = √6
‖v‖     = √(1 + 1 + 0) = √2
```

**Step 4**: Distance:

```
d = √6 / √2 = √3 ≈ 1.732
```

**Verify with Formula 1**:

```
u · u = 4 + 16 + 1 = 21
u · v = 2 + 4 + 0  = 6
v · v = 2

d = √(21 - 36/2) = √(21 - 18) = √3  ✓
```

```cpp
float DistPointLine(const Point3D& q, const Point3D& p, const Vector3D& v) {
    Vector3D a = Cross(q - p, v);
    return sqrt(Dot(a, a) / Dot(v, v));
}
```

### 3.3.3 Distance Between Two Lines

Two lines in 3D that aren't parallel and don't intersect are called **skew lines**. Let:

```
L₁(t₁) = p₁ + t₁·v₁       L₂(t₂) = p₂ + t₂·v₂
```

We need to find `t₁` and `t₂` such that the segment `L₂(t₂) - L₁(t₁)` is perpendicular to both **v₁** and **v₂**:

```
(p₂ + t₂·v₂ - p₁ - t₁·v₁) · v₁ = 0
(p₂ + t₂·v₂ - p₁ - t₁·v₁) · v₂ = 0
```

Let `dp = p₂ - p₁`. This becomes a 2×2 linear system:

```
| v₁·v₁   -v₁·v₂ | | t₁ |   | dp·v₁ |
| v₁·v₂   -v₂·v₂ | | t₂ | = | dp·v₂ |
```

Solving by Cramer's rule (or matrix inversion):

```
Δ = (v₁·v₂)² - (v₁·v₁)(v₂·v₂)

t₁ = [ (v₁·v₂)(dp·v₂) - (v₂·v₂)(dp·v₁) ] / Δ

t₂ = [ (v₁·v₁)(dp·v₂) - (v₁·v₂)(dp·v₁) ] / Δ
```

The distance is `d = ‖L₂(t₂) - L₁(t₁)‖`.

If `Δ = 0`, the lines are parallel — use point-to-line distance instead.

#### Worked Example

**Line 1**: `p₁ = (0, 0, 0)`, `v₁ = (1, 0, 0)` (the x-axis)

**Line 2**: `p₂ = (0, 1, 1)`, `v₂ = (0, 1, 0)` (parallel to y-axis, offset by y=1, z=1)

**Step 1**: Compute dot products:

```
dp = (0, 1, 1)

v₁·v₁ = 1      v₂·v₂ = 1      v₁·v₂ = 0
dp·v₁  = 0      dp·v₂  = 1
```

**Step 2**: Determinant:

```
Δ = 0² - 1·1 = -1
```

**Step 3**: Solve:

```
t₁ = (0·1 - 1·0) / (-1) = 0
t₂ = (1·1 - 0·0) / (-1) = -1
```

**Step 4**: Closest points:

```
L₁(0)  = (0, 0, 0)
L₂(-1) = (0, 0, 1)
```

**Step 5**: Distance:

```
d = ‖(0, 0, 1) - (0, 0, 0)‖ = 1
```

The closest approach is 1 unit apart, along the z-axis. This makes geometric sense: the x-axis and a y-parallel line at z=1 are separated by exactly 1 in the z-direction.

```cpp
float DistLineLine(const Point3D& p1, const Vector3D& v1,
                   const Point3D& p2, const Vector3D& v2)
{
    Vector3D dp = p2 - p1;
    float v12 = Dot(v1, v1);
    float v22 = Dot(v2, v2);
    float v1v2 = Dot(v1, v2);
    float det = v1v2 * v1v2 - v12 * v22;

    if (fabs(det) > FLT_MIN) {
        det = 1.0f / det;
        float dpv1 = Dot(dp, v1);
        float dpv2 = Dot(dp, v2);
        float t1 = (v1v2 * dpv2 - v22 * dpv1) * det;
        float t2 = (v12 * dpv2 - v1v2 * dpv1) * det;
        return Magnitude(dp + v2 * t2 - v1 * t1);
    }

    // Parallel: fall back to point-line distance
    Vector3D a = Cross(dp, v1);
    return sqrt(Dot(a, a) / v12);
}
```

---

## 3.4 Planes

### 3.4.1 Implicit Planes

#### Parametric Form

A plane through point **p** with two directions **u** and **v** (both lying in the plane):

```
Q(s, t) = p + s·u + t·v
```

This is rarely used in engines because it's cumbersome (two free parameters).

#### Implicit Form

A plane is far more conveniently described by its normal **n** and a scalar *d*:

```
n · p + d = 0
```

Here *d* = −**n** · **q** for any known point **q** in the plane. This equation is satisfied by every point **p** in the plane.

We pack these into a 4D row vector:

```
f = [n | d] = [nₓ, nᵧ, n_z, d]
```

The plane equation becomes `f · p = 0`, where **p** is extended to 4D with `w = 1`.

**Example**: Find the plane containing the point `(1, 2, 3)` with normal `(0, 0, 1)`.

```
d = -n · q = -(0·1 + 0·2 + 1·3) = -3

f = [0, 0, 1, -3]
```

Verify: `f · (1, 2, 3) = 0 + 0 + 3 - 3 = 0` ✓

This is the plane `z = 3`.

```cpp
struct Plane {
    float x, y, z, w;

    Plane() = default;
    Plane(float nx, float ny, float nz, float d) : x(nx), y(ny), z(nz), w(d) {}
    Plane(const Vector3D& n, float d) : x(n.x), y(n.y), z(n.z), w(d) {}

    const Vector3D& GetNormal() const {
        return reinterpret_cast<const Vector3D&>(x);
    }
};

float Dot(const Plane& f, const Vector3D& v) {
    return f.x * v.x + f.y * v.y + f.z * v.z;
}

float Dot(const Plane& f, const Point3D& p) {
    return f.x * p.x + f.y * p.y + f.z * p.z + f.w;
}
```

### 3.4.2 Distance Between a Point and a Plane

For a **normalized** plane (where `‖n‖ = 1`), the **signed perpendicular distance** from a point **p** to the plane is:

```
distance = f · p = n · p + d
```

- **Positive** → point is on the **front** (positive) side (the side the normal points toward)
- **Negative** → point is on the **back** (negative) side
- **Zero** → point lies exactly in the plane

#### Numerical Example

Plane: `f = [0, 1, 0, -5]` (the plane `y = 5`, normal pointing in +y).

Verify it's normalized: `‖(0, 1, 0)‖ = 1` ✓

Distance from `p₁ = (3, 8, -2)`:

```
f · p₁ = 0·3 + 1·8 + 0·(-2) + (-5) = 3
```

Point is 3 units in front of the plane.

Distance from `p₂ = (0, 2, 0)`:

```
f · p₂ = 0 + 2 + 0 - 5 = -3
```

Point is 3 units behind the plane.

Distance from `p₃ = (7, 5, 100)`:

```
f · p₃ = 0 + 5 + 0 - 5 = 0
```

Point lies in the plane.

### 3.4.3 Reflection Through a Plane

To reflect a point **p** through a normalized plane **f** = [**n** | d]:

```
p' = p - 2·(f · p)·n
```

The idea: subtract the signed distance once to reach the plane, then subtract it again to go the same distance on the other side.

#### 4×4 Reflection Matrix

```
                   | 1 - 2nₓ²    -2nₓnᵧ    -2nₓn_z    -2nₓd |
H_reflect(f)  =   | -2nₓnᵧ     1 - 2nᵧ²   -2nᵧn_z    -2nᵧd |
                   | -2nₓn_z    -2nᵧn_z    1 - 2n_z²   -2n_zd |
                   |  0           0           0           1     |
```

When `d = 0`, the fourth column becomes zeros (except the 1), and the matrix reduces to a simple reflection through a plane containing the origin.

#### Numerical Example

Reflect the point `p = (2, 7, 3)` through the plane `y = 4`, i.e., `f = [0, 1, 0, -4]`.

**Step 1**: The plane is normalized (`‖(0, 1, 0)‖ = 1`).

**Step 2**: Signed distance:

```
f · p = 0·2 + 1·7 + 0·3 + (-4) = 3
```

**Step 3**: Reflect:

```
p' = (2, 7, 3) - 2·3·(0, 1, 0) = (2, 7, 3) - (0, 6, 0) = (2, 1, 3)
```

The point at y=7 is reflected to y=1, which is symmetric about y=4. ✓

**Verify with the matrix** (`nₓ = 0, nᵧ = 1, n_z = 0, d = -4`):

```
      | 1   0   0   0 |
H  =  | 0  -1   0   8 |
      | 0   0   1   0 |
      | 0   0   0   1 |

      | 2 |     | 2       |     | 2 |
H  ·  | 7 |  =  | -7 + 8  |  =  | 1 |   ✓
      | 3 |     | 3       |     | 3 |
      | 1 |     | 1       |     | 1 |
```

```cpp
Transform4D MakeReflection(const Plane& f) {
    float x = f.x * -2.0f;
    float y = f.y * -2.0f;
    float z = f.z * -2.0f;
    float nxny = x * f.y;
    float nxnz = x * f.z;
    float nynz = y * f.z;

    return Transform4D(
        x * f.x + 1.0f, nxny,           nxnz,           x * f.w,
        nxny,           y * f.y + 1.0f,  nynz,           y * f.w,
        nxnz,           nynz,           z * f.z + 1.0f,  z * f.w);
}
```

### 3.4.4 Intersection of a Line and a Plane

Given line `L(t) = p + t·v` and plane **f** = [**n** | d], solve `f · L(t) = 0`:

```
t = -(f · p) / (f · v)
```

The intersection point is:

```
q = p - [(f · p) / (f · v)] · v
```

where `f · p = nₓpₓ + nᵧpᵧ + n_zpz + d` (4D dot, `p_w = 1`) and `f · v = nₓvₓ + nᵧvᵧ + n_zvz` (3D dot, `v_w = 0`).

If `f · v = 0`, the line is parallel to the plane (no intersection or lies in the plane).

**Ray conditions**: For a ray (t ≥ 0) hitting the front side:
- `f · p > 0` (ray starts on the front side)
- `f · v < 0` (ray points toward the plane)

#### Numerical Example

**Line**: `p = (0, 3, 0)`, `v = (1, -1, 0)`

**Plane**: `f = [0, 1, 0, 0]` (the xz-plane, i.e., y = 0)

**Step 1**: `f · p = 0·0 + 1·3 + 0·0 + 0 = 3`

**Step 2**: `f · v = 0·1 + 1·(-1) + 0·0 = -1`

**Step 3**: `t = -3 / (-1) = 3`

**Step 4**: `q = (0, 3, 0) + 3·(1, -1, 0) = (3, 0, 0)`

Verify: `f · q = 0 + 0 + 0 + 0 = 0` ✓ — the point lies in the plane.

Ray check: `t = 3 ≥ 0`, `f · p = 3 > 0`, `f · v = -1 < 0`. All conditions met — the ray hits the front of the plane.

```cpp
bool IntersectLinePlane(const Point3D& p, const Vector3D& v,
                        const Plane& f, Point3D *q)
{
    float fv = Dot(f, v);
    if (fabs(fv) > FLT_MIN) {
        *q = p - v * (Dot(f, p) / fv);
        return true;
    }
    return false;
}
```

### 3.4.5 Intersection of Three Planes

Three planes with linearly independent normals intersect at a **single point**:

```
       d₁(n₃ × n₂) + d₂(n₁ × n₃) + d₃(n₂ × n₁)
p  =  ─────────────────────────────────────────────
                    [n₁, n₂, n₃]
```

where `[n₁, n₂, n₃] = n₁ · (n₂ × n₃)` is the scalar triple product (= determinant of the matrix whose rows are the normals).

**Degenerate cases** (determinant = 0):
- At least two planes are parallel
- No two are parallel, but pairwise intersections are parallel lines

#### Numerical Example

**Plane 1**: `x = 2` → `f₁ = [1, 0, 0, -2]`, so `n₁ = (1, 0, 0)`, `d₁ = -2`

**Plane 2**: `y = 3` → `f₂ = [0, 1, 0, -3]`, so `n₂ = (0, 1, 0)`, `d₂ = -3`

**Plane 3**: `z = 5` → `f₃ = [0, 0, 1, -5]`, so `n₃ = (0, 0, 1)`, `d₃ = -5`

**Step 1**: Cross products:

```
n₃ × n₂ = (0, 0, 1) × (0, 1, 0) = (-1, 0, 0)
n₁ × n₃ = (1, 0, 0) × (0, 0, 1) = (0, -1, 0)
n₂ × n₁ = (0, 1, 0) × (1, 0, 0) = (0, 0, -1)
```

**Step 2**: Scalar triple product:

```
[n₁, n₂, n₃] = (1, 0, 0) · ((0, 1, 0) × (0, 0, 1)) = (1, 0, 0) · (1, 0, 0) = 1
```

**Step 3**: Intersection point:

```
p = [ (-2)(-1, 0, 0) + (-3)(0, -1, 0) + (-5)(0, 0, -1) ] / 1 = (2, 3, 5)
```

The three axis-aligned planes `x=2, y=3, z=5` meet at exactly `(2, 3, 5)`. ✓

```cpp
bool IntersectThreePlanes(const Plane& f1, const Plane& f2,
                          const Plane& f3, Point3D *p)
{
    const Vector3D& n1 = f1.GetNormal();
    const Vector3D& n2 = f2.GetNormal();
    const Vector3D& n3 = f3.GetNormal();

    Vector3D n1xn2 = Cross(n1, n2);
    float det = Dot(n1xn2, n3);

    if (fabs(det) > FLT_MIN) {
        *p = (Cross(n3, n2) * f1.w + Cross(n1, n3) * f2.w
              + n1xn2 * f3.w) / det;
        return true;
    }
    return false;
}
```

> **Note**: The code uses `f.w` which stores *d*, and the signs work out because the formula has `dᵢ` in the numerator and the cross products are arranged to absorb the minus signs (each `dᵢ` in the formula is already negative for the axis-aligned planes above).

### 3.4.6 Intersection of Two Planes

Two nonparallel planes intersect along a **line**.

**Direction**: `v = n₁ × n₂` (perpendicular to both normals).

**Point on line**: Introduce a third plane `[v | 0]` (containing the origin) and solve the three-plane intersection:

```
       d₁(v × n₂) + d₂(n₁ × v)
p  =  ───────────────────────────
                v²
```

#### Numerical Example

**Plane 1**: `x + y = 4` → `f₁ = [1, 1, 0, -4]`, `n₁ = (1, 1, 0)`, `d₁ = -4`

**Plane 2**: `y + z = 2` → `f₂ = [0, 1, 1, -2]`, `n₂ = (0, 1, 1)`, `d₂ = -2`

**Step 1**: Direction:

```
v = n₁ × n₂ = (1, 1, 0) × (0, 1, 1)
  = (1·1 - 0·1,  0·0 - 1·1,  1·1 - 1·0) = (1, -1, 1)
```

**Step 2**: Cross products:

```
v × n₂ = (1, -1, 1) × (0, 1, 1) = (-1 - 1,  0 - 1,  1 - 0) = (-2, -1, 1)
n₁ × v = (1, 1, 0) × (1, -1, 1) = (1 - 0,  0 - 1,  -1 - 1) = (1, -1, -2)
```

**Step 3**: `v² = 1 + 1 + 1 = 3`

**Step 4**: Point on line:

```
p = [ (-4)(-2, -1, 1) + (-2)(1, -1, -2) ] / 3
  = [ (8, 4, -4) + (-2, 2, 4) ] / 3
  = (6, 6, 0) / 3
  = (2, 2, 0)
```

**Verify**: Plane 1: `2 + 2 = 4` ✓; Plane 2: `2 + 0 = 2` ✓.

The intersection line is `L(t) = (2, 2, 0) + t·(1, -1, 1)`.

Spot-check at `t = 1`: point `(3, 1, 1)`. Plane 1: `3 + 1 = 4` ✓; Plane 2: `1 + 1 = 2` ✓.

```cpp
bool IntersectTwoPlanes(const Plane& f1, const Plane& f2,
                        Point3D *p, Vector3D *v)
{
    const Vector3D& n1 = f1.GetNormal();
    const Vector3D& n2 = f2.GetNormal();
    *v = Cross(n1, n2);
    float det = Dot(*v, *v);

    if (fabs(det) > FLT_MIN) {
        *p = (Cross(*v, n2) * f1.w + Cross(n1, *v) * f2.w) / det;
        return true;
    }
    return false;
}
```

### 3.4.7 Transforming Planes

Let **H** be a 4×4 affine transformation matrix (3×3 matrix **M** plus translation **t**). A plane transforms as:

```
f_B = f_A · adj(H)
```

This is the 4D analog of the normal vector transformation. In practice, since planes are usually renormalized, this is equivalent to:

```
f_B = f_A · H⁻¹
```

(up to a scalar factor of det(**H**)).

Unlike normal vectors, **translation matters** for planes (the *d* component changes), so even when **M** is orthogonal, you cannot skip the inverse.

#### Numerical Example

**Plane**: `f_A = [0, 1, 0, -3]` (the plane `y = 3`)

**Transform**: Translate by `t = (0, 5, 0)`, no rotation or scale:

```
      | 1  0  0  0 |              | 1  0  0   0 |
H  =  | 0  1  0  5 |      H⁻¹ =  | 0  1  0  -5 |
      | 0  0  1  0 |              | 0  0  1   0 |
      | 0  0  0  1 |              | 0  0  0   1 |
```

```
                         | 1  0  0   0 |
f_B = [0, 1, 0, -3]  ·  | 0  1  0  -5 |
                         | 0  0  1   0 |
                         | 0  0  0   1 |
```

Computing the row-vector × matrix product:

```
f_B,x = 0     f_B,y = 1     f_B,z = 0     f_B,w = 1·(-5) + (-3)·1 = -8

f_B = [0, 1, 0, -8]
```

This is the plane `y = 8`. Since we translated everything up by 5, the plane `y = 3` becomes `y = 8`. ✓

```cpp
Plane operator*(const Plane& f, const Transform4D& H) {
    return Plane(
        f.x * H(0,0) + f.y * H(1,0) + f.z * H(2,0),
        f.x * H(0,1) + f.y * H(1,1) + f.z * H(2,1),
        f.x * H(0,2) + f.y * H(1,2) + f.z * H(2,2),
        f.x * H(0,3) + f.y * H(1,3) + f.z * H(2,3) + f.w);
}
```

---

## 3.5 Plücker Coordinates

Previously, our line representation `L(t) = p + t·v` requires an explicit point. Just as we replaced "point + normal" with an implicit 4-tuple for planes, we can represent lines implicitly using **Plücker coordinates** — six numbers that describe a line without reference to any particular point on it.

### 3.5.1 Implicit Lines

Given two distinct points **p₁** and **p₂** on a line:

**Direction**: `v = p₂ - p₁`

**Moment**: `m = p₁ × p₂`

The line's Plücker coordinates are written `{v | m}`.

#### Why the Moment Is an Implicit Property

The moment doesn't depend on *which* pair of equidistant points you use. If **q₁**, **q₂** are another pair on the same line with `q₂ - q₁ = p₂ - p₁`, then:

```
q₁ × q₂ = (p₁ + r) × (p₂ + r)
         = p₁ × p₂ + r × (p₂ - p₁) + r × r
         = p₁ × p₂
```

The cross products with **r** vanish because **r** is parallel to the line direction.

#### Properties

- **Homogeneous**: any scalar multiple `{s·v | s·m}` represents the same line.
- **Normalized** when `‖v‖ = 1`.
- **Perpendicularity**: `v · m = 0` always.
- **Distance to origin**: `‖m‖ / ‖v‖` gives the perpendicular distance.
- **Moment direction**: determined by the right-hand rule — when the right hand's fingers curl along **v** with the palm facing the origin, the thumb points along **m**.

#### Example: Plücker Coordinates of the x-axis

Points: `p₁ = (0, 0, 0)`, `p₂ = (1, 0, 0)`.

```
v = (1, 0, 0)
m = (0, 0, 0) × (1, 0, 0) = (0, 0, 0)
```

`{(1, 0, 0) | (0, 0, 0)}`. The moment is zero because the line passes through the origin.

#### Example: A Line Offset from the Origin

Points: `p₁ = (0, 1, 0)`, `p₂ = (1, 1, 0)`.

```
v = (1, 0, 0)
m = (0, 1, 0) × (1, 1, 0)
  = (1·0 - 0·1,  0·1 - 0·0,  0·1 - 1·1)
  = (0, 0, -1)
```

`{(1, 0, 0) | (0, 0, -1)}`. Distance to origin = `‖m‖ / ‖v‖ = 1/1 = 1`. ✓ (the line y=1 is 1 unit from the origin)

Check perpendicularity: `v · m = 0 + 0 + 0 = 0` ✓

#### Line Data Structure

```cpp
struct Line {
    Vector3D direction;
    Vector3D moment;

    Line() = default;
    Line(float vx, float vy, float vz, float mx, float my, float mz)
        : direction(vx, vy, vz), moment(mx, my, mz) {}
    Line(const Vector3D& v, const Vector3D& m) : direction(v), moment(m) {}
};
```

### 3.5.2 Homogeneous Formulas

The following table collects the most important formulas involving homogeneous points `(p | w)`, planes `[n | d]`, and lines `{v | m}`. All results are homogeneous (may need normalization).

| ID | Formula | Description |
|----|---------|-------------|
| A | `{v | p × v}` | Line through point **p** with direction **v** |
| B | `{p₂ - p₁ | p₁ × p₂}` | Line through two points |
| C | `{p | 0}` | Line through point **p** and the origin |
| D | `{w₁p₂ - w₂p₁ | p₁ × p₂}` | Line through two homogeneous points |
| E | `{n₁ × n₂ | d₁n₂ - d₂n₁}` | Line where two planes intersect |
| F | `(m × n + d·v | -n · v)` | Point where line meets plane |
| G | `(v × m | v²)` | Point on line closest to origin |
| H | `(-d·n | n²)` | Point on plane closest to origin |
| I | `[v × u | -u · m]` | Plane containing line, parallel to direction **u** |
| J | `[v × p + m | -p · m]` | Plane containing line and point **p** |
| K | `[m | 0]` | Plane containing line and the origin |
| L | `[v × p + w·m | -p · m]` | Plane containing line and homogeneous point |
| O | `\|v₁ · m₂ + v₂ · m₁\| / ‖v₁ × v₂‖` | Distance between two lines |
| P | `‖v × p + m‖ / ‖v‖` | Distance from line to point **p** |
| Q | `‖m‖ / ‖v‖` | Distance from line to origin |
| R | `\|n · p + d\| / ‖n‖` | Distance from plane to point **p** |

#### Duality Between Points and Planes

There is a beautiful symmetry: swapping direction and moment in a line (`v ↔ m`) and reinterpreting a point's components as a plane (and vice versa) transforms one formula into another. Rows B↔E, F↔L, G↔M, and H↔N are **dual pairs**.

#### Numerical Example: Line Through a Point with a Direction (Row A)

Point `p = (3, 0, 2)`, direction `v = (0, 1, 0)`.

```
m = p × v = (3, 0, 2) × (0, 1, 0)
  = (0·0 - 2·1,  2·0 - 3·0,  3·1 - 0·0)
  = (-2, 0, 3)
```

Line: `{(0, 1, 0) | (-2, 0, 3)}`

Distance to origin: `‖m‖ / ‖v‖ = √(4 + 0 + 9) / 1 = √13 ≈ 3.606`

Verify: the origin's closest approach to the line through (3, 0, 2) going in +y is the distance in the xz-plane: `√(9 + 4) = √13` ✓

#### Numerical Example: Line Where Two Planes Intersect (Row E)

**Plane 1**: `[1, 0, 0, -1]` (`x = 1`), so `n₁ = (1, 0, 0)`, `d₁ = -1`

**Plane 2**: `[0, 0, 1, -2]` (`z = 2`), so `n₂ = (0, 0, 1)`, `d₂ = -2`

```
v = n₁ × n₂ = (1, 0, 0) × (0, 0, 1) = (0, -1, 0)

m = d₁·n₂ - d₂·n₁
  = (-1)(0, 0, 1) - (-2)(1, 0, 0)
  = (0, 0, -1) + (2, 0, 0)
  = (2, 0, -1)
```

Line: `{(0, -1, 0) | (2, 0, -1)}`

This line runs in the -y direction at x=1, z=2. Plugging point (1, 0, 2): it lies on both planes, and the line direction is along y. ✓

#### Numerical Example: Point Where Line Meets Plane (Row F)

Use the line from the previous example: `v = (0, -1, 0)`, `m = (2, 0, -1)`.

Plane: `[0, 1, 0, -5]` (`y = 5`), so `n = (0, 1, 0)`, `d = -5`.

```
m × n = (2, 0, -1) × (0, 1, 0)
      = (0·0 - (-1)·1,  (-1)·0 - 2·0,  2·1 - 0·0)
      = (1, 0, 2)

d·v = (-5)(0, -1, 0) = (0, 5, 0)

m × n + d·v = (1, 5, 2)

-n · v = -(0·0 + 1·(-1) + 0·0) = 1
```

Homogeneous point: `(1, 5, 2 | 1)`. Dividing by `w = 1`: point `(1, 5, 2)`.

Verify: x=1 ✓ (on plane 1), z=2 ✓ (on plane 2), y=5 ✓ (on the y=5 plane). The line at x=1, z=2 passes through y=5 at exactly (1, 5, 2). ✓

#### Numerical Example: Plane Containing a Line and a Point (Row J)

Line: `{(1, 0, 0) | (0, 0, -1)}` (the line at y=1 from our earlier example)

Point: `p = (0, 0, 3)`

```
v × p = (1, 0, 0) × (0, 0, 3)
      = (0·3 - 0·0,  0·0 - 1·3,  1·0 - 0·0)
      = (0, -3, 0)

v × p + m = (0, -3, 0) + (0, 0, -1) = (0, -3, -1)

-p · m = -((0)(0) + (0)(0) + (3)(-1)) = 3
```

Plane: `[0, -3, -1, 3]`.

Normalize: `‖n‖ = √(0 + 9 + 1) = √10`. Normalized: `[0, -3/√10, -1/√10, 3/√10]`.

Verify with the point (0, 0, 3): `0 + 0 + (-1)(3) + 3 = 0` ✓

Verify with a point on the line, say (5, 1, 0): `0 + (-3)(1) + 0 + 3 = 0` ✓

#### Numerical Example: Distance Between Two Lines (Row O)

**Line 1**: Through (0, 0, 0) and (1, 0, 0): `v₁ = (1, 0, 0)`, `m₁ = (0, 0, 0)`

**Line 2**: Through (0, 1, 1) and (0, 2, 1): `v₂ = (0, 1, 0)`, `m₂ = (0, 1, 0) × (0, 2, 1)`

Wait, let's compute carefully. `m₂ = p₁ × p₂ = (0, 1, 1) × (0, 2, 1) = (1·1 - 1·2, 1·0 - 0·1, 0·2 - 1·0) = (-1, 0, 0)`

```
v₁ · m₂ + v₂ · m₁ = (1, 0, 0)·(-1, 0, 0) + (0, 1, 0)·(0, 0, 0) = -1 + 0 = -1

v₁ × v₂ = (1, 0, 0) × (0, 1, 0) = (0, 0, 1)

d = |-1| / ‖(0, 0, 1)‖ = 1/1 = 1
```

This matches our earlier calculation: the x-axis and the y-parallel line at z=1 are 1 unit apart. ✓

### 3.5.3 Transforming Lines

Given a line `{v_A | m_A}` and an affine transform **H** (3×3 matrix **M** plus translation **t**):

```
v_B = M · v_A

m_B = m_A · adj(M) + t × v_B
```

The direction transforms like a regular vector. The moment transforms via the adjugate (like a normal/cross-product) plus an extra term from the translation.

For orthogonal **M** (rotation/reflection), `adj(M) = M` (since det = ±1 and `adj = det · M⁻ᵀ = ±M`), so the formula simplifies.

#### Numerical Example

**Line**: Through (0, 1, 0) and (1, 1, 0), so `v_A = (1, 0, 0)`, `m_A = (0, 0, -1)`.

**Transform**: Scale x by 2, translate by (0, 0, 3):

```
      | 2  0  0 |
M  =  | 0  1  0 |        t = (0, 0, 3)
      | 0  0  1 |
```

**Step 1**: Direction:

```
v_B = M · v_A = (2, 0, 0)
```

**Step 2**: Adjugate of **M**:

For a diagonal matrix, the adjugate has entries:

```
              | M₂₂·M₃₃   0          0        |     | 1  0  0 |
adj(M)  =     | 0          M₁₁·M₃₃   0        |  =  | 0  2  0 |
              | 0          0          M₁₁·M₂₂  |     | 0  0  2 |
```

**Step 3**: Moment:

```
                     | 1  0  0 |
m_A · adj(M) = (0, 0, -1) · | 0  2  0 | = (0, 0, -2)
                     | 0  0  2 |

t × v_B = (0, 0, 3) × (2, 0, 0)
        = (0·0 - 3·0,  3·2 - 0·0,  0·0 - 0·2)
        = (0, 6, 0)

m_B = (0, 0, -2) + (0, 6, 0) = (0, 6, -2)
```

**Result**: `{(2, 0, 0) | (0, 6, -2)}`

**Verify by transforming the original points**:

```
p₁' = M·(0, 1, 0) + t = (0, 1, 0) + (0, 0, 3) = (0, 1, 3)
p₂' = M·(1, 1, 0) + t = (2, 1, 0) + (0, 0, 3) = (2, 1, 3)
```

Direction: `(2, 1, 3) - (0, 1, 3) = (2, 0, 0)` ✓

Moment: `(0, 1, 3) × (2, 1, 3) = (1·3 - 3·1, 3·2 - 0·3, 0·1 - 1·2) = (0, 6, -2)` ✓

```cpp
Line TransformLine(const Line& line, const Transform4D& H) {
    Matrix3D adj(Cross(H[1], H[2]), Cross(H[2], H[0]), Cross(H[0], H[1]));
    const Point3D& t = H.GetTranslation();
    Vector3D v = H * line.direction;
    Vector3D m = adj * line.moment + Cross(t, v);
    return Line(v, m);
}
```

---

## Summary

| Concept | Key Formula | Data |
|---------|-------------|------|
| Triangle mesh | V - E + F = 2 | Vertex list + index list |
| Face normal | **n** = (p₁ - p₀) × (p₂ - p₀) | 3D vector |
| Normal transform | n_B = n_A · M⁻¹ | Row vector × inverse |
| Parametric line | L(t) = p + t·v | Point + direction |
| Point-line dist. | d = ‖u × v‖ / ‖v‖ | Cross product formula |
| Implicit plane | n · p + d = 0 | 4D row vector [n \| d] |
| Point-plane dist. | f · p (when normalized) | Signed distance |
| Plane reflection | p' = p − 2(f · p)·n | 4×4 matrix form exists |
| Line-plane intersection | t = −(f · p)/(f · v) | q = p + t·v |
| Three-plane intersection | Determinant-based formula | Single point |
| Two-plane intersection | v = n₁ × n₂ | Line result |
| Plane transform | f_B = f_A · adj(H) | 4D row × adjugate |
| Plücker line | {v \| m} where m = p₁ × p₂ | 6 components |
| Line transform | v_B = M·v_A, m_B = m_A·adj(M) + t × v_B | Direction + moment |

---

*Next: [Chapter 4 — Advanced Algebra](./04-advanced-algebra.md)* | *Previous: [Chapter 2 — Transforms](./02-transforms.md)* | *[Overview](./00-overview.md)*
