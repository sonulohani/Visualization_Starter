# Chapter 9: Collision Detection

Collision detection answers a deceptively simple question: "Are these two things
touching?" In practice, it's one of the most computationally demanding parts of a game
engine, and getting it wrong leads to characters walking through walls, bullets missing
targets, and physics that feel broken.

---

## 9.1 Why Collision Detection Matters

| System          | What Collision Detection Does                              |
|-----------------|-----------------------------------------------------------|
| **Gameplay**    | Sword hits enemy, player picks up coin, ball enters goal  |
| **Physics**     | Objects bounce, slide, stack, and rest on each other      |
| **Triggers**    | Entering a room loads new content, crossing a line = lap  |
| **Raycasting**  | Bullets hit targets, mouse clicks select objects          |
| **Navigation**  | AI avoids walls, characters don't walk through furniture  |
| **Cameras**     | Camera doesn't clip through walls in third-person view    |

Without collision detection, every object in your game is a ghost.

---

## 9.2 Bounding Volumes

Real game objects are complex: a character model might have 10,000 triangles. Testing
every triangle against every other triangle is absurdly expensive. Instead, we wrap
objects in **simple shapes** and test those first.

### AABB — Axis-Aligned Bounding Box

A box whose edges are parallel to the coordinate axes. Defined by a minimum and
maximum corner.

```
  ┌─────────────────┐ max (xMax, yMax)
  │                 │
  │    Object       │
  │      inside     │
  │                 │
  └─────────────────┘
  min (xMin, yMin)
```

**Pros**: Extremely fast intersection tests. Trivial to compute.
**Cons**: Fits poorly on rotated or elongated objects (lots of empty space).

```
  ┌──────────────────┐
  │    /\            │   ← lots of wasted space
  │   /  \           │      for a rotated object
  │  /    \          │
  │ /      \         │
  │/        \        │
  └──────────────────┘
```

### OBB — Oriented Bounding Box

A box that rotates with the object. Tighter fit, but more expensive to test.

```
       ╱─────────────╲
      ╱    Object     ╲
     ╱    (rotated)    ╲
    ╲                  ╱
     ╲                ╱
      ╲──────────────╱
```

### Bounding Sphere

A sphere (circle in 2D) that encloses the object. Defined by center and radius.

```
        ****
      **    **
     *  Object *
     *         *
      **    **
        ****
```

**Pros**: Rotation-invariant (stays the same no matter how the object rotates).
Intersection test is trivial.
**Cons**: Bad fit for elongated objects.

### Bounding Capsule

Two hemispheres connected by a cylinder. Great for characters.

```
       ____
      / oo \      ← hemisphere
     |  --  |
     | body |     ← cylinder
     | body |
     |______|
      \    /      ← hemisphere
       ----
```

**Pros**: Fits humanoid characters well. Smooth collision response (no sharp corners).
**Cons**: Slightly more complex math than spheres.

### Trade-Off: Accuracy vs Speed

```
  Speed:    AABB > Sphere > Capsule > OBB > Convex Hull > Triangle Mesh
  Accuracy: AABB < Sphere < Capsule < OBB < Convex Hull < Triangle Mesh
```

Most engines use a **hierarchy**: fast bounding volume first, detailed mesh only if
the bounding volume test passes.

---

## 9.3 Point vs Shape Tests

The simplest collision tests: is a point inside a shape?

### Point Inside Circle/Sphere

```
function pointInCircle(point, center, radius):
    dx = point.x - center.x
    dy = point.y - center.y
    distSquared = dx*dx + dy*dy
    return distSquared <= radius * radius
```

We compare squared distances to avoid the expensive `sqrt()`.

```
        ****
      **    **
     *   ●   *      ← ● is inside (dist < radius)
     *  center *
      **    **
        ****
              ○      ← ○ is outside (dist > radius)
```

For 3D spheres, add the z component:

```
distSquared = dx*dx + dy*dy + dz*dz
```

### Point Inside AABB

Check each axis independently:

