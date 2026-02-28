# Chapter 4: Advanced Algebra

> **Prerequisites:** Chapters 1–3 (vectors, matrices, transforms, geometry, Plücker coordinates, homogeneous coordinates).

The previous chapters used conventional linear algebra — dot products, cross products, matrices. This chapter reveals the deeper mathematical framework from which all of those tools emerge: **Grassmann algebra** and **geometric algebra**. Along the way, we'll see that:

- The **cross product** is really a wedge product in disguise
- The **dot product** originates from wedging a vector with an antivector
- The **scalar triple product** is a triple wedge product
- **Plücker coordinates** are 4D bivectors
- **Quaternions** are rotors in 3D geometric algebra

---

## 4.1 Grassmann Algebra

Grassmann algebra is built from scalars and vectors by defining a single new multiplication called the **wedge product**. From this one operation, an entire zoo of higher-dimensional elements (bivectors, trivectors, …) emerges with perfect algebraic symmetry.

---

### 4.1.1 Wedge Product

The wedge product is denoted by ∧. Its defining rule is deceptively simple:

```
a ∧ a = 0
```

Any vector wedged with itself is zero.

**Consequence — Anticommutativity:**

Consider the wedge product of `(a + b)` with itself:

```
(a + b) ∧ (a + b) = a ∧ a + a ∧ b + b ∧ a + b ∧ b = 0
```

Since `a∧a = 0` and `b∧b = 0`, we're left with:

```
a ∧ b + b ∧ a = 0
```

```
┌─────────────────────────┐
│  a ∧ b = −(b ∧ a)      │
└─────────────────────────┘
```

Swapping the order of two vectors negates the result.

**Scalars behave normally:**

For scalars `s` and `t`:

```
s ∧ t = st          (ordinary multiplication)
```

```
s ∧ a = a ∧ s = sa  (scalar-vector product)
```

#### Properties of the Wedge Product

| Property | Formula | Notes |
|---|---|---|
| Anticommutativity | **a** ∧ **b** = −**b** ∧ **a** | For vectors |
| Associativity | (**a** ∧ **b**) ∧ **c** = **a** ∧ (**b** ∧ **c**) | For any elements |
| Left distributive | **a** ∧ (**b** + **c**) = **a** ∧ **b** + **a** ∧ **c** | |
| Right distributive | (**a** + **b**) ∧ **c** = **a** ∧ **c** + **b** ∧ **c** | |
| Scalar factorization | (t**a**) ∧ **b** = **a** ∧ (t**b**) = t(**a** ∧ **b**) | |
| Scalar wedge | s ∧ t = t ∧ s = st | Commutative |
| Scalar-vector wedge | s ∧ **a** = **a** ∧ s = s**a** | Commutative |

#### Numerical Example

Let **a** = (2, 1, 0) and **b** = (1, 0, 0):

```
a ∧ b = ?
```

Using the anticommutativity rule we can predict that `b ∧ a` should equal `−(a ∧ b)`. We'll compute the full result in the next section once we develop the component formula for bivectors.

For now, note the key insight: `a ∧ b ≠` a scalar, and `a ∧ b ≠` a vector. It's something new.

---

### 4.1.2 Bivectors

The wedge product **a** ∧ **b** of two vectors creates a **bivector** — a new type of mathematical element representing an **oriented area**.

**Visualization:** A bivector can be pictured as a parallelogram whose sides are parallel to **a** and **b**, with a winding direction (counterclockwise or clockwise) determined by the order of the factors.

#### Component Formula in 3D

Let `e₁, e₂, e₃` be orthonormal basis vectors. Write:

```
a = aₓ e₁ + aᵧ e₂ + a_z e₃
```

```
b = bₓ e₁ + bᵧ e₂ + b_z e₃
```

Expanding `a ∧ b`:

```
a ∧ b = aₓbᵧ(e₁ ∧ e₂) + aₓb_z(e₁ ∧ e₃) + aᵧbₓ(e₂ ∧ e₁)
      + aᵧb_z(e₂ ∧ e₃) + a_zbₓ(e₃ ∧ e₁) + a_zbᵧ(e₃ ∧ e₂)
```

Terms with `eᵢ ∧ eᵢ` vanish. Using anticommutativity (e.g., `e₂ ∧ e₁ = −e₁ ∧ e₂`), collect terms:

```
┌──────────────────────────────────────────────────────────────────────────┐
│ a ∧ b = (aᵧb_z − a_zbᵧ) e₂₃ + (a_zbₓ − aₓb_z) e₃₁ + (aₓbᵧ − aᵧbₓ) e₁₂ │
└──────────────────────────────────────────────────────────────────────────┘
```

where `e₂₃ = e₂ ∧ e₃`, `e₃₁ = e₃ ∧ e₁`, `e₁₂ = e₁ ∧ e₂`.

**Key observation:** The three coefficients are *identical* to the cross product `a × b`! But the result is a **bivector** (with basis elements `e₂₃, e₃₁, e₁₂`), not a vector (with basis elements `e₁, e₂, e₃`).

**Bivector properties:**
- In 3D, a bivector has **3 components** (basis: `e₂₃, e₃₁, e₁₂`)
- Carries **orientation** (winding direction) and **area** (magnitude)
- Does *not* carry a specific shape — infinitely many vector pairs produce the same bivector

#### Worked Example

Let **a** = (3, 1, −2) and **b** = (1, 4, 0).

Compute each coefficient:

- `e₂₃` component: `a_y b_z − a_z b_y = (1)(0) − (−2)(4) = 0 + 8 = 8`
- `e₃₁` component: `a_z b_x − a_x b_z = (−2)(1) − (3)(0) = −2 − 0 = −2`
- `e₁₂` component: `a_x b_y − a_y b_x = (3)(4) − (1)(1) = 12 − 1 = 11`

```
a ∧ b = 8 e₂₃ − 2 e₃₁ + 11 e₁₂
```

Cross-check: the cross product `a × b` gives the vector `(8, −2, 11)` — same numbers, different type of object.

**Verify anticommutativity:**

```
b ∧ a = (bᵧa_z − b_zaᵧ) e₂₃ + (b_zaₓ − bₓa_z) e₃₁ + (bₓaᵧ − bᵧaₓ) e₁₂
```

- `e₂₃`: `(4)(−2) − (0)(1) = −8`
- `e₃₁`: `(0)(3) − (1)(−2) = 2`
- `e₁₂`: `(1)(1) − (4)(3) = −11`

```
b ∧ a = −8 e₂₃ + 2 e₃₁ − 11 e₁₂ = −(a ∧ b)  ✓
```

**Area of the parallelogram:**

```
‖a ∧ b‖ = √(8² + (−2)² + 11²) = √(64 + 4 + 121) = √189 ≈ 13.75
```

---

### 4.1.3 Trivectors

