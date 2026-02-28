# Chapter 14: Vertex Buffers

[<< Previous: Frames in Flight](13-frames-in-flight.md) | [Next: Staging Buffers >>](15-staging-buffers.md)

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

🎯 Now: Send vertex data from CPU to GPU with vertex buffers
```

---

## The Problem: Hardcoded Vertices

In our current setup, the triangle's vertices are **hardcoded inside the vertex shader**:

```glsl
// Current vertex shader — vertices baked in!
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);
```

This is like writing a novel with every character's name carved in stone — if you want to change anything, you have to rewrite the whole stone. We need a way to **send data from the CPU to the GPU** at runtime.

```
BEFORE (hardcoded):                    AFTER (vertex buffer):

┌──────────────────┐                  ┌─────────────────┐
│  Vertex Shader   │                  │  CPU Memory     │
│  positions = ... │                  │  [vertices]     │
│  colors = ...    │                  └────────┬────────┘
│  (fixed!)        │                           │ upload
└──────────────────┘                           ▼
                                      ┌─────────────────┐
                                      │  GPU Memory     │
                                      │  Vertex Buffer  │
                                      └────────┬────────┘
                                               │ read
                                               ▼
                                      ┌─────────────────┐
                                      │  Vertex Shader  │
                                      │  reads from buf │
                                      │  (flexible!)    │
                                      └─────────────────┘
```

---

## Step 1: Define the Vertex Structure

We need a C++ struct that matches what we want to send to the GPU. Each vertex has a **position** and a **color**:

```cpp
#include <glm/glm.hpp>
#include <array>

struct Vertex {
    glm::vec2 pos;    // 2D position (x, y)
    glm::vec3 color;  // RGB color (r, g, b)
};
```

And our triangle data:

```cpp
const std::vector<Vertex> vertices = {
    {{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},   // Top:    red
    {{0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},    // Right:  green
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}     // Left:   blue
};
```

### Vertex Memory Layout

```
Each Vertex in memory:
┌─────────────────────────────────────────────────────────┐
│  pos.x   │  pos.y   │ color.r  │ color.g  │ color.b   │
│ (float)  │ (float)  │ (float)  │ (float)  │ (float)   │
│ 4 bytes  │ 4 bytes  │ 4 bytes  │ 4 bytes  │ 4 bytes   │
├──────────┴──────────┼──────────┴──────────┴───────────┤
│   offset = 0        │   offset = 8                     │
│   sizeof(vec2) = 8  │   sizeof(vec3) = 12              │
└─────────────────────┴─────────────────────────────────┘
                  Total: 20 bytes per vertex

Full buffer:
┌──────────────────┬──────────────────┬──────────────────┐
│    Vertex 0      │    Vertex 1      │    Vertex 2      │
│  20 bytes        │  20 bytes        │  20 bytes        │
└──────────────────┴──────────────────┴──────────────────┘
  stride = 20       stride = 20       stride = 20
```

---

## Step 2: Describe the Vertex Layout to Vulkan

Vulkan doesn't know what a `Vertex` struct looks like. We need to tell it two things:

1. **Binding description**: How to step through the vertex buffer (stride, input rate)
2. **Attribute descriptions**: What each piece of data is (offset, format)

Think of it like filling out a customs declaration form: "This package contains items at these positions, in these formats."

### Binding Description

```cpp
static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;                          // Binding index
    bindingDescription.stride = sizeof(Vertex);              // 20 bytes between vertices
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;  // Move per-vertex

    return bindingDescription;
}
```

#### Input Rate Explained

| Input Rate | Advances Every... | Use Case |
|-----------|-------------------|----------|
| `VK_VERTEX_INPUT_RATE_VERTEX` | Vertex | Standard vertex data (position, color, normal) |
| `VK_VERTEX_INPUT_RATE_INSTANCE` | Instance | Per-instance data (world matrix for instanced rendering) |

```
RATE_VERTEX (each vertex gets unique data):
  Vertex 0 → data[0]
  Vertex 1 → data[1]
  Vertex 2 → data[2]

RATE_INSTANCE (each instance gets unique data, all vertices in that instance share it):
  Instance 0, Vertex 0 → data[0]
  Instance 0, Vertex 1 → data[0]  ← same!
  Instance 0, Vertex 2 → data[0]  ← same!
  Instance 1, Vertex 0 → data[1]
  Instance 1, Vertex 1 → data[1]  ← same!
