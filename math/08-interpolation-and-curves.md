# Chapter 8: Interpolation and Curves

Interpolation is the art of finding values between known data points. In games, it
is everywhere: smooth camera movement, health bar animations, color gradients, spline
paths, terrain sampling, and transition effects. Mastering interpolation is the
difference between a game that feels snappy and polished and one that feels janky.

---

## 8.1 What Is Interpolation?

**Interpolation** means estimating a value that falls between two (or more) known values.

Think of it like a GPS: you know the position of two landmarks, and the GPS tells you
where you are between them based on how far you've traveled.

```
  Known A              You are here             Known B
    *─────────────────────●──────────────────────*
    t=0                  t=0.6                   t=1
```

The parameter **t** (often called the "interpolation factor") ranges from 0 to 1:
- t = 0 → you're at A
- t = 1 → you're at B
- t = 0.5 → you're exactly halfway
- t = 0.6 → you're 60% of the way from A to B

---

## 8.2 Linear Interpolation (Lerp)

The most fundamental interpolation: a straight line between two values.

### Formula

```
lerp(a, b, t) = a + t × (b - a)
```

Equivalently: `lerp(a, b, t) = (1 - t) × a + t × b`

Both forms are mathematically identical. The first is more common in code because
it uses one fewer multiplication.

### Geometric Interpretation

```
  a                                            b
  *────────────────────────────────────────────*
  |←──── t × (b-a) ────→|
  |                      |
  a                    result
```

You start at `a` and walk a fraction `t` of the total distance `(b - a)`.

### Pseudocode

```
function lerp(a, b, t):
    return a + t * (b - a)
```

### Example: Smoothly Moving a Health Bar

Instead of the health bar instantly jumping from 80 to 60, animate it:

```
function updateHealthBar(bar, deltaTime):
    speed = 3.0
    bar.displayHealth = lerp(bar.displayHealth, bar.actualHealth, speed * deltaTime)
```

Note: This is an exponential decay approach, not a linear slide. Each frame, the
displayed health moves a fraction closer to the actual health. The result is fast
movement when far away and slow movement when close — it feels natural.

### Example: Color Blending

```
function lerpColor(colorA, colorB, t):
    r = lerp(colorA.r, colorB.r, t)
    g = lerp(colorA.g, colorB.g, t)
    b = lerp(colorA.b, colorB.b, t)
    a = lerp(colorA.a, colorB.a, t)
    return Color(r, g, b, a)

// Blend from red to blue at 30%:
result = lerpColor(RED, BLUE, 0.3)   // mostly red, a bit of blue
```

### Lerp in 2D and 3D

Lerp each component independently:

```
function lerpVec2(a, b, t):
    return Vec2(
        lerp(a.x, b.x, t),
        lerp(a.y, b.y, t)
    )

function lerpVec3(a, b, t):
    return Vec3(
        lerp(a.x, b.x, t),
        lerp(a.y, b.y, t),
        lerp(a.z, b.z, t)
    )
```

### Important Caveat: Clamping t

In many situations, you want to ensure `t` stays in [0, 1]. Otherwise, you get
**extrapolation** — values outside the range of a and b. Sometimes that's useful;
often it's a bug.

---

## 8.3 Inverse Lerp

Inverse lerp answers the reverse question: "I have a value — where does it fall
between a and b?"

### Formula

```
inverseLerp(a, b, value) = (value - a) / (b - a)
```

- Returns 0 when value = a
- Returns 1 when value = b
- Returns 0.5 when value is midway between a and b
- Can return values outside [0,1] if value is outside [a,b]

### Example: Mapping Health to a Color Gradient

```
function getHealthBarColor(health, maxHealth):
    t = inverseLerp(0, maxHealth, health)   // 0.0 = dead, 1.0 = full
    return lerpColor(RED, GREEN, t)
```

### Diagram

```
  a=0         value=35         b=100
  *──────────────●─────────────────────────*
  |              |
  t = (35-0)/(100-0) = 0.35

  The value 35 is 35% of the way from 0 to 100.
```

---

## 8.4 Remap

Remap combines inverse lerp and lerp: take a value from one range and map it to another.

### Formula

```
function remap(value, inMin, inMax, outMin, outMax):
    t = inverseLerp(inMin, inMax, value)
    return lerp(outMin, outMax, t)

// Expanded:
// return outMin + (value - inMin) / (inMax - inMin) * (outMax - outMin)
```

### Example: Joystick Input to Rotation Speed

```
// Joystick gives values in [-1, 1]
// We want rotation speed in [-180, 180] degrees per second
rotationSpeed = remap(joystick.x, -1, 1, -180, 180)
```