Wedging three vectors `a ∧ b ∧ c` creates a **trivector** — an oriented volume element.

#### Component Formula in 3D

In 3D, every term in the expansion must contain all three basis vectors `e₁, e₂, e₃`. The six nonzero terms (one for each permutation of three items) are:

```
a ∧ b ∧ c = (aₓbᵧc_z + aᵧb_zcₓ + a_zbₓcᵧ − aₓb_zcᵧ − aᵧbₓc_z − a_zbᵧcₓ) e₁₂₃
```

The scalar coefficient is exactly the **determinant** of the matrix `[a | b | c]`, which equals the **scalar triple product** `(a × b) · c`.

**In 3D, a trivector has only 1 component** (basis: `e₁₂₃ = e₁ ∧ e₂ ∧ e₃`). Its magnitude equals the volume of the parallelepiped spanned by **a**, **b**, **c**.

#### Six Orderings and Sign

The sign depends on the permutation parity:

| Ordering | Parity | Sign |
|---|---|---|
| **a** ∧ **b** ∧ **c** | even (0 swaps) | + |
| **b** ∧ **c** ∧ **a** | even (2 swaps) | + |
| **c** ∧ **a** ∧ **b** | even (2 swaps) | + |
| **b** ∧ **a** ∧ **c** | odd (1 swap) | − |
| **a** ∧ **c** ∧ **b** | odd (1 swap) | − |
| **c** ∧ **b** ∧ **a** | odd (3 swaps) | − |

Three positive, three negative — matching even and odd permutations.

#### Worked Example

Let **a** = (1, 0, 0), **b** = (0, 1, 0), **c** = (0, 0, 1) (the standard basis).

```
a ∧ b ∧ c = (1·1·1 + 0 + 0 − 0 − 0 − 0) e₁₂₃ = 1 · e₁₂₃
```

The parallelepiped is a unit cube with volume 1. ✓

Now try **a** = (2, 0, 1), **b** = (1, 3, 0), **c** = (0, 1, 2):

```
coefficient = (2)(3)(2) + (0)(0)(0) + (1)(1)(1) − (2)(0)(1) − (0)(1)(2) − (1)(3)(0)
            = 12 + 0 + 1 − 0 − 0 − 0 = 13
```

```
a ∧ b ∧ c = 13 e₁₂₃
```

Verify: this is `det([a | b | c])`:

```
det | 2  1  0 |
    | 0  3  1 | = 2(6−0) − 1(0−1) + 0 = 12 + 1 = 13  ✓
    | 1  0  2 |
```

**Check a negative ordering:**

```
b ∧ a ∧ c = −(a ∧ b ∧ c) = −13 e₁₂₃  ✓
```

---

### 4.1.4 Algebraic Structure

#### k-Vectors and Grade

| Grade k | Name | 3D Components | What it represents |
|---|---|---|---|
| 0 | Scalar (0-vector) | 1 | Magnitude |
| 1 | Vector (1-vector) | 3 | Direction + magnitude |
| 2 | Bivector (2-vector) | 3 | Oriented area |
| 3 | Trivector (3-vector) | 1 | Oriented volume |

The **grade** of a k-vector is k. We write `gr(A)` for the grade of element A.

#### k-Blades

A **k-blade** is a k-vector that can be written as the wedge product of k individual vectors:

```
A = v₁ ∧ v₂ ∧ ⋯ ∧ vₖ
```

**In 3D or fewer dimensions, every k-vector is a k-blade.** In 4D+, this is not always true. For example, in 4D, the bivector `B = e₁₂ + e₃₄` cannot be factored as `v₁ ∧ v₂` for any vectors `v₁, v₂`.

#### Grade of a Wedge Product

```
┌──────────────────────────────────────┐
│ gr(A ∧ B) = gr(A) + gr(B)           │
└──────────────────────────────────────┘
```

The grades add. A vector (grade 1) wedged with a bivector (grade 2) gives a trivector (grade 3).

#### Commutation Rule

```
┌────────────────────────────────────────────────────────┐
│ A ∧ B = (−1)^(gr(A) · gr(B))  ×  B ∧ A               │
└────────────────────────────────────────────────────────┘
```

- If **either** grade is even → they **commute** (A ∧ B = B ∧ A)
- If **both** grades are odd → they **anticommute** (A ∧ B = −B ∧ A)

**Examples:**
- Two vectors (both grade 1, odd × odd = odd): anticommute ✓
- Vector ∧ bivector (1 × 2 = 2, even): commute
- Scalar ∧ anything (0 × k = 0, even): commute ✓

#### Number of Components: Binomial Coefficients

The number of independent basis elements of grade k in n dimensions:

```
C(n, k) = n! / (k! × (n−k)!)
```

This is Pascal's triangle:

```
            0D:    1                            total =  1 = 2^0
           1D:   1  1                           total =  2 = 2^1
          2D:  1  2  1                          total =  4 = 2^2
         3D: 1  3  3  1                         total =  8 = 2^3
        4D: 1  4  6  4  1                       total = 16 = 2^4
       5D: 1  5 10 10  5  1                     total = 32 = 2^5
```

The row for 3D reads 1, 3, 3, 1 — meaning: 1 scalar, 3 vector components, 3 bivector components, 1 trivector component. Total = 8 = 2³.

The row for 4D reads 1, 4, 6, 4, 1 — meaning: 1 scalar, 4 vectors, 6 bivectors, 4 trivectors, 1 quadrivector. Total = 16 = 2⁴.

#### Table of Basis Elements (0D through 4D)

| Dim | Grade | Count | Basis Elements |
|---|---|---|---|
| **0D** | Scalar | 1 | `1` |
| **1D** | Scalar | 1 | `1` |
| | Vector | 1 | `e₁` |
| **2D** | Scalar | 1 | `1` |
| | Vector | 2 | `e₁, e₂` |
| | Bivector | 1 | `e₁₂` |
| **3D** | Scalar | 1 | `1` |
| | Vector | 3 | `e₁, e₂, e₃` |
| | Bivector | 3 | `e₂₃, e₃₁, e₁₂` |
| | Trivector | 1 | `e₁₂₃` |
| **4D** | Scalar | 1 | `1` |
| | Vector | 4 | `e₁, e₂, e₃, e₄` |
| | Bivector | 6 | `e₄₁, e₄₂, e₄₃, e₂₃, e₃₁, e₁₂` |
| | Trivector | 4 | `e₂₃₄, e₃₁₄, e₁₂₄, e₁₃₂` |
| | Quadrivector | 1 | `e₁₂₃₄` |

#### Multivectors

A **multivector** mixes components of different grades. In 3D, a general multivector has 8 components:

```
a₀ + a₁ e₁ + a₂ e₂ + a₃ e₃ + a₄ e₂₃ + a₅ e₃₁ + a₆ e₁₂ + a₇ e₁₂₃
```

