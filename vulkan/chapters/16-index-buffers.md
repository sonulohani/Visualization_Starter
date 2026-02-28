# Chapter 16: Index Buffers

[<< Previous: Staging Buffers](15-staging-buffers.md) | [Next: Uniform Buffers >>](17-uniforms-descriptors.md)

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
✅ Draw Loop          → Working triangle with frames in flight!
✅ Vertex Buffer      → Vertex data on GPU
✅ Staging Buffers    → Optimized DEVICE_LOCAL memory

🎯 Now: Index buffers for vertex reuse — draw a rectangle!
```

---

## The Problem: Vertex Duplication

We want to draw a **rectangle** (two triangles). With what we have now, we need 6 vertices:

```
Rectangle = 2 triangles = 6 vertices

Triangle 1: top-left, top-right, bottom-right
Triangle 2: top-left, bottom-right, bottom-left

  0 ─────────── 1         Without index buffer:
  │ ╲           │         Vertex list = [0, 1, 2, 0, 2, 3]
  │   ╲  tri1   │         = 6 vertices × 20 bytes = 120 bytes
  │     ╲       │
  │       ╲     │         Vertices 0 and 2 are DUPLICATED!
  │  tri2   ╲   │         That's 40 bytes wasted.
  │           ╲ │
  3 ─────────── 2
```

For a rectangle, 2 duplicated vertices seem harmless. But consider a 3D mesh:

```
A cube has 8 corners but 36 vertices (6 faces × 2 triangles × 3 vertices)
A sphere with 1000 triangles might need 3000 vertices instead of ~500

Real game character: 50,000+ triangles
  Without indices: 150,000 vertices (huge!)
  With indices:     30,000 vertices + 150,000 indices (much smaller!)
```

### The Solution: Index Buffers

Instead of duplicating vertex data, we:
1. Store each **unique vertex** once in the vertex buffer
2. Store a list of **indices** that reference those vertices

```
WITH INDEX BUFFER:

Vertex Buffer (4 unique vertices):
  [0] = top-left      {-0.5, -0.5}  red
  [1] = top-right     { 0.5, -0.5}  green
  [2] = bottom-right  { 0.5,  0.5}  blue
  [3] = bottom-left   {-0.5,  0.5}  white

Index Buffer (6 indices telling the GPU which vertices to use):
  [0, 1, 2, 2, 3, 0]
   \_____/  \_____/
   tri 1    tri 2

  Triangle 1: vertex[0], vertex[1], vertex[2]
  Triangle 2: vertex[2], vertex[3], vertex[0]
```

### Memory Comparison

```
WITHOUT indices:
  6 vertices × 20 bytes/vertex = 120 bytes total

WITH indices:
  4 vertices × 20 bytes/vertex =  80 bytes (vertex buffer)
  6 indices  ×  2 bytes/index  =  12 bytes (index buffer, uint16)
                                 ──────
                           Total: 92 bytes  (saved 28 bytes!)

For a 50,000-triangle mesh:
  WITHOUT: 150,000 × 32 bytes = 4,800,000 bytes (4.8 MB)
  WITH:     30,000 × 32 bytes +
           150,000 ×  2 bytes = 1,260,000 bytes (1.26 MB)
                                                  74% savings!
```

---

## Step 1: Define the Rectangle Data

```cpp
const std::vector<Vertex> vertices = {
    // pos (x, y)        color (r, g, b)
    {{-0.5f, -0.5f},    {1.0f, 0.0f, 0.0f}},   // [0] top-left:     red
    {{ 0.5f, -0.5f},    {0.0f, 1.0f, 0.0f}},   // [1] top-right:    green
    {{ 0.5f,  0.5f},    {0.0f, 0.0f, 1.0f}},   // [2] bottom-right: blue
    {{-0.5f,  0.5f},    {1.0f, 1.0f, 1.0f}}    // [3] bottom-left:  white
};

const std::vector<uint16_t> indices = {
    0, 1, 2,    // First triangle  (top-left, top-right, bottom-right)
    2, 3, 0     // Second triangle (bottom-right, bottom-left, top-left)
};
```

### Visual Layout

```
Vulkan coordinate system:
  (-1,-1) ──────────── (1,-1)
     │                    │
     │     (-0.5,-0.5)────(0.5,-0.5)
     │       │  0   ╲   1  │
     │       │    ╲       │
     │       │  3   ╲   2  │
     │     (-0.5,0.5)────(0.5,0.5)
     │                    │
  (-1, 1) ──────────── (1, 1)

  Triangle 1: indices [0,1,2] → red, green, blue corners
  Triangle 2: indices [2,3,0] → blue, white, red corners

  Result: A colorful rectangle with smooth gradient blending!
```

---

## Step 2: Create the Index Buffer

The process is almost identical to creating the vertex buffer — we use the same staging buffer pattern:

```cpp
VkBuffer indexBuffer;
VkDeviceMemory indexBufferMemory;

void createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    // Copy index data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // Create device-local index buffer
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 indexBuffer, indexBufferMemory);

    // Copy staging → device-local
    copyBuffer(stagingBuffer, indexBuffer, bufferSize);

    // Destroy staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
```

Notice the key difference from vertex buffer creation: the usage flag is `VK_BUFFER_USAGE_INDEX_BUFFER_BIT` instead of `VERTEX_BUFFER_BIT`.

### Side-by-Side Comparison

```
createVertexBuffer()                    createIndexBuffer()
────────────────────                    ──────────────────
staging: TRANSFER_SRC                   staging: TRANSFER_SRC
final:   TRANSFER_DST | VERTEX_BUFFER  final:   TRANSFER_DST | INDEX_BUFFER
data:    vertices.data()                data:    indices.data()
size:    sizeof(Vertex) × count         size:    sizeof(uint16_t) × count

Everything else is identical! The createBuffer/copyBuffer helpers
make adding new buffer types trivial.
```

---

## Step 3: Index Type — uint16 vs uint32

When binding the index buffer, you specify the type of indices:

| Type | Size | Max Vertices | Use When |
|------|------|-------------|----------|
| `VK_INDEX_TYPE_UINT16` | 2 bytes | 65,535 | Meshes with < 65K unique vertices |
| `VK_INDEX_TYPE_UINT32` | 4 bytes | 4,294,967,295 | Very large meshes |

```cpp
// Our rectangle has 4 vertices — uint16_t is more than enough
const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

// A massive terrain mesh might need uint32_t
// const std::vector<uint32_t> indices = { ... };
```

For most game meshes, `uint16` is sufficient and saves memory. Only use `uint32` when a single mesh has more than 65,535 unique vertices.

```
Memory comparison for 150,000 indices:
  uint16: 150,000 × 2 = 300,000 bytes (293 KB)
  uint32: 150,000 × 4 = 600,000 bytes (586 KB)

  uint16 saves ~50% on index buffer size!
```

---

## Step 4: Bind the Index Buffer and Draw Indexed

Update the command recording to bind the index buffer and use `vkCmdDrawIndexed`:

```cpp
void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // ... begin command buffer, begin render pass, bind pipeline ...

    // Bind vertex buffer (same as before)
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // NEW: Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer,
                         indexBuffer,                // The index buffer
                         0,                          // Offset into buffer
                         VK_INDEX_TYPE_UINT16);      // Index type

    // CHANGED: Use DrawIndexed instead of Draw
    vkCmdDrawIndexed(commandBuffer,
                     static_cast<uint32_t>(indices.size()),  // Index count (6)
                     1,    // Instance count
                     0,    // First index
                     0,    // Vertex offset (added to each index)
                     0);   // First instance

    // ... end render pass, end command buffer ...
}
```

### vkCmdDraw vs vkCmdDrawIndexed

```
vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance):
  Draws vertices sequentially: vertex[0], vertex[1], vertex[2], ...
  For a rectangle: needs 6 vertices (with duplicates)

vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance):
  Uses the index buffer to look up vertices:
    index[0]=0 → vertex[0]
    index[1]=1 → vertex[1]
    index[2]=2 → vertex[2]
    index[3]=2 → vertex[2]  ← reused!
    index[4]=3 → vertex[3]
    index[5]=0 → vertex[0]  ← reused!
  For a rectangle: 4 unique vertices + 6 indices
```

### The vertexOffset Parameter

The `vertexOffset` parameter is added to every index value before looking up the vertex. This is useful for storing multiple meshes in one vertex buffer:

```
vertexBuffer: [meshA_v0, meshA_v1, meshA_v2, meshB_v0, meshB_v1, meshB_v2]
                                              ↑ offset = 3

indexBuffer for mesh B: [0, 1, 2]

vkCmdDrawIndexed(..., indexCount=3, ..., vertexOffset=3, ...):
  index[0]=0 + 3 = vertex[3] = meshB_v0
  index[1]=1 + 3 = vertex[4] = meshB_v1
  index[2]=2 + 3 = vertex[5] = meshB_v2
