# Chapter 1: Vectors and Matrices

Vectors and matrices are the fundamental building blocks of game engine mathematics. They appear practically everywhere — from representing positions and directions to transforming entire coordinate systems. This chapter builds from the ground up, assuming only basic trigonometry knowledge.

---

## 1.1 Vector Fundamentals

### Scalars vs Vectors

A **scalar** is a single number that describes a magnitude — things like distance, mass, or time. For example, "5 meters" is a scalar.

A **vector** is a quantity that has both a **magnitude** (size) and a **direction**. Examples include:

- **Displacement**: the difference between two points has both a distance (magnitude) and a direction
- **Velocity**: a projectile has both speed (magnitude) and a heading (direction)
- **Force**: applied with a certain strength (magnitude) in a certain direction

### Components and Notation

A vector's direction and magnitude are described by numerical **components**. We write them inside parentheses:

```
v = (1, 2, 3)
```

This is a 3D vector with components 1, 2, and 3.

In general, an *n*-dimensional vector **v** is written as:

```
v = (v0, v1, ..., v(n-1))
```

### Zero-Based Indexing

We use **zero-based subscripts**: `v0` is the first component, `v1` is the second, and so on. This matches how computers access array elements.

### x, y, z, w Components

In 3D, the components correspond to the coordinate axes:

```
v = (vx, vy, vz)
```

For the vector (1, 2, 3):
- vx = 1
- vy = 2
- vz = 3

In **4D**, we add a fourth component called **w** (sometimes called the "weight"):

```
v = (vx, vy, vz, vw) = (1, 2, 3, 1)
```

The letter "w" is used because it's the closest letter to x, y, z in the alphabet. Four-dimensional vectors arise frequently in game engines for homogeneous coordinates (covered in Chapter 2).

### Vectors Have No Fixed Location

A vector by itself has no position in space — it only represents a direction and magnitude. Two arrows drawn in different places but with the same direction and length represent the **same** vector. A vector can represent a point only when thought of as an offset from some origin.

### C++ Vector3D Data Structure

```cpp
struct Vector3D
{
    float x, y, z;

    Vector3D() = default;

    Vector3D(float a, float b, float c)
    {
        x = a;
        y = b;
        z = c;
    }

    float& operator [](int i)
    {
        return ((&x)[i]);
    }

    const float& operator [](int i) const
    {
        return ((&x)[i]);
    }
};
```

The bracket operators allow zero-based index access: `v[0]` returns `x`, `v[1]` returns `y`, `v[2]` returns `z`.

---

## 1.2 Basic Vector Operations

### 1.2.1 Magnitude and Scalar Multiplication

#### Magnitude (Length)

The **magnitude** (or length) of a vector **v** is written `‖v‖` and calculated with:

```
‖v‖ = √(vx² + vy² + vz²)
```

This is just the Pythagorean theorem extended to *n* dimensions.

**Example 1**: Find the magnitude of **v** = (3, 4, 0).

```
‖v‖ = √(3² + 4² + 0²)
     = √(9 + 16 + 0)
     = √(25)
     = 5
```

**Example 2**: Find the magnitude of **v** = (1, 2, 3).

```
‖v‖ = √(1² + 2² + 3²)
     = √(1 + 4 + 9)
     = √(14)
     ≈ 3.742
```

**Example 3**: Find the magnitude of **v** = (2, -1, 2).

```
‖v‖ = √(2² + (-1)² + 2²)
     = √(4 + 1 + 4)
     = √(9)
     = 3
```

#### The Zero Vector

The **zero vector** has all components equal to zero: **0** = (0, 0, 0). It is the only vector with magnitude zero. It has no direction.

#### Scalar Multiplication

Multiplying a vector by a scalar *t* multiplies each component:

```
tv = (t·vx, t·vy, t·vz)
```

**Example**: Multiply **v** = (2, -3, 1) by *t* = 4.

```
4v = (4·2, 4·(-3), 4·1)
   = (8, -12, 4)
```

**Key property**: Scalar multiplication scales the magnitude:

```
‖tv‖ = |t| · ‖v‖
```

**Example**: v = (2, -3, 1), t = 4.

```
‖v‖ = √(4 + 9 + 1) = √(14) ≈ 3.742
‖4v‖ = √(64 + 144 + 16) = √(224) ≈ 14.967

Check: |4| · ‖v‖ = 4 · 3.742 ≈ 14.967  ✓
```

When *t* is positive, the scaled vector points in the same direction. When *t* is negative, it points in the **opposite** direction but has the same magnitude as |*t*| · ‖**v**‖.

#### Negation

**Negation** is scalar multiplication by -1:

```
-v = (-vx, -vy, -vz)
```

**Example**: **v** = (3, -1, 5), so **-v** = (-3, 1, -5). Same magnitude, opposite direction.

#### Unit Vectors and Normalization

A **unit vector** has magnitude 1. Any nonzero vector can be **normalized** (turned into a unit vector) by dividing by its magnitude:

```
v̂ = v / ‖v‖
```

The hat symbol `v̂` indicates a unit vector.

**Example**: Normalize **v** = (3, 4, 0).

```
‖v‖ = √(9 + 16 + 0) = √(25) = 5

v̂ = v / ‖v‖ = (3/5, 4/5, 0/5) = (0.6, 0.8, 0)

Check: ‖v̂‖ = √(0.36 + 0.64 + 0) = √(1) = 1  ✓
```

**Example**: Normalize **v** = (1, 2, 2).

```
‖v‖ = √(1 + 4 + 4) = √(9) = 3

v̂ = (1/3, 2/3, 2/3) ≈ (0.333, 0.667, 0.667)

Check: ‖v̂‖ = √(1/9 + 4/9 + 4/9) = √(9/9) = 1  ✓
```

#### C++ Implementation

```cpp
struct Vector3D
{
    // ... (members and constructors as before) ...

    Vector3D& operator *=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
        return (*this);
    }

    Vector3D& operator /=(float s)
    {
        s = 1.0F / s;
        x *= s;
        y *= s;
        z *= s;
        return (*this);
    }
};

inline Vector3D operator *(const Vector3D& v, float s)
{
    return (Vector3D(v.x * s, v.y * s, v.z * s));
}

inline Vector3D operator /(const Vector3D& v, float s)
{
    s = 1.0F / s;
    return (Vector3D(v.x * s, v.y * s, v.z * s));
}

inline Vector3D operator -(const Vector3D& v)
{
    return (Vector3D(-v.x, -v.y, -v.z));
}

inline float Magnitude(const Vector3D& v)
{
    return (sqrt(v.x * v.x + v.y * v.y + v.z * v.z));
}

inline Vector3D Normalize(const Vector3D& v)
{
    return (v / Magnitude(v));
}
```

Note: the division operator computes the reciprocal once and multiplies — this avoids three separate divisions.

---

### 1.2.2 Addition and Subtraction

Vectors are added and subtracted **componentwise**:

```
a + b = (ax + bx, ay + by, az + bz)
```

```
a - b = (ax - bx, ay - by, az - bz)
```

**Example — Addition**: **a** = (1, 3, -2), **b** = (4, -1, 5).

```
a + b = (1+4, 3+(-1), -2+5)
      = (5, 2, 3)
```

**Example — Subtraction**: **a** = (7, 2, 4), **b** = (3, 5, 1).

```
a - b = (7-3, 2-5, 4-1)
      = (4, -3, 3)
```

#### Geometric Visualization (Tip-to-Tail)

- **Addition**: Draw **a**, then place the *beginning* of **b** at the *tip* of **a**. The vector from the start of **a** to the end of **b** is **a** + **b**.
- **Subtraction**: **a** - **b** = **a** + (-**b**). Negate **b** and add it tip-to-tail.

#### Properties

| Property | Description |
|----------|-------------|
| (**a** + **b**) + **c** = **a** + (**b** + **c**) | Associative law |
| **a** + **b** = **b** + **a** | Commutative law |
| (*st*)**a** = *s*(*t***a**) | Associative law for scalar multiplication |
| *t***a** = **a***t* | Commutative law for scalar multiplication |
| *t*(**a** + **b**) = *t***a** + *t***b** | Distributive law |
| (*s* + *t*)**a** = *s***a** + *t***a** | Distributive law |

