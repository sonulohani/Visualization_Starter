# Chapter 23: Compute Shaders

[<< Previous: Push Constants](22-push-constants.md) | [Next: Advanced Render Passes >>](24-advanced-renderpasses.md)

---

**Progress Tracker:**
```
[██████████████████████░░░] Chapter 23 of 30
Topics: Compute Shaders | Work Groups | SSBO | Particle System
```

---

## Table of Contents

1. [What Are Compute Shaders?](#what-are-compute-shaders)
2. [Use Cases](#use-cases)
3. [Work Groups and Invocations](#work-groups-and-invocations)
4. [Local Size and Global Invocation ID](#local-size-and-global-invocation-id)
5. [Storage Buffers vs Uniform Buffers](#storage-buffers-vs-uniform-buffers)
6. [Complete Particle System Example](#complete-particle-system-example)
7. [Creating Storage Buffers](#creating-storage-buffers)
8. [Compute Descriptor Set Layout](#compute-descriptor-set-layout)
9. [Creating the Compute Pipeline](#creating-the-compute-pipeline)
10. [Dispatching Compute Work](#dispatching-compute-work)
11. [Synchronization Between Compute and Graphics](#synchronization-between-compute-and-graphics)
12. [Try It Yourself](#try-it-yourself)
13. [Key Takeaways](#key-takeaways)

---

## What Are Compute Shaders?

Compute shaders are **general-purpose programs** that run on the GPU outside the traditional graphics pipeline. No vertices, no fragments, no rasterization — just raw parallel computation.

**Analogy:** The graphics pipeline is like an assembly line in a car factory — parts flow through fixed stations (vertex → rasterization → fragment → blending) in a rigid order. A compute shader is like giving every worker in the factory a general instruction ("sort these bolts by size") and letting them all work simultaneously, with no assembly line structure.

```
Graphics Pipeline (fixed stages):

  Vertices ──► Vertex Shader ──► Rasterizer ──► Fragment Shader ──► Framebuffer
               (per vertex)                     (per pixel)

Compute Pipeline (freeform):

  ┌─────────────────────────────────────────────┐
  │              Compute Shader                  │
  │                                              │
  │  Input: Any buffers/images you want          │
  │  Output: Any buffers/images you want         │
  │  Structure: You decide                       │
  │  Parallelism: Thousands of threads           │
  │                                              │
  │  No vertices. No pixels. Just computation.   │
  └─────────────────────────────────────────────┘
```

Compute shaders have access to the same memory and resources as graphics shaders (buffers, images, samplers), but they're dispatched independently using `vkCmdDispatch`.

---

## Use Cases

| Category | Examples |
|---|---|
| **Particle systems** | Update millions of particle positions/velocities |
| **Physics simulation** | Cloth, fluid, rigid body physics |
| **Image processing** | Blur, bloom, tone mapping, histogram |
| **Culling** | Frustum culling, occlusion culling |
| **Terrain** | LOD computation, heightmap generation |
| **Animation** | Skinning, morph targets, procedural animation |
| **Data processing** | Sorting, prefix sum, reduction |
| **AI/ML** | Neural network inference on GPU |

**When to use compute vs fragment shaders for post-processing:**

| Aspect | Fragment Shader | Compute Shader |
|---|---|---|
| Access pattern | One output per pixel | Flexible read/write |
| Shared memory | None | Up to 32+ KB per work group |
| Synchronization | None within shader | `barrier()` within work group |
| Output | Fixed to attachment | Any buffer or image |
| Dispatch | Automatic (rasterization) | Manual (`vkCmdDispatch`) |

Use compute when you need shared memory, flexible access patterns, or are not writing to a render target.

---

## Work Groups and Invocations

Compute shaders are organized in a two-level hierarchy: **work groups** and **invocations** (threads within a work group).

```
Dispatch(4, 3, 1) launches a grid of work groups:

         x=0       x=1       x=2       x=3
      ┌─────────┬─────────┬─────────┬─────────┐
y=0   │ WG(0,0) │ WG(1,0) │ WG(2,0) │ WG(3,0) │
      ├─────────┼─────────┼─────────┼─────────┤
y=1   │ WG(0,1) │ WG(1,1) │ WG(2,1) │ WG(3,1) │
      ├─────────┼─────────┼─────────┼─────────┤
y=2   │ WG(0,2) │ WG(1,2) │ WG(2,2) │ WG(3,2) │
      └─────────┴─────────┴─────────┴─────────┘

Total work groups: 4 × 3 × 1 = 12
```

Each work group contains a fixed number of invocations defined in the shader:

```
Inside one work group (local_size_x=8, local_size_y=8, local_size_z=1):

  ┌───┬───┬───┬───┬───┬───┬───┬───┐
  │0,0│1,0│2,0│3,0│4,0│5,0│6,0│7,0│
  ├───┼───┼───┼───┼───┼───┼───┼───┤
  │0,1│1,1│2,1│3,1│4,1│5,1│6,1│7,1│
  ├───┼───┼───┼───┼───┼───┼───┼───┤
  │0,2│1,2│...│   │   │   │   │7,2│
  ├───┼───┼───┼───┼───┼───┼───┼───┤
  │   │   │   │   │   │   │   │   │
  ├───┼───┼───┼───┼───┼───┼───┼───┤
  │   │   │   │   │   │   │   │   │
  ├───┼───┼───┼───┼───┼───┼───┼───┤
  │   │   │   │   │   │   │   │   │
  ├───┼───┼───┼───┼───┼───┼───┼───┤
  │   │   │   │   │   │   │   │   │
  ├───┼───┼───┼───┼───┼───┼───┼───┤
  │0,7│1,7│2,7│3,7│4,7│5,7│6,7│7,7│
  └───┴───┴───┴───┴───┴───┴───┴───┘

  64 invocations per work group (8 × 8 × 1)
```

**Why the two-level structure?** Invocations within the same work group can:
- Share data via **shared memory** (fast on-chip SRAM)
- Synchronize with `barrier()` calls
- Communicate without going through global memory

Invocations in *different* work groups cannot communicate directly — they must use global memory (storage buffers or images) and external synchronization (pipeline barriers).

---

## Local Size and Global Invocation ID

The shader declares its local work group size. The dispatch call defines how many work groups to launch. The product gives you the total number of invocations.

```glsl
// Shader declaration
layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;
```

```cpp
// CPU dispatch: launch enough work groups to cover all particles
uint32_t groupCount = (particleCount + 255) / 256;  // round up
vkCmdDispatch(commandBuffer, groupCount, 1, 1);
```

```
Total invocations = local_size × dispatch_count

Example: 10000 particles, local_size_x = 256
  groupCount = ceil(10000 / 256) = 40
  Total invocations = 40 × 256 = 10240
  (240 invocations will be "extra" — guard with bounds check)
```

**Built-in variables available in compute shaders:**

| Variable | Type | Description |
|---|---|---|
| `gl_GlobalInvocationID` | `uvec3` | Unique ID across all work groups |
| `gl_LocalInvocationID` | `uvec3` | ID within the current work group |
| `gl_WorkGroupID` | `uvec3` | ID of the current work group |
| `gl_WorkGroupSize` | `uvec3` | Size of each work group (= local_size) |
| `gl_LocalInvocationIndex` | `uint` | Flattened 1D index within work group |
| `gl_NumWorkGroups` | `uvec3` | Total number of work groups dispatched |

```
Relationship between IDs:

gl_GlobalInvocationID = gl_WorkGroupID * gl_WorkGroupSize + gl_LocalInvocationID

Example: WorkGroup(2), local_size=256, LocalInvocation(100)
  GlobalInvocationID = 2 * 256 + 100 = 612

  This means: "I am thread 612 globally, thread 100 in work group 2"
```

**Choosing local_size:**
- Must be a power of 2 or multiple of the GPU's warp/wavefront size
- NVIDIA: warp size = 32, so use multiples of 32 (64, 128, 256)
- AMD: wavefront size = 32 or 64, so 64 is a safe default
- Common choices: 64 for 1D, 8×8 for 2D, 4×4×4 for 3D
- Max invocations per work group: at least 128 (most GPUs support 1024)

---

## Storage Buffers vs Uniform Buffers

Compute shaders typically use **Storage Buffers (SSBOs)** because they need read/write access to large data:

| Feature | Uniform Buffer (UBO) | Storage Buffer (SSBO) |
|---|---|---|
| Max size | 16 KB minimum (often 64 KB) | 128 MB+ |
| Access | Read-only | Read/Write |
| Performance | Fastest reads (cached, broadcast) | Slightly slower (uncached path possible) |
| Dynamic arrays | No (fixed size at compile time) | Yes (`buffer[]` unsized arrays) |
| Atomic operations | No | Yes |
| Best for | Small, read-only config data | Large, mutable data arrays |

In GLSL:

```glsl
// Uniform buffer — small, read-only
layout(std140, set = 0, binding = 0) uniform SimParams {
    float deltaTime;
    float gravity;
    int particleCount;
} params;

// Storage buffer — large, read/write
layout(std430, set = 0, binding = 1) buffer ParticleBuffer {
    Particle particles[];  // unsized array!
};
```

**`std140` vs `std430` layout:**

| Rule | `std140` (UBO) | `std430` (SSBO) |
|---|---|---|
| `float` array stride | 16 bytes (padded!) | 4 bytes (tight) |
| `vec3` size | 16 bytes | 12 bytes |
| Struct alignment | Rounded to `vec4` | Rounded to largest member |

`std430` is more memory-efficient and matches C++ struct layout more closely. Always use `std430` for SSBOs.

---

## Complete Particle System Example

Let's build a GPU particle system from scratch. Thousands of particles will have their positions updated every frame by a compute shader, then rendered by the graphics pipeline.

### Particle Struct

```cpp
struct Particle {
    glm::vec2 position;    //  8 bytes
    glm::vec2 velocity;    //  8 bytes
    glm::vec4 color;       // 16 bytes
};                         // 32 bytes total
```

Matching GLSL:

```glsl
struct Particle {
    vec2 position;
    vec2 velocity;
    vec4 color;
};
```

### Compute Shader (`particle.comp`)

```glsl
#version 450

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

struct Particle {
    vec2 position;
    vec2 velocity;
    vec4 color;
};

layout(std140, set = 0, binding = 0) uniform ParameterUBO {
    float deltaTime;
    float gravity;
    int particleCount;
} params;

layout(std430, set = 0, binding = 1) buffer ParticleSSBOIn {
    Particle particlesIn[];
};

layout(std430, set = 0, binding = 2) buffer ParticleSSBOOut {
    Particle particlesOut[];
};

void main() {
    uint index = gl_GlobalInvocationID.x;

    if (index >= params.particleCount) {
        return;  // guard against extra invocations
    }

    Particle particleIn = particlesIn[index];

    // Apply gravity
    particleIn.velocity.y += params.gravity * params.deltaTime;

    // Update position
    particleIn.position += particleIn.velocity * params.deltaTime;

    // Bounce off screen edges
    if (particleIn.position.x < -1.0 || particleIn.position.x > 1.0) {
        particleIn.velocity.x *= -0.8;  // dampen
        particleIn.position.x = clamp(particleIn.position.x, -1.0, 1.0);
    }
    if (particleIn.position.y < -1.0 || particleIn.position.y > 1.0) {
        particleIn.velocity.y *= -0.8;
        particleIn.position.y = clamp(particleIn.position.y, -1.0, 1.0);
    }

    // Fade color over time
    particleIn.color.a -= params.deltaTime * 0.1;
    particleIn.color.a = max(particleIn.color.a, 0.0);

    particlesOut[index] = particleIn;
}
```

**Why two buffers (In/Out)?** This is the "ping-pong" or "double-buffer" pattern. If all threads read and write the same buffer, thread A might read thread B's *updated* value instead of the original. Using separate input/output buffers eliminates this race condition. Each frame, you swap which buffer is input and which is output.

```
Frame N:   Buffer A (read) ──compute──► Buffer B (write)
Frame N+1: Buffer B (read) ──compute──► Buffer A (write)
Frame N+2: Buffer A (read) ──compute──► Buffer B (write)
...
```

### Vertex Shader for Particle Rendering (`particle.vert`)

```glsl
#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inVelocity;    // unused in vertex shader
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_PointSize = 4.0;
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}
```

### Fragment Shader for Particle Rendering (`particle.frag`)

```glsl
#version 450

layout(location = 0) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    // Soft circular particle
    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);
    if (dist > 0.5) {
        discard;
    }

    float alpha = 1.0 - smoothstep(0.3, 0.5, dist);
    outColor = vec4(fragColor.rgb, fragColor.a * alpha);
}
```

---

## Creating Storage Buffers

Initialize the particles and create the ping-pong storage buffers:

```cpp
const uint32_t PARTICLE_COUNT = 8192;

VkBuffer storageBuffers[2];
VkDeviceMemory storageBufferMemories[2];

void createStorageBuffers() {
    std::vector<Particle> particles(PARTICLE_COUNT);

    std::default_random_engine rndEngine(
        static_cast<unsigned>(time(nullptr)));
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);

    for (auto& particle : particles) {
        // Random positions in NDC space
        float angle = dist(rndEngine) * glm::pi<float>();
        float radius = 0.25f + std::abs(dist(rndEngine)) * 0.5f;
        particle.position = glm::vec2(
            cos(angle) * radius,
            sin(angle) * radius
        );

        // Small random velocities
        particle.velocity = glm::vec2(
            dist(rndEngine) * 0.1f,
            dist(rndEngine) * 0.1f
        );

        // Vibrant random colors
        particle.color = glm::vec4(
            colorDist(rndEngine),
            colorDist(rndEngine),
            colorDist(rndEngine),
            1.0f
        );
    }

    VkDeviceSize bufferSize = sizeof(Particle) * PARTICLE_COUNT;

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, particles.data(), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    for (int i = 0; i < 2; i++) {
        createBuffer(bufferSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     storageBuffers[i], storageBufferMemories[i]);

        copyBuffer(stagingBuffer, storageBuffers[i], bufferSize);
    }

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
```

The key usage flags:
- `VK_BUFFER_USAGE_STORAGE_BUFFER_BIT` — used as SSBO in compute shader
- `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT` — used as vertex buffer in graphics pipeline
- `VK_BUFFER_USAGE_TRANSFER_DST_BIT` — initial data upload from staging buffer

---

## Compute Descriptor Set Layout

The compute shader needs three bindings: the parameter UBO, and two SSBOs (input and output):

```cpp
VkDescriptorSetLayout computeDescriptorSetLayout;
VkDescriptorSet computeDescriptorSets[MAX_FRAMES_IN_FLIGHT];

void createComputeDescriptorSetLayout() {
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};

    // Binding 0: Uniform buffer (simulation parameters)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 1: Storage buffer (input particles)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding 2: Storage buffer (output particles)
    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                    &computeDescriptorSetLayout)
        != VK_SUCCESS) {
        throw std::runtime_error(
            "failed to create compute descriptor set layout!");
    }
}
```

When writing the descriptor sets, alternate which buffer is input/output based on the frame index:

```cpp
void createComputeDescriptorSets() {
    // ... allocate descriptor sets ...

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo uniformBufferInfo{};
        uniformBufferInfo.buffer = uniformBuffers[i];
        uniformBufferInfo.offset = 0;
        uniformBufferInfo.range = sizeof(SimParams);

        // Ping-pong: frame 0 reads buffer 0, writes buffer 1
        //            frame 1 reads buffer 1, writes buffer 0
        VkDescriptorBufferInfo storageBufferInfoIn{};
        storageBufferInfoIn.buffer = storageBuffers[i % 2];
        storageBufferInfoIn.offset = 0;
        storageBufferInfoIn.range = sizeof(Particle) * PARTICLE_COUNT;

        VkDescriptorBufferInfo storageBufferInfoOut{};
        storageBufferInfoOut.buffer = storageBuffers[(i + 1) % 2];
        storageBufferInfoOut.offset = 0;
        storageBufferInfoOut.range = sizeof(Particle) * PARTICLE_COUNT;

        std::array<VkWriteDescriptorSet, 3> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = computeDescriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType =
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &uniformBufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = computeDescriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = &storageBufferInfoIn;

        descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[2].dstSet = computeDescriptorSets[i];
        descriptorWrites[2].dstBinding = 2;
        descriptorWrites[2].descriptorType =
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrites[2].descriptorCount = 1;
        descriptorWrites[2].pBufferInfo = &storageBufferInfoOut;

        vkUpdateDescriptorSets(device,
            static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(), 0, nullptr);
    }
}
```

---

## Creating the Compute Pipeline

A compute pipeline is dramatically simpler than a graphics pipeline — just one shader stage and a pipeline layout:

```cpp
VkPipeline computePipeline;
VkPipelineLayout computePipelineLayout;

void createComputePipeline() {
    auto compShaderCode = readFile("shaders/particle_comp.spv");
    VkShaderModule compShaderModule =
        createShaderModule(compShaderCode);

    VkPipelineShaderStageCreateInfo compShaderStageInfo{};
    compShaderStageInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compShaderStageInfo.module = compShaderModule;
    compShaderStageInfo.pName = "main";

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                               &computePipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error(
            "failed to create compute pipeline layout!");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = computePipelineLayout;
    pipelineInfo.stage = compShaderStageInfo;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1,
                                 &pipelineInfo, nullptr,
                                 &computePipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline!");
    }

    vkDestroyShaderModule(device, compShaderModule, nullptr);
}
```

```
Graphics pipeline vs Compute pipeline creation:

Graphics Pipeline:                 Compute Pipeline:
┌─────────────────────┐           ┌─────────────────────┐
│ Vertex input state   │           │                     │
│ Input assembly state │           │                     │
│ Viewport state       │           │                     │
│ Rasterization state  │           │  Just ONE shader    │
│ Multisample state    │           │  stage + layout     │
│ Depth stencil state  │           │                     │
│ Color blend state    │           │  That's it!         │
│ Dynamic state        │           │                     │
│ Vertex shader        │           │                     │
│ Fragment shader      │           │                     │
│ Pipeline layout      │           │  Pipeline layout    │
│ Render pass          │           │                     │
└─────────────────────┘           └─────────────────────┘
    ~15 structs                       ~3 structs
```

---

## Dispatching Compute Work

Record the compute commands in your main loop:

```cpp
void recordComputeCommandBuffer(VkCommandBuffer commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      computePipeline);

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout,
                            0, 1,
                            &computeDescriptorSets[currentFrame],
                            0, nullptr);

    // Launch enough work groups to cover all particles
    uint32_t groupCountX = (PARTICLE_COUNT + 255) / 256;
    vkCmdDispatch(commandBuffer, groupCountX, 1, 1);

    vkEndCommandBuffer(commandBuffer);
}
```

`vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ)`:

| Parameter | Description |
|---|---|
| `groupCountX` | Number of work groups in X dimension |
| `groupCountY` | Number of work groups in Y dimension |
| `groupCountZ` | Number of work groups in Z dimension |

**Total invocations = groupCount × local_size** (per dimension).

Common dispatch patterns:

```
1D data (particles, arrays):
  local_size_x = 256
  dispatch((count + 255) / 256, 1, 1)

2D data (images):
  local_size_x = 16, local_size_y = 16
  dispatch((width + 15) / 16, (height + 15) / 16, 1)

3D data (volume):
  local_size_x = 4, local_size_y = 4, local_size_z = 4
  dispatch((X + 3) / 4, (Y + 3) / 4, (Z + 3) / 4)
```

---

## Synchronization Between Compute and Graphics

This is the most critical and error-prone part. The compute shader writes to a storage buffer, then the graphics pipeline reads it as a vertex buffer. You **must** ensure the compute write is complete before the vertex read begins.

```
Timeline without synchronization (WRONG):

  Compute: ░░░░░░░░░WRITE░░░░░░░░░
  Graphics:     ░░░░READ░░░░░░░░░░░░
                     ▲
                     └── Data race! Reading partially written data!

Timeline with barrier (CORRECT):

  Compute: ░░░░░░░░░WRITE░░░░░
                              │ barrier
  Graphics:                   ▼░░░░READ░░░░░░░░░░░░
```

**Option 1: Submit compute and graphics on the same queue with a pipeline barrier**

```cpp
void recordCommandBuffer(VkCommandBuffer commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // --- Compute pass ---
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                      computePipeline);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            computePipelineLayout, 0, 1,
                            &computeDescriptorSets[currentFrame],
                            0, nullptr);
    vkCmdDispatch(commandBuffer, (PARTICLE_COUNT + 255) / 256, 1, 1);

    // --- Barrier: compute write → vertex read ---
    VkBufferMemoryBarrier bufferBarrier{};
    bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bufferBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    bufferBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bufferBarrier.buffer = storageBuffers[(currentFrame + 1) % 2];
    bufferBarrier.offset = 0;
    bufferBarrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,    // src: after compute
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,       // dst: before vertex input
        0,
        0, nullptr,
        1, &bufferBarrier,
        0, nullptr
    );

    // --- Graphics pass ---
    // ... begin render pass, bind graphics pipeline, draw particles ...
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      graphicsPipeline);

    VkBuffer vertexBuffers[] = {
        storageBuffers[(currentFrame + 1) % 2]  // output of compute
    };
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdDraw(commandBuffer, PARTICLE_COUNT, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);
    vkEndCommandBuffer(commandBuffer);
}
```

**Option 2: Separate command buffers with semaphores (async compute)**

If your GPU has a dedicated compute queue, you can run compute and graphics *in parallel* on different frames:

```cpp
// Submit compute work
VkSubmitInfo computeSubmitInfo{};
computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
computeSubmitInfo.commandBufferCount = 1;
computeSubmitInfo.pCommandBuffers = &computeCommandBuffer;
computeSubmitInfo.signalSemaphoreCount = 1;
computeSubmitInfo.pSignalSemaphores = &computeFinishedSemaphore;

vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, VK_NULL_HANDLE);

// Submit graphics work (waits for compute)
VkSemaphore waitSemaphores[] = {
    imageAvailableSemaphore,
    computeFinishedSemaphore
};
VkPipelineStageFlags waitStages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
};

VkSubmitInfo graphicsSubmitInfo{};
graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
graphicsSubmitInfo.waitSemaphoreCount = 2;
graphicsSubmitInfo.pWaitSemaphores = waitSemaphores;
graphicsSubmitInfo.pWaitDstStageMask = waitStages;
graphicsSubmitInfo.commandBufferCount = 1;
graphicsSubmitInfo.pCommandBuffers = &graphicsCommandBuffer;
graphicsSubmitInfo.signalSemaphoreCount = 1;
graphicsSubmitInfo.pSignalSemaphores = &renderFinishedSemaphore;

vkQueueSubmit(graphicsQueue, 1, &graphicsSubmitInfo, inFlightFence);
```

```
Async compute timeline:

Queue:      Frame N                Frame N+1             Frame N+2
Compute:    ██COMPUTE██            ██COMPUTE██            ██COMPUTE██
                      ╲                      ╲
Graphics:              █GRAPHICS█              █GRAPHICS█
                       ▲                       ▲
                       semaphore wait           semaphore wait

Compute and graphics for DIFFERENT frames overlap!
```

---

## Try It Yourself

1. **Gravity Wells:** Add 2–3 "gravity well" positions to the simulation parameter UBO. In the compute shader, apply an attractive force from each gravity well to every particle based on distance. Watch particles orbit the wells.

2. **Particle Respawning:** When a particle's alpha reaches 0, respawn it at a random position with full alpha. Use a pseudo-random function based on `gl_GlobalInvocationID` and a time-based seed from the UBO.

3. **Image Processing:** Write a compute shader that takes an input image (sampled image) and writes a blurred version to an output storage image. Use a 5×5 Gaussian kernel with shared memory to cache the neighborhood.

4. **Reduction (Sum):** Implement a parallel reduction to sum all particle velocities. Use shared memory within each work group to sum locally, then write partial results to a buffer. Sum the partial results in a second dispatch or on the CPU.

5. **Compute Frustum Culling:** Given a list of bounding spheres and camera frustum planes (in UBO), write a compute shader that outputs a buffer of visible object indices. The graphics pipeline then uses this for indirect drawing.

---

## Key Takeaways

- **Compute shaders** run general-purpose parallel code on the GPU with no graphics pipeline overhead.
- Work is organized in **work groups** (dispatched from CPU) containing **invocations** (declared in shader via `local_size`).
- `gl_GlobalInvocationID` uniquely identifies each thread — use it to index into your data arrays.
- **Storage buffers (SSBOs)** provide large, read/write GPU memory — use `std430` layout for efficient packing.
- The **ping-pong pattern** (double buffering) prevents read/write race conditions in iterative simulations.
- Compute pipelines are simple: one shader stage, one pipeline layout, no render pass.
- Dispatch with `vkCmdDispatch(groupCountX, groupCountY, groupCountZ)` — always round up to cover all elements.
- **Synchronization is mandatory** between compute writes and graphics reads — use pipeline barriers (same queue) or semaphores (different queues).
- Choose `local_size` as a multiple of the GPU's warp/wavefront size (32 for NVIDIA, 64 for AMD) for best occupancy.
- Compute shaders unlock GPU-accelerated physics, image processing, culling, and any embarrassingly parallel workload.

---

[<< Previous: Push Constants](22-push-constants.md) | [Next: Advanced Render Passes >>](24-advanced-renderpasses.md)
