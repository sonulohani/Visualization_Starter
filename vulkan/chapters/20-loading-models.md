# Chapter 20: Loading 3D Models

[<< Previous: Depth Buffering](19-depth-buffering.md) | [Next: Mipmaps & MSAA >>](21-mipmaps-msaa.md)

---

## What We've Built So Far

```
✅ Instance           → Connection to Vulkan
✅ Validation Layers  → Error checking
✅ Surface            → Window connection
✅ Physical Device    → GPU selected
✅ Logical Device     → GPU interface
✅ Swap Chain         → Presentation images (+recreation)
✅ Image Views        → Image interpretation
✅ Render Pass        → Rendering blueprint
✅ Pipeline           → Complete render configuration
✅ Framebuffers       → Image-to-renderpass bindings
✅ Command Pool/Bufs  → Command recording (per frame in flight)
✅ Sync Objects       → Fences + semaphores (per frame in flight)
✅ Draw Loop          → Working frames in flight!
✅ Vertex Buffer      → Vertex data on GPU
✅ Staging Buffers    → Optimized DEVICE_LOCAL memory
✅ Index Buffer       → Vertex reuse
✅ Uniform Buffers    → MVP matrices, spinning geometry!
✅ Descriptor Sets    → Shader resource binding
✅ Texture Mapping    → Images on geometry!
✅ Depth Buffering    → Correct 3D occlusion!

🎯 Now: Load real 3D models from files instead of hardcoded geometry!
```

---

## The Problem: Hardcoded Geometry

So far, our "3D scene" is a rectangle defined by 4 vertices and 6 indices typed directly in C++. Real games and applications use meshes with **thousands to millions** of triangles created in tools like Blender, Maya, or ZBrush.

```
Our current approach:            What real applications do:

  const std::vector<Vertex>        ┌──────────────┐
  vertices = {                     │   Blender /  │
    {{-0.5, -0.5, 0.0}, ...},     │   Maya       │
    {{ 0.5, -0.5, 0.0}, ...},     │              │──export──→ model.obj
    {{ 0.5,  0.5, 0.0}, ...},     └──────────────┘           (thousands
    {{-0.5,  0.5, 0.0}, ...}                                  of vertices)
  };                                                              │
                                                            load at runtime
  4 vertices, 2 triangles.                                        │
  Typed by hand. 😅                                              ↓
                                                          std::vector<Vertex>
                                                          (from file data)
```

We need to:
1. Understand a 3D model file format
2. Load and parse vertex/index data at runtime
3. Handle vertex deduplication efficiently

---

## The OBJ File Format

**OBJ** (Wavefront OBJ) is one of the simplest and most widely-supported 3D model formats. It's a plain text file that stores geometry data line by line.

### OBJ File Anatomy

```
# This is a comment
# A simple cube

# Vertex positions (v x y z)
v -1.0  1.0  1.0
v  1.0  1.0  1.0
v  1.0 -1.0  1.0
v -1.0 -1.0  1.0
v -1.0  1.0 -1.0
v  1.0  1.0 -1.0
v  1.0 -1.0 -1.0
v -1.0 -1.0 -1.0

# Texture coordinates (vt u v)
vt 0.0 0.0
vt 1.0 0.0
vt 1.0 1.0
vt 0.0 1.0

# Vertex normals (vn x y z)
vn 0.0  0.0  1.0
vn 1.0  0.0  0.0
vn 0.0  0.0 -1.0

# Faces (f v/vt/vn v/vt/vn v/vt/vn)
f 1/1/1 2/2/1 3/3/1
f 1/1/1 3/3/1 4/4/1
```

### OBJ Line Types

| Prefix | Meaning | Example |
|--------|---------|---------|
| `#` | Comment | `# my model` |
| `v` | Vertex position (x, y, z) | `v 1.0 2.0 3.0` |
| `vt` | Texture coordinate (u, v) | `vt 0.5 0.5` |
| `vn` | Vertex normal (x, y, z) | `vn 0.0 1.0 0.0` |
| `f` | Face (indices into v/vt/vn) | `f 1/1/1 2/2/1 3/3/1` |
| `mtllib` | Material library file | `mtllib model.mtl` |
| `usemtl` | Use material | `usemtl wood` |

### How OBJ Faces Reference Data