In Grassmann algebra we rarely need full multivectors — we work with elements of a single grade. (Geometric algebra, covered later, uses mixed grades productively.)

#### Unit Volume Element

The highest-grade element is the **unit volume element**:

```
┌──────────────────────────────────────────────┐
│ Eₙ = e₁ ∧ e₂ ∧ ⋯ ∧ eₙ = e₁₂…ₙ             │
└──────────────────────────────────────────────┘
```

- `E₃ = e₁₂₃` (unit volume in 3D)
- `E₄ = e₁₂₃₄` (unit hypervolume in 4D)

Any multiple of `Eₙ` is called a **pseudoscalar**.

---

### 4.1.5 Complements

The **complement** of a basis element B is whatever you need to wedge with it (on the right) to get the unit volume element `Eₙ`:

```
B ∧ B̄ = Eₙ
```

#### 3D Complements

From the requirement that each product yields `E₃ = e₁₂₃`:

```
e₁ ∧ (e₂ ∧ e₃) = E₃   →   ē₁ = e₂₃
```

```
e₂ ∧ (e₃ ∧ e₁) = E₃   →   ē₂ = e₃₁
```

```
e₃ ∧ (e₁ ∧ e₂) = E₃   →   ē₃ = e₁₂
```

And vice versa: `e̅₂₃ = e₁`, `e̅₃₁ = e₂`, `e̅₁₂ = e₃`.

**Complete 3D complement table:**

| Basis Element B | Complement B̄ |
|---|---|
| `1` | `e₁₂₃` |
| `e₁` | `e₂₃` |
| `e₂` | `e₃₁` |
| `e₃` | `e₁₂` |
| `e₂₃` | `e₁` |
| `e₃₁` | `e₂` |
| `e₁₂` | `e₃` |
| `e₁₂₃` | `1` |

#### Complement is Linear

For any scalar s and blades A, B:

```
complement(sA) = s × complement(A)
```

```
complement(A + B) = complement(A) + complement(B)
```

#### Complement of a 3D Vector

For **v** = x**e**₁ + y**e**₂ + z**e**₃:

```
v̄ = x e₂₃ + y e₃₁ + z e₁₂
```

Same components, reinterpreted over the bivector basis.

#### v ∧ v̄ = v² · Eₙ

```
v ∧ v̄ = (x² + y² + z²) e₁₂₃ = ‖v‖² × E₃
```

**Worked example:** Let **v** = (3, 4, 0).

```
v̄ = 3 e₂₃ + 4 e₃₁ + 0 e₁₂
```

```
v ∧ v̄ = (9 + 16 + 0) E₃ = 25 E₃ = ‖v‖² × E₃  ✓
```

#### Cross Product via Complement

The cross product is the complement of the wedge product:

```
┌──────────────────────────────────┐
│ a × b = complement(a ∧ b)       │
└──────────────────────────────────┘
```

Each bivector basis element is replaced by its complement: `e₂₃ → e₁`, `e₃₁ → e₂`, `e₁₂ → e₃`.

**Example:** From our earlier computation, `a ∧ b = 8 e₂₃ − 2 e₃₁ + 11 e₁₂`.

```
complement(a ∧ b) = 8 e₁ − 2 e₂ + 11 e₃ = (8, −2, 11) = a × b  ✓
```

This is *why* the cross product loses associativity — taking the complement between successive multiplications destroys it.

#### Right Complement vs Left Complement

The **right complement** B̄ satisfies B ∧ B̄ = Eₙ.

The **left complement** B̲ (bar below) satisfies B̲ ∧ B = Eₙ.

- In **odd** dimensions (1D, 3D, 5D, …): right and left complements are **identical**.
- In **even** dimensions (2D, 4D, …): they can differ by sign.

The relationship for a k-blade A in n dimensions:

```
left_complement(A) = (−1)^(k(n−k)) × right_complement(A)
```

#### Double Complement

```
complement(complement(A)) = (−1)^(k(n−k)) × A
```

- In 3D: `k(3−k)` is always even for k = 0, 1, 2, 3, so `Ā̄ = A` for all grades. The double complement always restores the original.
- In 4D: Vectors (k=1) and trivectors (k=3) pick up a minus sign: `Ā̄ = −A`.

A **mixed** double complement (one right, one left) always restores the original: `Ā̲ = Ā̲ = A`.

#### 4D Complement Table

| Basis Element B | Right Complement B̄ | Left Complement B̲ | Double Complement B̄̄ |
|---|---|---|---|
| `1` | `e₁₂₃₄` | `e₁₂₃₄` | `1` |
| `e₁` | `e₂₃₄` | `−e₂₃₄` | `−e₁` |
| `e₂` | `e₃₁₄` | `−e₃₁₄` | `−e₂` |
| `e₃` | `e₁₂₄` | `−e₁₂₄` | `−e₃` |
| `e₄` | `e₁₃₂` | `−e₁₃₂` | `−e₄` |
| `e₄₁` | `−e₂₃` | `−e₂₃` | `e₄₁` |
| `e₄₂` | `−e₃₁` | `−e₃₁` | `e₄₂` |
| `e₄₃` | `−e₁₂` | `−e₁₂` | `e₄₃` |
| `e₂₃` | `−e₄₁` | `−e₄₁` | `e₂₃` |
| `e₃₁` | `−e₄₂` | `−e₄₂` | `e₃₁` |
| `e₁₂` | `−e₄₃` | `−e₄₃` | `e₁₂` |
| `e₂₃₄` | `−e₁` | `e₁` | `−e₂₃₄` |
| `e₃₁₄` | `−e₂` | `e₂` | `−e₃₁₄` |
| `e₁₂₄` | `−e₃` | `e₃` | `−e₁₂₄` |
| `e₁₃₂` | `−e₄` | `e₄` | `−e₁₃₂` |
| `e₁₂₃₄` | `1` | `1` | `e₁₂₃₄` |

Notice: vectors (grade 1) and trivectors (grade 3) have **opposite** right/left complements and pick up a **minus sign** under double complement. Scalars (grade 0), bivectors (grade 2), and the quadrivector (grade 4) are unaffected.

---

### 4.1.6 Antivectors

The complement of a 1-vector (vector) in n dimensions is an (n−1)-vector called an **antivector**. Both vectors and antivectors have exactly **n components**.

An antivector represents "everything a vector is not" — all the directions perpendicular to the vector, excluding the vector's own direction.

#### Antivector Construction

For a vector **v** = v₁**e**₁ + v₂**e**₂ + ⋯ + vₙ**e**ₙ, its antivector is:

```
v̄ = v₁ē₁ + v₂ē₂ + ⋯ + vₙēₙ
```

Same components, but over the complement basis.

#### Transformation Rule

