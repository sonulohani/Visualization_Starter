# OpenGL Tutorial — From Zero to Hero

A comprehensive, beginner-friendly OpenGL tutorial covering **everything** from setting up your first window to advanced rendering techniques like PBR, deferred shading, and shadow mapping. All topics are based on the [learnopengl.com](https://learnopengl.com) curriculum.

## Prerequisites

- Basic knowledge of **C++** (variables, functions, classes, pointers)
- A C++ compiler (GCC, Clang, or MSVC)
- CMake 3.16+
- Basic math (we explain the rest as we go)

## How to Build

```bash
mkdir build && cd build
cmake ..
make
```

Each chapter has its own executable. After building, you'll find them in `build/bin/`.

## Table of Contents

### 1. Getting Started

| # | Chapter | Description |
|---|---------|-------------|
| 1.1 | [What is OpenGL?](docs/01_getting_started/01_opengl.md) | History, graphics pipeline overview, OpenGL vs Vulkan |
| 1.2 | [The OpenGL Context](docs/01_getting_started/02_opengl_context.md) | What a context is, why it's needed, shared contexts, multi-threaded rendering |
| 1.3 | [Creating a Window](docs/01_getting_started/03_creating_a_window.md) | Setting up GLFW, GLAD, and your first window |
| 1.4 | [Hello Window](docs/01_getting_started/04_hello_window.md) | Window loop, input handling, clearing the screen |
| 1.5 | [Hello Triangle](docs/01_getting_started/05_hello_triangle.md) | Vertices, VAO, VBO, shaders, drawing your first triangle |
| 1.6 | [Shaders](docs/01_getting_started/06_shaders.md) | GLSL in depth, uniforms, vertex attributes, shader class |
| 1.7 | [Textures](docs/01_getting_started/07_textures.md) | Loading images, texture coordinates, texture units, mixing |
| 1.8 | [Transformations](docs/01_getting_started/08_transformations.md) | Vectors, matrices, GLM, translate/rotate/scale |
| 1.9 | [Coordinate Systems](docs/01_getting_started/09_coordinate_systems.md) | Model/View/Projection, perspective, orthographic, 3D scene |
| 1.10 | [Camera](docs/01_getting_started/10_camera.md) | Look-at matrix, FPS camera, mouse input, zoom |
| 1.11 | [Review](docs/01_getting_started/11_review.md) | Summary and exercises |

### 2. Lighting

| # | Chapter | Description |
|---|---------|-------------|
| 2.1 | [Colors](docs/02_lighting/01_colors.md) | How colors work in OpenGL, light and object colors |
| 2.2 | [Basic Lighting](docs/02_lighting/02_basic_lighting.md) | Ambient, diffuse, specular (Phong model) |
| 2.3 | [Materials](docs/02_lighting/03_materials.md) | Material properties, different material looks |
| 2.4 | [Lighting Maps](docs/02_lighting/04_lighting_maps.md) | Diffuse and specular maps, emission maps |
| 2.5 | [Light Casters](docs/02_lighting/05_light_casters.md) | Directional, point, and spotlights |
| 2.6 | [Multiple Lights](docs/02_lighting/06_multiple_lights.md) | Combining different light types in one scene |
| 2.7 | [Review](docs/02_lighting/07_review.md) | Summary and exercises |

### 3. Model Loading

| # | Chapter | Description |
|---|---------|-------------|
| 3.1 | [Assimp](docs/03_model_loading/01_assimp.md) | What is Assimp, installing, basic usage |
| 3.2 | [Mesh](docs/03_model_loading/02_mesh.md) | Mesh class design, vertex data, index drawing |
| 3.3 | [Model](docs/03_model_loading/03_model.md) | Loading a complete 3D model, rendering it |

### 4. Advanced OpenGL

| # | Chapter | Description |
|---|---------|-------------|
| 4.1 | [Depth Testing](docs/04_advanced_opengl/01_depth_testing.md) | Z-buffer, depth functions, visualizing depth |
| 4.2 | [Stencil Testing](docs/04_advanced_opengl/02_stencil_testing.md) | Stencil buffer, object outlining |
| 4.3 | [Blending](docs/04_advanced_opengl/03_blending.md) | Transparency, alpha testing, sorting |
| 4.4 | [Face Culling](docs/04_advanced_opengl/04_face_culling.md) | Winding order, back-face culling |
| 4.5 | [Framebuffers](docs/04_advanced_opengl/05_framebuffers.md) | Render to texture, post-processing effects |
| 4.6 | [Cubemaps](docs/04_advanced_opengl/06_cubemaps.md) | Skyboxes, environment mapping, reflection |
| 4.7 | [Advanced Data](docs/04_advanced_opengl/07_advanced_data.md) | Buffer types, mapping, batching |
| 4.8 | [Advanced GLSL](docs/04_advanced_opengl/08_advanced_glsl.md) | Interface blocks, uniform buffers, built-in variables |
| 4.9 | [Geometry Shader](docs/04_advanced_opengl/09_geometry_shader.md) | Generating new primitives, exploding objects, normals |
| 4.10 | [Instancing](docs/04_advanced_opengl/10_instancing.md) | Drawing thousands of objects efficiently |
| 4.11 | [Anti-Aliasing](docs/04_advanced_opengl/11_anti_aliasing.md) | MSAA, off-screen multisampling |

### 5. Advanced Lighting

| # | Chapter | Description |
|---|---------|-------------|
| 5.1 | [Advanced Lighting](docs/05_advanced_lighting/01_advanced_lighting.md) | Blinn-Phong shading model |
| 5.2 | [Gamma Correction](docs/05_advanced_lighting/02_gamma_correction.md) | Linear vs sRGB space, correct lighting |
| 5.3 | [Shadow Mapping](docs/05_advanced_lighting/03_shadow_mapping.md) | Directional light shadows, depth maps, PCF |
| 5.4 | [Point Shadows](docs/05_advanced_lighting/04_point_shadows.md) | Omnidirectional shadow maps, cubemap shadows |
| 5.5 | [Normal Mapping](docs/05_advanced_lighting/05_normal_mapping.md) | Tangent space, bumpy surfaces without extra geometry |
| 5.6 | [Parallax Mapping](docs/05_advanced_lighting/06_parallax_mapping.md) | Depth illusion, steep parallax, relief mapping |
| 5.7 | [HDR](docs/05_advanced_lighting/07_hdr.md) | High dynamic range, floating point framebuffers, tone mapping |
| 5.8 | [Bloom](docs/05_advanced_lighting/08_bloom.md) | Bright-pass filter, Gaussian blur, glow effect |
| 5.9 | [Deferred Shading](docs/05_advanced_lighting/09_deferred_shading.md) | G-buffer, deferred vs forward, many lights |
| 5.10 | [SSAO](docs/05_advanced_lighting/10_ssao.md) | Screen-space ambient occlusion |

### 6. PBR (Physically Based Rendering)

| # | Chapter | Description |
|---|---------|-------------|
| 6.1 | [Theory](docs/06_pbr/01_theory.md) | Microfacet model, energy conservation, reflectance equation |
| 6.2 | [Lighting](docs/06_pbr/02_lighting.md) | PBR direct lighting, Cook-Torrance BRDF |
| 6.3 | [Diffuse Irradiance (IBL)](docs/06_pbr/03_ibl_diffuse.md) | Image-based lighting, irradiance maps |
| 6.4 | [Specular IBL](docs/06_pbr/04_ibl_specular.md) | Pre-filtered environment maps, BRDF LUT |

### 7. In Practice

| # | Chapter | Description |
|---|---------|-------------|
| 7.1 | [Debugging](docs/07_in_practice/01_debugging.md) | glGetError, debug output, RenderDoc |
| 7.2 | [Text Rendering](docs/07_in_practice/02_text_rendering.md) | FreeType, bitmap fonts, rendering text |
| 7.3 | [2D Game (Breakout)](docs/07_in_practice/03_breakout.md) | Putting it all together in a real game |

## Source Code

All source code examples are in the `src/` directory, organized by chapter. Each example is self-contained and can be built independently.

## Dependencies

| Library | Purpose |
|---------|---------|
| [GLFW](https://www.glfw.org/) | Window creation and input handling |
| [GLAD](https://glad.dav1d.de/) | OpenGL function loader |
| [GLM](https://github.com/g-truc/glm) | Mathematics library for graphics |
| [stb_image](https://github.com/nothings/stb) | Image loading |
| [Assimp](https://github.com/assimp/assimp) | 3D model loading |
| [FreeType](https://freetype.org/) | Font rendering |

## License

This tutorial is for educational purposes. Code examples are free to use and modify.