```
function pointInAABB(point, aabb):
    return (point.x >= aabb.min.x and point.x <= aabb.max.x and
            point.y >= aabb.min.y and point.y <= aabb.max.y)

// 3D version adds:
//  and point.z >= aabb.min.z and point.z <= aabb.max.z
```

### Point Inside Triangle (Barycentric Coordinates)

Given triangle vertices A, B, C and a test point P, compute barycentric coordinates:

```
function pointInTriangle(P, A, B, C):
    v0 = C - A
    v1 = B - A
    v2 = P - A

    dot00 = dot(v0, v0)
    dot01 = dot(v0, v1)
    dot02 = dot(v0, v2)
    dot11 = dot(v1, v1)
    dot12 = dot(v1, v2)

    invDenom = 1 / (dot00 * dot11 - dot01 * dot01)
    u = (dot11 * dot02 - dot01 * dot12) * invDenom
    v = (dot00 * dot12 - dot01 * dot02) * invDenom

    return (u >= 0) and (v >= 0) and (u + v <= 1)
```

```
       A
      /|\
     / | \
    /  P  \       ← P is inside if u >= 0, v >= 0, u+v <= 1
   /   |   \
  B────+────C
```

Barycentric coordinates (u, v, w=1-u-v) also tell you the weights for interpolating
vertex attributes (colors, normals, texture coordinates) — this is how GPUs shade
triangles.

---

## 9.4 Circle/Sphere vs Circle/Sphere

The simplest dynamic collision test. Two circles overlap if the distance between their
centers is less than the sum of their radii.

```
function circlesCollide(c1, r1, c2, r2):
    dx = c2.x - c1.x
    dy = c2.y - c1.y
    distSquared = dx*dx + dy*dy
    radiusSum = r1 + r2
    return distSquared <= radiusSum * radiusSum
```

```
    ****          ****
  **    **      **    **
 *   c1  *    *   c2  *
 *   r1  *    *   r2  *
  **    **      **    **
    ****          ****
  |←─ r1 ──→|←─ r2 ──→|
  |←───── dist ──────→|

  Colliding if: dist <= r1 + r2
```

### Example: Checking If Two Balls Collide

```
function checkBallCollision(ball1, ball2):
    if circlesCollide(ball1.pos, ball1.radius, ball2.pos, ball2.radius):
        // They're colliding — resolve it
        handleCollision(ball1, ball2)
```

This extends trivially to 3D spheres by adding the z component to the distance
calculation.

---

## 9.5 AABB vs AABB

Two AABBs overlap if and only if they overlap on **every** axis. If there is any
axis where they don't overlap, they're separated.

```
function aabbOverlap(a, b):
    if a.max.x < b.min.x or a.min.x > b.max.x: return false
    if a.max.y < b.min.y or a.min.y > b.max.y: return false
    // For 3D, add:
    // if a.max.z < b.min.z or a.min.z > b.max.z: return false
    return true
```

### Visual: Overlap on Each Axis

```
  Overlap on X axis AND Y axis → collision
  Gap on ANY axis → no collision

  Y axis:
  ┌──A──┐
  │     │  ┌──B──┐     ← overlap on Y? YES
  │     │  │     │
  └─────┘  │     │
           └─────┘

  X axis:
  ├──A──┤
        ├──B──┤         ← overlap on X? YES (they share some X range)

  Both axes overlap → COLLISION
```

```
  ┌──A──┐
  │     │
  └─────┘
              ┌──B──┐   ← gap on X axis → NO COLLISION
              │     │
              └─────┘
```

### Example: Rectangular Sprite Overlap

```
function checkSpriteCollision(sprite1, sprite2):
    a = sprite1.getBoundingBox()   // returns AABB
    b = sprite2.getBoundingBox()

    if aabbOverlap(a, b):
        // Sprites are overlapping
        handleCollision(sprite1, sprite2)
```

---

## 9.6 AABB vs Sphere

Find the **closest point on the AABB** to the sphere center, then check if that
point is within the sphere's radius.

