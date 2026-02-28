# Chapter 15: Staging Buffers

[<< Previous: Vertex Buffers](14-vertex-buffers.md) | [Next: Index Buffers >>](16-index-buffers.md)

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
✅ Vertex Buffer      → Vertex data on GPU (HOST_VISIBLE)

🎯 Now: Optimize memory with staging buffers (DEVICE_LOCAL)
```

---

## The Performance Problem

In Chapter 14, we created our vertex buffer in `HOST_VISIBLE | HOST_COHERENT` memory. This works, but it's **slow for the GPU to read from**. Here's why:

```
┌─────────────────────────────────────────────────────────────────┐
│                    MEMORY ARCHITECTURE                           │
│                                                                  │
│   CPU                                     GPU                    │
│   ┌──────────┐                           ┌──────────┐           │
│   │          │                           │          │           │
│   │  Cores   │         PCIe Bus          │  Cores   │           │
│   │          │     ◄═══════════════►     │          │           │
│   └────┬─────┘     ~16 GB/s (PCIe 3)    └────┬─────┘           │
│        │           ~32 GB/s (PCIe 4)          │                 │
│        │                                      │                 │
│   ┌────┴─────┐                           ┌────┴─────┐          │
│   │ System   │                           │  VRAM    │          │
│   │ RAM      │                           │          │          │
│   │ 16-64 GB │                           │ 4-24 GB  │          │
│   │ ~50 GB/s │                           │~500 GB/s │          │
│   └──────────┘                           └──────────┘          │
│                                                                  │
│   HOST_VISIBLE memory = lives in System RAM (or shared)          │
│   DEVICE_LOCAL memory = lives in VRAM (fastest for GPU!)         │
└─────────────────────────────────────────────────────────────────┘
```

### The Speed Difference

| Memory Type | GPU Read Bandwidth | GPU Access |
|-------------|-------------------|------------|
| `HOST_VISIBLE` (System RAM) | ~16-32 GB/s (over PCIe) | Slow — crosses the bus every read |
| `DEVICE_LOCAL` (VRAM) | ~400-900 GB/s | Fast — right next to GPU cores |

That's a **15-30x difference**! For a simple triangle it doesn't matter, but for a mesh with millions of vertices, this is critical.

### A Warehouse Analogy

Think of the CPU as your **office** and the GPU as a **factory**:

- **HOST_VISIBLE** = Storing materials at the office and trucking them to the factory every time the factory needs them. The truck (PCIe bus) is the bottleneck.

- **DEVICE_LOCAL** = Storing materials **inside the factory warehouse**. The factory workers grab what they need instantly — no truck involved.

But there's a catch: you (the CPU) can't directly walk into the factory to drop off materials. You need a **delivery process** — that's the staging buffer.

---

## The Staging Buffer Pattern

The idea:

1. Create a **temporary buffer** in `HOST_VISIBLE` memory (the "staging buffer")
2. Copy your data from CPU into the staging buffer
3. Use a GPU command to **copy** from the staging buffer to a `DEVICE_LOCAL` buffer
4. Destroy the staging buffer — it's no longer needed

```
THE STAGING BUFFER PATTERN:

  ┌──────────┐     memcpy      ┌──────────────┐   vkCmdCopyBuffer   ┌──────────────┐
  │ CPU data │ ────────────► │ Staging Buf  │ ──────────────────► │ Device Buf   │
  │ vertices │                 │ HOST_VISIBLE │    (GPU command)    │ DEVICE_LOCAL │
  │ (RAM)    │                 │ (RAM)        │                     │ (VRAM)       │
  └──────────┘                 └──────────────┘                     └──────────────┘
                                       │                                    │
                                 (temporary)                          (permanent)
                                 destroyed after                    used every frame
                                 copy is done                       by vertex shader