```
OBJ stores positions, UVs, and normals in SEPARATE arrays.
Each face vertex references indices into all three arrays:

  f vertex/texcoord/normal

  Positions:  v[1] v[2] v[3] v[4] ...    (1-indexed!)
  TexCoords: vt[1] vt[2] vt[3] vt[4] ...
  Normals:   vn[1] vn[2] vn[3] ...

  Face: f 1/3/2  2/4/2  3/1/2
            │ │ │
            │ │ └── normal index 2  (vn[2])
            │ └──── texcoord index 3 (vt[3])
            └────── position index 1 (v[1])

  This means: "A triangle whose first vertex uses
   position #1, texcoord #3, and normal #2"
```

Note that OBJ indices are **1-based** (start at 1), not 0-based like C++ arrays!

---

## Using tinyobjloader

[tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) is a single-header C++ library for loading OBJ files. It handles all the parsing complexity for you.

### Setup

Like stb_image, include the header with an implementation define in exactly **one** `.cpp` file:

```cpp
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
```

### The Viking Room Model

The **Viking Room** model is commonly used in Vulkan tutorials. It's a detailed interior scene with textures. You can download it from the [Vulkan Tutorial resources](https://vulkan-tutorial.com/Loading_models).

```
Viking Room model:
  File: viking_room.obj          (~29,000 faces)
  Texture: viking_room.png       (detailed interior texture)

  ┌──────────────────────────────────┐
  │         _______________          │
  │        /              /|         │
  │       /   Viking     / |         │
  │      /   Room       /  |         │
  │     /______________/   |         │
  │     |              |   /         │
  │     |   benches,   |  /          │
  │     |   shields,   | /           │
  │     |   posts      |/            │
  │     |______________|             │
  └──────────────────────────────────┘
```

Place the model files in your project:

```
project/
  ├── models/
  │   └── viking_room.obj
  ├── textures/
  │   └── viking_room.png
  ├── shaders/
  │   ├── shader.vert
  │   └── shader.frag
  └── main.cpp
```

---

## Step 1: Updating the Vertex Struct for 3D

Our vertices need full 3D positions now. Update `pos` from `vec2` to `vec3`:

```cpp
struct Vertex {
    glm::vec3 pos;        // 3D position (was vec2)
    glm::vec3 color;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3>
    getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;  // vec3!
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos &&
               color == other.color &&
               texCoord == other.texCoord;
    }
};
```

### Vertex Memory Layout (Updated)

```
Before (vec2 pos):                     After (vec3 pos):

  ┌──────┬──────────┬────────┐        ┌────────┬──────────┬────────┐
  │ pos  │ color    │texCoord│        │ pos    │ color    │texCoord│
  │ vec2 │ vec3     │ vec2   │        │ vec3   │ vec3     │ vec2   │
  │ 8B   │ 12B     │ 8B     │        │ 12B    │ 12B     │ 8B     │
  └──────┴──────────┴────────┘        └────────┴──────────┴────────┘
   stride = 28 bytes                   stride = 32 bytes
```

---

## Step 2: Loading the Model

Remove the hardcoded vertex and index arrays and replace them with dynamically loaded data:

```cpp
const std::string MODEL_PATH = "models/viking_room.obj";
const std::string TEXTURE_PATH = "textures/viking_room.png";

std::vector<Vertex> vertices;
std::vector<uint32_t> indices;     // uint32_t for large models!

void loadModel() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                           MODEL_PATH.c_str())) {
        throw std::runtime_error(warn + err);
    }

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = {1.0f, 1.0f, 1.0f};

            vertices.push_back(vertex);
            indices.push_back(static_cast<uint32_t>(indices.size()));
        }
    }
}
```

### tinyobjloader Data Structures

```
tinyobj::attrib_t attrib:
  attrib.vertices[]    = flat array of floats: [x0,y0,z0, x1,y1,z1, ...]
  attrib.texcoords[]   = flat array of floats: [u0,v0, u1,v1, ...]
  attrib.normals[]     = flat array of floats: [nx0,ny0,nz0, ...]

tinyobj::shape_t shape:
  shape.mesh.indices[] = array of index structs, each containing:
    .vertex_index     index into attrib.vertices (÷3 for the vertex)
    .texcoord_index   index into attrib.texcoords (÷2 for the UV)
    .normal_index     index into attrib.normals (÷3 for the normal)

  shape.mesh.num_face_vertices[] = number of vertices per face (usually 3)
```

### Accessing OBJ Data