**Example — Verifying commutativity**: **a** = (2, 1, 3), **b** = (5, -2, 0).

```
a + b = (7, -1, 3)
b + a = (7, -1, 3)   ✓  Same result
```

**Example — Verifying distributive property**: *t* = 3, **a** = (1, 2, 0), **b** = (4, -1, 3).

```
t(a + b) = 3 · (5, 1, 3) = (15, 3, 9)

ta + tb = (3, 6, 0) + (12, -3, 9) = (15, 3, 9)  ✓
```

#### C++ Implementation

```cpp
struct Vector3D
{
    Vector3D& operator +=(const Vector3D& v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return (*this);
    }

    Vector3D& operator -=(const Vector3D& v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return (*this);
    }
};

inline Vector3D operator +(const Vector3D& a, const Vector3D& b)
{
    return (Vector3D(a.x + b.x, a.y + b.y, a.z + b.z));
}

inline Vector3D operator -(const Vector3D& a, const Vector3D& b)
{
    return (Vector3D(a.x - b.x, a.y - b.y, a.z - b.z));
}
```

---

## 1.3 Matrix Fundamentals

### What Is a Matrix?

A **matrix** is a rectangular array of numbers arranged in rows and columns. A matrix with *n* rows and *m* columns has size *n* × *m*.

Example — a 2×3 matrix:

```
M = | 1  2  3 |
    | 4  5  6 |
```

If *n* = *m*, it's a **square matrix**.

### Indexing

We use zero-based indices: `M_ij` is the entry in row *i*, column *j*.

For the matrix above:
- M₀₀ = 1, M₀₁ = 2, M₀₂ = 3
- M₁₀ = 4, M₁₁ = 5, M₁₂ = 6

### Diagonal and Off-Diagonal

The **main diagonal** entries are `M_ii` — where row index equals column index. The rest are **off-diagonal**.

A **diagonal matrix** has all off-diagonal entries equal to zero:

```
| 3   0   0 |
| 0  -1   0 |
| 0   0   7 |
```

### Transpose

The **transpose** `Mᵀ` swaps rows and columns: `(Mᵀ)_ij = M_ji`.

If M is *n* × *m*, then `Mᵀ` is *m* × *n*.

**Example**:

```
M = | 1  2  3 |        Mᵀ = | 1  4 |
    | 4  5  6 |              | 2  5 |
                              | 3  6 |
```

**Example** (square matrix):

```
A = | 1  2  3 |        Aᵀ = | 1  4  7 |
    | 4  5  6 |              | 2  5  8 |
    | 7  8  9 |              | 3  6  9 |
```

### Symmetric Matrices

A matrix is **symmetric** if `M = Mᵀ`, meaning `M_ij = M_ji`. It must be square.

**Example**:

```
S = | 1   3  -2 |
    | 3   5   0 |
    |-2   0   4 |

Sᵀ = | 1   3  -2 |  =  S   ✓ symmetric
     | 3   5   0 |
     |-2   0   4 |
```

Every diagonal matrix is automatically symmetric.

### Antisymmetric (Skew-Symmetric) Matrices

A matrix is **antisymmetric** if `Mᵀ = -M`, meaning `M_ij = -M_ji`. This forces all diagonal entries to be zero (since `M_ii = -M_ii` implies `M_ii = 0`).

**Example**:

```
A = |  0   3  -2 |
    | -3   0   5 |
    |  2  -5   0 |

Aᵀ = |  0  -3   2 |  =  -A   ✓ antisymmetric
     |  3   0  -5 |
     | -2   5   0 |
```

### Column Vectors vs Row Vectors

An *n*-dimensional vector can be viewed as:
- An *n* × 1 matrix → **column vector**
- A 1 × *n* matrix → **row vector**

We follow the convention of treating vectors as **column vectors**:

```
                | vx |
v = (vx, vy, vz) =  | vy |
                | vz |
```

The transpose converts a column vector to a row vector:

```
vᵀ = [ vx  vy  vz ]
```

### Matrix as Array of Column Vectors

A 3×3 matrix can be viewed as three column vectors side by side:

```
M = [a  b  c]
```

**Example**: If **a** = (1, 4, 7), **b** = (2, 5, 8), **c** = (3, 6, 9):

```
M = | 1  2  3 |     column 0 = a = (1, 4, 7)
    | 4  5  6 |     column 1 = b = (2, 5, 8)
    | 7  8  9 |     column 2 = c = (3, 6, 9)
```

### Column-Major Storage Order

Since we use column vectors, we store matrices in **column-major order**: consecutive memory addresses run down each column.

For a 3×3 matrix, the flat array order is:

```
Index:   0  1  2  3  4  5  6  7  8
Entry: M00 M10 M20 M01 M11 M21 M02 M12 M22
       ---- col 0 --- --- col 1 --- --- col 2 ---
```

This means the first array index is the **column** number and the second is the **row** number in the 2D array. The operators swap the indices back to the familiar (row, column) order.

### C++ Matrix3D Data Structure

```cpp
struct Matrix3D
{
private:
    float n[3][3];  // n[column][row] for column-major storage

public:
    Matrix3D() = default;

    Matrix3D(float n00, float n01, float n02,
             float n10, float n11, float n12,
             float n20, float n21, float n22)
    {
        n[0][0] = n00;  n[0][1] = n10;  n[0][2] = n20;
        n[1][0] = n01;  n[1][1] = n11;  n[1][2] = n21;
        n[2][0] = n02;  n[2][1] = n12;  n[2][2] = n22;
    }

    Matrix3D(const Vector3D& a, const Vector3D& b, const Vector3D& c)
    {
        n[0][0] = a.x;  n[0][1] = a.y;  n[0][2] = a.z;
        n[1][0] = b.x;  n[1][1] = b.y;  n[1][2] = b.z;
        n[2][0] = c.x;  n[2][1] = c.y;  n[2][2] = c.z;
    }

    // Access entry at (row i, column j) — familiar math ordering
    float& operator ()(int i, int j)
    {
        return (n[j][i]);
    }

    const float& operator ()(int i, int j) const
    {
        return (n[j][i]);
    }

    // Access column j as a Vector3D
    Vector3D& operator [](int j)
    {
        return (*reinterpret_cast<Vector3D *>(n[j]));
    }

    const Vector3D& operator [](int j) const
    {
        return (*reinterpret_cast<const Vector3D *>(n[j]));
    }
};
```

The `operator()` takes `(row, column)` so math notation works naturally: `M(1, 2)` is row 1, column 2. Internally it accesses `n[2][1]` because the array is `n[column][row]`.

The `operator[]` lets you extract entire columns as `Vector3D` references — `M[0]` is the first column vector.

---

## 1.4 Basic Matrix Operations

### 1.4.1 Addition, Subtraction, and Scalar Multiplication

These operations are performed **entrywise** — exactly like vectors but in 2D.

**Addition**: `(A + B)_ij = A_ij + B_ij`

**Example**:

```
A = | 1  2 |    B = | 5  6 |    A + B = | 6   8 |
    | 3  4 |        | 7  8 |           | 10  12 |
```

**Subtraction**: `(A - B)_ij = A_ij - B_ij`

**Example**:

```
A - B = | 1-5   2-6 | = | -4  -4 |
        | 3-7   4-8 |   | -4  -4 |
```

**Scalar Multiplication**: `(tM)_ij = t · M_ij`

**Example**: *t* = 3

```
3A = 3 · | 1  2 | = | 3   6 |
         | 3  4 |   | 9  12 |
```

These operations follow the same properties as vector operations:

| Property | Description |
|----------|-------------|
| (A + B) + C = A + (B + C) | Associative |
| A + B = B + A | Commutative |
| (*st*)A = *s*(*t*A) | Associative (scalar) |
| *t*A = A*t* | Commutative (scalar) |
| *t*(A + B) = *t*A + *t*B | Distributive |
| (*s* + *t*)A = *s*A + *t*A | Distributive |

---

### 1.4.2 Matrix Multiplication

Matrix multiplication is the single most important algebraic operation in game engines. It's used to transform directions, points, and planes between coordinate systems.

#### When Can You Multiply?

Two matrices can be multiplied **only if** the number of **columns** of the first equals the number of **rows** of the second:

```
A is (n × p)  ×  B is (p × m)  →  Result is (n × m)
```

