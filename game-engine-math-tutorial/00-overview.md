# Game Engine Mathematics Tutorial

> Based on *Foundations of Game Engine Development, Volume 1: Mathematics* by Eric Lengyel

This tutorial series breaks down the essential mathematics used in game engine development into digestible, example-rich chapters. Whether you're building a physics engine, a renderer, or a full game engine, mastering these concepts is non-negotiable.

## Chapter Overview

| Chapter | Topic | What You'll Learn |
|---------|-------|-------------------|
| [Chapter 1](./01-vectors-and-matrices.md) | **Vectors and Matrices** | Vector operations, dot/cross products, matrix multiplication, determinants, inverses |
| [Chapter 2](./02-transforms.md) | **Transforms** | Rotations, reflections, scales, skews, homogeneous coordinates, quaternions |
| [Chapter 3](./03-geometry.md) | **Geometry** | Triangle meshes, normals, lines, rays, planes, Plücker coordinates |
| [Chapter 4](./04-advanced-algebra.md) | **Advanced Algebra** | Grassmann algebra, projective geometry, geometric algebra, rotors |

## Prerequisites

- Solid understanding of basic trigonometry (sin, cos, tan)
- Working knowledge of C++ (for code examples)
- Familiarity with floating-point numbers

## Conventions Used

- **Vectors** are written in **bold lowercase**: **v**, **a**, **b**
- **Matrices** are written in **bold uppercase**: **M**, **A**, **B**
- **Scalars** are written in *italic*: *t*, *s*, *d*
- Zero-based indexing is used throughout (matching C++ conventions)
- Column vectors are the default convention
- Right-handed coordinate systems are assumed

## How to Read This Tutorial

Each chapter builds on the previous one, but the first three chapters can be read somewhat independently. Chapter 4 assumes familiarity with the notation and concepts from Chapter 3. Every section includes:

- Clear explanations of the theory
- Worked numerical examples
- Geometric intuition with descriptions of visual concepts
- Practical C++ code snippets
- Tips for game engine implementation
