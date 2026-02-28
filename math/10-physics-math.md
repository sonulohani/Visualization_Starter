# Chapter 10: Physics Math

> *"Games don't need to be physically accurate — they need to **feel** physically accurate."*

Physics simulation is the backbone of interactive worlds. Every bouncing ball, drifting
car, swinging rope, and ragdoll relies on a handful of core mathematical ideas. This
chapter breaks them down piece by piece.

---

## 10.1 Why Physics in Games?

Three reasons dominate:

| Goal                  | What physics provides                              |
|-----------------------|----------------------------------------------------|
| **Believable motion** | Objects fall, slide, and bounce the way players expect |
| **Realistic interaction** | Collisions, explosions, chains — they "just work" |
| **Fun gameplay**      | Angry Birds, Rocket League, and Half-Life 2's gravity gun all lean on physics for core mechanics |

Without physics math, every movement would need to be hand-animated or hard-coded.
Physics lets you describe **rules**, and the simulation produces the motion for you.

---

## 10.2 Position, Velocity, Acceleration

These three quantities form a chain, each being the **rate of change** of the one before it.

```
Position (where)
    │
    │  derivative (rate of change)
    ▼
Velocity (how fast and which direction)
    │
    │  derivative
    ▼
Acceleration (how velocity is changing)
```

### Definitions

| Quantity         | Symbol | Units            | Meaning                              |
|------------------|--------|------------------|--------------------------------------|
| **Position**     | \( \mathbf{p} \) | meters (m)       | Where the object is in space         |
| **Velocity**     | \( \mathbf{v} \) | m/s              | How fast position changes per second |
| **Acceleration** | \( \mathbf{a} \) | m/s²             | How fast velocity changes per second |

### The calculus connection (in plain English)

- **Velocity** is the *derivative* of position: \( \mathbf{v} = \frac{d\mathbf{p}}{dt} \)
- **Acceleration** is the *derivative* of velocity: \( \mathbf{a} = \frac{d\mathbf{v}}{dt} \)

Going backwards:

- **Position** is the *integral* of velocity over time.
- **Velocity** is the *integral* of acceleration over time.

In a game loop we don't have continuous time — we have small **time steps** \( \Delta t \)
(often written `dt`), so we approximate:

```
vel += acc * dt      // integrate acceleration → velocity
pos += vel * dt      // integrate velocity     → position
```

### Example: a falling apple

```
Initial: pos = (0, 10, 0)   vel = (0, 0, 0)   acc = (0, -9.81, 0)
dt = 0.016  (≈ 60 FPS)

Frame 1:
  vel = (0, 0, 0) + (0, -9.81, 0) * 0.016 = (0, -0.157, 0)
  pos = (0, 10, 0) + (0, -0.157, 0) * 0.016 = (0, 9.997, 0)

Frame 2:
  vel = (0, -0.157, 0) + (0, -9.81, 0) * 0.016 = (0, -0.314, 0)
  pos = (0, 9.997, 0) + (0, -0.314, 0) * 0.016 = (0, 9.992, 0)

... the apple accelerates downward each frame.
```

---

## 10.3 Newton's Laws of Motion (Applied to Games)

### First Law — Inertia

> *An object at rest stays at rest, and an object in motion stays in motion at the
> same speed and direction — **unless** a net external force acts on it.*

In code this means: if you set a spaceship's velocity, it will keep drifting forever
through empty space. You don't need to keep "pushing" it. This surprises many new
developers who expect things to stop on their own — that behavior comes from friction
and drag, not from the absence of force.

### Second Law — F = ma

\[
\mathbf{F} = m \, \mathbf{a}
\quad \Longrightarrow \quad
\mathbf{a} = \frac{\mathbf{F}}{m}
\]

This is the **workhorse** of game physics:

1. Sum up all forces on the object (gravity, thrust, drag, springs, …).
2. Divide by mass to get acceleration.
3. Integrate to get new velocity and position.

```
function physicsStep(body, dt):
    totalForce = gravity * body.mass
                 + thrustForce
                 + dragForce
                 + springForce
                 + ...

    body.acc = totalForce / body.mass
    body.vel += body.acc * dt
    body.pos += body.vel * dt
```