The shared dimension *p* must match.

#### The Formula

```
(AB)_ij = Σ(k=0 to p-1) A_ik · B_kj
```

In words: the (i, j) entry of the product is the **dot product** of row *i* of A with column *j* of B.

#### Worked Example — 2×3 times 3×2

```
A = | 1   3  -2 |      B = |  4  -2 |
    | 0   2   1 |          |  1   5 |
                            |  3   4 |

AB is 2×2.
```

**(0,0) entry**: row 0 of A · column 0 of B

```
= 1·4 + 3·1 + (-2)·3 = 4 + 3 - 6 = 1
```

**(0,1) entry**: row 0 of A · column 1 of B

```
= 1·(-2) + 3·5 + (-2)·4 = -2 + 15 - 8 = 5
```

**(1,0) entry**: row 1 of A · column 0 of B

```
= 0·4 + 2·1 + 1·3 = 0 + 2 + 3 = 5
```

**(1,1) entry**: row 1 of A · column 1 of B

```
= 0·(-2) + 2·5 + 1·4 = 0 + 10 + 4 = 14
```

**Result**:

```
AB = | 1   5 |
     | 5  14 |
```

#### Worked Example — 3×3 times 3×3

```
A = | 1  0  2 |      B = | 3  1  0 |
    | 0  1  0 |          | 2  0  1 |
    | 1  0  1 |          | 0  2  1 |

(AB)(0,0) = 1·3 + 0·2 + 2·0 = 3
(AB)(0,1) = 1·1 + 0·0 + 2·2 = 5
(AB)(0,2) = 1·0 + 0·1 + 2·1 = 2
(AB)(1,0) = 0·3 + 1·2 + 0·0 = 2
(AB)(1,1) = 0·1 + 1·0 + 0·2 = 0
(AB)(1,2) = 0·0 + 1·1 + 0·1 = 1
(AB)(2,0) = 1·3 + 0·2 + 1·0 = 3
(AB)(2,1) = 1·1 + 0·0 + 1·2 = 3
(AB)(2,2) = 1·0 + 0·1 + 1·1 = 1

AB = | 3  5  2 |
     | 2  0  1 |
     | 3  3  1 |
```

#### Matrix × Column Vector — The Key Operation

Multiplying a 3×3 matrix by a 3×1 column vector produces a new 3×1 column vector:

```
         | M00  M01  M02 |   | vx |     | M00·vx + M01·vy + M02·vz |
M · v =  | M10  M11  M12 | × | vy |  =  | M10·vx + M11·vy + M12·vz |
         | M20  M21  M22 |   | vz |     | M20·vx + M21·vy + M22·vz |
```

**Critical insight**: If the columns of M are vectors **a**, **b**, **c**, then:

```
M · v = vx·a + vy·b + vz·c
```

The result is a **linear combination** of the columns of M, with the components of **v** as coefficients.

**Example**: M = [**a** **b** **c**] where **a** = (1, 0, 0), **b** = (0, 0, 1), **c** = (0, -1, 0), and **v** = (3, 2, 5).

```
Mv = 3·(1, 0, 0) + 2·(0, 0, 1) + 5·(0, -1, 0)
   = (3, 0, 0) + (0, 0, 2) + (0, -5, 0)
   = (3, -5, 2)

Verification using matrix form:
M = | 1   0   0 |       | 1·3 + 0·2 + 0·5 |   | 3 |
    | 0   0  -1 |   v = | 0·3 + 0·2 -1·5  | = |-5 |
    | 0   1   0 |       | 0·3 + 1·2 + 0·5 |   | 2 |     ✓
```

This means the columns of M tell you where the x, y, and z axes end up after transformation — the foundation of coordinate transforms in Chapter 2.

#### Properties of Matrix Multiplication

| Property | Description |
|----------|-------------|
| (AB)C = A(BC) | Associative |
| A(B + C) = AB + AC | Left distributive |
| (A + B)C = AC + BC | Right distributive |
| (*t*A)B = A(*t*B) = *t*(AB) | Scalar factorization |
| (AB)ᵀ = BᵀAᵀ | **Transpose reverses order** |

**Matrix multiplication is NOT commutative**: AB ≠ BA in general.

**Example — Not commutative**:

```
A = | 1  2 |    B = | 0  1 |
    | 3  4 |        | 1  0 |

AB = | 1·0+2·1  1·1+2·0 | = | 2  1 |
     | 3·0+4·1  3·1+4·0 |   | 4  3 |

BA = | 0·1+1·3  0·2+1·4 | = | 3  4 |
     | 1·1+0·3  1·2+0·4 |   | 1  2 |

AB ≠ BA  ✓
```

**Proof that (AB)ᵀ = BᵀAᵀ**:

The (i,j) entry of (AB)ᵀ is the (j,i) entry of AB:

```
(AB)ᵀ_ij = (AB)_ji = Σ A_jk · B_ki
```

Since A_jk = (Aᵀ)_kj and B_ki = (Bᵀ)_ik, we can reverse the product order (scalar multiplication is commutative):

```
= Σ (Bᵀ)_ik · (Aᵀ)_kj = (BᵀAᵀ)_ij
```

#### C++ Implementation

```cpp
Matrix3D operator *(const Matrix3D& A, const Matrix3D& B)
{
    return (Matrix3D(
        A(0,0) * B(0,0) + A(0,1) * B(1,0) + A(0,2) * B(2,0),
        A(0,0) * B(0,1) + A(0,1) * B(1,1) + A(0,2) * B(2,1),
        A(0,0) * B(0,2) + A(0,1) * B(1,2) + A(0,2) * B(2,2),
        A(1,0) * B(0,0) + A(1,1) * B(1,0) + A(1,2) * B(2,0),
        A(1,0) * B(0,1) + A(1,1) * B(1,1) + A(1,2) * B(2,1),
        A(1,0) * B(0,2) + A(1,1) * B(1,2) + A(1,2) * B(2,2),
        A(2,0) * B(0,0) + A(2,1) * B(1,0) + A(2,2) * B(2,0),
        A(2,0) * B(0,1) + A(2,1) * B(1,1) + A(2,2) * B(2,1),
        A(2,0) * B(0,2) + A(2,1) * B(1,2) + A(2,2) * B(2,2)));
}

Vector3D operator *(const Matrix3D& M, const Vector3D& v)
{
    return (Vector3D(
        M(0,0) * v.x + M(0,1) * v.y + M(0,2) * v.z,
        M(1,0) * v.x + M(1,1) * v.y + M(1,2) * v.z,
        M(2,0) * v.x + M(2,1) * v.y + M(2,2) * v.z));
}
```

---

## 1.5 Vector Multiplication

### 1.5.1 Dot Product

The **dot product** (also called the **scalar product**) of two vectors produces a scalar:

```
a · b = ax·bx + ay·by + az·bz
```

**Example 1**: **a** = (2, 3, 1), **b** = (4, -1, 2).

```
a · b = 2·4 + 3·(-1) + 1·2
      = 8 - 3 + 2
      = 7
```

**Example 2**: **a** = (1, 0, 0), **b** = (0, 1, 0).

```
a · b = 1·0 + 0·1 + 0·0 = 0
```

(The x and y unit vectors are perpendicular, so their dot product is zero.)

**Example 3**: **a** = (3, -2, 5), **b** = (1, 4, 2).

```
a · b = 3·1 + (-2)·4 + 5·2
      = 3 - 8 + 10
      = 5
```

#### Dot Product as Squared Magnitude

The dot product of a vector with itself is its squared magnitude:

```
v · v = vx² + vy² + vz² = ‖v‖²
```

**Example**: **v** = (3, 4, 0).

```
v · v = 9 + 16 + 0 = 25 = ‖v‖² = 5²   ✓
```

#### The Geometric Formula

```
a · b = ‖a‖ · ‖b‖ · cos θ
```

where θ is the angle between **a** and **b**.

**Example**: Find the angle between **a** = (1, 0, 0) and **b** = (1, 1, 0).

```
a · b = 1·1 + 0·1 + 0·0 = 1
‖a‖ = 1
‖b‖ = √(1 + 1 + 0) = √(2)

cos θ = (a · b) / (‖a‖ · ‖b‖) = 1 / √(2) ≈ 0.707

θ = arccos(0.707) = 45°
```

