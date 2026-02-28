# Chapter 7: Trigonometry for Games

Trigonometry is the branch of mathematics that studies relationships between the sides
and angles of triangles. In game development, it is the engine behind rotation, aiming,
orbits, wave motion, procedural animation, and much more. If vectors are the skeleton
of game math, trigonometry is the muscle that makes things move in curves and circles.

---

## 7.1 Why Trigonometry Matters in Games

Almost every system in a game touches trigonometry at some point:

| System             | How Trig Is Used                                      |
|--------------------|-------------------------------------------------------|
| **Rotation**       | Spinning sprites, turning characters, aiming weapons  |
| **Circular motion**| Orbiting planets, rotating turrets, radar sweeps      |
| **Waves**          | Water surfaces, bobbing pickups, breathing animations |
| **Oscillation**    | Pendulums, swinging platforms, pulsing UI elements    |
| **AI**             | Finding angle to target, field-of-view cone checks    |
| **Projectiles**    | Launch angles, arc trajectories                       |
| **Camera**         | Orbit cameras, look-at logic, smooth rotation         |
| **Audio**          | Sound wave generation, Doppler calculations           |

Every time something rotates, oscillates, or follows a curved path, sine and cosine
are doing the heavy lifting behind the scenes.

---

## 7.2 Angles: Degrees vs Radians

### Degrees — The Human-Friendly Unit

We grow up thinking in degrees:

- A right angle is **90°**
- A half turn is **180°**
- A full circle is **360°**

Degrees are great for level editors and UI sliders because they feel intuitive.

### Radians — The Math-Friendly Unit

Radians measure angles by the arc length on a unit circle (radius = 1):

- A full circle has circumference \(2\pi r = 2\pi\), so a full turn = **2π rad ≈ 6.2832 rad**
- A half turn = **π rad ≈ 3.1416 rad**
- A right angle = **π/2 rad ≈ 1.5708 rad**

One radian is the angle where the arc length equals the radius:

```
        arc length = r
       ___---___
      /    r    \
     /  ↗        |
    | / 1 rad    |
    |/           |
    *────────────
         r
```

### Why Radians Are Used Internally

Every math library — `sin()`, `cos()`, `tan()`, `atan2()` — expects radians. This is
because the calculus of trig functions is clean in radians:

- \(\frac{d}{d\theta}\sin(\theta) = \cos(\theta)\) — only true in radians
- Taylor series for sin: \(\sin(\theta) \approx \theta - \frac{\theta^3}{6} + \frac{\theta^5}{120} - \ldots\)

In degrees, every derivative would need an ugly \(\pi/180\) factor.

### Conversion Formulas

```
radians = degrees × (π / 180)
degrees = radians × (180 / π)
```

Pseudocode:

```
function degreesToRadians(deg):
    return deg * PI / 180.0

function radiansToDegrees(rad):
    return rad * 180.0 / PI
```

### Quick Reference

| Degrees | Radians    | Approximate |
|---------|------------|-------------|
| 0°      | 0          | 0.000       |
| 30°     | π/6        | 0.524       |
| 45°     | π/4        | 0.785       |
| 60°     | π/3        | 1.047       |
| 90°     | π/2        | 1.571       |
| 120°    | 2π/3       | 2.094       |
| 180°    | π          | 3.142       |
| 270°    | 3π/2       | 4.712       |
| 360°    | 2π         | 6.283       |

**Rule of thumb**: store and compute in radians, display in degrees.

---

## 7.3 The Unit Circle

The unit circle is a circle of radius 1 centered at the origin. It is the Rosetta
Stone of trigonometry: every angle maps to a point on the circle, and the coordinates
of that point give you cosine and sine directly.

```
                        90° (π/2)
                     (0, 1)
                        |
                        |
           120°  .------+------.  60°
         (-0.5,  /      |       \  (0.5,
          0.87) /       |        \  0.87)
               /        |         \
     150°     /         |          \     30°
   (-0.87,   |          |           |  (0.87,
     0.5)    |          + (0,0)     |    0.5)
              |         |           |
  180° ------+─────────+───────────+------ 0° / 360°
  (-1, 0)    |         |           |      (1, 0)
              |         |           |
     210°    |          |           |    330°
   (-0.87,   |          |           |  (0.87,
    -0.5)     \         |          /   -0.5)
               \        |         /
     240°       \       |        /     300°
    (-0.5,       '------+------'     (0.5,
     -0.87)             |           -0.87)
                        |
                     (0, -1)
                      270° (3π/2)
```

### The Fundamental Relationship

For any angle \(\theta\), the point on the unit circle is:

```
x = cos(θ)
y = sin(θ)
```

This means:
- **cos(θ)** tells you how far **right** (or left, if negative) the point is
- **sin(θ)** tells you how far **up** (or down, if negative) the point is

### Key Values Table

| Angle (°) | Angle (rad) | cos(θ) | sin(θ) | tan(θ)    |
|------------|-------------|--------|--------|-----------|
| 0°         | 0           | 1      | 0      | 0         |
| 30°        | π/6         | √3/2   | 1/2    | 1/√3      |
| 45°        | π/4         | √2/2   | √2/2   | 1         |
| 60°        | π/3         | 1/2    | √3/2   | √3        |
| 90°        | π/2         | 0      | 1      | undefined |
| 180°       | π           | -1     | 0      | 0         |
| 270°       | 3π/2        | 0      | -1     | undefined |
| 360°       | 2π          | 1      | 0      | 0         |

### Quadrant Signs

```
          │
   II     │     I
 cos < 0  │  cos > 0
 sin > 0  │  sin > 0
          │
 ─────────┼─────────
          │
   III    │    IV
 cos < 0  │  cos > 0
 sin < 0  │  sin < 0
          │
```

Mnemonic: **A**ll **S**tudents **T**ake **C**alculus — starting from quadrant I and going
counterclockwise: All positive, Sine positive, Tangent positive, Cosine positive.

---

## 7.4 Sine, Cosine, and Tangent

### SOH-CAH-TOA

In a right triangle:

```
        /|
       / |
  hyp /  | opposite
     /   |
    / θ  |
   /_____|
   adjacent
```

- **SOH**: \(\sin(\theta) = \frac{\text{opposite}}{\text{hypotenuse}}\)
- **CAH**: \(\cos(\theta) = \frac{\text{adjacent}}{\text{hypotenuse}}\)
- **TOA**: \(\tan(\theta) = \frac{\text{opposite}}{\text{adjacent}} = \frac{\sin(\theta)}{\cos(\theta)}\)

### Graphs of Sine and Cosine

Both sine and cosine are periodic waves with period \(2\pi\) and amplitude 1.

**Sine wave** — starts at 0, peaks at π/2:

```
 1 |        *  *
   |      *      *
   |    *          *
   |  *              *
 0 |*--------+--------*--------+--------*---→ θ
   |         π         *      2π       *
   |                     *          *
   |                       *      *
-1 |                         *  *
```

**Cosine wave** — starts at 1, drops to 0 at π/2:

```
 1 |*  *
   |      *                               *
   |        *                           *
   |          *                       *
 0 |----+------*--------+--------*------+---→ θ
   |    π/2     *      3π/2      *
   |              *            *
   |                *        *
-1 |                  *  *  *
```

### Phase Relationship

Cosine is sine shifted left by π/2:

```
cos(θ) = sin(θ + π/2)
sin(θ) = cos(θ - π/2)
```

In games, this means if you use `cos` for the x-axis and `sin` for the y-axis
when doing circular motion, the object naturally starts on the right side (at angle 0)
and moves counterclockwise.

### Tangent

\(\tan(\theta) = \frac{\sin(\theta)}{\cos(\theta)}\)

Tangent represents the slope of the line from the origin to the point on the unit circle.

**Why it "blows up"**: At 90° and 270°, \(\cos(\theta) = 0\), so you're dividing by
zero. The tangent shoots off to ±infinity:

```
     |          :     |          :
     |          :     |          :
     |        / :     |        / :
     |      /   :     |      /   :
     |    /     :     |    /     :
     |  /       :     |  /       :
─────+/─────────:─────+/─────────:────→ θ
   / |          :   / |          :
 /   |     90°  : /   |    270°  :
     |          :     |          :
```

In game code, avoid raw `tan()` when possible; prefer `atan2()` which handles the
edge cases for you.

---

## 7.5 Inverse Trig Functions

The inverse trig functions go backward: given a ratio, they return an angle.

| Function  | Input    | Output Range              | Purpose                              |
|-----------|----------|---------------------------|--------------------------------------|
| asin(x)   | [-1, 1]  | [-π/2, π/2]              | Angle whose sine is x               |
| acos(x)   | [-1, 1]  | [0, π]                   | Angle whose cosine is x             |
| atan(x)   | (-∞, ∞)  | (-π/2, π/2)              | Angle whose tangent is x            |
| atan2(y,x)| any y,x  | (-π, π] or [-π, π]       | Angle of the vector (x, y)          |

### Why atan2 Is King in Game Dev