**Heavier objects need more force** to achieve the same acceleration. A bowling ball
(\( m = 7 \) kg) needs 7× the force of a 1 kg ball to accelerate equally.

### Third Law — Action and Reaction

> *For every force, there is an equal and opposite force.*

When a ball hits a wall, the wall pushes back on the ball just as hard. In a two-body
collision both objects receive impulses of equal magnitude but opposite direction.

```
Ball hits wall:
  Wall exerts force  +F  on ball  (bounces ball away)
  Ball exerts force  -F  on wall  (absorbed; wall is "infinite mass")
```

---

## 10.4 Basic Integration Methods

Integration is how we advance the simulation forward in time. Choosing the right
method affects **accuracy**, **stability**, and **performance**.

### 10.4.1 Explicit (Forward) Euler

The simplest possible approach:

```
acc = force / mass
vel += acc * dt
pos += vel * dt       ← uses the OLD velocity
```

**Problem**: by using the old velocity for the position update, Euler tends to
**overshoot** — it adds energy to the system over time. A spring simulated with
forward Euler will oscillate wider and wider until it explodes.

### 10.4.2 Semi-Implicit (Symplectic) Euler

A tiny re-ordering that makes a huge difference:

```
acc = force / mass
vel += acc * dt       ← update velocity FIRST
pos += vel * dt       ← use the NEW velocity
```

This one change makes the integrator **symplectic** — it conserves energy on average
and is far more stable for oscillatory systems. Most game physics engines use this.

### 10.4.3 Verlet Integration

Stores the *previous* position instead of an explicit velocity:

\[
\mathbf{p}_{new} = 2\,\mathbf{p}_{current} - \mathbf{p}_{old} + \mathbf{a}\,\Delta t^2
\]

```
newPos = 2 * pos - oldPos + acc * dt * dt
oldPos = pos
pos    = newPos
```

Velocity is implicit: \( \mathbf{v} \approx \frac{\mathbf{p}_{current} - \mathbf{p}_{old}}{\Delta t} \).

**Advantages**: excellent stability; trivial to add constraints (just move positions).
Used in cloth simulation, ragdolls, and Hitman's famous physics system.

### 10.4.4 RK4 (Runge-Kutta 4th Order)

Evaluates the derivative at **four** sample points per step and blends them:

\[
\mathbf{y}_{n+1} = \mathbf{y}_n + \frac{\Delta t}{6}(k_1 + 2k_2 + 2k_3 + k_4)
\]

where \( k_1 \ldots k_4 \) are slope evaluations at the start, two midpoints, and the end
of the time step. Very accurate, but 4× the derivative evaluations per frame. Overkill
for most games unless you're doing orbital mechanics or need high precision.

### Comparison Table

| Method              | Accuracy | Stability  | Cost    | Typical use               |
|---------------------|----------|------------|---------|---------------------------|
| Forward Euler       | O(dt)    | Poor       | Lowest  | Quick prototypes          |
| Semi-implicit Euler | O(dt)    | Good       | Low     | Most game physics engines |
| Verlet              | O(dt²)   | Very good  | Low     | Cloth, ragdoll, particles |
| RK4                 | O(dt⁴)   | Very good  | 4× Euler| Orbital sims, precision   |

---

## 10.5 Gravity

### Constant gravity

In most games, gravity is simply a constant downward acceleration:

\[
\mathbf{a}_{gravity} = (0,\; -g,\; 0), \quad g \approx 9.81\;\text{m/s}^2
\]

Many platformers use a **stronger** gravity (e.g., \( g = 20 \)) so jumps feel snappy.

### Projectile motion

When the only force is gravity, motion separates neatly into horizontal and vertical:

\[
x(t) = x_0 + v_{x}\,t
\]
\[
y(t) = y_0 + v_{y}\,t - \tfrac{1}{2}\,g\,t^2
\]

The trajectory is a **parabola**.

```
              *   *
           *         *
         *              *
       *                  *    ← parabolic arc
     *                      *
   *                          *
  🏀                            ___ground___
  launch                     landing
```

### Example: throwing a ball

A ball is thrown from position \( (0, 2) \) with velocity \( (10, 15) \) m/s,
\( g = 9.81 \) m/s².