```
function closestPointOnAABB(point, aabb):
    return Vec3(
        clamp(point.x, aabb.min.x, aabb.max.x),
        clamp(point.y, aabb.min.y, aabb.max.y),
        clamp(point.z, aabb.min.z, aabb.max.z)
    )

function aabbVsSphere(aabb, sphereCenter, sphereRadius):
    closest = closestPointOnAABB(sphereCenter, aabb)
    distSquared = distanceSquared(closest, sphereCenter)
    return distSquared <= sphereRadius * sphereRadius
```

```
  ┌─────────┐
  │         │
  │    ●────┼────→ ○   sphere center
  │ closest │    radius
  └─────────┘

  If dist(closest, sphereCenter) <= radius → COLLISION
```

---

## 9.7 Ray Casting

A ray is defined by an **origin** point and a **direction** vector:

```
ray(t) = origin + t × direction     (t >= 0)
```

```
  origin
    ●────────────────────→ direction
    t=0    t=1    t=2
```

Ray casting asks: "Does this ray hit something, and if so, where?"

### Ray vs Sphere

Substitute the ray equation into the sphere equation:

```
|origin + t × direction - center|² = radius²
```

Expanding gives a quadratic in t: `at² + bt + c = 0`

```
function rayVsSphere(rayOrigin, rayDir, center, radius):
    oc = rayOrigin - center
    a = dot(rayDir, rayDir)
    b = 2.0 * dot(oc, rayDir)
    c = dot(oc, oc) - radius * radius

    discriminant = b*b - 4*a*c

    if discriminant < 0:
        return NO_HIT           // ray misses the sphere

    sqrtDisc = sqrt(discriminant)
    t1 = (-b - sqrtDisc) / (2*a)    // near intersection
    t2 = (-b + sqrtDisc) / (2*a)    // far intersection

    if t1 >= 0: return t1       // hit at nearest point
    if t2 >= 0: return t2       // origin is inside sphere
    return NO_HIT               // sphere is behind the ray
```

```
  Ray:  ●──────────●──────────●──────→
                   t1         t2
                   ╱───────────╲
                  ╱   sphere    ╲
                  ╲             ╱
                   ╲───────────╱
```

### Ray vs AABB (Slab Method)

The AABB is the intersection of three "slabs" (pairs of parallel planes). Compute
the entry and exit t-values for each slab, then check if the resulting intervals
overlap.

```
function rayVsAABB(rayOrigin, rayDir, aabb):
    tMin = -INFINITY
    tMax = +INFINITY

    for each axis (x, y, z):
        if rayDir[axis] != 0:
            t1 = (aabb.min[axis] - rayOrigin[axis]) / rayDir[axis]
            t2 = (aabb.max[axis] - rayOrigin[axis]) / rayDir[axis]

            if t1 > t2: swap(t1, t2)

            tMin = max(tMin, t1)
            tMax = min(tMax, t2)

            if tMin > tMax: return NO_HIT
        else:
            // Ray is parallel to this slab
            if rayOrigin[axis] < aabb.min[axis] or rayOrigin[axis] > aabb.max[axis]:
                return NO_HIT

    if tMax < 0: return NO_HIT    // box is behind the ray
    return max(tMin, 0)            // clamp to ray start
```

### Ray vs Plane

A plane is defined by a normal `n` and a distance `d` from the origin:

```
dot(n, point) = d
```

```
function rayVsPlane(rayOrigin, rayDir, planeNormal, planeDist):
    denom = dot(planeNormal, rayDir)
    if abs(denom) < EPSILON:
        return NO_HIT              // ray is parallel to plane

    t = (planeDist - dot(planeNormal, rayOrigin)) / denom
    if t < 0: return NO_HIT       // plane is behind the ray
    return t
```

### Ray vs Triangle (Möller-Trumbore Algorithm)

This is the standard algorithm for ray-triangle intersection in 3D games.

```
function rayVsTriangle(rayOrigin, rayDir, v0, v1, v2):
    edge1 = v1 - v0
    edge2 = v2 - v0
    h = cross(rayDir, edge2)
    a = dot(edge1, h)

    if abs(a) < EPSILON:
        return NO_HIT              // ray is parallel to triangle

    f = 1.0 / a
    s = rayOrigin - v0
    u = f * dot(s, h)

    if u < 0 or u > 1:
        return NO_HIT

    q = cross(s, edge1)
    v = f * dot(rayDir, q)

    if v < 0 or u + v > 1:
        return NO_HIT

    t = f * dot(edge2, q)

    if t > EPSILON:
        return t                   // hit at distance t
    return NO_HIT
```

