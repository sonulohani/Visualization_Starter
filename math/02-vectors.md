# Chapter 2: Vectors

> *"A vector tells you how far to go and in which direction — nothing more, nothing less."*

---

## 1. What is a Vector?

A **vector** is a quantity that has both a **magnitude** (length) and a **direction**.

### Real-World Analogy

Saying *"walk 5 meters"* is incomplete — which direction? That's a **scalar** (just a
number).

Saying *"walk 5 meters north-east"* is a **vector** — it has both size (5 meters) and
direction (north-east).

### Vector vs Scalar vs Point

| Concept  | Has magnitude? | Has direction? | Has position? | Example                |
|----------|---------------|----------------|---------------|------------------------|
| Scalar   | Yes           | No             | No            | Temperature: 72°       |
| Vector   | Yes           | Yes            | No            | Velocity: 5 m/s east   |
| Point    | No*           | No             | Yes           | Location: (3, 4)       |

> *A point doesn't have an inherent magnitude — it's just a location. However, you can
> **interpret** a point as a vector from the origin to that point (called a **position
> vector**).

### Visually

A vector is drawn as an **arrow**:

```
          head (tip)
           \
            \
             \
              \
               * -----> direction
              /
             / length (magnitude)
            /
           /
     tail (start)
```

The arrow can be placed anywhere — vectors don't have a fixed position. A velocity of
"5 units right" is the same vector whether it starts at the origin or at (100, 200).

---

## 2. Vector Notation

### 2D Vector

\[
\vec{v} = (v_x, \, v_y) = (3, \, 4)
\]

### 3D Vector

\[
\vec{v} = (v_x, \, v_y, \, v_z) = (1, \, 2, \, -3)
\]

### Column vs Row Notation

Mathematically, a vector can be written as a **column** or **row**:

**Column vector** (most common in graphics math):

\[
\vec{v} = \begin{bmatrix} 3 \\ 4 \\ 0 \end{bmatrix}
\]

**Row vector**:

\[
\vec{v} = \begin{bmatrix} 3 & 4 & 0 \end{bmatrix}
\]

This distinction matters when multiplying with matrices:
- **Column vectors** are multiplied as \( M \cdot \vec{v} \) (matrix on the left)
- **Row vectors** are multiplied as \( \vec{v} \cdot M \) (matrix on the right)

OpenGL and most math textbooks use column vectors. DirectX historically uses row vectors.

---

## 3. Vector Components

Any vector can be decomposed into its **components** along each axis:

```
          Y
          ^
          |   * v = (3, 4)
          |  /|
          | / |
      4   |/  | <- Y component = 4
          +---+-------> X
            3
          X component = 3
```

The vector `(3, 4)` means: "go 3 units in the X direction and 4 units in the Y direction."

In 3D, a vector `(2, 5, -1)` means:
- 2 units along X
- 5 units along Y
- −1 units along Z (backwards along Z)

---

## 4. Magnitude (Length)

The **magnitude** (or **length** or **norm**) of a vector tells you "how long is this
arrow?"

### Formula

**2D:**

\[
|\vec{v}| = \sqrt{v_x^2 + v_y^2}
\]

**3D:**

\[
|\vec{v}| = \sqrt{v_x^2 + v_y^2 + v_z^2}
\]

This comes straight from the **Pythagorean theorem**.

```
          |  /|
          | / |
   |v|    |/  | v_y
          +---+
           v_x

   |v|² = v_x² + v_y²    (Pythagorean theorem)
```

### Example: Distance Between Player and Enemy

If the player is at `P = (2, 0, 3)` and the enemy is at `E = (7, 0, 1)`:

```
// Vector from player to enemy
toEnemy = E - P = (7 - 2, 0 - 0, 1 - 3) = (5, 0, -2)

// Distance is the magnitude of that vector
distance = sqrt(5² + 0² + (-2)²)
         = sqrt(25 + 0 + 4)
         = sqrt(29)
         ≈ 5.39
```