```
OBJ data is stored as flat arrays, so we index like this:

  Position of vertex at index i:
    x = attrib.vertices[3 * i + 0]
    y = attrib.vertices[3 * i + 1]
    z = attrib.vertices[3 * i + 2]

  Texture coord at index j:
    u = attrib.texcoords[2 * j + 0]
    v = attrib.texcoords[2 * j + 1]

  Why multiply by 3 or 2?
  Because they're packed flat: [x0,y0,z0, x1,y1,z1, x2,y2,z2, ...]
                                 └─ vertex 0 ─┘  └─ vertex 1 ─┘
```

### The Texture Coordinate Y-Flip

Notice this line:
```cpp
1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
```

OBJ files use a texture coordinate convention where **V = 0 is at the bottom** of the image. Vulkan (and the texture we loaded with stb_image) has **V = 0 at the top**. Flipping with `1.0 - v` corrects this.

```
OBJ convention:              Vulkan convention:

  (0,1)────────(1,1)          (0,0)────────(1,0)
    │            │              │            │
    │   image    │              │   image    │
    │            │              │            │
  (0,0)────────(1,0)          (0,1)────────(1,1)

  V=0 at BOTTOM               V=0 at TOP

  Fix: v_vulkan = 1.0 - v_obj
```

---

## Step 3: Vertex Deduplication

The simple loading code above creates a **unique vertex for every face corner** — even if the same position/texcoord/color combination appears many times. For the Viking Room model, this means ~87,000 vertices when there are only ~29,000 unique ones.

### The Problem

```
OBJ face indices can create duplicates:

  Face 1: uses position 1, texcoord 3  →  Vertex A
  Face 2: uses position 1, texcoord 3  →  Vertex B (identical to A!)
  Face 3: uses position 1, texcoord 3  →  Vertex C (identical to A!)

Without deduplication:
  vertices = [A, B, C, ...]  (B and C are copies of A)
  indices  = [0, ..., 1, ..., 2, ...]

With deduplication:
  vertices = [A, ...]        (only one copy)
  indices  = [0, ..., 0, ..., 0, ...]  (all point to the same vertex)

  Memory saved: ~66% for typical models!
```

### The Solution: Hash Map

We use `std::unordered_map` to track which unique vertex combinations we've already seen:

```cpp
#include <unordered_map>

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                    (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                    (hash<glm::vec2>()(vertex.texCoord) << 1);
        }
    };
}
```

GLM doesn't provide hash functions by default. Enable them before including GLM:

```cpp
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
```

### Updated loadModel with Deduplication

```cpp
void loadModel() {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                           MODEL_PATH.c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertex.color = {1.0f, 1.0f, 1.0f};

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] =
                    static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }
}
```

### How the Deduplication Works

```
Processing each face vertex:

  1. Assemble a Vertex from the OBJ data (pos + texcoord + color)
  2. Look up the Vertex in the hash map:

     ┌─────────────────────────────────────────────┐
     │ uniqueVertices (hash map)                    │
     │                                              │
     │  Key (Vertex)         → Value (index)        │
     │  ──────────────────    ──────────────        │
     │  {pos1, tc1, col1}   →  0                   │
     │  {pos2, tc2, col2}   →  1                   │
     │  {pos3, tc3, col3}   →  2                   │
     │  ...                                         │
     └─────────────────────────────────────────────┘

  3a. NOT found: Add to vertices[], insert into map with new index
  3b. Found: Just push the existing index into indices[]

  Result: vertices[] has only unique entries, indices[] references them.
```

### Impact on the Viking Room

```
Without deduplication:
  vertices: ~87,000 × 32 bytes = ~2.78 MB
  indices:  ~87,000 × 4 bytes  = ~0.35 MB
  Total: ~3.13 MB

With deduplication:
  vertices: ~29,000 × 32 bytes = ~0.93 MB
  indices:  ~87,000 × 4 bytes  = ~0.35 MB
  Total: ~1.28 MB

  Saved: ~1.85 MB (59% reduction!)
  Plus: vertex cache hits on GPU increase dramatically
```

---

## Step 4: Switching to uint32_t Indices

The Viking Room has ~29,000 unique vertices — well beyond the 65,535 limit of `uint16_t`. Update the index buffer to use 32-bit indices:

```cpp
// In createIndexBuffer:
VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
//                        ^^^^^^^^^^^^^ now sizeof(uint32_t) = 4

// In recordCommandBuffer:
vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0,
                     VK_INDEX_TYPE_UINT32);  // was UINT16
```

### Index Type Comparison

| Type | Size | Max Vertices | When to Use |
|------|------|-------------|-------------|
| `uint16_t` / `UINT16` | 2 bytes | 65,535 | Small meshes, saves memory |
| `uint32_t` / `UINT32` | 4 bytes | 4,294,967,295 | Large meshes, most models |

---

## Step 5: Updating the Vertex Shader

The vertex shader should now accept `vec3` positions:

```glsl
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;    // was vec2
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;
}
```

The key change: `vec4(inPosition, 1.0)` now uses all three components of the 3D position directly. Before, we had `vec4(inPosition, 0.0, 1.0)` because position was 2D.

---

## Step 6: Updating the Camera

The Viking Room model is larger and positioned differently than our rectangle. Adjust the camera in `updateUniformBuffer`:

```cpp
void updateUniformBuffer(uint32_t currentImage) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(
        currentTime - startTime).count();

    UniformBufferObject ubo{};

    ubo.model = glm::rotate(
        glm::mat4(1.0f),
        time * glm::radians(90.0f),
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    ubo.view = glm::lookAt(
        glm::vec3(2.0f, 2.0f, 2.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f)
    );

    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        swapChainExtent.width / (float) swapChainExtent.height,
        0.1f,
        10.0f
    );

    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}
```

---

## Initialization Order

Add `loadModel` before creating vertex and index buffers:

```cpp
void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createDepthResources();
    createFramebuffers();
    createCommandPool();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    loadModel();                    // NEW — load before creating buffers
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}
```

`loadModel` fills the `vertices` and `indices` vectors. Then `createVertexBuffer` and `createIndexBuffer` use those vectors to create the GPU buffers — no changes needed in those functions!

---

## Performance Considerations

### Loading Time

```
Model loading happens once at startup. Typical times:

  Model Size        Load Time (approx)
  ──────────        ──────────────────
  1K triangles      < 1 ms
  10K triangles     ~5 ms
  100K triangles    ~50 ms
  1M triangles      ~500 ms

For large models, consider:
  - Binary formats (glTF binary, FBX) load faster than text (OBJ)
  - Background loading threads
  - Level-of-detail (LOD) systems
```

### Memory Usage

| Component | Viking Room | High-Detail Character |
|-----------|-------------|----------------------|
| Vertices | ~29K × 32B = 0.9 MB | ~500K × 32B = 16 MB |
| Indices | ~87K × 4B = 0.3 MB | ~1.5M × 4B = 6 MB |
| Texture | 1024×1024 × 4B = 4 MB | 4096×4096 × 4B = 64 MB |
| **Total GPU** | **~5.2 MB** | **~86 MB** |

### GPU Vertex Cache

With vertex deduplication and proper indexing, the GPU's **vertex cache** can skip running the vertex shader for recently processed vertices:

```
Index buffer: [0, 1, 2,  1, 3, 2,  2, 3, 4, ...]
                         ↑     ↑   ↑  ↑
                         │     │   │  └── cache hit! (recently processed)
                         │     │   └───── cache hit!
                         │     └───────── cache hit!
                         └─────────────── cache hit!

The more vertices are shared between triangles,
the more cache hits occur, and the fewer times
the vertex shader actually runs.

Typical shared vertex: used by ~6 triangles
  Without indexing: vertex shader runs 6 times
  With indexing + cache: vertex shader runs 1 time (5 cache hits!)
```

---

## Common Issues and Debugging

### Model Appears Inside-Out

If the model looks wrong (you see the interior instead of the exterior), the winding order might be inverted. Fix by changing the front face in the rasterizer:

```cpp
rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
// Try: VK_FRONT_FACE_CLOCKWISE if the model appears inverted
```

### Model is Invisible or Tiny

The model might be at a vastly different scale. Try adjusting the camera distance or adding a scale to the model matrix:

```cpp
ubo.model = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));  // Scale down 10x
// or
ubo.model = glm::scale(glm::mat4(1.0f), glm::vec3(10.0f)); // Scale up 10x
```

### Textures Look Wrong

If the texture appears garbled or mirrored, check the UV Y-flip. Try removing or adding the `1.0f -` flip:

```cpp
// If texture is upside down, flip V:
vertex.texCoord = {
    attrib.texcoords[2 * index.texcoord_index + 0],
    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]  // flip
};
```

---

## The Complete Data Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    MODEL LOADING DATA FLOW                              │
│                                                                         │
│  Disk                                                                   │
│  ────                                                                   │
│  viking_room.obj ──tinyobjloader──→ attrib (positions, UVs, normals)   │
│                                     shapes (face indices)               │
│                                         │                               │
│                                         ↓                               │
│  CPU Processing                                                         │
│  ──────────────                                                         │
│  ┌────────────────────────────────────────────────────────┐            │
│  │ for each face vertex:                                   │            │
│  │   1. Assemble Vertex from attrib data                   │            │
│  │   2. Flip texcoord V (1.0 - v)                         │            │
│  │   3. Check uniqueVertices hash map                      │            │
│  │      → new? add to vertices[], record in map            │            │
│  │      → seen? just reuse the existing index              │            │
│  │   4. Push index to indices[]                            │            │
│  └────────────────────────────────────────────────────────┘            │
│                    │                    │                                │
│                    ↓                    ↓                                │
│            vertices[]              indices[]                            │
│            (unique only)           (references)                         │
│                    │                    │                                │
│                    ↓                    ↓                                │
│  GPU Upload (staging buffer pattern)                                    │
│  ───────────────────────────────                                        │
│  createVertexBuffer()          createIndexBuffer()                      │
│    → staging buf → copy          → staging buf → copy                   │
│    → DEVICE_LOCAL VkBuffer       → DEVICE_LOCAL VkBuffer               │
│                    │                    │                                │
│                    ↓                    ↓                                │
│  Rendering                                                              │
│  ─────────                                                              │
│  vkCmdBindVertexBuffers()     vkCmdBindIndexBuffer()                   │
│                    └──────────┬─────────┘                              │
│                               ↓                                         │
│                  vkCmdDrawIndexed(indexCount, 1, 0, 0, 0)              │
│                               ↓                                         │
│                    ┌──────────────────────┐                             │
│                    │   Rendered 3D model  │                             │
│                    │   with textures and  │                             │
│                    │   depth buffering!   │                             │
│                    └──────────────────────┘                             │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Try It Yourself

1. **Run the program** — you should see the textured Viking Room model spinning in 3D! The depth buffer ensures correct occlusion of the room's complex geometry.

2. **Print model statistics**: After loading, print the number of unique vertices and indices. Compare with the total number of face vertices (without deduplication) to see how much the hash map saved.

```cpp
std::cout << "Vertices: " << vertices.size() << std::endl;
std::cout << "Indices:  " << indices.size() << std::endl;
```

3. **Try a different model**: Download a different OBJ model (many free ones are available on sites like Sketchfab or TurboSquid). Load it instead of the Viking Room. You may need to adjust the camera.

4. **Remove deduplication**: Comment out the hash map logic and push every vertex unconditionally. Compare vertex counts and observe that the rendered result is the same (just uses more memory).

5. **Scale and position**: Add a `glm::translate` to the model matrix to move the model, or `glm::scale` to resize it. Chain transformations: `model = translate * rotate * scale`.

---

## Key Takeaways

- **OBJ format** is a simple text format storing positions (`v`), texture coordinates (`vt`), normals (`vn`), and faces (`f`) with indices into those arrays.
- **tinyobjloader** handles all OBJ parsing — one header, one `#define`, done.
- OBJ texture coordinates have **V=0 at the bottom**; Vulkan has **V=0 at the top**. Flip with `1.0f - v`.
- **Vertex deduplication** with `std::unordered_map` eliminates duplicate vertices, typically reducing vertex count by **50-66%** and improving GPU vertex cache hit rates.
- Custom `hash<Vertex>` and `operator==` are required for unordered_map lookup.
- Switch to **`uint32_t` indices** for models with more than 65,535 unique vertices.
- The vertex/index creation functions don't need changes — they already work with `std::vector<Vertex>` and `std::vector<uint32_t>`.
- `loadModel()` runs once at startup; the GPU buffers it creates are used every frame.
- With model loading, texture mapping, and depth buffering, we have a **complete 3D rendering pipeline** — the same foundation used by real games and applications!

---

[<< Previous: Depth Buffering](19-depth-buffering.md) | [Next: Mipmaps & MSAA >>](21-mipmaps-msaa.md)