### Example: Shooting a Gun

```
function fireWeapon(player):
    rayOrigin = player.gunBarrelPosition
    rayDir = player.aimDirection

    closestHit = INFINITY
    hitTarget = null

    for each entity in world.entities:
        // Broad phase: test bounding sphere first
        t = rayVsSphere(rayOrigin, rayDir, entity.center, entity.boundingRadius)
        if t < closestHit:
            // Narrow phase: test actual mesh triangles
            for each triangle in entity.mesh:
                tTri = rayVsTriangle(rayOrigin, rayDir, tri.v0, tri.v1, tri.v2)
                if tTri < closestHit:
                    closestHit = tTri
                    hitTarget = entity

    if hitTarget != null:
        hitPoint = rayOrigin + closestHit * rayDir
        applyDamage(hitTarget, hitPoint)
```

### Example: Mouse Picking (Clicking on 3D Objects)

```
function getClickedObject(mouseX, mouseY, camera, entities):
    // Convert 2D screen position to a 3D ray
    rayOrigin = camera.position
    rayDir = camera.screenToWorldRay(mouseX, mouseY)

    closestHit = INFINITY
    clicked = null

    for each entity in entities:
        t = rayVsSphere(rayOrigin, rayDir, entity.center, entity.boundingRadius)
        if t >= 0 and t < closestHit:
            closestHit = t
            clicked = entity

    return clicked
```

---

## 9.8 Separating Axis Theorem (SAT)

### The Core Idea

Two convex shapes do **not** overlap if and only if there exists an axis along which
their projections (shadows) don't overlap. If you can find such a "separating axis,"
the shapes are separated.

```
  Shape A           Shape B
  ┌─────┐          ┌─────┐
  │     │          │     │
  │     │   gap    │     │
  └─────┘          └─────┘

  Project onto X axis:
  ├──A──┤   gap    ├──B──┤    ← gap found! No collision.
```

```
  Shape A
  ┌─────┐
  │  ┌──┼──┐  Shape B
  │  │  │  │
  └──┼──┘  │
     └─────┘

  Project onto X axis:
  ├──A──┤              ← overlap
     ├──B──┤

  Project onto Y axis:
  ├──A──┤              ← overlap
    ├──B──┤

  All axes overlap → COLLISION
```

### Which Axes to Test?

For two convex polygons in 2D:
- Test every **edge normal** of both shapes
- For two rectangles, that's 4 axes (2 from each rectangle, but parallel pairs
  are redundant, so effectively 2 unique axes per rectangle)

For 3D convex polyhedra:
- Face normals of both shapes
- Cross products of edge pairs (one edge from each shape)

### 2D SAT Example: Two Rectangles

```
function satOverlap(shapeA, shapeB):
    axes = getAllSeparatingAxes(shapeA, shapeB)

    for each axis in axes:
        projA = projectShape(shapeA, axis)   // returns (min, max)
        projB = projectShape(shapeB, axis)

        if projA.max < projB.min or projB.max < projA.min:
            return false   // found a separating axis → no collision

    return true   // no separating axis found → collision

function projectShape(shape, axis):
    min = +INFINITY
    max = -INFINITY
    for each vertex in shape.vertices:
        projection = dot(vertex, axis)
        min = minimum(min, projection)
        max = maximum(max, projection)
    return (min, max)

function getAllSeparatingAxes(shapeA, shapeB):
    axes = []
    for each edge in shapeA.edges:
        normal = perpendicular(edge)    // rotate 90°
        axes.append(normalize(normal))
    for each edge in shapeB.edges:
        normal = perpendicular(edge)
        axes.append(normalize(normal))
    return axes
```

---

## 9.9 GJK Algorithm

The Gilbert-Johnson-Keerthi algorithm determines if two convex shapes intersect by
working in **Minkowski difference** space.

### The Key Insight