**Example**: Find the angle between **a** = (1, 2, 3) and **b** = (4, -5, 6).

```
a · b = 1·4 + 2·(-5) + 3·6 = 4 - 10 + 18 = 12
‖a‖ = √(1 + 4 + 9) = √(14) ≈ 3.742
‖b‖ = √(16 + 25 + 36) = √(77) ≈ 8.775

cos θ = 12 / (3.742 · 8.775) = 12 / 32.838 ≈ 0.3654

θ = arccos(0.3654) ≈ 68.6°
```

#### Properties

| Property | Description |
|----------|-------------|
| **a** · **b** = **b** · **a** | Commutative |
| **a** · (**b** + **c**) = **a** · **b** + **a** · **c** | Distributive |
| (*t***a**) · **b** = **a** · (*t***b**) = *t*(**a** · **b**) | Scalar factorization |

#### Orthogonality

When **a** · **b** = 0, the vectors are **orthogonal** (perpendicular, θ = 90°).

**Geometric interpretation of the dot product sign**:

| Condition | Angle θ | Dot product sign |
|-----------|---------|-----------------|
| **a** and **b** point in the same direction | θ = 0° | Maximum positive |
| **a** and **b** at an acute angle | 0° < θ < 90° | Positive |
| **a** and **b** perpendicular | θ = 90° | Zero |
| **a** and **b** at an obtuse angle | 90° < θ < 180° | Negative |
| **a** and **b** point in opposite directions | θ = 180° | Maximum negative |

**Example — Checking orthogonality**: Are (1, 1, 0) and (-1, 1, 0) perpendicular?

```
(1, 1, 0) · (-1, 1, 0) = 1·(-1) + 1·1 + 0·0 = -1 + 1 = 0   ✓ Perpendicular!
```

#### Proof of the Cosine Formula Using the Law of Cosines

Consider a triangle with sides **a** and **b** starting from the same point. The third side is **a** - **b**.

The law of cosines states:

```
‖a - b‖² = ‖a‖² + ‖b‖² - 2·‖a‖·‖b‖·cos θ
```

Expand the left side using the dot product:

```
‖a - b‖² = (a - b) · (a - b)
           = a·a - a·b - b·a + b·b
           = ‖a‖² - 2(a·b) + ‖b‖²
```

The ‖a‖² and ‖b‖² terms cancel from both sides:

```
-2(a · b) = -2 ‖a‖ ‖b‖ cos θ

a · b = ‖a‖ ‖b‖ cos θ    ■
```

#### C++ Implementation

```cpp
inline float Dot(const Vector3D& a, const Vector3D& b)
{
    return (a.x * b.x + a.y * b.y + a.z * b.z);
}
```

---

### 1.5.2 Cross Product

The **cross product** (or **vector product**) of two 3D vectors produces a new 3D vector:

```
a × b = (ay·bz - az·by,  az·bx - ax·bz,  ax·by - ay·bx)
```

The cross product is defined **only for 3D vectors** (unlike the dot product which works in any dimension).

#### Memory Trick — Cyclic Pattern

Each component uses the **next two** letters in the cycle x → y → z → x → ...:

- **x component**: uses y, z → ay·bz - az·by
- **y component**: uses z, x → az·bx - ax·bz
- **z component**: uses x, y → ax·by - ay·bx

The positive term has subscripts in cyclic order; the negative term has them reversed.

**Example 1**: **a** = (2, 3, 4), **b** = (5, 6, 7).

```
x: ay·bz - az·by = 3·7 - 4·6 = 21 - 24 = -3
y: az·bx - ax·bz = 4·5 - 2·7 = 20 - 14 =  6
z: ax·by - ay·bx = 2·6 - 3·5 = 12 - 15 = -3

a × b = (-3, 6, -3)
```

**Verification that the result is perpendicular to both inputs**:

```
a · (a × b) = 2·(-3) + 3·6 + 4·(-3) = -6 + 18 - 12 = 0   ✓
b · (a × b) = 5·(-3) + 6·6 + 7·(-3) = -15 + 36 - 21 = 0  ✓
```

**Example 2**: **a** = (1, 0, 0), **b** = (0, 1, 0).

```
x: 0·0 - 0·1 = 0
y: 0·0 - 1·0 = 0
z: 1·1 - 0·0 = 1

a × b = (0, 0, 1)
```

The cross product of the x-axis and y-axis unit vectors gives the z-axis unit vector — exactly the right-hand rule.

**Example 3**: **a** = (1, 3, -2), **b** = (2, -1, 4).

```
x: 3·4 - (-2)·(-1) = 12 - 2 = 10
y: (-2)·2 - 1·4     = -4 - 4 = -8
z: 1·(-1) - 3·2     = -1 - 6 = -7

a × b = (10, -8, -7)
```

Verify perpendicularity:

```
a · (a × b) = 1·10 + 3·(-8) + (-2)·(-7) = 10 - 24 + 14 = 0  ✓
b · (a × b) = 2·10 + (-1)·(-8) + 4·(-7) = 20 + 8 - 28 = 0   ✓
```

#### Anticommutativity

```
a × b = -(b × a)
```

Reversing the order **negates** the result.

**Example**: From Example 1, a × b = (-3, 6, -3). Now compute b × a:

```
b × a with b = (5, 6, 7), a = (2, 3, 4):

x: 6·4 - 7·3 = 24 - 21 =  3
y: 7·2 - 5·4 = 14 - 20 = -6
z: 5·3 - 6·2 = 15 - 12 =  3

b × a = (3, -6, 3) = -(−3, 6, −3) = −(a × b)  ✓
```

#### Parallel Vectors Have Zero Cross Product

If **a** and **b** are parallel (one is a scalar multiple of the other), then **a** × **b** = **0**.

**Example**: **a** = (1, 2, 3), **b** = 2**a** = (2, 4, 6).

```
x: 2·6 - 3·4 = 12 - 12 = 0
y: 3·2 - 1·6 = 6 - 6   = 0
z: 1·4 - 2·2 = 4 - 4   = 0

a × b = (0, 0, 0)  ✓
```

#### Magnitude of the Cross Product

```
‖a × b‖ = ‖a‖ · ‖b‖ · sin θ
```

This equals the **area of the parallelogram** formed by **a** and **b**.

**Example**: **a** = (1, 0, 0), **b** = (0, 2, 0).

```
a × b = (0·0 - 0·2, 0·0 - 1·0, 1·2 - 0·0) = (0, 0, 2)

‖a × b‖ = 2
‖a‖ · ‖b‖ · sin(90°) = 1 · 2 · 1 = 2   ✓

Area of parallelogram = 2 (a 1×2 rectangle)
```

#### Area of a Triangle

The area of a triangle with sides **a** and **b** is half the parallelogram area:

```
Area = (1/2) · ‖a × b‖
```

**Example**: Find the area of the triangle with vertices P₀ = (1, 0, 0), P₁ = (0, 1, 0), P₂ = (0, 0, 1).

```
a = P₁ - P₀ = (-1, 1, 0)
b = P₂ - P₀ = (-1, 0, 1)

a × b:
  x: 1·1 - 0·0 = 1
  y: 0·(-1) - (-1)·1 = 0 + 1 = 1
  z: (-1)·0 - 1·(-1) = 0 + 1 = 1

a × b = (1, 1, 1)
‖a × b‖ = √(1 + 1 + 1) = √(3) ≈ 1.732

Area = √(3) / 2 ≈ 0.866
```

#### The Right-Hand Rule

The cross product **a** × **b** points in the direction determined by curling the fingers of the right hand from **a** toward **b** (through the smaller angle). The thumb points in the direction of **a** × **b**.

Reversing the order (b × a) reverses the direction — hence anticommutativity.

#### Antisymmetric Matrix Formulation

The cross product can be written as a matrix-vector product using the **antisymmetric matrix** `[a]×`:

```
          |  0   -az   ay |
[a]×  =   |  az   0   -ax |
          | -ay   ax   0  |
```

So:

```
a × b = [a]× · b
```

**Example**: **a** = (1, 3, -2), **b** = (2, -1, 4).