Vectors transform as `v' = Mv`.

Antivectors transform as **row vectors multiplied by the adjugate**: `a' = a · adj(M)`.

This is exactly how **normal vectors** (3D antivectors) and **planes** (4D antivectors) transform — a fact that was introduced in Chapter 3 without a deep explanation. Now we see it's because they are antivectors!

#### Origin of the Dot Product: a ∧ b̄

The wedge product of a vector with an antivector reveals the dot product:

```
a ∧ b̄ = (a₁b₁ + a₂b₂ + ⋯ + aₙbₙ) Eₙ = (a · b) Eₙ
```

**This is the origin of the dot product!** The defining formula `a · b = Σ aᵢbᵢ` emerges from wedging a vector with an antivector.

#### Worked Example

Let **a** = (2, 3, 1) and **b** = (4, −1, 2) in 3D.

First, compute the antivector:

```
b̄ = 4 e₂₃ + (−1) e₃₁ + 2 e₁₂
```

Now wedge:

```
a ∧ b̄ = (2·4 + 3·(−1) + 1·2) E₃ = (8 − 3 + 2) E₃ = 7 E₃
```

Check: `a · b = 2(4) + 3(−1) + 1(2) = 8 − 3 + 2 = 7`. ✓

---

### 4.1.7 Antiwedge Product

The antiwedge product, denoted ∨ (downward wedge), is the mirror operation of the wedge product. The wedge product builds up dimensions; the antiwedge product tears them down.

| Wedge Product ∧ | Antiwedge Product ∨ |
|---|---|
| Combines included dimensions | Combines excluded dimensions |
| Increases grade | Decreases grade |
| Union-like | Intersection-like |
| Operates naturally on vectors | Operates naturally on antivectors |

#### Properties

**Anticommutativity for antivectors:**

```
ā ∨ b̄ = −(b̄ ∨ ā)
```

**General commutation rule:**

```
A ∨ B = (−1)^((n − gr(A))(n − gr(B))) × B ∨ A
```

Two elements commute under ∨ when their *complements* commute under ∧.

**De Morgan's laws:**

```
complement(A ∧ B) = Ā ∨ B̄
```

```
complement(A ∨ B) = Ā ∧ B̄
```

These connect ∧ and ∨ just as De Morgan's laws in logic connect AND and OR.

#### Grade of the Antiwedge Product

```
┌──────────────────────────────────────────────┐
│ gr(A ∨ B) = gr(A) + gr(B) − n               │
└──────────────────────────────────────────────┘
```

The antiwedge product is zero whenever this grade would be negative.

**Example (3D):** Two bivectors (grade 2 each): `gr(A ∨ B) = 2 + 2 − 3 = 1` → a vector.

#### Dot Product via Antiwedge

The antiwedge product with an antivector gives a scalar (no volume element needed):

```
┌────────────────────────────────────────────────────────────────────┐
│ a · b = a ∨ b̄ = −left_complement(a) ∨ b = b ∨ ā = left_complement(b) ∨ a │
└────────────────────────────────────────────────────────────────────┘
```

All four forms give the same scalar result.

#### Antiwedge of Two 3D Bivectors

For antivectors `ā = aₓē₁ + aᵧē₂ + a_zē₃` and `b̄ = bₓē₁ + bᵧē₂ + b_zē₃`, their antiwedge product is:

```
ā ∨ b̄ = (aᵧb_z − a_zbᵧ) e₁ + (a_zbₓ − aₓb_z) e₂ + (aₓbᵧ − aᵧbₓ) e₃
```

The antiwedge of two bivectors in 3D gives an ordinary **vector** with **cross product** components!

#### Worked Example

Let **a** = (1, 2, 3) and **b** = (4, 5, 6) in 3D.

**Dot product via antiwedge:**

```
a ∨ b̄ = 1·4 + 2·5 + 3·6 = 4 + 10 + 18 = 32
```

Check: `a · b = 4 + 10 + 18 = 32` ✓

**Antiwedge of their complements (bivectors):**

```
ā = 1 e₂₃ + 2 e₃₁ + 3 e₁₂
```

```
b̄ = 4 e₂₃ + 5 e₃₁ + 6 e₁₂
```

```
ā ∨ b̄ = (2·6 − 3·5) e₁ + (3·4 − 1·6) e₂ + (1·5 − 2·4) e₃
       = (12 − 15) e₁ + (12 − 6) e₂ + (5 − 8) e₃
       = −3 e₁ + 6 e₂ − 3 e₃
```

Check: `a × b = (2·6−3·5, 3·4−1·6, 1·5−2·4) = (−3, 6, −3)` ✓

---

## 4.2 Projective Geometry

We now apply Grassmann algebra in 4D to homogeneous coordinates. The result: points, lines, and planes become vectors, bivectors, and trivectors in 4D — and all geometric operations (joining, intersecting) become wedge/antiwedge products.

---

### 4.2.1 Lines

The wedge product of two 4D homogeneous points **p** and **q** (with implicit w = 1) naturally produces the 6 **Plücker coordinates** of the line through them.

#### Formula

```
p ∧ q = (qₓ − pₓ) e₄₁ + (qᵧ − pᵧ) e₄₂ + (q_z − p_z) e₄₃
      + (pᵧq_z − p_zqᵧ) e₂₃ + (p_zqₓ − pₓq_z) e₃₁ + (pₓqᵧ − pᵧqₓ) e₁₂
```

Using the notation `{v | m}`:

```
L = vₓ e₄₁ + vᵧ e₄₂ + v_z e₄₃ + mₓ e₂₃ + mᵧ e₃₁ + m_z e₁₂
```

where **v** = **q** − **p** (direction) and **m** = **p** × **q** (moment).

The 4D bivector represents an oriented plane in 4D. It intersects the 3D subspace w = 1 at the geometric line.

#### Worked Example

Let **p** = (1, 2, 3) and **q** = (4, 5, 6) (both with w = 1).

**Direction:** v = q − p = (3, 3, 3)

**Moment:** m = p × q = (2·6 − 3·5, 3·4 − 1·6, 1·5 − 2·4) = (−3, 6, −3)

```
L = p ∧ q = 3 e₄₁ + 3 e₄₂ + 3 e₄₃ − 3 e₂₃ + 6 e₃₁ − 3 e₁₂
```

Or in Plücker notation: `L = {(3, 3, 3) | (−3, 6, −3)}`.

Verify `v · m = 0`: `3(−3) + 3(6) + 3(−3) = −9 + 18 − 9 = 0` ✓ (direction and moment are always perpendicular).

---

### 4.2.2 Planes

The triple wedge product of three homogeneous points gives a **trivector** — the 4D representation of a plane.

#### Formula

```
p ∧ q ∧ r = nₓ ē₁ + nᵧ ē₂ + n_z ē₃ + d ē₄
```