The Minkowski difference of two shapes A and B is:

```
A ⊖ B = { a - b | a ∈ A, b ∈ B }
```

**Two convex shapes overlap if and only if their Minkowski difference contains the
origin.**

### How GJK Works (Simplified)

1. Pick an initial direction and find the **support point** (the farthest point in
   that direction on A minus the farthest point in the opposite direction on B).
2. Build a **simplex** (line, triangle, tetrahedron) that tries to enclose the origin.
3. If the simplex contains the origin → **collision**.
4. If the simplex can't reach the origin → **no collision**.
5. Otherwise, choose a new direction toward the origin and add another support point.

GJK is elegant because it works with **any convex shape** — you only need a "support
function" that returns the farthest point in a given direction.

### When to Use GJK

- Collision between arbitrary convex shapes
- Often paired with **EPA** (Expanding Polytope Algorithm) to find penetration depth
- Used in physics engines like Box2D and Bullet Physics

---

## 9.10 Spatial Partitioning

### The Problem

With N objects, checking every pair requires \(N(N-1)/2\) tests. For 1000 objects,
that's ~500,000 pair checks **per frame**. Most of these are between objects on
opposite sides of the world — obviously not colliding.

Spatial partitioning divides the world into regions so you only test objects that are
**nearby**.

### Grid-Based Partitioning

Divide the world into a uniform grid of cells. Each object is placed in the cell(s)
it overlaps. Only test objects in the same or neighboring cells.

```
  ┌────┬────┬────┬────┐
  │    │ A  │    │    │
  ├────┼────┼────┼────┤
  │    │ B  │    │ C  │     A and B are in adjacent cells → check them
  ├────┼────┼────┼────┤     A and C are far apart → skip
  │    │    │    │    │
  ├────┼────┼────┼────┤
  │ D  │    │    │    │
  └────┴────┴────┴────┘
```

**Pros**: Simple to implement, O(1) cell lookup.
**Cons**: Wastes memory in sparse worlds. Cell size must be tuned — too large and you
check too many pairs; too small and objects span multiple cells.

### Quadtree (2D) and Octree (3D)

Recursively subdivide space into 4 (2D) or 8 (3D) children, only subdividing regions
that contain many objects.

```
  Quadtree:

  ┌─────────────────────┐
  │          │          │
  │    A     │   empty  │
  │     B    │          │
  ├────┬─────┼──────────┤
  │    │ C D │          │
  │    │ E   │          │     This quadrant was subdivided further
  ├────┼─────┤   F      │     because it had many objects
  │    │     │          │
  │    │     │          │
  └────┴─────┴──────────┘
```

**Pros**: Adapts to object distribution. Efficient for clustered objects.
**Cons**: More complex than a grid. Tree traversal overhead.

### BVH — Bounding Volume Hierarchy

A tree where each node contains a bounding volume (usually AABB) that encloses all
objects in its subtree. Test the tree from the root down; if a node's bounding volume
doesn't intersect, skip its entire subtree.

```
         [World AABB]
          /        \
   [Left AABB]  [Right AABB]
    /    \         /     \
  [A]   [B]     [C]    [D,E]
```

**Pros**: Works well for both static and dynamic scenes. Great for ray casting.
**Cons**: Needs rebuilding or refitting when objects move.

### Comparison

| Method      | Best For                           | Complexity (query)  |
|-------------|-----------------------------------|---------------------|
| Uniform Grid| Evenly distributed objects         | O(1) per cell       |
| Quadtree    | 2D games with clustered objects    | O(log n)            |
| Octree      | 3D games with clustered objects    | O(log n)            |
| BVH         | Ray casting, mixed static/dynamic  | O(log n)            |

---

## 9.11 Broad Phase vs Narrow Phase

Real collision detection runs in two stages:

### Broad Phase — Quickly Eliminate Non-Colliding Pairs

Use spatial partitioning or sweep-and-prune to find **potentially** colliding pairs.
This reduces thousands of pairs to maybe a dozen.

Techniques:
- **Spatial hash / grid**: hash object position to a cell
- **Sweep and prune**: sort objects by one axis, find overlapping intervals
- **BVH queries**: traverse tree to find overlapping bounding volumes