`atan(y/x)` has two fatal flaws:
1. It can't distinguish between opposite directions (e.g., (1,1) and (-1,-1) both give y/x = 1)
2. It crashes when x = 0

`atan2(y, x)` fixes both. It takes y and x as separate arguments and returns the
correct angle in all four quadrants:

```
              atan2 output:

              π/2 (90°)
               |
       π (180°)|         0° (0)
    ───────────+───────────
      -π (-180°)|        -0 (or 2π)
               |
             -π/2 (-90°)
```

### Example: Angle from Player to Enemy

```
function getAngleToTarget(self, target):
    dx = target.x - self.x
    dy = target.y - self.y
    angle = atan2(dy, dx)    // returns radians, handles all quadrants
    return angle

// Usage: rotate a turret to face an enemy
turretAngle = getAngleToTarget(turret, enemy)
```

**Note the argument order**: most languages use `atan2(y, x)` — y first, x second.
Getting this backwards is one of the most common trig bugs in game code.

### Practical Example: Is the Enemy in My Field of View?

```
function isInFieldOfView(self, target, facingAngle, fovDegrees):
    angleToTarget = atan2(target.y - self.y, target.x - self.x)
    diff = angleToTarget - facingAngle

    // Normalize to [-π, π]
    while diff > PI:  diff -= 2 * PI
    while diff < -PI: diff += 2 * PI

    halfFOV = degreesToRadians(fovDegrees / 2)
    return abs(diff) <= halfFOV
```

---

## 7.6 Practical Examples

### 7.6.1 Circular Motion

To move an object in a circle, parameterize by angle:

```
x(t) = centerX + radius * cos(t)
y(t) = centerY + radius * sin(t)
```

As `t` increases from 0 to 2π, the object traces a full circle.

```
         *  *  *
       *    ↑    *
      *     |r    *
     *      |      *
     *   center    *
      *     ←t    *
       *         *
         *  *  *
```

**Example: Orbiting Satellite**

```
function updateSatellite(satellite, planet, orbitRadius, orbitSpeed, deltaTime):
    satellite.orbitAngle += orbitSpeed * deltaTime

    satellite.x = planet.x + orbitRadius * cos(satellite.orbitAngle)
    satellite.y = planet.y + orbitRadius * sin(satellite.orbitAngle)
```

**Example: Rotating Turret with Barrel Tip**

```
function getTurretBarrelTip(turret, barrelLength):
    tipX = turret.x + barrelLength * cos(turret.angle)
    tipY = turret.y + barrelLength * sin(turret.angle)
    return (tipX, tipY)

// Spawn bullet at the barrel tip, traveling in the turret's direction
function fireBullet(turret, barrelLength, bulletSpeed):
    tip = getTurretBarrelTip(turret, barrelLength)
    bullet.x = tip.x
    bullet.y = tip.y
    bullet.vx = bulletSpeed * cos(turret.angle)
    bullet.vy = bulletSpeed * sin(turret.angle)
```

### 7.6.2 Sine Waves for Animation

The general sine wave formula:

```
value = amplitude * sin(frequency * time + phase) + offset
```

- **amplitude** — how far it swings from center
- **frequency** — how fast it oscillates (higher = faster)
- **phase** — shifts the wave left/right (offsets the starting point)
- **offset** — shifts the entire wave up/down

**Example: Bobbing Pickup Item**

```
function updatePickupBob(item, time):
    bobAmplitude = 0.3     // bob 0.3 units up/down
    bobFrequency = 2.0     // 2 full cycles per second (in radians: * 2π)
    item.y = item.baseY + bobAmplitude * sin(bobFrequency * 2 * PI * time)
```

**Example: Breathing/Pulsing Effect on UI**

```
function getBreathScale(time):
    minScale = 1.0
    breathAmount = 0.05    // 5% scale variation
    breathSpeed = 1.5
    return minScale + breathAmount * sin(breathSpeed * time)
```

**Example: Horizontal Wave Motion (like a snake)**

```
function updateSnakeSegment(segment, index, time):
    waveAmplitude = 10.0
    waveFrequency = 3.0
    phaseOffset = index * 0.5    // each segment is offset in phase
    segment.x = segment.baseX + waveAmplitude * sin(waveFrequency * time + phaseOffset)
```

### 7.6.3 Rotating a 2D Sprite to Face a Target

```
function faceTarget(self, target):
    dx = target.x - self.x
    dy = target.y - self.y
    self.rotation = atan2(dy, dx)

// If your engine uses degrees:
function faceTargetDegrees(self, target):
    dx = target.x - self.x
    dy = target.y - self.y
    self.rotation = radiansToDegrees(atan2(dy, dx))
```