```

### Attribute Descriptions

Each attribute tells Vulkan where one field lives inside the vertex:

```cpp
static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

    // Attribute 0: position (vec2)
    attributeDescriptions[0].binding = 0;                          // Which binding
    attributeDescriptions[0].location = 0;                         // layout(location=0)
    attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;     // vec2 = 2x float32
    attributeDescriptions[0].offset = offsetof(Vertex, pos);       // 0 bytes from start

    // Attribute 1: color (vec3)
    attributeDescriptions[1].binding = 0;                          // Same binding
    attributeDescriptions[1].location = 1;                         // layout(location=1)
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 = 3x float32
    attributeDescriptions[1].offset = offsetof(Vertex, color);     // 8 bytes from start

    return attributeDescriptions;
}
```

### Format Mapping Table

The `format` field uses color channel names (R, G, B, A) even for non-color data. Here's the mapping:

| GLSL Type | Vulkan Format | Size |
|-----------|--------------|------|
| `float` | `VK_FORMAT_R32_SFLOAT` | 4 bytes |
| `vec2` | `VK_FORMAT_R32G32_SFLOAT` | 8 bytes |
| `vec3` | `VK_FORMAT_R32G32B32_SFLOAT` | 12 bytes |
| `vec4` | `VK_FORMAT_R32G32B32A32_SFLOAT` | 16 bytes |
| `ivec2` | `VK_FORMAT_R32G32_SINT` | 8 bytes |
| `uvec4` | `VK_FORMAT_R32G32B32A32_UINT` | 16 bytes |
| `double` | `VK_FORMAT_R64_SFLOAT` | 8 bytes |

The naming convention: `R32G32B32_SFLOAT` means **3 components** (R, G, B), each **32-bit**, **signed float**.

### How It All Connects

```
C++ struct Vertex:          Vulkan sees it as:
┌──────────────────┐        ┌──────────────────────────────┐
│ glm::vec2 pos    │ ◄──────│ location=0, R32G32_SFLOAT    │
│ (offset: 0)      │        │ (offset=0, reads 8 bytes)    │
├──────────────────┤        ├──────────────────────────────┤
│ glm::vec3 color  │ ◄──────│ location=1, R32G32B32_SFLOAT │
│ (offset: 8)      │        │ (offset=8, reads 12 bytes)   │
└──────────────────┘        └──────────────────────────────┘
│←── stride = 20 ──→│        binding=0, stride=20
```

---

## Step 3: Update the Pipeline

Tell the pipeline about our vertex input format:

```cpp
void createGraphicsPipeline() {
    // ... other pipeline setup ...

    auto bindingDescription = Vertex::getBindingDescription();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    // ... rest of pipeline creation ...
}
```

---

## Step 4: Update the Vertex Shader

The shader now reads from the vertex buffer instead of hardcoded arrays:

```glsl
#version 450

// Input from vertex buffer (must match attribute descriptions!)
layout(location = 0) in vec2 inPosition;   // Attribute 0: pos
layout(location = 1) in vec3 inColor;      // Attribute 1: color

// Output to fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}
```

The fragment shader stays the same:

```glsl
#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);
}
```

---

## Step 5: Create the Vertex Buffer

Now for the core of this chapter — actually creating a GPU buffer and uploading our vertex data.

### Understanding GPU Memory

```
┌──────────────────────────────────────────────────────────────────┐
│                     MEMORY ARCHITECTURE                          │
│                                                                  │
│  ┌────────────────────┐           ┌────────────────────┐        │
│  │    System RAM       │           │    GPU VRAM         │        │
│  │  (CPU accessible)   │           │  (GPU fast access)  │        │
│  │                     │           │                     │        │
│  │  Your C++ program   │   PCIe   │  Vertex buffers     │        │
│  │  vertices[]         │ ════════ │  Textures           │        │
│  │  application data   │   Bus    │  Framebuffers       │        │
│  │                     │           │                     │        │
│  └────────────────────┘           └────────────────────┘        │
│                                                                  │
│  HOST = CPU side          DEVICE = GPU side                      │
└──────────────────────────────────────────────────────────────────┘
```

### Memory Property Flags

| Flag | Meaning | Speed |
|------|---------|-------|
| `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` | CPU can read/write this memory | Slow for GPU |
| `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` | CPU writes are immediately visible to GPU (no flush needed) | — |
| `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT` | Lives on the GPU's own fast memory (VRAM) | Fast for GPU |

For now, we'll use `HOST_VISIBLE | HOST_COHERENT` — the simplest approach. The CPU can write directly to it, and the GPU can read from it. It's not the fastest for the GPU, but it works. (Chapter 15 covers the optimal approach.)

```
HOST_VISIBLE | HOST_COHERENT memory:

  CPU ──write──► [shared memory] ◄──read── GPU
                                      (works, but GPU reads are slower)