**When does it hit the ground?** Solve \( y(t) = 0 \):

\[
0 = 2 + 15t - 4.905t^2
\]

Using the quadratic formula:

\[
t = \frac{-15 \pm \sqrt{225 + 4 \cdot 4.905 \cdot 2}}{-2 \cdot 4.905}
  = \frac{-15 \pm \sqrt{264.24}}{-9.81}
\]
\[
t \approx 3.19 \;\text{s}  \quad \text{(taking the positive root)}
\]

**Where does it land?**

\[
x = 0 + 10 \cdot 3.19 = 31.9 \;\text{m}
\]

```
function predictLanding(pos, vel, gravity):
    // Solve: 0 = pos.y + vel.y*t - 0.5*gravity*t^2
    a = -0.5 * gravity
    b = vel.y
    c = pos.y
    discriminant = b*b - 4*a*c
    t = (-b - sqrt(discriminant)) / (2 * a)
    landX = pos.x + vel.x * t
    return (landX, 0), t
```

---

## 10.6 Drag and Friction

### Air resistance (drag)

Without drag, a falling object accelerates forever. Real objects hit a **terminal
velocity** because drag opposes motion.

**Linear drag** (slow speeds, small objects):

\[
\mathbf{F}_{drag} = -k \, \mathbf{v}
\]

**Quadratic drag** (faster speeds, more realistic):

\[
\mathbf{F}_{drag} = -k \, |\mathbf{v}| \, \mathbf{v}
\]

The quadratic model means drag grows with the *square* of speed — double your speed
and drag quadruples. This is why terminal velocity exists: eventually drag equals
gravity and acceleration drops to zero.

```
function applyDrag(body, dragCoeff):
    speed = length(body.vel)
    if speed > 0:
        dragForce = -dragCoeff * speed * body.vel   // quadratic
        body.addForce(dragForce)
```

### Ground friction

| Type     | When it applies                                 | Magnitude                    |
|----------|-------------------------------------------------|------------------------------|
| **Static**  | Object is stationary; resists starting motion | \( f_s \leq \mu_s \, N \)   |
| **Kinetic** | Object is already sliding                     | \( f_k = \mu_k \, N \)      |

\( N \) is the normal force (how hard the ground pushes up — usually \( mg \) on flat
ground). \( \mu \) is the friction coefficient (ice ≈ 0.03, rubber on concrete ≈ 0.8).

### Example: a car braking

```
Car: mass = 1500 kg, speed = 30 m/s, μ_k = 0.7

Normal force:  N = m * g = 1500 * 9.81 = 14715 N
Friction:      f = μ_k * N = 0.7 * 14715 = 10300.5 N
Deceleration:  a = f / m = 10300.5 / 1500 = 6.87 m/s²

Stopping time: t = v / a = 30 / 6.87 ≈ 4.37 s
Stopping dist: d = v² / (2a) = 900 / 13.74 ≈ 65.5 m
```

---

## 10.7 Impulse and Momentum

### Momentum

\[
\mathbf{p} = m \, \mathbf{v}
\]

Momentum is **mass times velocity**. A heavy object moving slowly can have the same
momentum as a light object moving fast.

### Impulse

An impulse is a sudden change in momentum:

\[
\mathbf{J} = \mathbf{F} \cdot \Delta t = \Delta \mathbf{p} = m \, \Delta \mathbf{v}
\]

Rather than applying a force over many frames, you can apply an **impulse** in a single
frame. This is how collisions are typically resolved — compute the required impulse and
apply it instantly.

```
function applyImpulse(body, impulse):
    body.vel += impulse / body.mass
```

### Conservation of momentum

In any collision (assuming no external forces during the instant of contact):

\[
m_1 \mathbf{v}_1 + m_2 \mathbf{v}_2 = m_1 \mathbf{v}_1' + m_2 \mathbf{v}_2'
\]

The total momentum before = total momentum after. This is the fundamental law used to
solve collision responses.

---

## 10.8 Collision Response

### Elastic vs. inelastic