### Example: Enemy Health to Behavior Aggression

```
// As health goes from 100 to 0, aggression goes from 0.2 to 1.0
aggression = remap(enemy.health, 100, 0, 0.2, 1.0)
```

---

## 8.5 Clamping

Clamping restricts a value to a given range.

```
function clamp(value, min, max):
    if value < min: return min
    if value > max: return max
    return value
```

Common variants:

```
function clamp01(value):
    return clamp(value, 0, 1)

function saturate(value):       // same as clamp01, common in shaders
    return clamp(value, 0, 1)
```

**Example**: Preventing health from going negative or exceeding max:

```
player.health = clamp(player.health - damage, 0, player.maxHealth)
```

---

## 8.6 Easing Functions

Linear interpolation moves at constant speed. Real objects don't do that — they
accelerate and decelerate. Easing functions transform the `t` parameter to create
more natural motion.

### The Concept

Instead of using `t` directly, you pass it through an easing function first:

```
easedT = easingFunction(t)
result = lerp(a, b, easedT)
```

### Ease-In (Starts Slow, Ends Fast)

The value accelerates. Power functions work well:

```
function easeInQuad(t):    return t * t
function easeInCubic(t):   return t * t * t
function easeInSine(t):    return 1 - cos(t * PI / 2)
```

```
 1 |                         .**
   |                      .**
   |                   .**
   |                .**
   |           ..**
   |     ...***
 0 |*****───────────────────────→ t
   0                            1
```

### Ease-Out (Starts Fast, Ends Slow)

The value decelerates:

```
function easeOutQuad(t):   return 1 - (1-t) * (1-t)
function easeOutCubic(t):  return 1 - pow(1-t, 3)
function easeOutSine(t):   return sin(t * PI / 2)
```

```
 1 |              ..*************
   |          ..**
   |       .**
   |     .**
   |   .**
   |  .*
 0 |**──────────────────────────→ t
   0                            1
```

### Ease-In-Out (Slow at Both Ends)

The classic S-curve:

```
function easeInOutQuad(t):
    if t < 0.5:
        return 2 * t * t
    else:
        return 1 - pow(-2 * t + 2, 2) / 2

function easeInOutCubic(t):
    if t < 0.5:
        return 4 * t * t * t
    else:
        return 1 - pow(-2 * t + 2, 3) / 2

function easeInOutSine(t):
    return 0.5 * (1 - cos(PI * t))
```

```
 1 |                     ..******
   |                  .**
   |               .**
   |             .*         ← S-curve
   |           *.
   |        **.
   |  ****..
 0 |**──────────────────────────→ t
   0                            1
```

### Example: A Door That Opens Smoothly

```
function updateDoor(door, deltaTime):
    if door.isOpening:
        door.t += deltaTime / door.openDuration
        if door.t >= 1.0:
            door.t = 1.0
            door.isOpening = false

        easedT = easeInOutCubic(door.t)
        door.angle = lerp(0, 90, easedT)     // 0° closed, 90° open
```

---

## 8.7 Smoothstep and Smootherstep

### Smoothstep

Smoothstep is a clamped, smooth ease-in-out function:

```
function smoothstep(edge0, edge1, x):
    t = clamp((x - edge0) / (edge1 - edge0), 0, 1)
    return t * t * (3 - 2 * t)
```

If you just need the [0,1] → [0,1] version:

```
function smoothstep01(t):
    t = clamp01(t)
    return t * t * (3 - 2 * t)
```

Properties:
- smoothstep(0) = 0
- smoothstep(1) = 1
- First derivative is 0 at both endpoints (smooth start and stop)

### Smootherstep (Ken Perlin's Improvement)

Smootherstep has zero first **and** second derivatives at the endpoints, making it
even smoother:

```
function smootherstep01(t):
    t = clamp01(t)
    return t * t * t * (t * (t * 6 - 15) + 10)
```

### Comparison

```
 1 |              ..***  <-- smootherstep (flattest at endpoints)
   |           .** ..**
   |         .*  .*
   |        .* .*        <-- smoothstep
   |       .* *
   |      * .*           <-- linear (for reference)
   |    .* *
   |   * .*
 0 |**.*───────────────→ t
   0                    1
```

### When to Use Which

| Function     | Use Case                                                  |
|-------------|-----------------------------------------------------------|
| Linear       | You want constant speed, or the motion is very short      |
| Smoothstep   | General-purpose smooth transitions, shader effects        |
| Smootherstep | When you need extra smoothness (Perlin noise, animations) |