```
[a]× = |  0   2   3 |
       | -2   0  -1 |
       | -3   1   0 |

[a]× · b = | 0·2 + 2·(-1) + 3·4    | = | 0 - 2 + 12 | = | 10 |
           | -2·2 + 0·(-1) + (-1)·4 |   | -4 + 0 - 4 |   | -8 |
           | -3·2 + 1·(-1) + 0·4    |   | -6 - 1 + 0 |   | -7 |

This matches a × b = (10, -8, -7) from Example 3 above.  ✓
```

#### Cross Product Properties

| Property | Description |
|----------|-------------|
| **a** × **b** = -(**b** × **a**) | Anticommutativity |
| **a** × (**b** + **c**) = **a** × **b** + **a** × **c** | Distributive |
| (*t***a**) × **b** = **a** × (*t***b**) = *t*(**a** × **b**) | Scalar factorization |
| **a** × (**b** × **c**) = **b**(**a** · **c**) - **c**(**a** · **b**) | Vector triple product (BAC-CAB) |
| (**a** × **b**)² = a²b² - (**a** · **b**)² | Lagrange's identity |

#### Vector Triple Product — BAC-CAB Rule

```
a × (b × c) = b·(a · c) - c·(a · b)
```

The mnemonic "**BAC minus CAB**" helps remember the arrangement.

**Example**: **a** = (1, 0, 0), **b** = (0, 1, 0), **c** = (0, 0, 1).

First compute directly:

```
b × c = (1·1 - 0·0, 0·0 - 0·1, 0·0 - 1·0) = (1, 0, 0)

a × (b × c) = a × (1, 0, 0)
            = (0·0 - 0·0, 0·1 - 1·0, 1·0 - 0·1)
            = (0, 0, 0)
```

Now via BAC-CAB:

```
a · c = 1·0 + 0·0 + 0·1 = 0
a · b = 1·0 + 0·1 + 0·0 = 0

b(a · c) - c(a · b) = (0,1,0)·0 - (0,0,1)·0 = (0,0,0)  ✓
```

**Example**: **a** = (2, 1, 0), **b** = (1, 0, 1), **c** = (0, 1, -1).

Direct calculation:

```
b × c:
  x: 0·(-1) - 1·1 = -1
  y: 1·0 - 1·(-1) = 1
  z: 1·1 - 0·0    = 1
  b × c = (-1, 1, 1)

a × (b × c) = a × (-1, 1, 1):
  x: 1·1 - 0·1    = 1
  y: 0·(-1) - 2·1 = -2
  z: 2·1 - 1·(-1) = 3
  = (1, -2, 3)
```

BAC-CAB:

```
a · c = 2·0 + 1·1 + 0·(-1) = 1
a · b = 2·1 + 1·0 + 0·1    = 2

b(a·c) - c(a·b) = (1,0,1)·1 - (0,1,-1)·2
                = (1, 0, 1) - (0, 2, -2)
                = (1, -2, 3)   ✓
```

Note: If the parentheses move to the other side:

```
(a × b) × c = b·(a · c) - a·(c · b)
```

This is NOT the same as **a** × (**b** × **c**) in general — the cross product is **not associative**.

#### Lagrange's Identity

```
‖a × b‖² = ‖a‖² · ‖b‖² - (a · b)²
```

**Example**: **a** = (1, 2, 0), **b** = (3, 0, 0).

```
a × b = (2·0 - 0·0, 0·3 - 1·0, 1·0 - 2·3) = (0, 0, -6)
‖a × b‖² = 36

‖a‖² = 1 + 4 + 0 = 5
‖b‖² = 9 + 0 + 0 = 9
(a · b)² = (3 + 0 + 0)² = 9

‖a‖²·‖b‖² - (a·b)² = 45 - 9 = 36   ✓
```

#### C++ Implementation

```cpp
inline Vector3D Cross(const Vector3D& a, const Vector3D& b)
{
    return (Vector3D(a.y * b.z - a.z * b.y,
                     a.z * b.x - a.x * b.z,
                     a.x * b.y - a.y * b.x));
}
```

---

### 1.5.3 Scalar Triple Product

The **scalar triple product** of three vectors **a**, **b**, **c** is:

```
[a, b, c] = (a × b) · c
```

It doesn't matter where the dot and cross go, as long as the cyclic order is preserved:

```
[a, b, c] = (a × b) · c = (b × c) · a = (c × a) · b
```

Reversing the order negates the result:

```
[c, b, a] = -[a, b, c]
```

#### Volume of a Parallelepiped

The scalar triple product gives the (signed) **volume** of the parallelepiped spanned by **a**, **b**, **c**.

**Example**: **a** = (1, 0, 0), **b** = (0, 1, 0), **c** = (0, 0, 1).

```
a × b = (0, 0, 1)
(a × b) · c = 0·0 + 0·0 + 1·1 = 1

Volume = |1| = 1  (unit cube)
```

**Example**: **a** = (2, 0, 0), **b** = (0, 3, 0), **c** = (0, 0, 5).

```
a × b = (0·0 - 0·3, 0·0 - 2·0, 2·3 - 0·0) = (0, 0, 6)
(a × b) · c = 0·0 + 0·0 + 6·5 = 30

Volume = 30  (a 2×3×5 box)
```

**Example**: **a** = (1, 3, -2), **b** = (2, -1, 4), **c** = (0, 1, 1).

```
a × b = (3·4 - (-2)·(-1), (-2)·2 - 1·4, 1·(-1) - 3·2)
      = (12 - 2, -4 - 4, -1 - 6)
      = (10, -8, -7)

(a × b) · c = 10·0 + (-8)·1 + (-7)·1 = 0 - 8 - 7 = -15

Volume = |-15| = 15
```

**Verify cyclic invariance**: Compute (b × c) · a:

```
b × c with b = (2, -1, 4), c = (0, 1, 1):
  x: (-1)·1 - 4·1 = -1 - 4 = -5
  y: 4·0 - 2·1     = 0 - 2 = -2
  z: 2·1 - (-1)·0  = 2 - 0 = 2

(b × c) · a = (-5)·1 + (-2)·3 + 2·(-2) = -5 - 6 - 4 = -15   ✓ Same result
```

---

## 1.6 Vector Projection

### Projection

The **projection** of **a** onto **b** is the component of **a** that is parallel to **b**:

```
a_∥b = ((a · b) / (b · b)) · b  =  ((a · b) / ‖b‖²) · b
```

The division by b² handles the case where **b** is not a unit vector. If **b** is a unit vector, this simplifies to `(a · b) · b`.

**Example 1**: Project **a** = (3, 4, 0) onto **b** = (1, 0, 0).

```
a · b = 3·1 + 4·0 + 0·0 = 3
b · b = 1

a_∥b = (3/1) · (1, 0, 0) = (3, 0, 0)
```

This makes geometric sense — projecting onto the x-axis extracts the x component.

**Example 2**: Project **a** = (3, 4, 0) onto **b** = (1, 1, 0).

```
a · b = 3·1 + 4·1 + 0·0 = 7
b · b = 1 + 1 + 0 = 2

a_∥b = (7/2) · (1, 1, 0) = (3.5, 3.5, 0)
```

**Example 3**: Project **a** = (2, 3, 1) onto **b** = (1, -1, 2).

```
a · b = 2·1 + 3·(-1) + 1·2 = 2 - 3 + 2 = 1
b · b = 1 + 1 + 4 = 6

a_∥b = (1/6) · (1, -1, 2) = (1/6, -1/6, 2/6) ≈ (0.167, -0.167, 0.333)
```

### Rejection

The **rejection** of **a** from **b** is the component of **a** perpendicular to **b**:

```
a_⊥b = a - a_∥b
```

Always: `a_∥b + a_⊥b = a`.

**Example**: Using Example 2 above, **a** = (3, 4, 0), a_∥b = (3.5, 3.5, 0).

```
a_⊥b = a - a_∥b = (3 - 3.5, 4 - 3.5, 0 - 0) = (-0.5, 0.5, 0)
```

Verify perpendicularity to **b** = (1, 1, 0):

```
a_⊥b · b = (-0.5)·1 + 0.5·1 + 0·0 = -0.5 + 0.5 = 0   ✓
```

Verify: a_∥b + a_⊥b = (3.5, 3.5, 0) + (-0.5, 0.5, 0) = (3, 4, 0) = **a** ✓