**Performance tip:** If you only need to *compare* distances (e.g., "is the enemy within
range?"), skip the `sqrt` and compare **squared** distances instead:

```
distanceSq = toEnemy.x² + toEnemy.y² + toEnemy.z²

if distanceSq < attackRange²:
    attack()
```

This avoids an expensive square-root operation every frame.

---

## 5. Unit Vectors (Normalization)

A **unit vector** is a vector with magnitude exactly **1**. It represents a pure direction
with no length information.

### How to Normalize

Divide each component by the magnitude:

\[
\hat{v} = \frac{\vec{v}}{|\vec{v}|} = \left( \frac{v_x}{|\vec{v}|}, \, \frac{v_y}{|\vec{v}|}, \, \frac{v_z}{|\vec{v}|} \right)
\]

The resulting vector \( \hat{v} \) (read "v hat") points in the same direction as \( \vec{v} \)
but has a length of 1.

### Example: Enemy AI Chase Direction

An enemy at `E = (10, 0, 5)` wants to chase the player at `P = (3, 0, 2)`:

```
// Direction from enemy to player
toPlayer = P - E = (3 - 10, 0 - 0, 2 - 5) = (-7, 0, -3)

// Normalize to get a unit direction vector
length = sqrt((-7)² + 0² + (-3)²) = sqrt(49 + 9) = sqrt(58) ≈ 7.62

direction = (-7 / 7.62, 0 / 7.62, -3 / 7.62) ≈ (-0.92, 0, -0.39)

// Move enemy toward player at constant speed
enemy.position = enemy.position + direction * speed * deltaTime
```

Without normalization, the enemy would move **faster** when farther away and **slower** when
closer (because the un-normalized vector is longer when the distance is greater). Normalizing
decouples **direction** from **distance**.

### Why Unit Vectors Matter

- They let you control speed separately from direction.
- Dot products with unit vectors give clean cosine values.
- They're required inputs for many formulas (lighting, reflections, etc.).

---

## 6. Vector Addition and Subtraction

### Addition (Head-to-Tail)

Place the **tail** of the second vector at the **head** of the first. The result goes from
the first tail to the second head.

```
Component-wise:   (a_x + b_x, a_y + b_y, a_z + b_z)
```

```
           b
          /
     a   /
    ---> * ---->
         |      result = a + b
         |     /
         |    /
         |   /
         |  /
         | /
         *----------->
         a + b (head-to-tail)
```

**Properties:**
- Commutative: \( \vec{a} + \vec{b} = \vec{b} + \vec{a} \)
- Associative: \( (\vec{a} + \vec{b}) + \vec{c} = \vec{a} + (\vec{b} + \vec{c}) \)

### Subtraction

\[
\vec{a} - \vec{b} = \vec{a} + (-\vec{b})
\]

Geometrically, \( \vec{b} - \vec{a} \) gives you the vector **from A to B**.

```
    A *-----------* B

    vector from A to B = B - A
```

### Example: Applying Velocity to Position

The most fundamental game-loop equation:

```
// Every frame:
newPosition = position + velocity * deltaTime
```

If `position = (100, 200)`, `velocity = (5, -3)`, and `deltaTime = 0.016` (60 FPS):

```
displacement = velocity * deltaTime = (5 * 0.016, -3 * 0.016) = (0.08, -0.048)
newPosition  = (100 + 0.08, 200 + (-0.048)) = (100.08, 199.952)
```

### Example: Combining Forces

A spaceship has thrust `(0, 10)` and gravity `(0, -9.8)`:

```
totalForce = thrust + gravity = (0 + 0, 10 + (-9.8)) = (0, 0.2)
```

The ship slowly rises because thrust barely overcomes gravity.

---

## 7. Scalar Multiplication

Multiplying a vector by a scalar **scales** its length without changing its direction
(unless the scalar is negative, which reverses direction).

\[
k \cdot \vec{v} = (k \cdot v_x, \, k \cdot v_y, \, k \cdot v_z)
\]

```
v      = (2, 1)       ---->
2 * v  = (4, 2)       -------->        (twice as long)
-1 * v = (-2, -1)     <----            (same length, opposite direction)
0.5* v = (1, 0.5)     -->              (half as long)
```

**Game use case:** Speeding up or slowing down movement:

```
speed = 5.0
direction = normalize(target - self.position)
velocity = direction * speed     // unit direction scaled by speed
```

---

## 8. Dot Product

The **dot product** (or **scalar product**) takes two vectors and returns a **single
number** (a scalar).

### Formula

\[
\vec{a} \cdot \vec{b} = a_x b_x + a_y b_y + a_z b_z
\]

### Geometric Meaning

\[
\vec{a} \cdot \vec{b} = |\vec{a}| \, |\vec{b}| \, \cos(\theta)
\]

where \( \theta \) is the angle between the two vectors.

If both vectors are **unit vectors**, this simplifies to:

\[
\hat{a} \cdot \hat{b} = \cos(\theta)
\]

### What the Sign Tells You

```
    dot > 0          dot = 0           dot < 0
   (acute)        (perpendicular)     (obtuse)

      b               b                  b
     /                |                   \
    / θ < 90°         | θ = 90°    θ > 90° \
   a                  a                     a
```

| Dot Product Value | Angle Between Vectors     | Meaning                  |
|-------------------|---------------------------|--------------------------|
| 1                 | 0° (same direction)       | Parallel, same way       |
| > 0               | 0° – 90°                  | Mostly same direction    |
| 0                 | 90°                       | Perpendicular            |
| < 0               | 90° – 180°                | Mostly opposite          |
| −1                | 180° (opposite direction) | Parallel, opposite way   |

### Properties

- Commutative: \( \vec{a} \cdot \vec{b} = \vec{b} \cdot \vec{a} \)
- Distributive: \( \vec{a} \cdot (\vec{b} + \vec{c}) = \vec{a} \cdot \vec{b} + \vec{a} \cdot \vec{c} \)
- \( \vec{v} \cdot \vec{v} = |\vec{v}|^2 \) (handy for getting squared length)

### Example: Is the Enemy in Front of or Behind the Player?

```
// Player's forward direction (unit vector)
forward = player.forwardDirection    // e.g., (0, 0, 1)

// Direction from player to enemy
toEnemy = normalize(enemy.position - player.position)

// Dot product tells us the angular relationship
dotResult = dot(forward, toEnemy)

if dotResult > 0:
    print("Enemy is in FRONT of the player")
elif dotResult < 0:
    print("Enemy is BEHIND the player")
else:
    print("Enemy is exactly to the side")
```

```
        Enemy
          *
         /
        / toEnemy
       /
      Player ----> forward

      dot(forward, toEnemy) > 0, so enemy is "in front"
```

### Example: Diffuse Lighting (Lambert's Law)

How brightly a surface is lit depends on the angle between the surface **normal** and the
**light direction**:

```
// N = surface normal (unit vector, pointing away from surface)
// L = direction TO the light (unit vector)

brightness = max(dot(N, L), 0.0)

// Result:
//   1.0 = surface directly facing the light
//   0.0 = surface edge-on or facing away
```

```
       Light
        \  L
         \
          \
    -------*------  Surface
           |
           | N (normal)
           |
```

---

## 9. Cross Product

The **cross product** takes two 3D vectors and returns a **new vector** that is
**perpendicular** to both inputs.

### Formula

\[
\vec{a} \times \vec{b} = \begin{pmatrix}
a_y b_z - a_z b_y \\
a_z b_x - a_x b_z \\
a_x b_y - a_y b_x
\end{pmatrix}
\]

A useful mnemonic — each component leaves out its own row and "cross-multiplies" the other
two:

```
x component: (a_y * b_z) - (a_z * b_y)     // uses y,z of a and b
y component: (a_z * b_x) - (a_x * b_z)     // uses z,x of a and b
z component: (a_x * b_y) - (a_y * b_x)     // uses x,y of a and b
```

### Geometric Meaning

```
          a × b
           ^
           |
           |
           |
     a ----+----> b
          /
         / (a and b define a plane;
        /   a × b is perpendicular to it)
```

- The result is **perpendicular** to both \( \vec{a} \) and \( \vec{b} \).
- The **direction** follows the **right-hand rule**: curl fingers from \( \vec{a} \) toward
  \( \vec{b} \), and your thumb points in the direction of \( \vec{a} \times \vec{b} \).
- The **magnitude** equals the area of the parallelogram formed by \( \vec{a} \) and
  \( \vec{b} \):

\[
|\vec{a} \times \vec{b}| = |\vec{a}| \, |\vec{b}| \, \sin(\theta)
\]

### Properties

- **NOT commutative**: \( \vec{a} \times \vec{b} = -(\vec{b} \times \vec{a}) \)
  (reversing the order flips the direction)
- \( \vec{a} \times \vec{a} = \vec{0} \) (a vector crossed with itself is zero)
- Only defined in 3D (though a 2D analog exists — see below)

### Example: Computing a Surface Normal

Given a triangle with vertices `A`, `B`, `C`, find the surface normal:

```
edge1 = B - A
edge2 = C - A

normal = normalize(cross(edge1, edge2))
```

```
        C
       /|
      / |
     /  |  edge2 = C - A
    /   |
   A----B
     edge1 = B - A

   normal = cross(edge1, edge2), pointing out of the screen
```

This is how mesh normals are computed for lighting calculations.

### Example: 2D "Cross Product" — Left or Right Turn?

In 2D, the cross product reduces to a single scalar (the Z component):

\[
\vec{a} \times \vec{b} = a_x b_y - a_y b_x
\]

This tells you the **winding direction**:

```
result > 0   →   b is to the LEFT of a  (counter-clockwise turn)
result = 0   →   a and b are parallel (collinear)
result < 0   →   b is to the RIGHT of a (clockwise turn)
```

**Game use case:** Determining if a character should turn left or right to face a target:

```
forward   = player.forwardDirection2D    // e.g., (1, 0)
toTarget  = normalize(target.pos - player.pos)

crossResult = forward.x * toTarget.y - forward.y * toTarget.x

if crossResult > 0:
    turnLeft()
elif crossResult < 0:
    turnRight()
else:
    // already facing the target (or facing directly away)
```

---

## 10. Vector Projection

**Projecting** vector \( \vec{a} \) onto vector \( \vec{b} \) gives you the component of
\( \vec{a} \) that lies along \( \vec{b} \).

### Formula

\[
\text{proj}_{\vec{b}} \, \vec{a} = \frac{\vec{a} \cdot \vec{b}}{|\vec{b}|^2} \, \vec{b}
\]

If \( \vec{b} \) is already a unit vector (\( |\hat{b}| = 1 \)):

\[
\text{proj}_{\hat{b}} \, \vec{a} = (\vec{a} \cdot \hat{b}) \, \hat{b}
\]

```
              a
             /|
            / |
           /  |  (perpendicular component)
          /   |
    -----*----+-------> b
         |<-->|
      projection of a onto b
```

### Example: Sliding a Character Along a Wall

When a character tries to walk into a wall, you don't want them to stop dead — you want them
to **slide** along it.

```
        wall normal N
             ^
             |
    Wall ----+----
             |
         --> |
     velocity v  (player tries to walk into the wall)
```

Remove the component of velocity that goes into the wall (the part along the normal):

```
// N = wall normal (unit vector, pointing away from the wall surface)
// v = character's desired velocity

// Component of velocity going into the wall
velocityIntoWall = dot(v, N) * N

// Remove it to get the slide velocity
slideVelocity = v - velocityIntoWall
```

The character now moves parallel to the wall instead of getting stuck.

---

## 11. Reflection

**Reflecting** a vector off a surface means bouncing it. The incoming vector's component
along the surface normal is reversed, while the component along the surface is preserved.

### Formula

\[
\vec{r} = \vec{v} - 2 (\vec{v} \cdot \hat{n}) \, \hat{n}
\]

where:
- \( \vec{v} \) = incoming vector
- \( \hat{n} \) = surface normal (unit vector)
- \( \vec{r} \) = reflected vector

```
        v (incoming)         r (reflected)
         \                  /
          \   |n           /
           \  |           /
            \ |          /
             \|         /
    ----------*--------*------  Surface
              |
              | n (normal)
```

### Example: Bouncing a Ball Off a Wall

```
// Ball hits a wall with normal N
// v = ball's current velocity

dotVN = dot(v, N)

if dotVN < 0:   // ball is moving TOWARD the wall
    reflected = v - 2 * dotVN * N
    ball.velocity = reflected * bounciness   // bounciness in [0, 1]
```

Step-by-step with numbers:

```
v = (3, -4)          // ball moving right and down
N = (0, 1)           // floor normal pointing up

dot(v, N) = 3*0 + (-4)*1 = -4

reflected = (3, -4) - 2*(-4)*(0, 1)
          = (3, -4) - (0, -8)
          = (3, -4 + 8)
          = (3, 4)                // now moving right and UP
```

The ball keeps its horizontal velocity but its vertical velocity is flipped — exactly what
bouncing looks like.

---

## 12. Linear Independence and Basis Vectors

### Basis Vectors

In 3D, the **standard basis vectors** are:

\[
\hat{i} = (1, 0, 0) \quad \hat{j} = (0, 1, 0) \quad \hat{k} = (0, 0, 1)
\]

Any vector can be written as a **linear combination** of basis vectors:

\[
\vec{v} = v_x \hat{i} + v_y \hat{j} + v_z \hat{k}
\]

For example, \( (3, 4, -2) = 3\hat{i} + 4\hat{j} - 2\hat{k} \).

### Linear Independence

Vectors are **linearly independent** if none of them can be written as a combination of the
others.

- `(1, 0)` and `(0, 1)` are linearly independent — you can't make one from the other.
- `(1, 0)` and `(2, 0)` are **not** independent — the second is just 2× the first.

**Why it matters:** You need *n* linearly independent vectors to form a basis for *n*-dimensional
space. If your basis vectors are dependent, you've "lost a dimension" — you can't represent
all positions.

In games, this shows up when building coordinate frames (e.g., constructing a camera's
right/up/forward vectors). If two of those accidentally become parallel, the camera's
orientation breaks.

### Orthonormal Basis

A basis where all vectors are:
- **Unit length** (normalized)
- **Perpendicular** to each other (orthogonal)

The standard basis \( \hat{i}, \hat{j}, \hat{k} \) is orthonormal. Rotation matrices
always produce orthonormal bases. This is important for numerical stability.

---

## 13. Common Pitfalls

### Forgetting to Normalize

If you use an un-normalized vector where a unit vector is expected, every formula that
depends on the magnitude being 1 will produce wrong results.

```
// BAD: direction is not normalized, speed depends on distance
direction = target.pos - self.pos
self.pos = self.pos + direction * speed * dt
// Object moves FAST when far, SLOW when close!

// GOOD: normalize first
direction = normalize(target.pos - self.pos)
self.pos = self.pos + direction * speed * dt
// Consistent speed regardless of distance
```

### Dividing by Zero Magnitude

If two objects are at the exact same position, the vector between them has magnitude 0.
Normalizing it divides by zero.

```
direction = target.pos - self.pos
length = magnitude(direction)

if length > 0.0001:    // small epsilon to avoid division by zero
    direction = direction / length
else:
    direction = (0, 0, 0)   // or pick a default direction
```

### Mixing Up Points and Vectors

A point represents a **location**. A vector represents a **displacement** or **direction**.

- **Point − Point = Vector** (the displacement from one location to another)
- **Point + Vector = Point** (moving a location by a displacement)
- **Point + Point = ???** (undefined — adding two locations makes no geometric sense)

```
playerPos = (5, 0, 3)       // Point
enemyPos  = (10, 0, 7)      // Point
toEnemy   = enemyPos - playerPos  // Vector (5, 0, 4)

// This is valid:
newPos = playerPos + toEnemy    // Point: (10, 0, 7)

// This is NOT meaningful:
nonsense = playerPos + enemyPos  // (15, 0, 10)? What does that even mean?
```

### Confusing Dot and Cross Product

- **Dot product** → returns a **scalar** (a number). Use for angles, projections, lighting.
- **Cross product** → returns a **vector** (perpendicular). Use for normals, winding, torque.

### Using atan() Instead of atan2()

`atan(y/x)` only returns values in \((-\pi/2, \pi/2)\) and explodes when `x = 0`.
Always use `atan2(y, x)` which handles all four quadrants correctly.

---

## Summary: Quick Reference

| Operation                 | Formula                                                          | Returns   |
|---------------------------|------------------------------------------------------------------|-----------|
| Magnitude                 | \(\|\vec{v}\| = \sqrt{x^2+y^2+z^2}\)                            | Scalar    |
| Normalize                 | \(\hat{v} = \vec{v} / \|\vec{v}\|\)                             | Vector    |
| Addition                  | \((a_x+b_x, \, a_y+b_y, \, a_z+b_z)\)                          | Vector    |
| Subtraction               | \((a_x-b_x, \, a_y-b_y, \, a_z-b_z)\)                          | Vector    |
| Scalar multiply           | \((k \cdot v_x, \, k \cdot v_y, \, k \cdot v_z)\)              | Vector    |
| Dot product               | \(a_x b_x + a_y b_y + a_z b_z\)                                | Scalar    |
| Cross product             | \((a_yb_z - a_zb_y, \; a_zb_x - a_xb_z, \; a_xb_y - a_yb_x)\)| Vector    |
| Projection of a onto b    | \(\frac{\vec{a}\cdot\vec{b}}{\|\vec{b}\|^2}\vec{b}\)            | Vector    |
| Reflection                | \(\vec{v} - 2(\vec{v}\cdot\hat{n})\hat{n}\)                     | Vector    |
| Angle between (unit vecs) | \(\theta = \arccos(\hat{a}\cdot\hat{b})\)                       | Scalar    |

---

## Cheat Sheet: "When Do I Use What?"

| I want to...                                | Use            |
|---------------------------------------------|----------------|
| Find distance between two points            | Magnitude of (B − A) |
| Get a direction for movement / AI           | Normalize(B − A) |
| Check if something is in front / behind     | Dot product    |
| Calculate lighting intensity on a surface   | Dot product    |
| Find a surface normal                       | Cross product  |
| Determine left/right turn in 2D             | 2D cross product |
| Slide along a wall                          | Projection (then subtract) |
| Bounce off a surface                        | Reflection     |
| Apply velocity to position                  | Vector addition |
| Speed up or slow down movement              | Scalar multiplication |

---

*Next up: [Chapter 3 — Matrices](03-matrices.md), the powerful tool that lets us rotate,
scale, translate, and project vectors in a single operation.*