DEVICE_LOCAL memory (Chapter 15):

  CPU ──write──► [staging buf] ──copy──► [VRAM buffer] ◄──read── GPU
                 (HOST_VISIBLE)           (DEVICE_LOCAL)    (fast!)
```

---

### findMemoryType() Helper

GPUs offer different memory types. We need to find one that meets our requirements:

```cpp
uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        // Check two things:
        // 1. The type is one of the allowed types (typeFilter bitmask)
        // 2. The type has ALL the properties we need
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}
```

#### How the Bitmask Works

```
typeFilter = 0b00001011 means: types 0, 1, and 3 are acceptable

memoryTypes:
  Type 0: DEVICE_LOCAL                        ← bit 0 set in filter ✓
  Type 1: HOST_VISIBLE | HOST_COHERENT        ← bit 1 set in filter ✓
  Type 2: HOST_VISIBLE                        ← bit 2 NOT set in filter ✗
  Type 3: DEVICE_LOCAL | HOST_VISIBLE         ← bit 3 set in filter ✓

If we want HOST_VISIBLE | HOST_COHERENT:
  Type 0: DEVICE_LOCAL → missing HOST_VISIBLE → skip
  Type 1: HOST_VISIBLE | HOST_COHERENT → has both! → RETURN 1 ✓
```

---

### Creating the Buffer and Allocating Memory

```cpp
VkBuffer vertexBuffer;
VkDeviceMemory vertexBufferMemory;

void createVertexBuffer() {
    // ═══════════════════════════════════════
    //  STEP 1: Create the buffer object
    // ═══════════════════════════════════════
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(vertices[0]) * vertices.size();  // Total bytes
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;     // Used as vertex buffer
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;       // One queue family only

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create vertex buffer!");
    }

    // ═══════════════════════════════════════
    //  STEP 2: Query memory requirements
    // ═══════════════════════════════════════
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);
    //   memRequirements.size            → how many bytes needed
    //   memRequirements.alignment       → memory alignment needed
    //   memRequirements.memoryTypeBits  → which memory types are OK

    // ═══════════════════════════════════════
    //  STEP 3: Allocate GPU memory
    // ═══════════════════════════════════════
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    if (vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate vertex buffer memory!");
    }

    // ═══════════════════════════════════════
    //  STEP 4: Bind memory to buffer
    // ═══════════════════════════════════════
    vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);
    //                                                          ↑
    //                                               offset into allocation

    // ═══════════════════════════════════════
    //  STEP 5: Copy vertex data into the buffer
    // ═══════════════════════════════════════
    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, bufferInfo.size, 0, &data);
    //          │                            │  │               │  │
    //          │                     offset─┘  size──┘        flags  output ptr
    //          device memory

    memcpy(data, vertices.data(), (size_t)bufferInfo.size);
    // Copy vertex data from CPU → mapped GPU memory

    vkUnmapMemory(device, vertexBufferMemory);
    // We're done writing — unmap
}
```

### The Buffer Creation Flow

```
STEP 1: vkCreateBuffer         →  Buffer object (no memory yet!)
                                   Like a labeled empty box
                                        │
STEP 2: vkGetBufferMemory      →  "I need 60 bytes, aligned to 4,
        Requirements               types 0/1/3 work"
                                        │
STEP 3: vkAllocateMemory       →  Actual GPU memory allocated
                                   Like renting storage space
                                        │
STEP 4: vkBindBufferMemory     →  Buffer is connected to memory
                                   Like putting the box in the storage
                                        │
STEP 5: vkMapMemory            →  CPU gets a pointer to write to
        memcpy                 →  Copy vertex data in
        vkUnmapMemory          →  Release the pointer
```

### Why So Many Steps?

In OpenGL, `glBufferData` does all of this in one call. Vulkan separates them so you have full control:

- You can **sub-allocate** (put multiple buffers in one memory allocation)
- You can choose the **exact memory type** based on your needs
- You can **reuse memory** for different buffers over time

---

## Step 6: Bind the Vertex Buffer During Command Recording

Update `recordCommandBuffer` to bind our vertex buffer before drawing:

```cpp
void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // ... begin command buffer, begin render pass, bind pipeline ...

    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer,
                           0,              // First binding
                           1,              // Binding count
                           vertexBuffers,  // Buffer array
                           offsets);       // Offset array

    vkCmdDraw(commandBuffer,
              static_cast<uint32_t>(vertices.size()),  // Vertex count
              1,    // Instance count
              0,    // First vertex
              0);   // First instance

    // ... end render pass, end command buffer ...
}
```

### What vkCmdBindVertexBuffers Does

```
Before binding:
  Pipeline: "I expect vertex data at binding 0 with stride=20"
  GPU: "But where is the data?"