| Type              | Kinetic energy | Example                    |
|-------------------|----------------|----------------------------|
| Perfectly elastic | Conserved      | Ideal billiard balls       |
| Inelastic         | Lost (→ heat, sound, deformation) | Car crash     |
| Perfectly inelastic | Maximum loss (objects stick) | Clay balls sticking |

### Coefficient of restitution

\[
e = \frac{\text{relative speed after}}{\text{relative speed before}}
\]

- \( e = 1 \): perfectly elastic (superball)
- \( e = 0 \): perfectly inelastic (lump of clay)
- \( 0 < e < 1 \): typical real objects

### 1D collision formula

For two objects colliding head-on:

\[
v_1' = \frac{(m_1 - e \cdot m_2)\,v_1 + (1 + e)\,m_2\,v_2}{m_1 + m_2}
\]
\[
v_2' = \frac{(m_2 - e \cdot m_1)\,v_2 + (1 + e)\,m_1\,v_1}{m_1 + m_2}
\]

For perfectly elastic (\( e = 1 \)):

\[
v_1' = \frac{(m_1 - m_2)\,v_1 + 2\,m_2\,v_2}{m_1 + m_2}
\]

### Example: two billiard balls

Both balls have mass \( m = 0.17 \) kg. Ball 1 moves at 5 m/s, ball 2 is stationary.
\( e = 0.95 \) (nearly elastic).

\[
v_1' = \frac{(0.17 - 0.95 \cdot 0.17) \cdot 5 + (1 + 0.95) \cdot 0.17 \cdot 0}{0.17 + 0.17}
     = \frac{0.17 \cdot 0.05 \cdot 5}{0.34}
     = \frac{0.0425}{0.34}
     \approx 0.125 \;\text{m/s}
\]

\[
v_2' = \frac{(0.17 - 0.95 \cdot 0.17) \cdot 0 + (1 + 0.95) \cdot 0.17 \cdot 5}{0.34}
     = \frac{1.95 \cdot 0.17 \cdot 5}{0.34}
     = \frac{1.6575}{0.34}
     \approx 4.875 \;\text{m/s}
\]

Ball 1 nearly stops; ball 2 picks up almost all the speed. Classic pool shot.

```
function resolveCollision1D(m1, v1, m2, v2, restitution):
    v1_new = ((m1 - restitution*m2)*v1 + (1+restitution)*m2*v2) / (m1+m2)
    v2_new = ((m2 - restitution*m1)*v2 + (1+restitution)*m1*v1) / (m1+m2)
    return v1_new, v2_new
```

---

## 10.9 Angular Motion

So far everything has been **linear** (translation). Objects can also **rotate**.

### Key quantities

| Linear               | Angular                            |
|----------------------|------------------------------------|
| Position \( \mathbf{p} \) | Angle \( \theta \)            |
| Velocity \( \mathbf{v} \) | Angular velocity \( \boldsymbol{\omega} \) (rad/s) |
| Acceleration \( \mathbf{a} \) | Angular acceleration \( \boldsymbol{\alpha} \) |
| Force \( \mathbf{F} \)   | Torque \( \boldsymbol{\tau} \) |
| Mass \( m \)              | Moment of inertia \( I \)     |

### Torque

Torque is the rotational equivalent of force:

\[
\boldsymbol{\tau} = \mathbf{r} \times \mathbf{F}
\]

where \( \mathbf{r} \) is the vector from the center of mass to the point where the
force is applied. The **cross product** means forces applied farther from the pivot,
or more perpendicular to the lever arm, produce more torque.

```
      ──────────⊙──────────
      pivot          │
                     │ r
                     │
                     ▼ F
           (force applied at end of lever)

Torque = |r| × |F| × sin(angle between r and F)
```

### Moment of inertia

The rotational equivalent of mass. How hard it is to spin an object.

| Shape             | Moment of inertia \( I \)          |
|-------------------|------------------------------------|
| Solid sphere      | \( \frac{2}{5} m r^2 \)           |
| Hollow sphere     | \( \frac{2}{3} m r^2 \)           |
| Solid cylinder    | \( \frac{1}{2} m r^2 \)           |
| Thin rod (center) | \( \frac{1}{12} m L^2 \)          |

Newton's second law for rotation:

\[
\boldsymbol{\tau} = I \, \boldsymbol{\alpha}
\quad \Longrightarrow \quad
\boldsymbol{\alpha} = \frac{\boldsymbol{\tau}}{I}
\]

### Example: spinning a wheel

A solid disc of mass 5 kg, radius 0.3 m. You apply 10 N tangentially at the rim.

\[
I = \frac{1}{2} m r^2 = \frac{1}{2} \cdot 5 \cdot 0.09 = 0.225 \;\text{kg·m}^2
\]
\[
\tau = r \times F = 0.3 \times 10 = 3 \;\text{N·m}
\]
\[
\alpha = \frac{\tau}{I} = \frac{3}{0.225} = 13.33 \;\text{rad/s}^2
\]

After 2 seconds (starting from rest):

\[
\omega = \alpha \cdot t = 13.33 \times 2 = 26.67 \;\text{rad/s} \approx 4.24 \;\text{rev/s}
\]

### Integration for angular motion

```
function angularStep(body, dt):
    body.angularVel += body.torque / body.inertia * dt
    body.angle      += body.angularVel * dt
    body.torque      = 0   // reset accumulated torque
```

---

## 10.10 Springs and Damping

### Hooke's Law

A spring exerts a restoring force proportional to how far it's stretched:

\[
F = -k \, x
\]

where \( k \) is the **spring constant** (stiffness, in N/m) and \( x \) is the
displacement from rest length.

```
  rest length
  |<-------->|

  ╔══════════╗
  ║~~~~~~~~~~║-----> +x (stretched: F pulls back ←)
  ╚══════════╝
```

### Damped spring

Without damping, a spring oscillates forever. Adding a velocity-dependent term damps
the oscillation:

\[
F = -k \, x - b \, v
\]

where \( b \) is the damping coefficient.

```
                  Undamped               Critically damped
Displacement  ─┐                    ─┐
               │  /\    /\    /\     │  ╲
               │ /  \  /  \  /  \    │    ╲_______________
               │/    \/    \/    \   │
               └──────────────────   └──────────────────
                      Time                   Time
```

The three damping regimes:

| Regime              | Condition              | Behavior                   |
|---------------------|------------------------|----------------------------|
| Underdamped         | \( b < 2\sqrt{km} \)   | Oscillates, amplitude decays |
| Critically damped   | \( b = 2\sqrt{km} \)   | Returns to rest fastest, no oscillation |
| Overdamped          | \( b > 2\sqrt{km} \)   | Slow return, no oscillation |

### Example: car suspension

```
function springForce(anchorPos, objectPos, restLength, k, b, objectVel):
    displacement = objectPos - anchorPos
    dist = length(displacement)
    direction = displacement / dist

    stretch = dist - restLength              // positive = stretched
    springF = -k * stretch                   // restoring force
    dampF   = -b * dot(objectVel, direction) // damping along spring axis

    return (springF + dampF) * direction
```

Each wheel of a car has a spring connecting the chassis to the wheel. When the car hits
a bump, the spring compresses, then pushes back. The damper prevents infinite bouncing.

### Example: wobbly antenna

Attach a small mass to the tip of a spring anchored to a car's roof. As the car moves
over bumps, the antenna wiggles with damped oscillation — cheap and visually pleasing.

---

## 10.11 Constraints and Joints

Full constraint solvers are complex, but the core idea is simple: after a physics step,
**correct** positions and velocities so they satisfy the constraint.

### Common constraint types

| Constraint          | Description                                             |
|---------------------|---------------------------------------------------------|
| **Distance**        | Two points must stay a fixed distance apart (rope, rod) |
| **Contact**         | Objects must not overlap; push apart if they do          |
| **Hinge (revolute)**| Two bodies share a point; one axis of rotation           |
| **Ball-and-socket** | Two bodies share a point; free rotation                  |
| **Slider (prismatic)** | Movement along one axis only                         |

### Verlet-style distance constraint

```
function solveDistanceConstraint(p1, p2, restLength):
    delta = p2 - p1
    dist  = length(delta)
    diff  = (dist - restLength) / dist
    p1 += delta * 0.5 * diff   // push p1 toward p2
    p2 -= delta * 0.5 * diff   // push p2 toward p1
```