```

### Memory Heap Diagram

```
┌────────────────────────────────────────────────────────────┐
│  Physical Memory Layout (typical discrete GPU)             │
│                                                            │
│  ┌─────────────────────────────┐   Heap 0: DEVICE_LOCAL   │
│  │          VRAM               │   (GPU's own memory)     │
│  │       4-24 GB               │                          │
│  │                             │   Types available:       │
│  │   ┌───────────────────┐     │   • DEVICE_LOCAL         │
│  │   │  vertex buffer    │     │     (fast GPU access)    │
│  │   │  (DEVICE_LOCAL)   │     │                          │
│  │   └───────────────────┘     │                          │
│  │   ┌───────────────────┐     │                          │
│  │   │  textures, etc    │     │                          │
│  │   └───────────────────┘     │                          │
│  └─────────────────────────────┘                          │
│                                                            │
│  ┌─────────────────────────────┐   Heap 1: HOST_VISIBLE   │
│  │        System RAM           │   (CPU-accessible)       │
│  │       16-64 GB              │                          │
│  │                             │   Types available:       │
│  │   ┌───────────────────┐     │   • HOST_VISIBLE         │
│  │   │  staging buffer   │     │   • HOST_VISIBLE |       │
│  │   │  (temporary)      │     │     HOST_COHERENT        │
│  │   └───────────────────┘     │   • HOST_VISIBLE |       │
│  │                             │     HOST_COHERENT |      │
│  │   ┌───────────────────┐     │     HOST_CACHED          │
│  │   │  your C++ app     │     │                          │
│  │   └───────────────────┘     │                          │
│  └─────────────────────────────┘                          │
│                                                            │
│  Note: Integrated GPUs (Intel/AMD APUs) may have a single │
│  shared heap — DEVICE_LOCAL | HOST_VISIBLE.                │
└────────────────────────────────────────────────────────────┘
```

---

## Step 1: createBuffer() — A Reusable Helper

We'll create buffers several more times (index buffers, uniform buffers, etc.), so let's extract a helper:

```cpp
void createBuffer(VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties,
                  VkBuffer& buffer,
                  VkDeviceMemory& bufferMemory)
{
    // Create the buffer object
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    // Find suitable memory and allocate
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}
```

Now creating any buffer is a single function call:

```cpp
// Staging buffer (CPU-writable, used as transfer source)
createBuffer(size,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    stagingBuffer, stagingBufferMemory);

// Device-local buffer (GPU-optimized, used as transfer destination + vertex buffer)
createBuffer(size,
    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    vertexBuffer, vertexBufferMemory);
```

### Usage Flags Explained

| Flag | Meaning |
|------|---------|
| `VK_BUFFER_USAGE_VERTEX_BUFFER_BIT` | Can be used as a vertex buffer |
| `VK_BUFFER_USAGE_TRANSFER_SRC_BIT` | Can be used as a **source** for copy operations |
| `VK_BUFFER_USAGE_TRANSFER_DST_BIT` | Can be used as a **destination** for copy operations |
| `VK_BUFFER_USAGE_INDEX_BUFFER_BIT` | Can be used as an index buffer (Chapter 16) |
| `VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT` | Can be used as a uniform buffer (Chapter 17) |

The staging buffer is `TRANSFER_SRC` — we copy **from** it. The vertex buffer is `TRANSFER_DST | VERTEX_BUFFER` — we copy **to** it, and then use it as a vertex buffer for rendering.

---

## Step 2: copyBuffer() — GPU-Side Copy

We can't just `memcpy` between GPU buffers from the CPU. We need to **record a GPU command** to do the copy. This requires a temporary command buffer:

```cpp
void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    // Allocate a temporary command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    // Record the copy command
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);  // Wait for the copy to finish

    // Clean up the temporary command buffer
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}
```

### Why `ONE_TIME_SUBMIT_BIT`?

This tells the driver: "I'm only going to submit this command buffer once, then throw it away." The driver may use this hint to optimize allocation and recording.

### Why `vkQueueWaitIdle`?

We need the copy to finish before we destroy the staging buffer. `vkQueueWaitIdle` blocks the CPU until all commands submitted to that queue are done. For a one-time operation during initialization, this is perfectly fine. (For runtime transfers, you'd use fences for non-blocking behavior.)

---

## Step 3: Reusable One-Time Command Helpers

Since we'll need one-time commands for other things too (texture uploads, layout transitions), let's extract helpers:

```cpp
VkCommandBuffer beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}
```

Now `copyBuffer` becomes much cleaner:

```cpp
void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}
```

These helpers follow a pattern you'll see throughout Vulkan code: **begin → record commands → end**. It's like writing a letter: open envelope, write message, seal and send.

---

## Step 4: Updated createVertexBuffer()

Now we put it all together:

```cpp
void createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // ═══════════════════════════════════════════════════
    //  Create staging buffer (CPU-writable, transfer source)
    // ═══════════════════════════════════════════════════
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    // ═══════════════════════════════════════════════════
    //  Copy vertex data into staging buffer
    // ═══════════════════════════════════════════════════
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    // ═══════════════════════════════════════════════════
    //  Create device-local buffer (GPU-optimized, vertex + transfer dst)
    // ═══════════════════════════════════════════════════
    createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 vertexBuffer, vertexBufferMemory);

    // ═══════════════════════════════════════════════════
    //  Copy from staging buffer to device-local buffer
    // ═══════════════════════════════════════════════════
    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    // ═══════════════════════════════════════════════════
    //  Clean up staging buffer (no longer needed)
    // ═══════════════════════════════════════════════════
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
```

### The Complete Data Journey

```
Step 1: CPU → Staging Buffer (via memcpy)

  vertices[]               stagingBuffer
  ┌──────────────┐         ┌──────────────┐
  │ v0,v1,v2     │ ──────► │ v0,v1,v2     │   HOST_VISIBLE
  │ (CPU memory) │ memcpy  │ (System RAM) │   HOST_COHERENT
  └──────────────┘         └──────┬───────┘
                                  │