The projection and rejection form a right triangle with **a** as the hypotenuse:

```
‖a_∥b‖ = ‖a‖ cos θ
‖a_⊥b‖ = ‖a‖ sin θ
```

### Outer Product

The **outer product** `u ⊗ v` of two vectors produces a matrix where entry (i, j) = u_i · v_j:

```
          | ux·vx  ux·vy  ux·vz |
u ⊗ v  =  | uy·vx  uy·vy  uy·vz |
          | uz·vx  uz·vy  uz·vz |
```

The projection formula can be written as a matrix product using the outer product:

```
a_∥b = (1 / b²) · (b ⊗ b) · a
```

**Example**: Outer product of **u** = (1, 2, 3) and **v** = (4, 5, 6).

```
u ⊗ v = | 1·4  1·5  1·6 |   | 4   5   6 |
        | 2·4  2·5  2·6 | = | 8  10  12 |
        | 3·4  3·5  3·6 |   |12  15  18 |
```

### Gram-Schmidt Orthogonalization

Given a set of linearly independent vectors, the **Gram-Schmidt process** makes them mutually orthogonal by iteratively subtracting projections.

For three vectors {**v₁**, **v₂**, **v₃**}:

```
u₁ = v₁
u₂ = v₂ - proj(v₂ onto u₁)
u₃ = v₃ - proj(v₃ onto u₁) - proj(v₃ onto u₂)
```

Where proj(a onto b) = (a·b / b·b) · b.

**Example**: Orthogonalize **v₁** = (1, 1, 0), **v₂** = (1, 0, 1), **v₃** = (0, 1, 1).

**Step 1**: u₁ = v₁ = (1, 1, 0)

**Step 2**: Subtract projection of v₂ onto u₁:

```
v₂ · u₁ = 1·1 + 0·1 + 1·0 = 1
u₁ · u₁ = 1 + 1 + 0 = 2

proj = (1/2) · (1, 1, 0) = (0.5, 0.5, 0)

u₂ = v₂ - proj = (1 - 0.5, 0 - 0.5, 1 - 0) = (0.5, -0.5, 1)
```

Verify: u₁ · u₂ = 1·0.5 + 1·(-0.5) + 0·1 = 0 ✓

**Step 3**: Subtract projections of v₃ onto u₁ and u₂:

```
v₃ · u₁ = 0·1 + 1·1 + 1·0 = 1
u₁ · u₁ = 2

proj₁ = (1/2) · (1, 1, 0) = (0.5, 0.5, 0)

v₃ · u₂ = 0·0.5 + 1·(-0.5) + 1·1 = 0.5
u₂ · u₂ = 0.25 + 0.25 + 1 = 1.5

proj₂ = (0.5/1.5) · (0.5, -0.5, 1) = (1/3)(0.5, -0.5, 1) = (1/6, -1/6, 1/3)

u₃ = v₃ - proj₁ - proj₂
   = (0, 1, 1) - (0.5, 0.5, 0) - (1/6, -1/6, 1/3)
   = (0 - 0.5 - 1/6, 1 - 0.5 + 1/6, 1 - 0 - 1/3)
   = (-2/3, 2/3, 2/3)
```

Verify all pairs are orthogonal:

```
u₁ · u₃ = 1·(-2/3) + 1·(2/3) + 0·(2/3) = 0    ✓
u₂ · u₃ = 0.5·(-2/3) + (-0.5)·(2/3) + 1·(2/3)
        = -1/3 - 1/3 + 2/3 = 0                    ✓
```

To produce **orthonormal** vectors (all unit length), normalize each u_i after the process.

### Linear Independence

A set of vectors {**v₁**, **v₂**, ..., **vₙ**} is **linearly independent** if no vector can be written as a combination of the others. Formally, there exist no scalars (not all zero) such that:

```
a₁·v₁ + a₂·v₂ + ... + aₙ·vₙ = 0
```

In *n* dimensions, at most *n* vectors can be linearly independent.

**Example**: Are (1, 0, 0), (0, 1, 0), (0, 0, 1) linearly independent?

```
a₁(1,0,0) + a₂(0,1,0) + a₃(0,0,1) = (0,0,0)
→ (a₁, a₂, a₃) = (0, 0, 0)

The only solution is all zeros → linearly independent.  ✓
```

**Example**: Are (1, 2, 3), (2, 4, 6) linearly independent?

```
(2, 4, 6) = 2 · (1, 2, 3)

The second is a scalar multiple of the first → NOT independent.
```

#### C++ Implementation

```cpp
inline Vector3D Project(const Vector3D& a, const Vector3D& b)
{
    return (b * (Dot(a, b) / Dot(b, b)));
}

inline Vector3D Reject(const Vector3D& a, const Vector3D& b)
{
    return (a - b * (Dot(a, b) / Dot(b, b)));
}
```

---

## 1.7 Matrix Inversion

A matrix **M** describes a transformation. Its **inverse** M⁻¹ undoes that transformation:

```
M · M⁻¹ = M⁻¹ · M = I
```

A matrix has an inverse if and only if its determinant is not zero.

---

### 1.7.1 Identity Matrices

The **identity matrix** `Iₙ` is the *n* × *n* matrix with 1s on the diagonal and 0s elsewhere:

```
      | 1  0  0 |
I₃ =  | 0  1  0 |
      | 0  0  1 |
```

Properties:
- AI = A for any matrix A with the right number of rows
- IB = B for any matrix B with the right number of columns
- Multiplying by I is like multiplying a number by 1

**Example**: Verify that IA = A for A = [[2, 3], [4, 5], [6, 7]]:

```
I₃ · A = | 1 0 0 | | 2 3 |   | 1·2+0·4+0·6  1·3+0·5+0·7 |   | 2 3 |
         | 0 1 0 | | 4 5 | = | 0·2+1·4+0·6  0·3+1·5+0·7 | = | 4 5 | = A  ✓
         | 0 0 1 | | 6 7 |   | 0·2+0·4+1·6  0·3+0·5+1·7 |   | 6 7 |
```

The **inverse** of *M* is the matrix M⁻¹ such that MM⁻¹ = M⁻¹M = I. Not every matrix has an inverse — a matrix without one is called **singular**.

---

### 1.7.2 Determinants

The **determinant** of a square matrix is a scalar value that acts as a sort of "magnitude" for the matrix. A matrix is invertible if and only if its determinant is nonzero.

#### 2×2 Determinant

```
det | a  b | = ad - bc
    | c  d |
```

**Example**:

```
det | 3  2 | = 3·5 - 2·4 = 15 - 8 = 7
    | 4  5 |
```

**Example**:

```
det | 1  2 | = 1·4 - 2·3 = 4 - 6 = -2
    | 3  4 |
```

**Example** (singular):

```
det | 2  4 | = 2·6 - 4·3 = 12 - 12 = 0   (no inverse exists)
    | 3  6 |
```

#### 3×3 Determinant

For a 3×3 matrix, the determinant is:

```
det(M) = M00·(M11·M22 - M12·M21)
       - M01·(M10·M22 - M12·M20)
       + M02·(M10·M21 - M11·M20)
```

**Example**:

```
M = | 1  2  3 |
    | 0  1  4 |
    | 5  6  0 |

det(M) = 1·(1·0 - 4·6) - 2·(0·0 - 4·5) + 3·(0·6 - 1·5)
       = 1·(0 - 24) - 2·(0 - 20) + 3·(0 - 5)
       = 1·(-24) - 2·(-20) + 3·(-5)
       = -24 + 40 - 15
       = 1
```

**Example**:

```
M = | 2  -1   0 |
    | 3   1   4 |
    |-2   5   3 |

det(M) = 2·(1·3 - 4·5) - (-1)·(3·3 - 4·(-2)) + 0·(3·5 - 1·(-2))
       = 2·(3 - 20) + 1·(9 + 8) + 0
       = 2·(-17) + 17
       = -34 + 17
       = -17
```

#### The Leibniz Formula

The determinant sums over all *n*! permutations of {0, 1, ..., n-1}. Each permutation contributes a product of one entry from each row, with a sign based on whether the permutation is even or odd.

For 3×3, the 6 permutations of {0,1,2} are:

| Permutation | Even/Odd | Sign | Term |
|-------------|----------|------|------|
| {0,1,2} | Even (0 swaps) | + | M₀₀M₁₁M₂₂ |
| {1,2,0} | Even (2 swaps) | + | M₀₁M₁₂M₂₀ |
| {2,0,1} | Even (2 swaps) | + | M₀₂M₁₀M₂₁ |
| {0,2,1} | Odd (1 swap) | - | M₀₀M₁₂M₂₁ |
| {1,0,2} | Odd (1 swap) | - | M₀₁M₁₀M₂₂ |
| {2,1,0} | Odd (1 swap) | - | M₀₂M₁₁M₂₀ |

#### Expansion by Minors

A **minor** of entry (i,j) is the determinant of the submatrix obtained by deleting row *i* and column *j*.

To expand by minors along row *k*:

```
det(M) = Σ(j=0 to n-1) M_kj · (-1)^(k+j) · |M̄_kj|
```

The sign alternates in a checkerboard pattern:

```
| +  -  +  - |
| -  +  -  + |
| +  -  +  - |
| -  +  -  + |
```

**Example** — 3×3 expanded along row 0:

```
M = | 1  2  3 |
    | 0  1  4 |
    | 5  6  0 |

det(M) = (+1)·1·det|1 4| + (-1)·2·det|0 4| + (+1)·3·det|0 1|
                    |6 0|             |5 0|             |5 6|

       = 1·(0 - 24) - 2·(0 - 20) + 3·(0 - 5)
       = -24 + 40 - 15 = 1  ✓
```

You can also expand by minors along any column (since det(Mᵀ) = det(M)).

#### Properties of the Determinant

| Property | Description |
|----------|-------------|
| det(I) = 1 | Identity matrix |
| det(Mᵀ) = det(M) | Transpose doesn't change determinant |
| det(M⁻¹) = 1/det(M) | Inverse |
| det(AB) = det(A) · det(B) | Product rule |
| det(tA) = tⁿ · det(A) | Scalar factorization (n×n matrix) |

**Example — det(tA) = tⁿ det(A)**:

```
A = | 1  2 |,  t = 3
    | 3  4 |

det(A) = 4 - 6 = -2

3A = | 3   6 |
     | 9  12 |

det(3A) = 36 - 54 = -18

t² · det(A) = 9 · (-2) = -18  ✓  (n = 2)
```

#### C++ Implementation

```cpp
float Determinant(const Matrix3D& M)
{
    return (M(0,0) * (M(1,1) * M(2,2) - M(1,2) * M(2,1))
          + M(0,1) * (M(1,2) * M(2,0) - M(1,0) * M(2,2))
          + M(0,2) * (M(1,0) * M(2,1) - M(1,1) * M(2,0)));
}
```

---

### 1.7.3 Elementary Matrices

The inverse is found by applying elementary row operations to M until it becomes the identity. There are three types:

**(a) Scale a row** — Multiply row *r* by nonzero scalar *t*:

The elementary matrix is the identity with `I_rr` replaced by *t*.

```
E_scale = | 1  0  0 |   (scaling row 1 by t)
          | 0  t  0 |
          | 0  0  1 |
```

**Effect on determinant**: multiplied by *t*.

**(b) Swap two rows** — Exchange rows *r* and *s*:

The elementary matrix is the identity with rows *r* and *s* swapped.

```
E_swap = | 0  1  0 |   (swapping rows 0 and 1)
         | 1  0  0 |
         | 0  0  1 |
```

**Effect on determinant**: negated.

**(c) Add a multiple of one row to another** — Add *t* × row *s* to row *r*:

The elementary matrix is the identity with *t* placed at position (*r*, *s*).

```
E_add = | 1  0  0 |   (adding t × row 1 to row 0)
        | 0  1  0 |
        | 0  0  1 |
```

But with `t` at position (0, 1):

```
E_add = | 1  t  0 |
        | 0  1  0 |
        | 0  0  1 |
```

**Effect on determinant**: unchanged.

**Example**: Apply E_scale to scale row 1 of M by 3:

```
M = | 2  1 |    E = | 1  0 |
    | 4  5 |        | 0  3 |

EM = | 1·2+0·4  1·1+0·5 | = | 2   1 |
     | 0·2+3·4  0·1+3·5 |   | 12  15 |
```

Row 1 was multiplied by 3. det(M) = 10 - 4 = 6, det(EM) = 30 - 12 = 18 = 3 · 6. ✓

---

### 1.7.4 Inverse Calculation — Gauss-Jordan Elimination

To find M⁻¹, apply elementary row operations to transform M into the identity. Apply the same operations to I; the result is M⁻¹.

#### Step-by-Step Procedure

For each column *k* (from 0 to n-1):

**A.** Find the row *r* (with r ≥ k) where |M'_rk| is largest. Let p = M'_rk. If p = 0, the matrix is singular.

**B.** If r ≠ k, swap rows r and k (**pivoting**).

**C.** Multiply row k by 1/p (makes the diagonal entry 1).

**D.** For every other row *i* (i ≠ k), add -M'_ik × row k to row i (clears the column above and below).

**Worked Example**: Find the inverse of:

```
M = | 2  1 |
    | 5  3 |
```

Set up [M | I]:

```
| 2  1 | 1  0 |
| 5  3 | 0  1 |
```

**Column 0**: Pivot is in row 1 (|5| > |2|). Swap rows:

```
| 5  3 | 0  1 |
| 2  1 | 1  0 |
```

Multiply row 0 by 1/5:

```
| 1  3/5 | 0    1/5 |
| 2  1   | 1    0   |
```

Subtract 2 × row 0 from row 1:

```
| 1   3/5  |  0     1/5  |
| 0  -1/5  |  1    -2/5  |
```

**Column 1**: Pivot is row 1. Multiply row 1 by -5:

```
| 1   3/5  |  0     1/5  |
| 0   1    | -5     2    |
```

Subtract (3/5) × row 1 from row 0:

```
| 1   0  |  0 - (-3)     1/5 - 6/5  | = | 1  0 |  3   -1  |
| 0   1  | -5            2          |   | 0  1 | -5    2  |
```

So: M⁻¹ = [[3, -1], [-5, 2]].

**Verification**:

```
M · M⁻¹ = | 2  1 | | 3  -1 | = | 6-5    -2+2 | = | 1  0 |  ✓
           | 5  3 | |-5   2 |   | 15-15  -5+6 |   | 0  1 |
```

---

### 1.7.5 Inverses of Small Matrices

For small matrices in game engines, there are direct formulas that are faster than Gauss-Jordan elimination.

#### 2×2 Inverse

```
                         1       |  d  -b |
inv( | a  b | )  =  ---------  · |        |
    ( | c  d | )    (ad - bc)    | -c   a |
```

Swap the diagonal entries, negate the off-diagonal entries, divide by the determinant.

**Example**:

```
A = | 4  7 |
    | 2  6 |

det(A) = 4·6 - 7·2 = 24 - 14 = 10

A⁻¹ = (1/10) | 6  -7 | = | 0.6  -0.7 |
              |-2   4 |   |-0.2   0.4 |

Verify: A · A⁻¹ = | 4·0.6 + 7·(-0.2)   4·(-0.7) + 7·0.4 |
                   | 2·0.6 + 6·(-0.2)   2·(-0.7) + 6·0.4 |

               = | 2.4 - 1.4   -2.8 + 2.8 | = | 1  0 |  ✓
                 | 1.2 - 1.2   -1.4 + 2.4 |   | 0  1 |
```

**Example**:

```
A = | 1  2 |
    | 3  4 |

det(A) = 4 - 6 = -2

A⁻¹ = (1/(-2)) |  4  -2 | = | -2     1   |
                | -3   1 |   |  1.5  -0.5 |

Verify: | 1  2 | | -2     1   | = | -2+3     1-1   | = | 1  0 |  ✓
        | 3  4 | |  1.5  -0.5 |   | -6+6     3-2   |   | 0  1 |
```

#### 3×3 Inverse Using Cross Products

For M = [**a** **b** **c**] (columns are 3D vectors **a**, **b**, **c**):

```
              1            | — (b × c)ᵀ — |
M⁻¹ = ————————————————  · | — (c × a)ᵀ — |
        [a, b, c]          | — (a × b)ᵀ — |
```