---

## 8.8 Bezier Curves

Bezier curves are the workhorses of curve design — used in vector graphics, font
rendering, animation paths, and camera trajectories.

### Linear Bezier (2 control points — same as lerp)

```
B(t) = (1-t) × P0 + t × P1
```

```
  P0 ─────────────────── P1
```

### Quadratic Bezier (3 control points)

```
B(t) = (1-t)² × P0 + 2(1-t)t × P1 + t² × P2
```

P1 acts as a "magnet" that pulls the curve toward it without the curve passing
through it (usually).

```
          P1
          *
         / \        The curve is pulled toward P1
        /   \       but doesn't reach it
       /     .
      / . .    .
  P0 *           * P2
```

### Cubic Bezier (4 control points)

```
B(t) = (1-t)³ × P0 + 3(1-t)²t × P1 + 3(1-t)t² × P2 + t³ × P3
```

This is the most commonly used Bezier. CSS animations, SVG paths, font outlines —
they all use cubic Beziers.

```
  P0 *                  * P3
       \  . . . . .  /
        *           *
       P1          P2
```

P1 controls the departure direction from P0. P2 controls the arrival direction at P3.

### De Casteljau's Algorithm

An elegant, recursive way to evaluate any Bezier curve. Instead of plugging into the
formula, you repeatedly lerp between adjacent control points until one point remains.

**For a cubic Bezier with points P0, P1, P2, P3 at parameter t:**

```
Step 1: Lerp between adjacent points
   A = lerp(P0, P1, t)
   B = lerp(P1, P2, t)
   C = lerp(P2, P3, t)

Step 2: Lerp between results
   D = lerp(A, B, t)
   E = lerp(B, C, t)

Step 3: Final lerp
   result = lerp(D, E, t)
```

Visually for t = 0.5:

```
  P0 ──A── P1 ──B── P2 ──C── P3     (original points)
        \  /    \  /
         D       E                    (first reduction)
          \     /
           *                          (final point on curve)
```

**Pseudocode**:

```
function deCasteljau(points, t):
    if length(points) == 1:
        return points[0]

    newPoints = []
    for i in 0 .. length(points) - 2:
        newPoints.append(lerp(points[i], points[i+1], t))

    return deCasteljau(newPoints, t)
```

### Example: Projectile Arc

```
function getProjectilePosition(start, controlPoint, end, t):
    // Quadratic Bezier for a nice arc
    return quadraticBezier(start, controlPoint, end, t)

function quadraticBezier(P0, P1, P2, t):
    a = lerp(P0, P1, t)
    b = lerp(P1, P2, t)
    return lerp(a, b, t)

// Control point is above the midpoint for an arc
midpoint = lerp(start, end, 0.5)
controlPoint = midpoint + Vec2(0, -arcHeight)
```

### Example: Camera Path Between Waypoints

```
function updateCameraAlongPath(camera, pathPoints, t):
    // pathPoints has 4 control points: start, handle1, handle2, end
    camera.position = cubicBezier(pathPoints[0], pathPoints[1],
                                   pathPoints[2], pathPoints[3], t)
```

---

## 8.9 Catmull-Rom Splines

### The Problem with Bezier

Bezier curves don't pass through their middle control points. If you have waypoints
that an object must pass through, you need a different tool.

### Catmull-Rom to the Rescue

A Catmull-Rom spline passes through every control point. It uses four points to
define the curve segment between the middle two:

```
  P0         P1 ─── curve ─── P2         P3
  (prev)     (start)          (end)       (next)
```

The curve goes from P1 to P2. P0 and P3 influence the tangent directions.

### Formula

```
function catmullRom(P0, P1, P2, P3, t):
    t2 = t * t
    t3 = t2 * t

    return 0.5 * (
        (2 * P1) +
        (-P0 + P2) * t +
        (2*P0 - 5*P1 + 4*P2 - P3) * t2 +
        (-P0 + 3*P1 - 3*P2 + P3) * t3
    )
```

This looks complex, but each component (x, y, z) is computed the same way.

### Example: Camera Rail in a Cutscene

```
function updateCutsceneCamera(camera, waypoints, totalTime, currentTime):
    numSegments = length(waypoints) - 1
    t = (currentTime / totalTime) * numSegments
    segment = floor(t)
    localT = t - segment

    // Clamp segment index
    segment = clamp(segment, 0, numSegments - 1)

    // Get the 4 points needed for this segment
    i0 = max(segment - 1, 0)
    i1 = segment
    i2 = min(segment + 1, length(waypoints) - 1)
    i3 = min(segment + 2, length(waypoints) - 1)

    camera.position = catmullRom(waypoints[i0], waypoints[i1],
                                  waypoints[i2], waypoints[i3], localT)
```