```

---

## Step 5: Clean Up

Add index buffer cleanup alongside the vertex buffer:

```cpp
void cleanup() {
    cleanupSwapChain();

    vkDestroyBuffer(device, indexBuffer, nullptr);
    vkFreeMemory(device, indexBufferMemory, nullptr);

    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);

    // ... rest of cleanup ...
}
```

---

## The Complete Data Flow

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  C++ Code:                                                       │
│    vertices = [{-0.5,-0.5,R}, {0.5,-0.5,G},                    │
│                {0.5,0.5,B},   {-0.5,0.5,W}]                    │
│    indices  = [0, 1, 2, 2, 3, 0]                                │
│              │                    │                               │
│         staging buf           staging buf                        │
│         (memcpy)              (memcpy)                           │
│              │                    │                               │
│         GPU copy              GPU copy                           │
│              ▼                    ▼                               │
│    ┌──────────────┐    ┌──────────────┐                          │
│    │ vertexBuffer │    │ indexBuffer  │                          │
│    │ DEVICE_LOCAL │    │ DEVICE_LOCAL │                          │
│    │ 4 vertices   │    │ 6 indices    │                          │
│    └──────┬───────┘    └──────┬───────┘                          │
│           │                   │                                   │
│     vkCmdBind            vkCmdBind                               │
│     VertexBuffers        IndexBuffer                             │
│           │                   │                                   │
│           └─────────┬─────────┘                                  │
│                     │                                             │
│              vkCmdDrawIndexed(indexCount=6)                       │
│                     │                                             │
│                     ▼                                             │
│         GPU reads index[0]=0 → fetches vertex[0]                 │
│         GPU reads index[1]=1 → fetches vertex[1]                 │
│         GPU reads index[2]=2 → fetches vertex[2]                 │
│         (rasterize triangle 1)                                   │
│         GPU reads index[3]=2 → fetches vertex[2] (cached!)      │
│         GPU reads index[4]=3 → fetches vertex[3]                 │
│         GPU reads index[5]=0 → fetches vertex[0] (cached!)      │
│         (rasterize triangle 2)                                   │
│                     │                                             │
│                     ▼                                             │
│         ┌─────────────────────────┐                              │
│         │                         │                              │
│         │   ┌─────────────────┐   │                              │
│         │   │ Red    Green    │   │                              │
│         │   │   ╲      │     │   │                              │
│         │   │     ╲    │     │   │                              │
│         │   │  White  Blue   │   │                              │
│         │   └─────────────────┘   │                              │
│         │                         │                              │
│         │  A colorful rectangle!  │                              │
│         └─────────────────────────┘                              │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## Post-Vertex-Cache Optimization

Modern GPUs have a **post-transform vertex cache** — when the same vertex index is encountered again, the GPU reuses the already-transformed result instead of running the vertex shader again:

```
Index buffer: [0, 1, 2, 2, 3, 0]

Processing:
  index 0 → vertex shader runs for vertex 0 → cached
  index 1 → vertex shader runs for vertex 1 → cached
  index 2 → vertex shader runs for vertex 2 → cached
  index 2 → CACHE HIT! reuse vertex 2 result (no shader run)
  index 3 → vertex shader runs for vertex 3 → cached
  index 0 → CACHE HIT! reuse vertex 0 result (no shader run)

Result: 4 vertex shader invocations instead of 6!
```

This is a significant optimization for complex meshes where vertices are shared by many triangles. A typical 3D mesh vertex might be shared by **6 triangles** — that's 6x fewer vertex shader invocations.

---

## Initialization Order

Call `createIndexBuffer` right after `createVertexBuffer`:

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
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createVertexBuffer();
    createIndexBuffer();       // NEW
    createCommandBuffers();
    createSyncObjects();
}
```

---

## Try It Yourself

1. **Run the program** — you should see a colorful rectangle instead of a triangle!

2. **Change the colors**: Make the rectangle a solid color (all vertices the same color). Then try different colors per vertex to see the gradient blending.

3. **Draw a hexagon**: Define 6 vertices in a hexagonal layout and the indices for the triangles that make up the hexagon (you'll need a center vertex too, so 7 vertices and 18 indices for 6 triangles).

4. **Try uint32**: Change the index type from `uint16_t` to `uint32_t` and update `VK_INDEX_TYPE_UINT16` to `VK_INDEX_TYPE_UINT32`. Verify it still works.

5. **Vertex offset experiment**: Put two copies of the vertex data in the vertex buffer (8 vertices total) with different colors for the second copy. Use `vertexOffset = 4` in `vkCmdDrawIndexed` to draw the second copy.

6. **Think about it**: For a terrain grid of 256×256 vertices, how many indices would you need? How much memory would `uint16` vs `uint32` use for the index buffer? (Hint: each grid cell is 2 triangles, and there are 255×255 cells.)

---

## Key Takeaways

- **Index buffers** let you reuse vertices, dramatically reducing memory usage and vertex shader work.
- A rectangle needs only **4 unique vertices** + **6 indices** instead of 6 duplicated vertices.
- Index buffer creation uses the **same staging buffer pattern** as vertex buffers.
- The usage flag is `VK_BUFFER_USAGE_INDEX_BUFFER_BIT` (instead of `VERTEX_BUFFER_BIT`).
- **`VK_INDEX_TYPE_UINT16`** handles up to 65,535 vertices (good for most meshes); use **`UINT32`** for larger meshes.
- Bind with `vkCmdBindIndexBuffer`, draw with `vkCmdDrawIndexed` (instead of `vkCmdDraw`).
- The **vertex cache** on modern GPUs means reused vertices skip the vertex shader entirely.
- `vertexOffset` in `vkCmdDrawIndexed` lets you store multiple meshes in one vertex buffer.
- From here, our rendering foundation is solid: **vertex buffers + index buffers + staging = efficient mesh rendering**.

---

[<< Previous: Staging Buffers](15-staging-buffers.md) | [Next: Uniform Buffers >>](17-uniforms-descriptors.md)