**Smooth rotation** (don't snap instantly):

```
function smoothFaceTarget(self, target, turnSpeed, deltaTime):
    desiredAngle = atan2(target.y - self.y, target.x - self.x)
    diff = desiredAngle - self.rotation

    // Normalize to [-π, π] so we turn the short way
    while diff > PI:  diff -= 2 * PI
    while diff < -PI: diff += 2 * PI

    // Clamp the turn amount
    maxTurn = turnSpeed * deltaTime
    if diff > maxTurn:  diff = maxTurn
    if diff < -maxTurn: diff = -maxTurn

    self.rotation += diff
```

### 7.6.4 Pendulum Motion

A simple pendulum swings back and forth. For small angles, it behaves like a
sine wave:

```
angle(t) = maxAngle * sin(time * speed)
```

```
         pivot
          /|\
         / | \
        /  |  \      maxAngle
       /   |   \    ←─────────→
      /    |    \
     *     |     *   extreme positions
           |
           *         rest position
```

**Example: Swinging Lantern**

```
function updateSwingingLantern(lantern, time):
    maxSwing = degreesToRadians(30)   // swings 30° each way
    swingSpeed = 2.0
    currentAngle = maxSwing * sin(swingSpeed * time)

    // Position the lantern relative to its pivot
    ropeLength = 2.0
    lantern.x = lantern.pivotX + ropeLength * sin(currentAngle)
    lantern.y = lantern.pivotY + ropeLength * cos(currentAngle)
```

---

## 7.7 Trigonometric Identities

You don't need to memorize all of these, but knowing a few key identities can
simplify code and avoid redundant calculations.

### The Pythagorean Identity

\[
\sin^2(\theta) + \cos^2(\theta) = 1
\]

This is just the Pythagorean theorem on the unit circle. It's useful for:
- Deriving one function from the other: \(\cos(\theta) = \sqrt{1 - \sin^2(\theta)}\)
- Verifying normalized directions
- Optimizing shaders

### Angle Addition Formulas

\[
\sin(a + b) = \sin(a)\cos(b) + \cos(a)\sin(b)
\]
\[
\sin(a - b) = \sin(a)\cos(b) - \cos(a)\sin(b)
\]
\[
\cos(a + b) = \cos(a)\cos(b) - \sin(a)\sin(b)
\]
\[
\cos(a - b) = \cos(a)\cos(b) + \sin(a)\sin(b)
\]

**Game use**: These are the formulas behind 2D rotation. To rotate a point
\((x, y)\) by angle \(\theta\):

```
x' = x * cos(θ) - y * sin(θ)
y' = x * sin(θ) + y * cos(θ)
```

This is exactly the angle addition formula in disguise. It's the foundation of
every 2D rotation matrix.

### Double Angle Formulas

\[
\sin(2\theta) = 2\sin(\theta)\cos(\theta)
\]
\[
\cos(2\theta) = \cos^2(\theta) - \sin^2(\theta) = 2\cos^2(\theta) - 1 = 1 - 2\sin^2(\theta)
\]

**Game use**: The half-angle version \(\cos^2(\theta) = \frac{1 + \cos(2\theta)}{2}\)
appears in lighting calculations and shader tricks.

### Product-to-Sum (Occasionally Useful)

\[
\sin(a)\sin(b) = \frac{1}{2}[\cos(a-b) - \cos(a+b)]
\]

This shows up when combining multiple wave frequencies (audio, water simulation).

---

## 7.8 Law of Sines and Law of Cosines

These extend trigonometry to **non-right** triangles.

### Law of Sines

```
     A
    /\
   /  \
  b    c         a / sin(A) = b / sin(B) = c / sin(C)
 /    \
/______\
B   a   C
```

\[
\frac{a}{\sin(A)} = \frac{b}{\sin(B)} = \frac{c}{\sin(C)}
\]

**Use when**: You know two angles and one side (AAS), or two sides and an angle
opposite one of them (SSA — but watch for the ambiguous case).

### Law of Cosines

\[
c^2 = a^2 + b^2 - 2ab\cos(C)
\]

This is the generalized Pythagorean theorem. When C = 90°, \(\cos(90°) = 0\) and
it reduces to \(c^2 = a^2 + b^2\).

**Use when**: You know two sides and the included angle (SAS), or all three sides (SSS)
and want to find an angle.

### Example: Triangulation — Finding Distance to a Target

Imagine two observation towers A and B, distance `d` apart. Each tower measures
the angle to a target T:

```
         T (target)
        /|\
       / | \
      /  |  \
     / α |   \ β
    /    |    \
   A ----d---- B
```

Using the Law of Sines:

```
function triangulateDistance(d, angleA, angleB):
    angleT = PI - angleA - angleB    // angles of triangle sum to π
    // Distance from A to T:
    distAT = d * sin(angleB) / sin(angleT)
    // Distance from B to T:
    distBT = d * sin(angleA) / sin(angleT)
    return (distAT, distBT)
```

### Example: Finding the Angle of a Triangle Given Three Side Lengths

```
function angleBetweenSides(a, b, c):
    // Returns the angle opposite side c
    cosC = (a*a + b*b - c*c) / (2 * a * b)
    cosC = clamp(cosC, -1, 1)    // prevent floating-point errors in acos
    return acos(cosC)
```

---

## 7.9 Smoothstep and Trigonometric Easing

### Trigonometric Easing

Sine and cosine produce naturally smooth acceleration and deceleration curves.

**Ease-in (starts slow, ends fast)**:

```
function easeInSine(t):
    return 1 - cos(t * PI / 2)
```

**Ease-out (starts fast, ends slow)**:

```
function easeOutSine(t):
    return sin(t * PI / 2)
```

**Ease-in-out (slow at both ends)**:

```
function easeInOutSine(t):
    return 0.5 * (1 - cos(PI * t))
```

These are the smoothest possible easing functions because sine and cosine have
continuous derivatives of all orders.

### Visual Comparison (t goes from 0 to 1, output goes from 0 to 1)

```
 1 |                    ..*****
   |                ..**
   |             ..*          ← ease-in-out (S-curve)
   |          .**
   |   .*****
 0 +────────────────────────→ t
   0                         1

 1 |               .**********
   |           ..**
   |        .**               ← ease-out (fast then slow)
   |     .**
   |  .**
 0 +────────────────────────→ t

 1 |                      .***
   |                   .**
   |                .**       ← ease-in (slow then fast)
   |           ..**
   | **********
 0 +────────────────────────→ t
```

### Example: Smooth Door Opening

```
function updateDoor(door, deltaTime):
    if door.isOpening:
        door.t += deltaTime / door.openDuration
        if door.t > 1: door.t = 1

        eased = easeInOutSine(door.t)
        door.currentAngle = lerp(door.closedAngle, door.openAngle, eased)
```

---

## 7.10 Summary and Quick Reference

### Common Angle Values

| Degrees | Radians | sin    | cos    | tan       |
|---------|---------|--------|--------|-----------|
| 0°      | 0       | 0.000  | 1.000  | 0.000     |
| 30°     | π/6     | 0.500  | 0.866  | 0.577     |
| 45°     | π/4     | 0.707  | 0.707  | 1.000     |
| 60°     | π/3     | 0.866  | 0.500  | 1.732     |
| 90°     | π/2     | 1.000  | 0.000  | ±∞        |
| 120°    | 2π/3    | 0.866  | -0.500 | -1.732    |
| 135°    | 3π/4    | 0.707  | -0.707 | -1.000    |
| 150°    | 5π/6    | 0.500  | -0.866 | -0.577    |
| 180°    | π       | 0.000  | -1.000 | 0.000     |
| 270°    | 3π/2    | -1.000 | 0.000  | ±∞        |
| 360°    | 2π      | 0.000  | 1.000  | 0.000     |

### Quick Formulas Cheat Sheet

```
Conversion:       rad = deg × π/180          deg = rad × 180/π

Unit circle:      x = cos(θ)                 y = sin(θ)

Circular motion:  x = cx + r*cos(t)          y = cy + r*sin(t)

Angle to target:  angle = atan2(dy, dx)      — ALWAYS use atan2

2D rotation:      x' = x*cos(θ) - y*sin(θ)  y' = x*sin(θ) + y*cos(θ)

Sine wave:        val = amp * sin(freq * time + phase) + offset

Pythagorean ID:   sin²θ + cos²θ = 1

Law of Cosines:   c² = a² + b² - 2ab·cos(C)

Ease-in-out:      eased = 0.5 * (1 - cos(π * t))
```

### Key Takeaways

1. **Store angles in radians** — all math functions expect them.
2. **Display angles in degrees** — humans understand them better.
3. **Use atan2(y, x)** — never atan(y/x). Note the argument order: y first.
4. **cos gives x, sin gives y** — on the unit circle.
5. **Normalize angle differences** to [-π, π] before comparing or interpolating.
6. **Sine waves** are your go-to tool for any smooth oscillation.
7. **The angle addition formulas** are the foundation of 2D rotation matrices.

---

*Next chapter: [Chapter 8 — Interpolation and Curves](08-interpolation-and-curves.md)*