### Catmull-Rom vs Bezier

| Property              | Bezier               | Catmull-Rom            |
|-----------------------|----------------------|------------------------|
| Passes through points | Only endpoints       | All points             |
| Control               | Explicit handles     | Automatic from neighbors|
| Best for              | Artistic curves      | Following waypoints    |
| Continuity            | C0 or higher         | C1 (smooth)            |

---

## 8.10 Hermite Splines

A Hermite spline is defined by two points **and their tangents** (velocity vectors):

```
function hermite(P0, T0, P1, T1, t):
    t2 = t * t
    t3 = t2 * t

    h00 = 2*t3 - 3*t2 + 1      // basis for P0
    h10 = t3 - 2*t2 + t         // basis for T0
    h01 = -2*t3 + 3*t2          // basis for P1
    h11 = t3 - t2               // basis for T1

    return h00*P0 + h10*T0 + h01*P1 + h11*T1
```

```
  T0 →               ← T1
      \             /
  P0   * . . . . . *   P1
        (the curve)
```

**When to use**: When you want explicit control over both position and direction at
each point — e.g., a racing game where the track must pass through gates at specific
angles.

Catmull-Rom is actually a special case of Hermite interpolation where the tangents
are automatically derived from neighboring points.

---

## 8.11 B-Splines

B-splines (Basis splines) are a generalization of Bezier curves.

### Key Properties

- The curve does **not** generally pass through any control point
- Provide **local control**: moving one control point only affects the curve nearby
- Higher continuity than Bezier chains (C2 or better)
- Used heavily in CAD software and surface modeling

### When You'd Use Them in Games

- **Terrain generation**: defining smooth elevation profiles
- **Racing game tracks**: smooth, continuous curves with local editing
- **Procedural content**: generating smooth paths or shapes

B-splines are less common in real-time games than Bezier and Catmull-Rom because they
don't pass through their control points, making them harder to design intuitively.

---

## 8.12 SLERP — Spherical Linear Interpolation

When interpolating **directions** or **rotations**, regular lerp doesn't work correctly.
On a sphere, the shortest path between two points is a great arc, not a straight line.

### The Problem with Lerp on Directions

```
         lerp goes through here (WRONG — not on sphere)
                   ↓
    A ─ ─ ─ ─ ─ x ─ ─ ─ ─ ─ B    ← straight line (cuts through sphere)
     \                       /
      \                     /
       \_______*___________/       ← slerp follows the surface (CORRECT)
```

### Formula

```
function slerp(a, b, t):
    dot = dotProduct(a, b)
    dot = clamp(dot, -1, 1)
    theta = acos(dot)

    if theta < 0.001:      // nearly parallel, lerp is fine
        return lerp(a, b, t)

    sinTheta = sin(theta)
    return (sin((1-t) * theta) / sinTheta) * a +
           (sin(t * theta) / sinTheta) * b
```

### When to Use SLERP

- Interpolating **unit vectors** (normals, directions)
- Interpolating **quaternions** (rotations) — see the quaternion chapter
- Blending orientations in animation systems

---

## 8.13 Bilinear Interpolation

Bilinear interpolation extends lerp to a 2D grid. Given four corner values, find the
value at any point inside the rectangle.

### Setup

```
  Q01 ────────────── Q11
   |                  |
   |        ●         |      ● = the point we want to interpolate
   |     (tx, ty)     |
   |                  |
  Q00 ────────────── Q10

  tx = horizontal position [0,1]
  ty = vertical position [0,1]
```

### Formula

```
function bilinearInterpolation(Q00, Q10, Q01, Q11, tx, ty):
    // Lerp along the bottom edge
    bottom = lerp(Q00, Q10, tx)
    // Lerp along the top edge
    top = lerp(Q01, Q11, tx)
    // Lerp between bottom and top
    return lerp(bottom, top, ty)
```

### Example: Terrain Height Sampling

A heightmap stores elevation at grid points. To find the height between grid points:

```
function getTerrainHeight(heightmap, worldX, worldZ, cellSize):
    // Convert world position to grid coordinates
    gx = worldX / cellSize
    gz = worldZ / cellSize

    // Integer grid coordinates (bottom-left corner)
    ix = floor(gx)
    iz = floor(gz)

    // Fractional part (position within the cell)
    tx = gx - ix
    tz = gz - iz

    // Sample the four corners
    h00 = heightmap[ix][iz]
    h10 = heightmap[ix+1][iz]
    h01 = heightmap[ix][iz+1]
    h11 = heightmap[ix+1][iz+1]

    return bilinearInterpolation(h00, h10, h01, h11, tx, tz)
```

