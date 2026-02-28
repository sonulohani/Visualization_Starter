# Chapter 22: Push Constants

[<< Previous: Mipmaps & MSAA](21-mipmaps-msaa.md) | [Next: Compute Shaders >>](23-compute-shaders.md)

---

**Progress Tracker:**
```
[█████████████████████░░░] Chapter 22 of 30
Topics: Push Constants | Per-Object Data | Shader Integration
```

---

## Table of Contents

1. [What Are Push Constants?](#what-are-push-constants)
2. [Push Constants vs Other Methods](#push-constants-vs-other-methods)
3. [Size Limitations](#size-limitations)
4. [Defining the Push Constants Struct](#defining-the-push-constants-struct)
5. [Pipeline Layout Configuration](#pipeline-layout-configuration)
6. [Recording Push Constants](#recording-push-constants)
7. [Shader Side](#shader-side)
8. [Example: Drawing Multiple Objects](#example-drawing-multiple-objects)
9. [Best Use Cases](#best-use-cases)
10. [Try It Yourself](#try-it-yourself)
11. [Key Takeaways](#key-takeaways)

---

## What Are Push Constants?

Push constants are the **fastest way to pass small amounts of data** from the CPU to shaders. Unlike uniform buffers that live in GPU memory and require descriptor sets, push constants are embedded directly into the command buffer itself.

**Analogy:** Think of it like the difference between sending a letter (uniform buffer) and whispering directly into someone's ear (push constant). The letter requires an envelope, a mailbox, and a delivery system. The whisper is instant and direct — but you can only say a few words.

```
Data flow comparison:

Uniform Buffer:
  CPU ──► Staging Buffer ──► GPU Memory ──► Descriptor Set ──► Shader
  (Multiple indirections, allocated memory, descriptor management)

Push Constants:
  CPU ──► Command Buffer ──────────────────────────────────► Shader
  (Direct path, no allocation, no descriptors, embedded in commands)
```

When you call `vkCmdPushConstants`, the data is written directly into the command buffer. The GPU reads it from the command stream with zero indirection — no memory fetches, no descriptor lookups, no cache misses.

---

## Push Constants vs Other Methods

| Feature | Push Constants | Uniform Buffers | Storage Buffers (SSBO) |
|---|---|---|---|
| **Max size** | 128–256 bytes (device-dependent) | 64 KB+ (typically 16 KB minimum) | 128 MB+ |
| **Speed** | Fastest | Fast | Moderate |
| **Requires descriptor set** | No | Yes | Yes |
| **Requires buffer allocation** | No | Yes | Yes |
| **GPU can write** | No | No | Yes |
| **Dynamic indexing** | No | With dynamic offsets | Yes |
| **Best for** | Per-draw data (transforms, IDs) | Per-frame data (camera, lights) | Large data (particles, meshes) |
| **Updated how** | `vkCmdPushConstants` | `memcpy` to mapped memory | `memcpy` or compute shader |
| **Persistence** | Per command buffer recording | Until buffer is updated | Until buffer is updated |

```
Speed hierarchy (fastest to slowest):

  ┌─────────────────┐
  │ Push Constants   │ ← Embedded in command stream
  ├─────────────────┤
  │ Uniform Buffers  │ ← One indirection (descriptor → memory)
  ├─────────────────┤
  │ Storage Buffers  │ ← More flexible but potentially uncached
  ├─────────────────┤
  │ Textures/Images  │ ← Texture unit + sampler overhead
  └─────────────────┘
```

---

## Size Limitations

The Vulkan spec guarantees a **minimum of 128 bytes** for push constants. Most desktop GPUs offer 256 bytes. This is a hard limit per pipeline layout — all push constant ranges across all stages must fit within it.

```cpp
// Query your device's limit
VkPhysicalDeviceProperties properties;
vkGetPhysicalDeviceProperties(physicalDevice, &properties);

uint32_t maxPushConstantsSize = properties.limits.maxPushConstantsSize;
std::cout << "Max push constants size: " << maxPushConstantsSize << " bytes\n";
```

| GPU | `maxPushConstantsSize` |
|---|---|
| NVIDIA (desktop) | 256 bytes |
| AMD (desktop) | 256 bytes |
| Intel (integrated) | 256 bytes |
| Qualcomm Adreno | 256 bytes |
| ARM Mali | 256 bytes |
| Vulkan spec minimum | 128 bytes |

**What fits in 256 bytes?**

| Data | Size | Cumulative |
|---|---|---|
| `mat4` model matrix | 64 bytes | 64 |
| `mat4` normal matrix | 64 bytes | 128 |
| `vec4` color | 16 bytes | 144 |
| `float` time | 4 bytes | 148 |
| `int` materialIndex | 4 bytes | 152 |
| `vec2` uvOffset | 8 bytes | 160 |

That's plenty for per-object data. If you need more, use a uniform buffer.

---

## Defining the Push Constants Struct

Define a C++ struct that mirrors what the shader expects. Keep it aligned to GPU rules:

```cpp
struct PushConstants {
    glm::mat4 model;      // 64 bytes — per-object transform
    glm::vec4 color;      // 16 bytes — per-object tint
    float     time;       //  4 bytes — animation time
    int       texIndex;   //  4 bytes — texture array index
    // Total: 88 bytes (well within 128-byte minimum)
};
```

**Alignment rules** (same as `std140`):

| Type | Alignment | Size |
|---|---|---|
| `float` / `int` | 4 bytes | 4 bytes |
| `vec2` | 8 bytes | 8 bytes |
| `vec3` | 16 bytes | 12 bytes |
| `vec4` | 16 bytes | 16 bytes |
| `mat4` | 16 bytes | 64 bytes |

**Warning:** `vec3` aligns to 16 bytes but only occupies 12. This leaves a 4-byte gap. Prefer `vec4` or add explicit padding to avoid subtle misalignment bugs.

```cpp
// BAD — implicit padding after vec3
struct BadPush {
    glm::vec3 position;  // 12 bytes, but aligned to 16 → 4 bytes wasted
    float     scale;     // this starts at offset 16, not 12!
};

// GOOD — explicit and predictable
struct GoodPush {
    glm::vec4 positionAndScale;  // position in xyz, scale in w
};
```

---

## Pipeline Layout Configuration

Tell Vulkan about your push constants when creating the pipeline layout:

```cpp
void createPipelineLayout() {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                               &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }
}
```

**Key fields explained:**

- `stageFlags` — which shader stages can access these push constants. Use the minimum set of stages that actually read the data.
- `offset` — byte offset into the push constant block. Useful when splitting ranges across stages.
- `size` — number of bytes in this range.

You can define **multiple ranges** for different stages:

```cpp
// Vertex shader gets the model matrix (bytes 0–63)
VkPushConstantRange vertexRange{};
vertexRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
vertexRange.offset = 0;
vertexRange.size = sizeof(glm::mat4);  // 64 bytes

// Fragment shader gets color and parameters (bytes 64–83)
VkPushConstantRange fragmentRange{};
fragmentRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
fragmentRange.offset = sizeof(glm::mat4);  // 64
fragmentRange.size = sizeof(glm::vec4) + sizeof(float) + sizeof(int);  // 24

std::array<VkPushConstantRange, 2> ranges = {vertexRange, fragmentRange};

pipelineLayoutInfo.pushConstantRangeCount =
    static_cast<uint32_t>(ranges.size());
pipelineLayoutInfo.pPushConstantRanges = ranges.data();
```

```
Push constant memory layout:

Byte:  0              64             80    84    88
       ├──────────────┼──────────────┼─────┼─────┤
       │  model (mat4) │  color (vec4) │time │texID│
       ├──────────────┼──────────────┼─────┼─────┤
       │◄─ vertex ───►│◄────── fragment ──────►│
       │              │              │     │     │
       │  stageFlags: │  stageFlags: │     │     │
       │  VERTEX_BIT  │  FRAGMENT_BIT│     │     │
```

---

## Recording Push Constants

Use `vkCmdPushConstants` during command buffer recording:

```cpp
void recordCommandBuffer(VkCommandBuffer commandBuffer,
                         uint32_t imageIndex) {
    // ... begin render pass ...

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphicsPipeline);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1,
                            &descriptorSets[currentFrame],
                            0, nullptr);

    PushConstants push{};
    push.model = glm::rotate(glm::mat4(1.0f),
                             time * glm::radians(90.0f),
                             glm::vec3(0.0f, 0.0f, 1.0f));
    push.color = glm::vec4(1.0f, 0.5f, 0.2f, 1.0f);
    push.time = time;
    push.texIndex = 0;

    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,                        // offset
        sizeof(PushConstants),    // size
        &push                     // data pointer
    );

    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // ... end render pass ...
}
```

`vkCmdPushConstants` parameters:

| Parameter | Description |
|---|---|
| `commandBuffer` | The command buffer being recorded |
| `layout` | Pipeline layout that defines the push constant ranges |
| `stageFlags` | Stages being updated (must be subset of layout's stageFlags) |
| `offset` | Byte offset into push constant block |
| `size` | Number of bytes to update |
| `pValues` | Pointer to the data |

**Important:** Push constants persist across draw calls within the same command buffer until overwritten. You don't need to re-push data that hasn't changed.

```cpp
// Push once, draw many (if data is the same)
vkCmdPushConstants(cmd, layout, stages, 0, sizeof(push), &push);
vkCmdDrawIndexed(cmd, count1, 1, 0, 0, 0);
vkCmdDrawIndexed(cmd, count2, 1, 0, firstIndex2, 0);  // same push data
```

---

## Shader Side

In GLSL, push constants use a special `push_constant` layout qualifier:

**Vertex shader (`shader.vert`):**

```glsl
#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    float time;
    int texIndex;
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * push.model * vec4(inPosition, 1.0);
    fragColor = push.color.rgb;
    fragTexCoord = inTexCoord;
}
```

**Fragment shader (`shader.frag`):**

```glsl
#version 450

layout(set = 0, binding = 1) uniform sampler2D texSamplers[16];

layout(push_constant) uniform PushConstants {
    mat4 model;
    vec4 color;
    float time;
    int texIndex;
} push;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(texSamplers[push.texIndex], fragTexCoord);
    outColor = texColor * vec4(fragColor, 1.0);
}
```

**Rules for push constants in GLSL:**

- Only **one** `push_constant` block per shader stage
- The block layout must match the C++ struct exactly
- Both vertex and fragment shaders can declare the same block (they share the same push constant memory)
- You can declare a subset of the block in a stage that doesn't need all fields, but offsets must match

---

## Example: Drawing Multiple Objects

This is where push constants truly shine. Instead of creating separate uniform buffers or dynamic offsets for each object, just push new data before each draw:

```cpp
struct GameObject {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec4 color;
    int textureIndex;
    uint32_t indexCount;
    uint32_t firstIndex;
};

std::vector<GameObject> gameObjects;

void recordCommandBuffer(VkCommandBuffer commandBuffer,
                         uint32_t imageIndex) {
    // ... begin render pass, bind pipeline, bind descriptor sets ...

    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0,
                         VK_INDEX_TYPE_UINT32);

    for (const auto& obj : gameObjects) {
        PushConstants push{};

        // Build model matrix from position and rotation
        push.model = glm::translate(glm::mat4(1.0f), obj.position);
        push.model = glm::rotate(push.model, obj.rotation.x,
                                 glm::vec3(1, 0, 0));
        push.model = glm::rotate(push.model, obj.rotation.y,
                                 glm::vec3(0, 1, 0));
        push.model = glm::rotate(push.model, obj.rotation.z,
                                 glm::vec3(0, 0, 1));
        push.color = obj.color;
        push.time = currentTime;
        push.texIndex = obj.textureIndex;

        vkCmdPushConstants(
            commandBuffer, pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0, sizeof(PushConstants), &push);

        vkCmdDrawIndexed(commandBuffer, obj.indexCount, 1,
                         obj.firstIndex, 0, 0);
    }

    // ... end render pass ...
}
```

```
Command buffer recording for 3 objects:

┌──────────────────────────────────────────────────────────┐
│ Bind Pipeline                                             │
│ Bind Descriptor Sets                                      │
│ Bind Vertex/Index Buffers                                 │
│                                                           │
│ ┌── Object 0 ──────────────────────────────────────────┐ │
│ │ Push Constants { model=translate(0,0,0), color=red } │ │
│ │ DrawIndexed(36, 1, 0, 0, 0)                          │ │
│ └──────────────────────────────────────────────────────┘ │
│                                                           │
│ ┌── Object 1 ──────────────────────────────────────────┐ │
│ │ Push Constants { model=translate(3,0,0), color=blue }│ │
│ │ DrawIndexed(36, 1, 0, 0, 0)                          │ │
│ └──────────────────────────────────────────────────────┘ │
│                                                           │
│ ┌── Object 2 ──────────────────────────────────────────┐ │
│ │ Push Constants { model=translate(0,3,0), color=green}│ │
│ │ DrawIndexed(24, 1, 36, 0, 0)                         │ │
│ └──────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────┘
```

**Why this is better than uniform buffers for per-object data:**

1. **No buffer allocation** — no `vkCreateBuffer`, no `vkAllocateMemory`
2. **No descriptor sets per object** — one set shared across all objects
3. **No dynamic offsets** — no alignment calculations
4. **Minimal overhead** — data is inlined in the command stream
5. **Simple code** — just fill a struct and push it

---

## Best Use Cases

| Use Case | Push Constant Data | Why Push Constants? |
|---|---|---|
| Per-object transform | `mat4 model` (64 bytes) | Changes every draw call |
| Material selection | `int materialID` (4 bytes) | Tiny, per-draw data |
| Animation time | `float time` (4 bytes) | Changes every frame |
| Color tint/highlight | `vec4 color` (16 bytes) | Per-object variation |
| Texture atlas offset | `vec2 uvOffset` (8 bytes) | Per-sprite data |
| Debug visualization | `int debugMode` (4 bytes) | Toggle at will |
| Screen resolution | `vec2 resolution` (8 bytes) | Needed in many shaders |
| Object ID for picking | `uint objectID` (4 bytes) | Per-draw, read back in picking pass |

**When NOT to use push constants:**

- Data larger than 128 bytes that must be portable across all devices
- Data shared across many draw calls without changes (use uniform buffer)
- Large arrays (light lists, bone matrices) — use SSBO or uniform buffer
- Data the GPU needs to write — push constants are read-only

---

## Try It Yourself

1. **Grid of Objects:** Render a 10×10 grid of cubes, each with a unique position and color passed via push constants. Use nested loops in your draw recording to push different transforms for each cube.

2. **Animated Colors:** Add a `float time` to your push constants and use it in the fragment shader to create a pulsing color effect: `color.rgb * (0.5 + 0.5 * sin(time + objectIndex))`. Each object should pulse at a different phase.

3. **Partial Updates:** Instead of pushing the entire struct every time, experiment with pushing only the `model` matrix for the vertex stage and only the `color` for the fragment stage using separate ranges and offsets. Verify it works correctly.

4. **Size Limit Test:** Create a push constant struct that approaches 256 bytes (e.g., 4 `mat4` matrices). Test on your hardware. Then try exceeding the limit and observe the validation layer error.

5. **Push Constants + Instancing:** Compare two approaches for drawing 1000 identical meshes at different positions: (a) push constants in a loop with 1000 draw calls, (b) instanced drawing with positions in a storage buffer. Measure which is faster on your GPU.

---

## Key Takeaways

- **Push constants** are the fastest mechanism for passing small data from CPU to shaders — no buffers, no descriptors, no allocations.
- The minimum guaranteed size is **128 bytes**; most GPUs support **256 bytes**.
- Define a C++ struct matching the shader's `layout(push_constant) uniform` block, respecting `std140`-like alignment rules.
- Add a `VkPushConstantRange` to your pipeline layout specifying which stages access which bytes.
- Call `vkCmdPushConstants` before each draw call to update per-object data.
- Push constants **persist** in the command buffer until overwritten — you don't need to re-push unchanged data.
- **Best for:** per-object transforms, material IDs, time values, and any small per-draw data.
- **Not suitable for:** large arrays, GPU-writable data, or anything exceeding 128 bytes that must be portable.
- Combining push constants (per-object) with uniform buffers (per-frame) and SSBOs (large data) gives you an efficient three-tier data passing strategy.

---

[<< Previous: Mipmaps & MSAA](21-mipmaps-msaa.md) | [Next: Compute Shaders >>](23-compute-shaders.md)