After binding:
  Pipeline: "I expect vertex data at binding 0 with stride=20"
  vkCmdBindVertexBuffers: "Here — vertexBuffer, starting at offset 0"
  GPU: "Got it! I'll read 20 bytes per vertex from that buffer."

  ┌───────────────────────────────────────────────┐
  │  vertexBuffer (GPU memory):                   │
  │                                               │
  │  Vertex 0         Vertex 1         Vertex 2   │
  │  ┌───────────┐   ┌───────────┐   ┌──────────┐│
  │  │pos  │color│   │pos  │color│   │pos │color ││
  │  └───────────┘   └───────────┘   └──────────┘│
  │  ↑               ↑               ↑            │
  │  stride 0        stride 1        stride 2     │
  │  offset: 0       offset: 20      offset: 40   │
  └───────────────────────────────────────────────┘
```

---

## Step 7: Clean Up

Destroy the buffer and free the memory in `cleanup()`:

```cpp
void cleanup() {
    cleanupSwapChain();

    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);

    // ... rest of cleanup (sync objects, pipeline, etc.) ...
}
```

**Order matters**: Destroy the buffer first, then free its memory. (The buffer references the memory, not the other way around.)

---

## The Full Picture: Vertex Data Flow

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  1. C++ Code                                                 │
│     const std::vector<Vertex> vertices = { ... };            │
│                           │                                  │
│                     vkMapMemory + memcpy                     │
│                           │                                  │
│  2. GPU Memory                                               │
│     ┌─────────────────────────────────────────┐              │
│     │ vertexBuffer (HOST_VISIBLE, COHERENT)   │              │
│     │ [v0.pos|v0.col|v1.pos|v1.col|v2.pos|...]│              │
│     └──────────────────────┬──────────────────┘              │
│                            │                                 │
│                   vkCmdBindVertexBuffers                      │
│                            │                                 │
│  3. Vertex Shader                                            │
│     layout(location=0) in vec2 inPosition;   ← reads pos    │
│     layout(location=1) in vec3 inColor;      ← reads color  │
│                            │                                 │
│  4. Rasterizer → Fragment Shader → Framebuffer → Screen      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## Try It Yourself

1. **Run the updated program** — you should see the same triangle, but now the vertices come from CPU data, not the shader.

2. **Change the vertex positions** in the C++ code (not the shader). For example, make the triangle bigger or move it. No shader recompilation needed!

3. **Add a fourth vertex** to make a diamond shape:
   ```cpp
   const std::vector<Vertex> vertices = {
       {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
       {{ 0.5f,  0.0f}, {0.0f, 1.0f, 0.0f}},
       {{ 0.0f,  0.5f}, {0.0f, 0.0f, 1.0f}},
   // Three vertices = first triangle, then need more for a second...
   };
   ```
   What happens when you have 4 vertices with `vkCmdDraw` set to vertex count 4? (Hint: think about triangle topology.)

4. **Add a third attribute**: Try adding a `float pointSize` field to `Vertex` and the corresponding attribute description. (You don't need to use it in the shader yet — just practice the description setup.)

5. **Inspect with `offsetof`**: Print `offsetof(Vertex, pos)` and `offsetof(Vertex, color)` to verify they match what you set in the attribute descriptions.

---

## Key Takeaways

- **Vertex buffers** store vertex data in GPU memory so shaders can read it.
- The **Vertex struct** defines the per-vertex data layout (position, color, etc.).
- **Binding descriptions** tell Vulkan the stride (bytes between vertices) and input rate.
- **Attribute descriptions** tell Vulkan the format and offset of each vertex field.
- Vulkan format names use **color channel notation**: `R32G32_SFLOAT` = vec2, `R32G32B32_SFLOAT` = vec3.
- **Buffer creation is multi-step**: create buffer → query requirements → allocate memory → bind → upload.
- `findMemoryType()` searches for GPU memory that satisfies both the buffer's type requirements and your desired properties.
- `HOST_VISIBLE | HOST_COHERENT` is simple but not optimal — the GPU reads it over the PCIe bus.
- **Next chapter**: We'll fix the performance issue using staging buffers and `DEVICE_LOCAL` memory.

---

[<< Previous: Frames in Flight](13-frames-in-flight.md) | [Next: Staging Buffers >>](15-staging-buffers.md)