### Narrow Phase — Precise Check on Remaining Pairs

For each pair that passed the broad phase, run the actual collision test:
- AABB vs AABB (if bounding boxes are sufficient)
- SAT for convex polygons
- GJK for arbitrary convex shapes
- Triangle mesh tests for precise geometry

### The Pipeline

```
  1000 objects
       │
       ▼
  ┌─────────────┐
  │ Broad Phase │  → 30 potential pairs    (spatial partitioning)
  └─────────────┘
       │
       ▼
  ┌──────────────┐
  │ Narrow Phase │  → 8 actual collisions   (precise geometry tests)
  └──────────────┘
       │
       ▼
  ┌────────────────────┐
  │ Collision Response │  → resolve overlaps, apply forces
  └────────────────────┘
```

---

## 9.12 Collision Response (Overview)

Detecting a collision is only half the problem. You also need to **respond** to it.

### Resolving Penetration

When two objects overlap, push them apart by the **minimum translation vector** (MTV)
— the shortest direction and distance to separate them.

```
  Before:                    After:
  ┌──A──┐                  ┌──A──┐
  │  ┌──┼──B──┐            │     │ ┌──B──┐
  │  │  │     │   →  push  │     │ │     │
  └──┼──┘     │            └─────┘ │     │
     └────────┘                    └─────┘
```

```
function resolveOverlap(a, b, mtv):
    // Move each object half the penetration distance
    a.position -= mtv * 0.5
    b.position += mtv * 0.5
```

### Bounce

Reflect the velocity along the collision normal:

```
function bounce(velocity, normal, restitution):
    // restitution: 0 = no bounce, 1 = perfect bounce
    return velocity - (1 + restitution) * dot(velocity, normal) * normal
```

### Friction

Reduce the tangential component of velocity:

```
function applyFriction(velocity, normal, friction):
    normalComponent = dot(velocity, normal) * normal
    tangentComponent = velocity - normalComponent
    return normalComponent + tangentComponent * (1 - friction)
```

Full physics response (impulse-based, mass-aware) is covered in a dedicated physics
chapter.

---

## 9.13 Trigger Volumes

Not all collisions need physical response. Sometimes you just want to detect that
something **entered a region** — no bouncing, no pushing.

```
  ┌─ ─ ─ ─ ─ ─ ─ ─ ─┐
  │                   │
  │  TRIGGER ZONE     │     Player walks in → event fires
  │  (no collision    │     No physical response
  │   response)       │
  │           ●→      │     ● = player
  └─ ─ ─ ─ ─ ─ ─ ─ ─┘
```

### Common Uses

- **Room transitions**: entering a doorway loads the next area
- **Checkpoints**: save progress when the player passes through
- **Cutscene triggers**: start a cinematic when the player reaches a location
- **Damage zones**: lava floor, poison gas cloud
- **Sound zones**: change ambient audio when entering a cave

### Implementation Pattern

```
function updateTriggerVolumes(triggers, entities):
    for each trigger in triggers:
        for each entity in entities:
            isOverlapping = testOverlap(trigger.volume, entity.bounds)

            wasInside = trigger.occupants.contains(entity)

            if isOverlapping and not wasInside:
                trigger.onEnter(entity)
                trigger.occupants.add(entity)

            else if not isOverlapping and wasInside:
                trigger.onExit(entity)
                trigger.occupants.remove(entity)

            else if isOverlapping and wasInside:
                trigger.onStay(entity)
```

---

## 9.14 Continuous Collision Detection (CCD)

### The Tunneling Problem

When an object moves fast enough to skip past a thin wall in a single frame, discrete
collision detection misses it entirely:

```
  Frame N:              Frame N+1:
                wall
  ● ──────────→ │ │ ──────────→ ●
  (bullet)      │ │             (bullet passed through!)
```

The bullet was on one side of the wall, then suddenly on the other. At no point
during the frame check was it overlapping the wall.

### Solution: Swept Tests

Instead of testing the object at its current position, test the **swept volume** — the
shape traced by the object between its old and new positions.

