# The Complete Vulkan Tutorial

> A beginner-friendly, chapter-by-chapter guide to mastering the Vulkan Graphics API — from absolute zero to advanced rendering.

---

## How to Use This Tutorial

Each chapter builds on the previous one. Start from Chapter 1 and work your way through. Every chapter includes:

- **Concept explanations** in plain English with analogies
- **Diagrams** using ASCII art
- **Complete, working code examples**
- **"Try It Yourself"** exercises at the end
- **Common mistakes** and how to avoid them

---

## Part I — Foundations

| # | Chapter | What You'll Learn |
|---|---------|-------------------|
| 01 | [Introduction to Vulkan](chapters/01-introduction.md) | What Vulkan is, how it differs from OpenGL, and when to use it |
| 02 | [Development Environment Setup](chapters/02-setup.md) | Install the SDK, set up CMake, verify everything works |
| 03 | [Your First Vulkan Instance](chapters/03-instance.md) | The root Vulkan object — creating your connection to the GPU driver |
| 04 | [Validation Layers](chapters/04-validation-layers.md) | Error checking, debug messenger, reading validation messages |
| 05 | [Physical Devices & Queue Families](chapters/05-physical-devices.md) | Discovering GPUs, understanding queue families, picking the right GPU |
| 06 | [Logical Device & Queues](chapters/06-logical-device.md) | Creating your application's handle to the GPU and retrieving queues |

## Part II — Presenting to the Screen

| # | Chapter | What You'll Learn |
|---|---------|-------------------|
| 07 | [Window Surface & Swap Chain](chapters/07-surface-swapchain.md) | Connecting Vulkan to a window, managing presentation images |
| 08 | [Image Views](chapters/08-image-views.md) | How to interpret raw image data for use in rendering |

## Part III — The Rendering Pipeline

| # | Chapter | What You'll Learn |
|---|---------|-------------------|
| 09 | [Graphics Pipeline & Shaders](chapters/09-pipeline-shaders.md) | The full graphics pipeline, writing GLSL, compiling to SPIR-V |
| 10 | [Render Passes & Framebuffers](chapters/10-renderpasses-framebuffers.md) | Describing render operations and binding images to them |
| 11 | [Command Buffers](chapters/11-command-buffers.md) | Recording GPU commands, command pools, primary vs secondary |
| 12 | [Drawing & Synchronization](chapters/12-drawing-sync.md) | The draw loop, semaphores, fences, putting it all together |
| 13 | [Frames in Flight & Swap Chain Recreation](chapters/13-frames-in-flight.md) | Overlapping CPU/GPU work, handling window resize |

## Part IV — Geometry Data

| # | Chapter | What You'll Learn |
|---|---------|-------------------|
| 14 | [Vertex Buffers](chapters/14-vertex-buffers.md) | Passing vertex data to shaders, memory types |
| 15 | [Staging Buffers & GPU Memory](chapters/15-staging-buffers.md) | Efficient data transfer, device-local memory |
| 16 | [Index Buffers](chapters/16-index-buffers.md) | Reusing vertices, drawing complex shapes efficiently |

## Part V — Uniforms & Textures

| # | Chapter | What You'll Learn |
|---|---------|-------------------|
| 17 | [Uniform Buffers & Descriptor Sets](chapters/17-uniforms-descriptors.md) | Passing matrices and data to shaders, the descriptor system |
| 18 | [Texture Mapping & Samplers](chapters/18-textures-samplers.md) | Loading images, sampling, filtering, wrapping modes |
| 19 | [Depth Buffering](chapters/19-depth-buffering.md) | Correct 3D occlusion with the depth buffer |

## Part VI — Real-World Rendering

| # | Chapter | What You'll Learn |
|---|---------|-------------------|
| 20 | [Loading 3D Models](chapters/20-loading-models.md) | Loading OBJ files, deduplicating vertices |
| 21 | [Mipmaps & Multisampling](chapters/21-mipmaps-msaa.md) | Texture quality improvements and anti-aliasing |

## Part VII — Advanced Topics

| # | Chapter | What You'll Learn |
|---|---------|-------------------|
| 22 | [Push Constants](chapters/22-push-constants.md) | Fastest way to pass small data to shaders |
| 23 | [Compute Shaders](chapters/23-compute-shaders.md) | General-purpose GPU computing, particle systems |
| 24 | [Advanced Render Passes](chapters/24-advanced-renderpasses.md) | Multiple subpasses, deferred rendering |
| 25 | [Memory Management](chapters/25-memory-management.md) | VMA library, sub-allocation, memory pools |
| 26 | [Debugging & Profiling](chapters/26-debugging-profiling.md) | RenderDoc, GPU timestamps, debug labels |
| 27 | [Best Practices & What's Next](chapters/27-best-practices.md) | Performance tips, learning roadmap, resources |

---

## Prerequisites

Before starting, you should be comfortable with:

- **C++ (C++17)** — classes, templates, vectors, smart pointers
- **Basic math** — vectors, matrices (we'll explain as we go)
- **A terminal/command line** — running commands, navigating directories

No prior graphics programming experience is needed!

---

## Quick Start

```bash
# Clone or create your project
mkdir vulkan-project && cd vulkan-project

# Follow Chapter 02 to set up your environment
# Then work through each chapter in order
```

---

*Each chapter is self-contained with full code examples. Happy learning!*