The rows of the inverse are the cross products of pairs of columns, divided by the scalar triple product.

**Example**: Find the inverse of:

```
M = | 1  0  1 |      columns: a = (1, 0, 1), b = (0, 1, 0), c = (1, 0, -1)
    | 0  1  0 |
    | 1  0 -1 |
```

**Step 1**: Compute cross products:

```
b × c:
  x: 1·(-1) - 0·0 = -1
  y: 0·1 - 0·(-1) = 0
  z: 0·0 - 1·1    = -1
  b × c = (-1, 0, -1)

c × a:
  x: 0·1 - (-1)·0 = 0
  y: (-1)·1 - 1·1  = -2
  z: 1·0 - 0·1    = 0
  c × a = (0, -2, 0)

a × b:
  x: 0·0 - 1·1 = -1
  y: 1·0 - 1·0 = 0
  z: 1·1 - 0·0 = 1
  a × b = (-1, 0, 1)
```

**Step 2**: Scalar triple product [a, b, c] = (a × b) · c:

```
[a, b, c] = (-1)·1 + 0·0 + 1·(-1) = -1 + 0 - 1 = -2
```

**Step 3**: Assemble the inverse:

```
M⁻¹ = (1/(-2)) | -1   0  -1 |   | 1/2    0    1/2 |
                |  0  -2   0 | = |  0     1     0  |
                | -1   0   1 |   | 1/2    0   -1/2 |
```

**Verification**:

```
M · M⁻¹ = | 1  0  1 | | 1/2   0   1/2 |
           | 0  1  0 | |  0    1    0  |
           | 1  0 -1 | | 1/2   0  -1/2 |

Row 0: (1·1/2+0+1·1/2, 0+0+0, 1·1/2+0+1·(-1/2)) = (1, 0, 0)  ✓
Row 1: (0+0+0, 0+1+0, 0+0+0) = (0, 1, 0)  ✓
Row 2: (1·1/2+0-1·1/2, 0+0+0, 1·1/2+0-1·(-1/2)) = (0, 0, 1)  ✓
```

#### Cofactor Matrix and Adjugate

More generally, the inverse uses the **cofactor matrix** C(M):

```
C_ij(M) = (-1)^(i+j) · det(M̄_ij)
```

The **adjugate** (adjoint) is the transpose of the cofactor matrix:

```
adj(M) = Cᵀ(M)
```

The inverse formula:

```
M⁻¹ = adj(M) / det(M) = Cᵀ(M) / det(M)
```

**Example**: Find the inverse of:

```
M = | 1  2  3 |
    | 0  1  4 |
    | 5  6  0 |
```

We already know det(M) = 1 (computed above).

**Cofactors** (using the checkerboard sign pattern):

```
C₀₀ = +det|1 4| = +(0 - 24) = -24
           |6 0|

C₀₁ = -det|0 4| = -(0 - 20) = 20
           |5 0|

C₀₂ = +det|0 1| = +(0 - 5) = -5
           |5 6|

C₁₀ = -det|2 3| = -(0 - 18) = 18
           |6 0|

C₁₁ = +det|1 3| = +(0 - 15) = -15
           |5 0|

C₁₂ = -det|1 2| = -(6 - 10) = 4
           |5 6|

C₂₀ = +det|2 3| = +(8 - 3) = 5
           |1 4|

C₂₁ = -det|1 3| = -(4 - 0) = -4
           |0 4|

C₂₂ = +det|1 2| = +(1 - 0) = 1
           |0 1|
```

Cofactor matrix:

```
C(M) = | -24   20  -5 |
       |  18  -15   4 |
       |   5   -4   1 |
```

Adjugate (transpose of cofactor matrix):

```
adj(M) = Cᵀ(M) = | -24  18   5 |
                  |  20 -15  -4 |
                  |  -5   4   1 |
```

Inverse = adj(M) / det(M) = adj(M) / 1 = adj(M):

```
M⁻¹ = | -24  18   5 |
       |  20 -15  -4 |
       |  -5   4   1 |
```

**Verify** (just check row 0 of the product):

```
Row 0 of M · Col 0 of M⁻¹: 1·(-24) + 2·20 + 3·(-5) = -24 + 40 - 15 = 1  ✓
Row 0 of M · Col 1 of M⁻¹: 1·18 + 2·(-15) + 3·4 = 18 - 30 + 12 = 0   ✓
Row 0 of M · Col 2 of M⁻¹: 1·5 + 2·(-4) + 3·1 = 5 - 8 + 3 = 0       ✓
```

#### Efficient 4×4 Inverse

For a 4×4 matrix M with columns **a**, **b**, **c**, **d** (as 3D vectors filling the first three rows) and fourth row [x, y, z, w]:

```
    a  b  c  d       (columns, first 3 rows)
    x  y  z  w       (fourth row)
```

Define intermediate vectors:

```
s = a × b
t = c × d
u = ya - xb
v = wc - zd
```

Then:

```
det(M) = s · v + t · u
```

And M⁻¹ is computed from these intermediates (see C++ code below).

In game engines, the fourth row is almost always [0, 0, 0, 1], which simplifies things enormously: u = **0**, v = **c**, and the determinant reduces to the 3×3 determinant of the upper-left block.

#### C++ Implementations

```cpp
// 3×3 inverse
Matrix3D Inverse(const Matrix3D& M)
{
    const Vector3D& a = M[0];
    const Vector3D& b = M[1];
    const Vector3D& c = M[2];

    Vector3D r0 = Cross(b, c);
    Vector3D r1 = Cross(c, a);
    Vector3D r2 = Cross(a, b);

    float invDet = 1.0F / Dot(r2, c);

    return (Matrix3D(r0.x * invDet, r0.y * invDet, r0.z * invDet,
                     r1.x * invDet, r1.y * invDet, r1.z * invDet,
                     r2.x * invDet, r2.y * invDet, r2.z * invDet));
}
```

```cpp
// 4×4 inverse
Matrix4D Inverse(const Matrix4D& M)
{
    const Vector3D& a = reinterpret_cast<const Vector3D&>(M[0]);
    const Vector3D& b = reinterpret_cast<const Vector3D&>(M[1]);
    const Vector3D& c = reinterpret_cast<const Vector3D&>(M[2]);
    const Vector3D& d = reinterpret_cast<const Vector3D&>(M[3]);

    const float& x = M(3, 0);
    const float& y = M(3, 1);
    const float& z = M(3, 2);
    const float& w = M(3, 3);

    Vector3D s = Cross(a, b);
    Vector3D t = Cross(c, d);
    Vector3D u = a * y - b * x;
    Vector3D v = c * w - d * z;

    float invDet = 1.0F / (Dot(s, v) + Dot(t, u));

    s *= invDet;
    t *= invDet;
    u *= invDet;
    v *= invDet;

    Vector3D r0 = Cross(b, v) + t * y;
    Vector3D r1 = Cross(v, a) - t * x;
    Vector3D r2 = Cross(d, u) + s * w;
    Vector3D r3 = Cross(u, c) - s * z;

    return (Matrix4D(r0.x, r0.y, r0.z, -Dot(b, t),
                     r1.x, r1.y, r1.z,  Dot(a, t),
                     r2.x, r2.y, r2.z, -Dot(d, s),
                     r3.x, r3.y, r3.z,  Dot(c, s)));
}
```

---

## Quick Reference Summary

| Operation | Formula | Result Type |
|-----------|---------|-------------|
| Magnitude | `√(vx² + vy² + vz²)` | Scalar |
| Normalize | `v / ‖v‖` | Vector |
| Dot product | `ax·bx + ay·by + az·bz` | Scalar |
| Cross product | `(ay·bz - az·by, az·bx - ax·bz, ax·by - ay·bx)` | Vector |
| Scalar triple product | `(a × b) · c` | Scalar |
| Projection | `((a · b) / b²) · b` | Vector |
| Rejection | `a - proj` | Vector |
| 2×2 determinant | `ad - bc` | Scalar |
| 3×3 determinant | Expansion by minors | Scalar |
| 2×2 inverse | `(1/(ad-bc)) · [[d, -b], [-c, a]]` | Matrix |
| 3×3 inverse | Cross product method | Matrix |

---

*Based on "Foundations of Game Engine Development, Volume 1: Mathematics" by Eric Lengyel.*