**Swept Sphere**: A capsule formed by sweeping a sphere from old position to new:

```
  old pos          new pos
    ○───────────────○
    ╲_______________╱     ← this capsule is the swept volume
```

**Swept AABB**: An AABB from the minimum of old/new positions to the maximum.

### Swept Sphere vs Plane

```
function sweptSphereVsPlane(center, velocity, radius, planeNormal, planeDist, dt):
    // Distance from sphere center to plane
    dist = dot(planeNormal, center) - planeDist

    // Velocity toward the plane
    speed = dot(planeNormal, velocity)

    if abs(speed) < EPSILON: return NO_HIT   // moving parallel

    // Time when closest surface point touches the plane
    tHit = (radius - dist) / speed   // approaching from front
    if tHit < 0 or tHit > dt:
        tHit = (-radius - dist) / speed   // approaching from back
        if tHit < 0 or tHit > dt: return NO_HIT

    return tHit
```

### Speculative Contacts (Another Approach)

Instead of swept tests, predict if objects **will** overlap next frame and apply
constraints preemptively. This is cheaper than full CCD and is used in many modern
physics engines.

### When You Need CCD

- **Bullets and projectiles**: high speed, small size
- **Fast-moving game objects**: racing games, physics-heavy games
- **Thin geometry**: paper walls, fences, single-sided surfaces

---

## 9.15 Summary and Decision Flowchart

### Choosing the Right Collision Method

```
  Start: What are you testing?
       │
       ├── Point vs Shape?
       │      ├── Circle/Sphere → distance check
       │      ├── AABB → per-axis range check
       │      └── Triangle → barycentric coordinates
       │
       ├── Two spheres/circles?
       │      └── distance < r1 + r2
       │
       ├── Two AABBs?
       │      └── overlap on every axis
       │
       ├── AABB vs Sphere?
       │      └── closest point on AABB, then distance check
       │
       ├── Ray vs something?
       │      ├── Sphere → quadratic formula
       │      ├── AABB → slab method
       │      ├── Plane → single dot product
       │      └── Triangle → Möller-Trumbore
       │
       ├── Two convex polygons?
       │      ├── Simple → SAT
       │      └── Complex → GJK
       │
       ├── Many objects?
       │      └── Spatial partitioning → broad/narrow phase
       │
       └── Fast-moving objects?
              └── CCD (swept volumes or speculative contacts)
```

### Complexity Reference

| Test                  | Cost       | Notes                                |
|-----------------------|-----------|---------------------------------------|
| Sphere vs Sphere      | O(1)      | Cheapest possible                    |
| AABB vs AABB          | O(1)      | 2-3 comparisons per axis             |
| Point in AABB         | O(1)      | Per-axis range check                 |
| Point in Triangle     | O(1)      | Barycentric coordinates              |
| AABB vs Sphere        | O(1)      | Clamp + distance                     |
| Ray vs Sphere         | O(1)      | Quadratic formula                    |
| Ray vs AABB           | O(1)      | Slab method (3 axes)                 |
| Ray vs Triangle       | O(1)      | Möller-Trumbore                      |
| SAT (2 rectangles)    | O(axes)   | Usually 4 axes in 2D                 |
| GJK                   | O(iters)  | Typically < 20 iterations            |
| Broad phase (grid)    | O(n)      | Linear scan of occupied cells        |
| BVH query             | O(log n)  | Tree traversal                       |
| Brute force all pairs | O(n²)     | Don't do this in production          |

### Key Takeaways

1. **Always use bounding volumes** — never test raw meshes without a broad phase.
2. **Sphere tests are the cheapest** — use them as your first line of defense.
3. **Compare squared distances** — avoid `sqrt()` until you actually need the distance.
4. **SAT** works for any convex shape and naturally gives you the separation vector.
5. **Spatial partitioning** is mandatory once you have more than ~50 dynamic objects.
6. **Broad phase + narrow phase** is the standard two-pass architecture.
7. **CCD** is needed only for fast or small objects — use it selectively.
8. **Trigger volumes** use the same math as collision but skip the response step.

---

*Next chapter: Chapter 10 — Transformations and Matrices*