Iterate this many times per frame for stability. This is how cloth simulation works —
a grid of particles connected by distance constraints.

```
  o---o---o---o---o
  |   |   |   |   |
  o---o---o---o---o     Each 'o' is a particle.
  |   |   |   |   |     Each '---' and '|' is a distance constraint.
  o---o---o---o---o
  |   |   |   |   |
  o---o---o---o---o
```

---

## 10.12 Fixed Timestep

### The problem with variable dt

If your physics uses the raw frame time as `dt`:

- At 60 FPS, `dt ≈ 0.016 s` — physics behaves well.
- At 15 FPS (lag spike), `dt ≈ 0.066 s` — physics takes huge steps and objects tunnel
  through walls, springs explode, collisions are missed.

Even small variations cause **non-determinism** — replays diverge and networked games
desync.

### The solution: fixed timestep with an accumulator

```
PHYSICS_DT = 1.0 / 60.0   // fixed 60 Hz physics
accumulator = 0.0

function gameLoop():
    frameTime = getElapsedSinceLastFrame()
    frameTime = min(frameTime, 0.25)   // clamp to prevent spiral of death
    accumulator += frameTime

    while accumulator >= PHYSICS_DT:
        physicsTick(PHYSICS_DT)
        accumulator -= PHYSICS_DT

    alpha = accumulator / PHYSICS_DT
    renderInterpolated(alpha)
```

### Why clamp frame time?

If the game freezes for 2 seconds (e.g., loading), the accumulator would demand 120
physics steps to catch up, causing another freeze. Clamping prevents this "spiral of
death."

### Interpolation for smooth rendering

Physics runs at a fixed rate, but rendering may run faster (144 Hz monitor). To avoid
visual jitter, **interpolate** between the previous and current physics state:

\[
\text{renderPos} = \text{prevPos} \cdot (1 - \alpha) + \text{currPos} \cdot \alpha
\]

where \( \alpha \) is how far into the next physics step we are (0 to 1).

```
Timeline:
  Physics:  |-------|-------|-------|
  Render:   | . . . | . . . | . .
             ^               ^
             interpolated    interpolated
```

---

## 10.13 Summary and Practical Tips

### Key formulas at a glance

| Concept             | Formula                                                |
|---------------------|--------------------------------------------------------|
| Newton's 2nd law    | \( \mathbf{a} = \mathbf{F} / m \)                     |
| Euler integration   | \( \mathbf{v} \mathrel{+}= \mathbf{a}\,dt;\; \mathbf{p} \mathrel{+}= \mathbf{v}\,dt \) |
| Verlet integration  | \( \mathbf{p}_{new} = 2\mathbf{p} - \mathbf{p}_{old} + \mathbf{a}\,dt^2 \) |
| Gravity             | \( \mathbf{a} = (0, -g, 0) \)                         |
| Projectile          | \( y(t) = y_0 + v_y t - \frac{1}{2}g t^2 \)           |
| Drag (quadratic)    | \( \mathbf{F} = -k \|\mathbf{v}\| \mathbf{v} \)       |
| Momentum            | \( \mathbf{p} = m\mathbf{v} \)                        |
| Impulse             | \( \mathbf{J} = \Delta(m\mathbf{v}) \)                |
| Torque              | \( \boldsymbol{\tau} = \mathbf{r} \times \mathbf{F} \)|
| Hooke's law         | \( F = -k\,x \)                                       |

### Practical tips

1. **Use semi-implicit Euler** unless you have a reason not to. It's simple and stable.
2. **Always use a fixed timestep** for physics. Variable dt leads to bugs.
3. **Tune gravity to feel good**, not to match reality. Platformers often use 2–3× Earth gravity.
4. **Add drag** to everything. Without it, objects drift forever and feel floaty.
5. **Debug visually**: draw velocity vectors, force arrows, and constraint lines.
6. **Start simple**: get basic movement and gravity working before adding collisions, springs, and angular physics.
7. **Separate physics from rendering**: physics at fixed rate, rendering as fast as possible, interpolate between.

---

*Next chapter: [Lighting and Shading](11-lighting-and-shading.md)*