where:

```
n = p ∧ q + q ∧ r + r ∧ p
```

```
d = −(p ∧ q ∧ r)    (3D triple wedge)
```

Or equivalently, using the conventional formulation:

```
n = (q − p) ∧ (r − p),    d = −(n ∨ p)
```

A plane is a **4D antivector** (trivector = (4−1)-vector).

You can also build a plane from a point and a line: `p ∧ L`.

#### Worked Example

Let **p** = (1, 0, 0), **q** = (0, 1, 0), **r** = (0, 0, 1).

Using the conventional approach:

```
n = (q − p) × (r − p) = (−1, 1, 0) × (−1, 0, 1)
  = (1·1 − 0·0,  0·(−1) − (−1)·1,  (−1)·0 − 1·(−1))
  = (1, 1, 1)
```

```
d = −(n · p) = −(1·1 + 1·0 + 1·0) = −1
```

So the plane is `[n | d] = [(1, 1, 1) | −1]`, or equivalently `x + y + z = 1`. ✓

The 4D trivector representation:

```
p ∧ q ∧ r = 1·ē₁ + 1·ē₂ + 1·ē₃ + (−1)·ē₄
```

---

### 4.2.3 Join and Meet

The wedge product is the **join** (union of directions), and the antiwedge product is the **meet** (intersection of directions).

#### Join Operations (Wedge Product ∧)

| Operation | Inputs | Result | Degenerate case |
|---|---|---|---|
| **p** ∧ **q** | Two points | Line through them | `{0|0}` if coincident |
| **p** ∧ L | Point + Line | Plane through both | `[0|0]` if p lies on L |
| **p** ∧ **q** ∧ **r** | Three points | Plane through them | `[0|0]` if collinear |

#### Meet Operations (Antiwedge Product ∨)

| Operation | Inputs | Result | Degenerate case |
|---|---|---|---|
| **f** ∨ **g** | Two planes | Line where they intersect | `{0|0}` if coincident; `{0|m}` if parallel |
| **f** ∨ **g** ∨ **h** | Three planes | Point where they meet | `(0|0)` if degenerate; `(p|0)` if parallel intersection lines |
| **f** ∨ L | Plane + Line | Point where they intersect | `(0|0)` if L lies in f; `(p|0)` if L parallel to f |

#### Worked Example: Meet of Two Planes

Let `f = [1, 0, 0 | −2]` (the plane x = 2) and `g = [0, 1, 0 | −3]` (the plane y = 3).

Using the antiwedge product formula for two 4D trivectors:

**Direction of intersection line:**
- `vₓ = fᵧg_z − f_zgᵧ = 0·0 − 0·1 = 0`
- `vᵧ = f_zgₓ − fₓg_z = 0·0 − 1·0 = 0`
- `v_z = fₓgᵧ − fᵧgₓ = 1·1 − 0·0 = 1`

**Moment of intersection line:**
- `mₓ = f_wgₓ − fₓg_w = (−2)(0) − (1)(−3) = 3`
- `mᵧ = f_wgᵧ − fᵧg_w = (−2)(1) − (0)(−3) = −2`
- `m_z = f_wg_z − f_zg_w = (−2)(0) − (0)(−3) = 0`

Result: `L = {(0, 0, 1) | (3, −2, 0)}` — a line in the z-direction passing through (2, 3, 0). ✓

---

### 4.2.4 Line Crossing

The wedge product of two lines `L₁ ∧ L₂` produces a 4D volume element (quadrivector) whose magnitude relates to the **signed distance** between the lines.

```
L₁ ∧ L₂ = (v₁ · m₂ + v₂ · m₁) E₄
```

The **signed distance** is:

```
┌──────────────────────────────────────────┐
│ d = (L₁ ∨ L₂) / ‖v₁ ∧ v₂‖             │
└──────────────────────────────────────────┘
```

where:

```
L₁ ∨ L₂ = −(v₁ · m₂ + v₂ · m₁)
```

and `‖v₁ ∧ v₂‖ = ‖v₁ × v₂‖` is the area of the parallelogram formed by the two direction vectors.

#### Crossing Orientation

- Positive → lines are wound **clockwise** around each other
- Negative → lines are wound **counterclockwise**
- Zero → lines are **coplanar** (they intersect or are parallel)

**Application:** Ray-triangle intersection. If a ray is tested against the three edges of a counterclockwise-wound triangle, and all three antiwedge products are positive, the ray hits the triangle.

#### Worked Example

Let `L₁ = {(1, 0, 0) | (0, 0, 0)}` (x-axis) and `L₂ = {(0, 1, 0) | (0, 0, 5)}` (a line in the y-direction, offset 5 units along x from origin).

```
L₁ ∨ L₂ = −(v₁ · m₂ + v₂ · m₁)
         = −((1, 0, 0) · (0, 0, 5) + (0, 1, 0) · (0, 0, 0))
         = −(0 + 0) = 0
```

These lines are coplanar (both lie in the z = 0 plane), so the distance is 0. They intersect at (0, 0, 0). ✓

Now let `L₂ = {(0, 1, 0) | (0, 0, −3)}` (y-direction, moment indicates distance 3 from origin along x-axis).

Actually, let's use explicit points. Let L₁ pass through (0,0,0) and (1,0,0), and L₂ pass through (0,0,5) and (0,1,5) (offset by 5 in z).

- `L₁ = {(1,0,0) | (0,0,0)}`
- `L₂ = {(0,1,0) | (5,0,0)}` since m = p×q = (0,0,5)×(0,1,5) = (0·5−5·1, 5·0−0·5, 0·1−0·0) = (−5, 0, 0). Let me recalculate:
  - p₂ = (0,0,5), q₂ = (0,1,5)
  - v₂ = (0,1,0)
  - m₂ = p₂ × q₂ = (0·5 − 5·1, 5·0 − 0·5, 0·1 − 0·0) = (−5, 0, 0)

```
L₁ ∨ L₂ = −((1,0,0) · (−5,0,0) + (0,1,0) · (0,0,0)) = −(−5 + 0) = 5
```

```
‖v₁ × v₂‖ = ‖(1,0,0) × (0,1,0)‖ = ‖(0,0,1)‖ = 1
```

```
d = 5 / 1 = 5
```

The distance between the lines is 5 (the z-offset). ✓

---

### 4.2.5 Plane Distance

The wedge product of a homogeneous point **p** (w = 1) and a plane **f** = [**n** | d] produces a 4D volume element proportional to the signed distance.

```
┌─────────────────────────────────────┐
│ d_signed = (p ∨ f) / ‖n‖           │
└─────────────────────────────────────┘
```

This is equivalent to the familiar `f · p = nₓpₓ + nᵧpᵧ + n_zp_z + d` from Chapter 3.

The technically correct Grassmann formulas:

```
f · p = p ∨ f = left_complement(f) ∨ p
```

The double complement cancels the negation from swapping factors.

#### Worked Example

Plane `f = [(1, 0, 0) | −3]` (the plane x = 3) and point `p = (7, 2, 5)`.

```
p ∨ f = 1·7 + 0·2 + 0·5 + (−3) = 7 − 3 = 4
```

```
‖n‖ = ‖(1,0,0)‖ = 1
```

```
d = 4 / 1 = 4
```

The point is 4 units from the plane (on the positive side). ✓

---

### 4.2.6 Summary and Implementation

#### Correspondence Table

| Grassmann Element | Grade | Components | Geometric Object | Projection Condition |
|---|---|---|---|---|
| Vector | 1 | 4 | Point (p \| w) | w = 1 |
| Bivector | 2 | 6 | Line {v \| m} | ‖v‖ = 1 |
| Trivector (antivector) | 3 | 4 | Plane [n \| d] | ‖n‖ = 1 |

#### Operations Summary

| Formula | Description | Result Grade |
|---|---|---|
| **p** ∧ **q** | Line joining two points | 2 (bivector) |
| **p** ∧ **q** ∧ **r** | Plane through three points | 3 (trivector) |
| **p** ∧ L | Plane through point and line | 3 (trivector) |
| **f** ∨ **g** | Line where two planes meet | 2 (bivector) |
| **f** ∨ **g** ∨ **h** | Point where three planes meet | 1 (vector) |
| **f** ∨ L | Point where line meets plane | 1 (vector) |
| L₁ ∨ L₂ | Signed distance × base area | 0 (scalar) |
| **p** ∨ **f** | Signed distance × ‖n‖ | 0 (scalar) |

#### C++ Implementation

The `^` operator in C++ conveniently resembles ∧. We use it for both wedge and antiwedge products, since for any given pair of input types, at most one of the two operations is non-trivial.

> **Warning:** The `^` operator has **very low precedence** in C++ — lower than comparison operators! Always wrap expressions in parentheses: `(a ^ b) < (c ^ d)`, not `a ^ b < c ^ d`.

```cpp
// Join: line through two points
inline Line operator^(const Point3D& p, const Point3D& q) {
    return Line(
        q.x - p.x, q.y - p.y, q.z - p.z,
        p.y * q.z - p.z * q.y,
        p.z * q.x - p.x * q.z,
        p.x * q.y - p.y * q.x
    );
}

// Meet: line where two planes intersect
inline Line operator^(const Plane& f, const Plane& g) {
    return Line(
        f.y * g.z - f.z * g.y,
        f.z * g.x - f.x * g.z,
        f.x * g.y - f.y * g.x,
        g.x * f.w - f.x * g.w,
        g.y * f.w - f.y * g.w,
        g.z * f.w - f.z * g.w
    );
}

// Join: plane through line and point
inline Plane operator^(const Line& L, const Point3D& p) {
    return Plane(
        L.direction.y * p.z - L.direction.z * p.y + L.moment.x,
        L.direction.z * p.x - L.direction.x * p.z + L.moment.y,
        L.direction.x * p.y - L.direction.y * p.x + L.moment.z,
       -L.moment.x * p.x - L.moment.y * p.y - L.moment.z * p.z
    );
}

inline Plane operator^(const Point3D& p, const Line& L) {
    return L ^ p;
}

// Meet: point where line intersects plane
inline Vector4D operator^(const Line& L, const Plane& f) {
    return Vector4D(
        L.moment.y * f.z - L.moment.z * f.y + L.direction.x * f.w,
        L.moment.z * f.x - L.moment.x * f.z + L.direction.y * f.w,
        L.moment.x * f.y - L.moment.y * f.x + L.direction.z * f.w,
       -L.direction.x * f.x - L.direction.y * f.y - L.direction.z * f.z
    );
}

inline Vector4D operator^(const Plane& f, const Line& L) {
    return L ^ f;
}

// Signed distance between two lines (times ||v1 x v2||)
inline float operator^(const Line& L1, const Line& L2) {
    return -(Dot(L1.direction, L2.moment) + Dot(L2.direction, L1.moment));
}

// Signed distance between point and plane (times ||n||)
inline float operator^(const Point3D& p, const Plane& f) {
    return p.x * f.x + p.y * f.y + p.z * f.z + f.w;
}

inline float operator^(const Plane& f, const Point3D& p) {
    return -(p ^ f);
}
```

---

## 4.3 Matrix Inverses

Grassmann algebra illuminates matrix inverses. The determinant of an n×n matrix is the wedge product of all its columns (complemented to a scalar):

```
D = complement(c₀ ∧ c₁ ∧ ⋯ ∧ cₙ₋₁)
```

Row i of `D · M⁻¹` is the wedge product of all columns *except* column i:

```
rᵢ = ((−1)ⁱ / D) × left_complement(c₀ ∧ ⋯ ∧ ĉᵢ ∧ ⋯ ∧ cₙ₋₁)
```

where `ĉᵢ` means "omit column i" and the left complement converts the result to an antivector (row vector).

### 2×2 Inverse via Wedge Products

Columns **a** and **b** of a 2×2 matrix M:

```
D = complement(a ∧ b) = aₓbᵧ − aᵧbₓ
```

```
         1   | bᵧ   −bₓ |
M⁻¹ =  --- × |          |
         D   | −aᵧ   aₓ |
```

This is the familiar 2×2 inverse formula — now derived from wedge products.

### 3×3 Inverse via Wedge Products

Columns **a**, **b**, **c** of a 3×3 matrix:

```
D = complement(a ∧ b ∧ c)    (= determinant = scalar triple product)
```

The rows of the inverse are the wedge products of pairs of columns, divided by D:

```
         1   | — left_complement(b ∧ c) — |
M⁻¹ =  --- × | — left_complement(c ∧ a) — |
         D   | — left_complement(a ∧ b) — |
```

The wedge products are complements (converted from bivectors to vectors to serve as rows). This is equivalent to the cross product formula: `b × c`, `c × a`, `a × b`.

### 4×4 Inverse via Wedge Products

For a 4×4 matrix with columns **a**, **b**, **c**, **d**, define intermediate 4D bivectors:

```
s = a ∧ b,    t = c ∧ d
```

and 3D auxiliary quantities:

```
u = a_w × b_xyz − b_w × a_xyz,    v = c_w × d_xyz − d_w × c_xyz
```

where `a_xyz` means the 3D part `(aₓ, aᵧ, a_z)`.

Also: **s** = `a_xyz ∧ b_xyz` (3D wedge/cross), **t** = `c_xyz ∧ d_xyz` (3D wedge/cross).

The **determinant** is:

```
D = (a ∧ b) ∧ (c ∧ d) = −ū · v − v̄ · u = s · v + t · u
```