### Example: Texture Filtering

This is exactly how "bilinear texture filtering" works in GPUs. When a texture
coordinate falls between texels, the GPU blends the four nearest texels using
bilinear interpolation.

---

## 8.14 Spring/Damping Interpolation

### The Problem with Lerp

Standard lerp has no concept of velocity or momentum. A camera using lerp snaps
into position and stops dead. Real objects have inertia — they overshoot, bounce,
and settle.

### SmoothDamp

SmoothDamp is a critically damped spring: it approaches the target as fast as possible
without oscillating.

```
function smoothDamp(current, target, velocity, smoothTime, deltaTime):
    omega = 2.0 / smoothTime
    x = omega * deltaTime
    exp = 1.0 / (1.0 + x + 0.48*x*x + 0.235*x*x*x)

    change = current - target
    maxChange = maxSpeed * smoothTime
    change = clamp(change, -maxChange, maxChange)

    temp = (velocity + omega * change) * deltaTime
    velocity = (velocity - omega * temp) * exp
    result = target + (change + temp) * exp

    // Prevent overshoot
    if (target - current > 0) == (result > target):
        result = target
        velocity = 0

    return (result, velocity)
```

### Example: Camera Smoothly Following the Player

```
cameraVelocity = Vec3(0, 0, 0)

function updateCamera(camera, player, deltaTime):
    smoothTime = 0.3    // seconds to reach target

    (camera.x, cameraVelocity.x) = smoothDamp(
        camera.x, player.x, cameraVelocity.x, smoothTime, deltaTime)
    (camera.y, cameraVelocity.y) = smoothDamp(
        camera.y, player.y, cameraVelocity.y, smoothTime, deltaTime)
    (camera.z, cameraVelocity.z) = smoothDamp(
        camera.z, player.z, cameraVelocity.z, smoothTime, deltaTime)
```

### Spring Interpolation (Under-Damped)

For a bouncy effect, use a spring with less damping:

```
function springInterp(current, target, velocity, stiffness, damping, deltaTime):
    force = (target - current) * stiffness
    force -= velocity * damping
    velocity += force * deltaTime
    current += velocity * deltaTime
    return (current, velocity)
```

**Use cases**: bouncy UI elements, juice effects, springy follow cameras.

### Comparison

```
  target ──────────────────────────────────────────
                     ___
  spring:      ___.──   ──.__                       ← oscillates, then settles
            .*'                '──..__________

  smoothDamp:   .──────────────────────────────     ← approaches fast, no overshoot
              .*

  lerp:  .*───────────────────────────────────      ← constant approach (exponential)
        *
```

---

## 8.15 Summary — Which Interpolation to Use When

| Situation                            | Method               |
|--------------------------------------|----------------------|
| Move between two values at constant speed | Lerp              |
| Smooth start and/or stop             | Easing function + Lerp |
| Smooth transition in a shader        | Smoothstep           |
| Find where a value sits in a range   | Inverse Lerp         |
| Map a value from one range to another| Remap                |
| Keep a value within bounds           | Clamp                |
| Artistic curve with handles          | Cubic Bezier         |
| Path through waypoints               | Catmull-Rom spline   |
| Path with explicit tangent control   | Hermite spline       |
| Smooth directions/rotations          | SLERP                |
| Value on a 2D grid                   | Bilinear interpolation|
| Follow something with inertia        | SmoothDamp / Spring  |
| Projectile arc                       | Quadratic Bezier     |
| Camera cutscene rail                 | Catmull-Rom spline   |
| Bouncy UI element                    | Spring interpolation |
| Terrain height at arbitrary position | Bilinear interpolation|

### Key Takeaways

1. **Lerp is the foundation** — almost every other technique builds on it.
2. **Easing functions** transform `t` to create natural-feeling motion.
3. **Smoothstep** is the go-to for shader transitions and is free on GPUs.
4. **Bezier curves** give artistic control; **Catmull-Rom** goes through all points.
5. **SLERP** is mandatory for interpolating directions and rotations.
6. **SmoothDamp** is the professional choice for camera following and UI animation.
7. **Bilinear interpolation** is how terrains and textures are sampled.
8. Always **clamp t** to [0, 1] unless you intentionally want extrapolation.

---

*Next chapter: [Chapter 9 — Collision Detection](09-collision-detection.md)*