Step 2: Staging Buffer → Vertex Buffer (via GPU copy command)
                                  │
                           vkCmdCopyBuffer
                           (GPU executes)
                                  │
                                  ▼
                           ┌──────────────┐
                           │ v0,v1,v2     │   DEVICE_LOCAL
                           │ (VRAM)       │   (fast for GPU!)
                           └──────────────┘
                             vertexBuffer

Step 3: Staging buffer destroyed (it was just a delivery truck)
```

---

## Before vs After Comparison

```
CHAPTER 14 (HOST_VISIBLE):

  CPU ──memcpy──► vertexBuffer (System RAM)
                       ↑
                  GPU reads over PCIe bus every frame
                       │
                  ~16-32 GB/s bandwidth
                  (slower)


CHAPTER 15 (Staging + DEVICE_LOCAL):

  CPU ──memcpy──► stagingBuffer ──GPU copy──► vertexBuffer (VRAM)
                  (temporary)                      ↑
                                              GPU reads from VRAM
                                                   │
                                              ~400-900 GB/s bandwidth
                                              (much faster!)
```

The one-time cost of a GPU copy during initialization is negligible compared to the per-frame savings of reading from fast VRAM.

---

## When to Use Which Memory Type

| Scenario | Memory Type | Why |
|----------|------------|-----|
| Static vertex/index buffers | `DEVICE_LOCAL` via staging | Uploaded once, read many times |
| Uniform buffers (updated every frame) | `HOST_VISIBLE + HOST_COHERENT` | CPU writes every frame; staging would be wasteful |
| Textures | `DEVICE_LOCAL` via staging | Large data, read frequently by GPU |
| Readback buffers (GPU → CPU) | `HOST_VISIBLE + HOST_CACHED` | CPU reads GPU results |

### A Rule of Thumb

```
Does the CPU write to it every frame?
  ├── YES → HOST_VISIBLE | HOST_COHERENT  (avoid staging overhead)
  └── NO  → DEVICE_LOCAL via staging      (upload once, read fast)
```

---

## A Note on Memory Allocation

In a real application, you should **not** call `vkAllocateMemory` for every individual buffer. GPUs have a limited number of allocations (often 4096). Instead, use a memory allocator like [Vulkan Memory Allocator (VMA)](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) that sub-allocates from large blocks:

```
BAD: one allocation per buffer
  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐
  │ buf0 │ │ buf1 │ │ buf2 │ │ buf3 │    4 allocations
  └──────┘ └──────┘ └──────┘ └──────┘

GOOD: sub-allocate from large blocks
  ┌──────────────────────────────────┐
  │ buf0 │ buf1 │ buf2 │ buf3 │ ... │    1 allocation!
  └──────────────────────────────────┘
```

For this tutorial, individual allocations are fine since we have very few buffers.

---

## Try It Yourself

1. **Run the program** — the output should look identical to Chapter 14, but the vertex data is now in fast VRAM.

2. **Add timing**: Measure how long `createVertexBuffer()` takes with and without the staging buffer approach. For a small triangle, there's no visible difference — but it teaches you to think about memory.

3. **Query your GPU's memory heaps**: Add this code and inspect the output:
   ```cpp
   VkPhysicalDeviceMemoryProperties memProps;
   vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
   for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
       std::cout << "Heap " << i << ": "
                 << (memProps.memoryHeaps[i].size / (1024*1024)) << " MB";
       if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
           std::cout << " [DEVICE_LOCAL]";
       std::cout << "\n";
   }
   ```

4. **Experiment**: What happens if you create the final vertex buffer as `HOST_VISIBLE | DEVICE_LOCAL`? Some integrated GPUs support this (unified memory). Check if `findMemoryType` finds a suitable type.

5. **Think about it**: Why does the staging buffer need `TRANSFER_SRC_BIT` and the vertex buffer need `TRANSFER_DST_BIT`? What would happen if you forgot one?

---

## Key Takeaways

- `HOST_VISIBLE` memory is **slow for GPU reads** (~16-32 GB/s over PCIe).
- `DEVICE_LOCAL` memory is **fast for GPU reads** (~400-900 GB/s in VRAM).
- The **staging buffer pattern**: upload to HOST_VISIBLE staging buffer → GPU copies to DEVICE_LOCAL → destroy staging buffer.
- `createBuffer()` is a reusable helper for creating any buffer with specified usage and memory properties.
- `copyBuffer()` uses a **one-time command buffer** to execute a GPU copy.
- `beginSingleTimeCommands()` / `endSingleTimeCommands()` are reusable helpers for any one-time GPU operation.
- The staging buffer usage flag is `TRANSFER_SRC`; the destination buffer needs `TRANSFER_DST`.
- For **static data** (vertices, indices, textures), always use `DEVICE_LOCAL` via staging.
- For **dynamic data** (uniforms updated per-frame), `HOST_VISIBLE` is more practical.
- In production, use a **memory allocator** (like VMA) instead of individual `vkAllocateMemory` calls.

---

[<< Previous: Vertex Buffers](14-vertex-buffers.md) | [Next: Index Buffers >>](16-index-buffers.md)