The four rows of `D · M⁻¹` are:

```
r₀ =  [v ∧ b + b_w × t  |  −b · t]
r₁ = −[v ∧ a + a_w × t  |  −a · t]
r₂ =  [u ∧ d + d_w × s  |  −d · s]
r₃ = −[u ∧ c + c_w × s  |  −c · s]
```

#### Worked Example: 3×3 Inverse

```
        | 1  0  1 |         | 1 |       | 0 |       | 1 |
M   =   | 0  1  0 |,   a = | 0 |, b = | 1 |, c = | 0 |
        | 0  0  1 |         | 0 |       | 0 |       | 1 |
```

**Determinant:**

```
D = complement(a ∧ b ∧ c) = det(M) = 1(1·1 − 0·0) − 0 + 0 = 1
```

**Rows of the inverse:**

```
r₀ = complement(b ∧ c) / D = b × c = (0,1,0) × (1,0,1) = (1, −1, −1)
```

Wait, let me redo: `b × c`:
- `(bᵧc_z − b_zcᵧ, b_zcₓ − bₓc_z, bₓcᵧ − bᵧcₓ)`
- `= (1·1 − 0·0, 0·1 − 0·1, 0·0 − 1·1) = (1, 0, −1)`

- `c × a`:
  - `= (0·0 − 1·0, 1·1 − 1·0, 1·0 − 0·1) = (0, 1, 0)`

  Wait: `c × a = (cᵧa_z − c_zaᵧ, c_zaₓ − cₓa_z, cₓaᵧ − cᵧaₓ)`
  - `= (0·0 − 1·0, 1·1 − 1·0, 1·0 − 0·1) = (0, 1, 0)`

- `a × b`:
  - `= (0·0 − 0·1, 0·0 − 1·0, 1·1 − 0·0) = (0, 0, 1)`

```
         1   | 1  0  −1 |
M⁻¹ =  --- × | 0  1   0 |
         1   | 0  0   1 |
```

**Verify:** `M · M⁻¹`:

```
| 1  0  1 |   | 1  0  −1 |   | 1  0  0 |
| 0  1  0 | × | 0  1   0 | = | 0  1  0 |  ✓
| 0  0  1 |   | 0  0   1 |   | 0  0  1 |
```

---

## 4.4 Geometric Algebra

Geometric algebra extends Grassmann algebra by changing the fundamental rule. Instead of `a ∧ a = 0`, the **geometric product** satisfies:

```
┌──────────────────────┐
│ a a = a · a          │
└──────────────────────┘
```

A vector squared equals its squared magnitude — a scalar, not zero!

---

### 4.4.1 Geometric Product

We write the geometric product as juxtaposition: `ab` (no symbol).

#### Key Properties

From `(a + b)(a + b) = (a + b) · (a + b)`, expanding both sides:

```
aa + ab + ba + bb = a·a + 2(a·b) + b·b
```

Since `aa = a·a` and `bb = b·b`, these cancel, leaving:

```
┌──────────────────────────────┐
│ ab + ba = 2(a · b)           │
└──────────────────────────────┘
```

**For orthonormal basis vectors:**

```
eᵢ eⱼ = −eⱼ eᵢ     (i ≠ j)
```

```
eᵢ eᵢ = 1
```

#### The Central Equation

For any two vectors **a** and **b**:

```
┌──────────────────────────────────┐
│ ab = a · b + a ∧ b              │
└──────────────────────────────────┘
```

The geometric product is the **sum of the dot product (scalar) and wedge product (bivector)** — a mixed-grade multivector!

Conversely:

```
a · b = (1/2)(ab + ba)     (symmetric part)
```

```
a ∧ b = (1/2)(ab − ba)     (antisymmetric part)
```

**When parallel** (a ∧ b = 0): `ab = a · b` (commutative, scalar only).

**When perpendicular** (a · b = 0): `ab = a ∧ b` (anticommutative, bivector only).

#### Worked Example

Let **a** = (3, 0, 0) and **b** = (2, 1, 0).

**Dot product:** `a · b = 6 + 0 + 0 = 6`

**Wedge product:** `a ∧ b = (0−0) e₂₃ + (0−0) e₃₁ + (3·1 − 0·2) e₁₂ = 3 e₁₂`

**Geometric product:**

```
ab = 6 + 3 e₁₂
```

This is a multivector with a scalar part (6) and a bivector part (3 e₁₂).

**Verify using basis expansion:**

```
(3 e₁)(2 e₁ + 1 e₂) = 6 e₁e₁ + 3 e₁e₂ = 6·1 + 3 e₁₂ = 6 + 3 e₁₂  ✓
```

**Now compute ba:**

```
ba = (2 e₁ + 1 e₂)(3 e₁) = 6 e₁e₁ + 3 e₂e₁ = 6 − 3 e₁₂
```

Check the identities:
- `(ab + ba)/2 = (6 + 3e₁₂ + 6 − 3e₁₂)/2 = 12/2 = 6 = a · b` ✓
- `(ab − ba)/2 = (6 + 3e₁₂ − 6 + 3e₁₂)/2 = 6e₁₂/2 = 3e₁₂ = a ∧ b` ✓

---

### 4.4.2 Vector Division

Since `vv = v · v = ‖v‖²` (a nonzero scalar for nonzero v), we can define the **inverse** of a vector:

```
┌────────────────────────────────┐
│ v⁻¹ = v / v² = v / ‖v‖²      │
└────────────────────────────────┘
```

Verify: `v · v⁻¹ = v · v / v² = v² / v² = 1` ✓

#### Quotient of Two Vectors

```
a/b = a b⁻¹ = (ab) / b² = (a · b) / b² + (a ∧ b) / b²
```

The first term is the **projection** of **a** onto **b**: `a‖b = (a · b) / b²  × b`.

The second term is the **rejection** of **a** from **b**: `a⊥b`.

#### Dividing a Bivector by a Vector

```
(a ∧ b) / b = rejection of a from b
```

A bivector contains no information about its shape (it's pure area + orientation). But dividing by one of its constituent vectors "extracts" the part of the other vector perpendicular to it — forming a right-angled parallelogram with the same area.

#### Worked Example

Let **a** = (5, 3, 0) and **b** = (4, 0, 0).

**Projection of a onto b:**

```
a_parallel = ((a · b) / b²) × b = (20/16)(4, 0, 0) = (5, 0, 0)
```

**Via geometric algebra:**

```
(a · b) / b² = 20/16 = 5/4
```

This is a scalar that, when multiplied by **b**, gives the projection.

**Rejection of a from b:**

```
(a ∧ b) / b = ?
```

First: `a ∧ b = (0) e₂₃ + (0) e₃₁ + (5·0 − 3·4) e₁₂ = −12 e₁₂`

```
(a ∧ b) / b = (−12 e₁₂) / (4 e₁) = (−12/4) e₁₂ e₁⁻¹ = −3 e₁₂ (e₁/1)
            = −3 e₁ e₂ e₁ = −3 × (−e₁ e₁ e₂) = −3 × (−e₂) = 3 e₂ = (0, 3, 0)
```

Check: `a − a‖ = (5, 3, 0) − (5, 0, 0) = (0, 3, 0)` ✓

**Inverse of a vector:**

```
b⁻¹ = (4, 0, 0) / 16 = (0.25, 0, 0)
```

```
b b⁻¹ = (4)(0.25) e₁e₁ = 1  ✓
```

---

### 4.4.3 Rotors

Rotors are the crown jewel of geometric algebra — they unify reflections, rotations, and quaternions.

#### Reflection

The geometric product sandwich `ava⁻¹` reflects **v** across the vector **a**:

```
v' = a v a⁻¹
```

This is because the projection component is preserved and the rejection is negated — exactly a reflection (involution).

#### Two Reflections = Rotation

A reflection across **a** followed by a reflection across **b**:

```
v'' = b(a v a⁻¹)b⁻¹ = (ba) v (ba)⁻¹
```

The quantity `R = ba` is a **rotor**. The rotation angle is **2θ**, where θ is the angle between **a** and **b**.

#### Rotor Formula

For unit vectors **a** and **b** separated by angle θ:

```
R = ba = a · b − a ∧ b
```

(Note the minus sign — from `ba = ab − 2(a ∧ b)`, but since `ab = a·b + a∧b`, we get `ba = a·b − a∧b`.)

```
R = cos θ − sin θ × n̄
```

where **n** is the unit vector in the direction of `a × b`, and **n̄** is its complement (a unit bivector in the plane of rotation).

To rotate by angle θ (not 2θ), use the half-angle:

```
┌──────────────────────────────────────┐
│ R = cos(θ/2) − sin(θ/2) × n̄        │
└──────────────────────────────────────┘
```

#### Rotors ARE Quaternions!

Comparing with the quaternion formula `q = cos(θ/2) + sin(θ/2)(nₓi + nᵧj + n_zk)`:

```
i = −e₂₃,    j = −e₃₁,    k = −e₁₂
```

The quaternion imaginary units are **negated bivector basis elements** in 3D geometric algebra!

- The quaternion sandwich product `qvq*` is the rotor sandwich `RvR⁻¹`
- Quaternion conjugation (negating i, j, k parts) corresponds to **reversing** the factor order in the geometric product: `(ba)* = ab`
- Quaternions are **even-grade multivectors** (scalar + bivector) in 3D geometric algebra

#### Worked Example: Rotation by 90° about the z-axis

Rotation axis: **n** = (0, 0, 1), angle θ = 90°.

```
R = cos(45°) − sin(45°) × n̄ = (√2/2) − (√2/2) e₁₂
```

(since **n̄** = ē₃ = e₁₂ for a z-axis vector).

Let's rotate **v** = (1, 0, 0) = e₁.

```
R v R⁻¹ = R e₁ R⁻¹
```

For a unit rotor, `R⁻¹ = R† = cos(45°) + sin(45°) e₁₂` (reverse = negate bivector part).

```
R e₁ = ((√2/2) − (√2/2) e₁₂) e₁
     = (√2/2) e₁ − (√2/2) e₁₂ e₁
     = (√2/2) e₁ − (√2/2) e₁ e₂ e₁
```

Since `e₁e₁ = 1`:

```
     = (√2/2) e₁ − (√2/2)(−e₂) = (√2/2) e₁ + (√2/2) e₂
```

Now multiply by R⁻¹:

```
((√2/2) e₁ + (√2/2) e₂)((√2/2) + (√2/2) e₁₂)

= (1/2) e₁ + (1/2) e₁ e₁₂ + (1/2) e₂ + (1/2) e₂ e₁₂
```

Compute `e₁e₁₂ = e₁e₁e₂ = e₂` and `e₂e₁₂ = e₂e₁e₂ = −e₁e₂e₂ = −e₁`:

```
= (1/2) e₁ + (1/2) e₂ + (1/2) e₂ − (1/2) e₁
= e₂ = (0, 1, 0)
```

A 90° rotation of (1, 0, 0) about the z-axis gives (0, 1, 0). ✓

**As a quaternion:** `q = cos(45°) + sin(45°)k = (√2/2) + (√2/2)k`. This matches `R` with the identification `k = −e₁₂`, meaning `sin(45°)k` in quaternion notation equals `−sin(45°)e₁₂` in geometric algebra — exactly what we have.

---

## 4.5 Conclusion

This chapter revealed the deeper structure underlying the conventional mathematics of game engines. Here is the grand summary:

| Conventional Concept | Grassmann / Geometric Algebra Concept |
|---|---|
| Cross product **a** × **b** | Wedge product **a** ∧ **b** (bivector, not a vector) |
| Scalar triple product [**a**, **b**, **c**] | Triple wedge **a** ∧ **b** ∧ **c** (trivector) |
| Dot product **a** · **b** | Antiwedge product **a** ∨ **b̄** (vector ∨ antivector) |
| Implicit plane [**n** \| d] | 4D antivector (trivector) |
| Plücker line {**v** \| **m**} | 4D bivector |
| Quaternion w + xi + yj + zk | Even-grade multivector w − x**e**₂₃ − y**e**₃₁ − z**e**₁₂ |
| Matrix determinant | Wedge product of all columns |
| Matrix inverse rows | Wedge products of all-but-one column sets |
| Reflection `v − 2(v·a)a` | Sandwich product `ava⁻¹` |
| Quaternion rotation `qvq*` | Rotor sandwich `RvR⁻¹` |

The wedge product, antiwedge product, and geometric product form a unified mathematical language that naturally expresses direction, area, volume, intersection, union, projection, rejection, reflection, and rotation — all from a handful of simple algebraic rules.

---

## Quick Reference: Key Formulas

### Wedge Product (3D vectors)

```
a ∧ b = (aᵧb_z − a_zbᵧ) e₂₃ + (a_zbₓ − aₓb_z) e₃₁ + (aₓbᵧ − aᵧbₓ) e₁₂
```

### Triple Wedge (3D)

```
a ∧ b ∧ c = det[a | b | c] × e₁₂₃
```

### 4D Wedge of Two Points → Line

```
p ∧ q = (q − p) as e₄ᵢ components + (p × q) as eⱼₖ components
```

### Geometric Product of Two Vectors

```
ab = a · b + a ∧ b
```

### Rotor for Rotation by θ about Unit Axis n

```
R = cos(θ/2) − sin(θ/2) × n̄
```

### Grade Count in n Dimensions

```
Grade-k components = C(n, k),    Total elements = 2ⁿ
```
