# Chapter 25: Memory Management Best Practices

[<< Previous: Advanced Render Passes](24-advanced-renderpasses.md) | [Next: Debugging & Profiling >>](26-debugging-profiling.md)

---

## The Problem: Vulkan's Allocation Limit

Here's something that catches every Vulkan beginner off guard: **the GPU driver only supports a limited number of memory allocations**. On most hardware, this limit — `maxMemoryAllocationCount` — is around **4096**.

```cpp
VkPhysicalDeviceProperties props;
vkGetPhysicalDeviceProperties(physicalDevice, &props);

// Typically 4096 on NVIDIA/AMD, sometimes less on integrated GPUs
std::cout << "Max allocations: " << props.limits.maxMemoryAllocationCount << "\n";
```

That sounds like a lot, but consider what a real application needs:

```
Typical Game Scene — Naive Allocation
──────────────────────────────────────
100 meshes × 1 vertex buffer each     = 100 allocations
100 meshes × 1 index buffer each      = 100 allocations
200 textures (diffuse, normal, etc.)   = 200 allocations
50 uniform buffers (per-object data)   = 50 allocations
5 framebuffer attachments              = 5 allocations
10 staging buffers                     = 10 allocations
                                       ─────────────────
                               Total   = 465 allocations (just one frame!)

With 3 frames in flight:               = 465 × 3 = 1,395
Add shadow maps, post-processing:      = 2,000+ allocations
Add more objects, scenes, UI:          = 4,096+ 💥 LIMIT HIT
```

Once you hit the limit, `vkAllocateMemory` returns `VK_ERROR_OUT_OF_DEVICE_MEMORY` and your application crashes.

### Analogy

Think of memory allocations like **parking permits** in a small parking lot. The lot has space for thousands of cars (gigabytes of VRAM), but the office only hands out 4,096 parking permits. If every motorcycle, bicycle, and truck each demands its own permit, you run out of permits long before you run out of space.

The solution? **Group vehicles together under shared permits** — one permit for the entire motorcycle section, one for compact cars, etc.

---

## Why Not Call vkAllocateMemory Per Buffer?

Beyond the hard limit, calling `vkAllocateMemory` for every buffer or image is bad for several reasons:

```
                        Per-Resource Allocation
                    (What beginners do — DON'T do this!)

  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │ Buffer A │  │ Buffer B │  │ Buffer C │  │ Buffer D │
  │  64 KB   │  │  128 KB  │  │  32 KB   │  │  256 KB  │
  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘
       │              │              │              │
       ↓              ↓              ↓              ↓
  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │ Alloc #1 │  │ Alloc #2 │  │ Alloc #3 │  │ Alloc #4 │  ← 4 allocations!
  │  64 KB   │  │  128 KB  │  │  32 KB   │  │  256 KB  │
  └──────────┘  └──────────┘  └──────────┘  └──────────┘

  Problems:
  ❌ Uses 4 out of ~4096 allocation slots
  ❌ Each vkAllocateMemory call is SLOW (~1ms on some drivers)
  ❌ Memory fragmentation — gaps between allocations waste VRAM
  ❌ Cache inefficiency — scattered memory hurts GPU performance
```

| Problem | Impact |
|---------|--------|
| **Allocation limit** | Hard crash when you exceed `maxMemoryAllocationCount` |
| **Allocation speed** | `vkAllocateMemory` can take ~1ms per call — 1000 calls = 1 second stall |
| **Fragmentation** | Small allocations leave gaps the driver can't reclaim |
| **Cache misses** | Scattered buffers cause more GPU cache misses |
| **Bookkeeping** | You must track every `VkDeviceMemory` handle for cleanup |

---

## The Solution: Sub-Allocation

The professional approach is **sub-allocation**: allocate a few large blocks of memory, then carve them up into smaller regions yourself.

```
                         Sub-Allocation
                   (What professionals do — DO this!)

  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
  │ Buffer A │  │ Buffer B │  │ Buffer C │  │ Buffer D │
  │  64 KB   │  │  128 KB  │  │  32 KB   │  │  256 KB  │
  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘
       │              │              │              │
       ↓              ↓              ↓              ↓
  ┌──────────────────────────────────────────────────────┐
  │                    Alloc #1                           │  ← 1 allocation!
  │  256 MB large block                                  │
  │  ┌────────┬──────────┬──────┬────────────┬─────────┐ │
  │  │ Buf A  │  Buf B   │Buf C │   Buf D    │  free   │ │
  │  │ 64 KB  │ 128 KB   │32 KB │  256 KB    │         │ │
  │  └────────┴──────────┴──────┴────────────┴─────────┘ │
  └──────────────────────────────────────────────────────┘

  Benefits:
  ✅ Only 1 allocation slot used (instead of 4)
  ✅ One fast vkAllocateMemory call at startup
  ✅ No fragmentation — contiguous memory
  ✅ Great cache locality — nearby data
```

### Analogy

Sub-allocation is like a **warehouse with shelving units**. Instead of renting a separate warehouse for every box (expensive, limited permits), you rent one big warehouse and organize the shelves yourself. You know exactly where everything is, and adding a new box just means finding empty shelf space.

### The Alignment Gotcha

When sub-allocating, each buffer needs to start at a memory offset that satisfies its alignment requirement. This is like needing each shelf to start at a specific grid position:

```cpp
VkMemoryRequirements memReqs;
vkGetBufferMemoryRequirements(device, buffer, &memReqs);

// memReqs.alignment might be 256 — the buffer MUST start at a multiple of 256
// memReqs.size is the actual bytes needed
// memReqs.memoryTypeBits tells you which memory types are compatible

// Manual alignment calculation:
VkDeviceSize alignedOffset = (currentOffset + memReqs.alignment - 1)
                           & ~(memReqs.alignment - 1);
```

```
  Memory block (alignment = 256):
  
  Offset:  0         256       512       768       1024
           │          │         │         │         │
           ▼          ▼         ▼         ▼         ▼
  ┌────────┬──────────┬─────────┬─────────┬─────────┐
  │ Buf A  │ padding  │  Buf B  │ padding │  Buf C  │
  │ 200 B  │  56 B    │  400 B  │ 112 B   │  100 B  │
  └────────┴──────────┴─────────┴─────────┴─────────┘
  
  Buf A starts at offset 0   (aligned ✓)
  Buf B starts at offset 256 (aligned ✓) — not 200!
  Buf C starts at offset 768 (aligned ✓) — not 656!
```

Writing a correct, efficient sub-allocator is hard. Thankfully, someone already did it for us.

---

## Vulkan Memory Allocator (VMA)

**VMA** (Vulkan Memory Allocator) is an open-source library by AMD that handles all memory management for you. It's the de-facto standard used by virtually every serious Vulkan application.

```
┌────────────────────────────────────────────────────────────────┐
│                    Without VMA (Manual)                         │
│                                                                │
│  1. vkCreateBuffer()                                           │
│  2. vkGetBufferMemoryRequirements()                            │
│  3. Find compatible memory type                                │
│  4. vkAllocateMemory() — manage pool yourself                  │
│  5. vkBindBufferMemory() at correct offset                     │
│  6. Track allocation for later cleanup                         │
│  7. Handle alignment, defragmentation, budget...               │
│                                                                │
│  = ~50 lines of careful code per buffer                        │
├────────────────────────────────────────────────────────────────┤
│                      With VMA                                  │
│                                                                │
│  1. vmaCreateBuffer()  ← DONE                                 │
│                                                                │
│  = 1 function call                                             │
└────────────────────────────────────────────────────────────────┘
```

### What VMA Does for You

| Feature | Description |
|---------|-------------|
| **Sub-allocation** | Allocates large blocks, carves them up automatically |
| **Memory type selection** | Picks the best memory type for your usage |
| **Alignment handling** | Respects all alignment requirements |
| **Defragmentation** | Can compact memory to reduce fragmentation |
| **Budget tracking** | Queries `VK_EXT_memory_budget` to avoid over-allocating |
| **Dedicated allocations** | Automatically uses dedicated allocation for large resources |
| **Stats & debugging** | Provides detailed allocation statistics and JSON dumps |
| **Thread safety** | Safe to call from multiple threads |

---

## Setting Up VMA

VMA is a single-header library. Download `vk_mem_alloc.h` from the [GPUOpen repository](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator).

### Integration

In **exactly one** `.cpp` file:

```cpp
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
```

In all other files that need VMA types:

```cpp
#include "vk_mem_alloc.h"  // no VMA_IMPLEMENTATION
```

If using CMake, you can also use the package:

```cmake
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
target_link_libraries(MyApp PRIVATE GPUOpen::VulkanMemoryAllocator)
```

### Creating the VMA Allocator

The `VmaAllocator` is the central object. Create it once, right after your `VkDevice`:

```cpp
class HelloTriangleApp {
    // ... existing members ...
    VmaAllocator allocator;  // Add this

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createAllocator();       // ← Add after device creation
        createSwapChain();
        // ... rest of init ...
    }

    void createAllocator() {
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = device;
        allocatorInfo.instance = instance;

        // Required for Vulkan 1.1+ features (highly recommended)
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_2;

        // Enable memory budget tracking if the extension is available
        // (requires VK_EXT_memory_budget, supported on most modern GPUs)
        allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

        if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create VMA allocator!");
        }
    }
};
```

That's it. VMA now manages all memory allocation for your application.

---

## Creating Buffers with VMA

Remember the painful manual buffer creation from earlier chapters? Here's the comparison:

### Before (Manual — ~50 lines)

```cpp
void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties, VkBuffer& buffer,
                  VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}
```

### After (VMA — ~15 lines)

```cpp
void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                  VmaMemoryUsage memoryUsage, VkBuffer& buffer,
                  VmaAllocation& allocation)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                        &buffer, &allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }
}
```

Notice what disappeared:
- No `vkGetBufferMemoryRequirements`
- No `findMemoryType` — VMA picks the best memory type
- No `vkAllocateMemory` — VMA sub-allocates from its pools
- No `vkBindBufferMemory` — VMA binds automatically
- No `VkDeviceMemory` to track — VMA tracks it via `VmaAllocation`

### Practical Examples

**Vertex buffer (GPU-only, fast access):**

```cpp
VkBuffer vertexBuffer;
VmaAllocation vertexAllocation;

VkBufferCreateInfo bufferInfo{};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = sizeof(vertices[0]) * vertices.size();
bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

VmaAllocationCreateInfo allocInfo{};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;  // large buffer hint

vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                &vertexBuffer, &vertexAllocation, nullptr);
```

**Staging buffer (CPU-writable, for uploads):**

```cpp
VkBuffer stagingBuffer;
VmaAllocation stagingAllocation;

VkBufferCreateInfo bufferInfo{};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = dataSize;
bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

VmaAllocationCreateInfo allocInfo{};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                  VMA_ALLOCATION_CREATE_MAPPED_BIT;

VmaAllocationInfo allocationInfo;
vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                &stagingBuffer, &stagingAllocation, &allocationInfo);

// Data is already mapped! Just memcpy:
memcpy(allocationInfo.pMappedData, sourceData, dataSize);
```

**Uniform buffer (CPU-writable, updated every frame):**

```cpp
VkBuffer uniformBuffer;
VmaAllocation uniformAllocation;
VmaAllocationInfo uniformAllocInfo;

VkBufferCreateInfo bufferInfo{};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = sizeof(UniformBufferObject);
bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

VmaAllocationCreateInfo allocInfo{};
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                  VMA_ALLOCATION_CREATE_MAPPED_BIT;

vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
                &uniformBuffer, &uniformAllocation, &uniformAllocInfo);

// Update every frame — persistent mapping, no map/unmap needed:
UniformBufferObject ubo{};
ubo.model = /* ... */;
ubo.view  = /* ... */;
ubo.proj  = /* ... */;
memcpy(uniformAllocInfo.pMappedData, &ubo, sizeof(ubo));
```

---

## Creating Images with VMA

Image creation gets the same simplification:

### Before (Manual)

```cpp
void createImage(uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties,
                 VkImage& image, VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    vkCreateImage(device, &imageInfo, nullptr, &image);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        memRequirements.memoryTypeBits, properties);

    vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory);
    vkBindImageMemory(device, image, imageMemory, 0);
}
```

### After (VMA)

```cpp
void createImage(uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage,
                 VkImage& image, VmaAllocation& allocation)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(allocator, &imageInfo, &allocInfo,
                       &image, &allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image!");
    }
}
```

Again: no `vkGetImageMemoryRequirements`, no `findMemoryType`, no `vkAllocateMemory`, no `vkBindImageMemory`.

---

## VMA Memory Usage Hints

VMA uses `VMA_MEMORY_USAGE_AUTO` as the recommended default, but you guide its decisions with flags:

| Scenario | VmaAllocationCreateInfo Settings | Result |
|----------|----------------------------------|--------|
| **GPU-only resource** (textures, most buffers) | `usage = VMA_MEMORY_USAGE_AUTO` | Places in `DEVICE_LOCAL` — fastest GPU access |
| **Staging buffer** (CPU→GPU upload) | `usage = VMA_MEMORY_USAGE_AUTO` + `flags = HOST_ACCESS_SEQUENTIAL_WRITE_BIT` | Places in `HOST_VISIBLE` — CPU can write, GPU can read |
| **Readback buffer** (GPU→CPU download) | `usage = VMA_MEMORY_USAGE_AUTO` + `flags = HOST_ACCESS_RANDOM_BIT` | Places in `HOST_VISIBLE | HOST_CACHED` — CPU can read efficiently |
| **Uniform buffer** (updated every frame) | `usage = VMA_MEMORY_USAGE_AUTO` + `flags = HOST_ACCESS_SEQUENTIAL_WRITE_BIT | MAPPED_BIT` | Persistently mapped, CPU-writable |
| **Large dedicated resource** | Add `DEDICATED_MEMORY_BIT` flag | Uses `VK_KHR_dedicated_allocation` for big textures |

### The Old Usage Enums (Deprecated)

You may see older code using these enums. They still work but `VMA_MEMORY_USAGE_AUTO` with flags is preferred:

```cpp
// OLD — still works but not recommended:
allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;       // → use AUTO instead
allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;     // → use AUTO + HOST_ACCESS_SEQUENTIAL_WRITE
allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;     // → use AUTO + HOST_ACCESS_RANDOM
allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;       // → use AUTO + HOST_ACCESS_RANDOM

// NEW — recommended:
allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
```

---

## GPU Memory Types Explained

Understanding GPU memory types is essential for making the right allocation decisions. Every GPU exposes different memory heaps and types:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        GPU Memory Architecture                          │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────┐       │
│  │                    GPU (Discrete)                            │       │
│  │                                                              │       │
│  │   ┌────────────────────────────────────────────┐            │       │
│  │   │          VRAM (Video RAM)                   │            │       │
│  │   │          e.g., 8 GB GDDR6                   │            │       │
│  │   │                                             │            │       │
│  │   │   Heap 0: DEVICE_LOCAL                      │            │       │
│  │   │   ───────────────────                       │            │       │
│  │   │   • Fastest GPU access (~500 GB/s)          │            │       │
│  │   │   • CPU CANNOT read/write directly          │            │       │
│  │   │   • Use for: textures, vertex/index buffers │            │       │
│  │   │                                             │            │       │
│  │   │   Heap 1 (ReBAR): DEVICE_LOCAL +            │            │       │
│  │   │                    HOST_VISIBLE             │            │       │
│  │   │   ─────────────────────────────             │            │       │
│  │   │   • Fast GPU access + CPU can write         │            │       │
│  │   │   • 256 MB (or full VRAM with ReBAR)        │            │       │
│  │   │   • Use for: dynamic uniform/storage bufs   │            │       │
│  │   └────────────────────────────────────────────┘            │       │
│  │                                                              │       │
│  └──────────────────────────────────────────────────────────────┘       │
│                          │ PCIe Bus                                      │
│                          │ (~16-32 GB/s)                                │
│  ┌───────────────────────┴──────────────────────────────────────┐       │
│  │                    System RAM                                 │       │
│  │                    e.g., 32 GB DDR5                            │       │
│  │                                                               │       │
│  │   Heap 2: HOST_VISIBLE + HOST_COHERENT                       │       │
│  │   ─────────────────────────────────────                       │       │
│  │   • CPU can read/write directly (~50 GB/s)                   │       │
│  │   • GPU reads are SLOW (crosses PCIe)                        │       │
│  │   • Use for: staging buffers, readback                       │       │
│  │                                                               │       │
│  │   Heap 3: HOST_VISIBLE + HOST_COHERENT + HOST_CACHED        │       │
│  │   ───────────────────────────────────────────────            │       │
│  │   • CPU reads are fast (cached)                              │       │
│  │   • Use for: GPU→CPU readback (screenshots, compute)        │       │
│  └──────────────────────────────────────────────────────────────┘       │
│                                                                         │
└──────────────────────────────────────────────────────────────────────────┘
```

### Memory Property Flags Reference

| Flag | Meaning |
|------|---------|
| `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT` | On the GPU — fastest for GPU operations |
| `VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT` | CPU can map and access it (read/write) |
| `VK_MEMORY_PROPERTY_HOST_COHERENT_BIT` | CPU writes are immediately visible to GPU (no flush needed) |
| `VK_MEMORY_PROPERTY_HOST_CACHED_BIT` | CPU reads are cached — fast for readback |
| `VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT` | Memory may not be allocated until actually used (mobile GPUs) |

### What is ReBAR (Resizable BAR)?

Traditionally, the CPU can only access 256 MB of VRAM through a "window" called the BAR (Base Address Register). **Resizable BAR** (ReBAR), also called **Smart Access Memory** (AMD), opens this window to the **full VRAM size**:

```
Without ReBAR:
  CPU ──[256 MB window]──→ 8 GB VRAM
  Only 256 MB is HOST_VISIBLE + DEVICE_LOCAL

With ReBAR:
  CPU ──[  8 GB window ]──→ 8 GB VRAM
  ALL of VRAM is HOST_VISIBLE + DEVICE_LOCAL!
  → Can skip staging buffers for many uploads
```

You can detect ReBAR by checking if a memory type has both `DEVICE_LOCAL` and `HOST_VISIBLE` with a large heap:

```cpp
VkPhysicalDeviceMemoryProperties memProps;
vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
    auto flags = memProps.memoryTypes[i].propertyFlags;
    auto heapIndex = memProps.memoryTypes[i].heapIndex;
    auto heapSize = memProps.memoryHeaps[heapIndex].size;

    bool deviceLocal  = flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    bool hostVisible  = flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;

    if (deviceLocal && hostVisible && heapSize > 256ULL * 1024 * 1024) {
        std::cout << "ReBAR detected! " << (heapSize / (1024*1024)) << " MB\n";
    }
}
```

---

## When to Use Which Memory Type — Decision Table

Use this flowchart to decide the right memory type for any resource:

```
                         ┌──────────────────────┐
                         │  Does the CPU need    │
                         │  to write this data?  │
                         └──────────┬───────────┘
                               ╱         ╲
                          YES ╱           ╲ NO
                            ╱               ╲
               ┌────────────────┐    ┌────────────────┐
               │  Is it updated  │    │  DEVICE_LOCAL   │
               │  every frame?   │    │  (GPU-only)     │
               └───────┬────────┘    │                  │
                  ╱         ╲        │  Use for:        │
             YES ╱           ╲ NO   │  • Textures      │
               ╱               ╲     │  • Vertex buffers│
  ┌──────────────────┐  ┌───────────┐│  • Index buffers │
  │  HOST_VISIBLE +   │  │  Staging  ││  • MSAA targets  │
  │  DEVICE_LOCAL     │  │  buffer   │└────────────────┘
  │  (if available)   │  │  upload   │
  │                   │  │           │
  │  Use for:         │  │  HOST_VISIBLE         
  │  • Uniform buffers│  │  + COHERENT            
  │  • Dynamic data   │  │                        
  │  • Push constants │  │  Upload once via        
  │    alternatives   │  │  staging → DEVICE_LOCAL 
  └──────────────────┘  └───────────┘
```

### Quick Reference Table

| Resource Type | Memory Type | Why |
|--------------|-------------|-----|
| Textures | `DEVICE_LOCAL` | Read millions of times by fragment shader |
| Vertex buffers | `DEVICE_LOCAL` | Read every draw call, never changes |
| Index buffers | `DEVICE_LOCAL` | Same as vertex buffers |
| Staging buffers | `HOST_VISIBLE + HOST_COHERENT` | CPU writes, GPU reads once, then discard |
| Uniform buffers | `HOST_VISIBLE + HOST_COHERENT` (or `DEVICE_LOCAL + HOST_VISIBLE` if available) | CPU writes every frame, GPU reads every draw |
| Storage buffers (GPU compute) | `DEVICE_LOCAL` | GPU reads/writes, CPU doesn't touch |
| Readback buffers | `HOST_VISIBLE + HOST_CACHED` | GPU writes, CPU reads — caching speeds up CPU reads |
| Framebuffer attachments | `DEVICE_LOCAL` | GPU-only render targets |
| Depth buffers | `DEVICE_LOCAL + LAZILY_ALLOCATED` (mobile) | May never need to store to memory |

---

## Memory Budget Tracking

Running out of GPU memory causes crashes or severe stuttering. VMA can track the **memory budget** if the `VK_EXT_memory_budget` extension is available:

```cpp
VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
vmaGetHeapBudgets(allocator, budgets);

for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
    std::cout << "Heap " << i << ":\n";
    std::cout << "  Usage:  " << (budgets[i].usage / (1024*1024)) << " MB\n";
    std::cout << "  Budget: " << (budgets[i].budget / (1024*1024)) << " MB\n";

    float percentUsed = 100.0f * budgets[i].usage / budgets[i].budget;
    std::cout << "  Used:   " << percentUsed << "%\n";

    if (percentUsed > 90.0f) {
        std::cout << "  ⚠ WARNING: Heap nearly full!\n";
    }
}
```

```
Example output:
  Heap 0 (DEVICE_LOCAL — VRAM):
    Usage:  1,847 MB
    Budget: 7,680 MB
    Used:   24.1%
  
  Heap 1 (HOST_VISIBLE — System RAM):
    Usage:    128 MB
    Budget: 16,384 MB
    Used:   0.8%
```

### VMA Statistics

VMA can also provide detailed allocation statistics:

```cpp
VmaTotalStatistics stats;
vmaCalculateStatistics(allocator, &stats);

std::cout << "Total allocations: " << stats.total.statistics.blockCount << " blocks\n";
std::cout << "Total sub-allocations: " << stats.total.statistics.allocationCount << "\n";
std::cout << "Total memory used: "
          << (stats.total.statistics.allocationBytes / (1024*1024)) << " MB\n";
std::cout << "Total memory reserved: "
          << (stats.total.statistics.blockBytes / (1024*1024)) << " MB\n";
```

You can even dump a full JSON report for analysis:

```cpp
char* statsString;
vmaBuildStatsString(allocator, &statsString, VK_TRUE);
// Write to file for visualization tools
std::ofstream file("vma_stats.json");
file << statsString;
vmaFreeStatsString(allocator, statsString);
```

---

## Cleanup with VMA

Cleanup mirrors creation — but in reverse order. Replace your manual memory frees with VMA calls:

```cpp
void cleanup() {
    // ... wait for device idle ...
    
    // Destroy buffers (replaces vkDestroyBuffer + vkFreeMemory)
    vmaDestroyBuffer(allocator, vertexBuffer, vertexAllocation);
    vmaDestroyBuffer(allocator, indexBuffer, indexAllocation);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vmaDestroyBuffer(allocator, uniformBuffers[i], uniformAllocations[i]);
    }

    // Destroy images (replaces vkDestroyImage + vkFreeMemory)
    // Note: destroy the VkImageView FIRST, then the image+allocation
    vkDestroyImageView(device, textureImageView, nullptr);
    vmaDestroyImage(allocator, textureImage, textureAllocation);

    vkDestroyImageView(device, depthImageView, nullptr);
    vmaDestroyImage(allocator, depthImage, depthAllocation);

    // Destroy staging buffers if still alive
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);

    // Destroy the allocator LAST (after all allocations are freed)
    vmaDestroyAllocator(allocator);

    // Then destroy the device, surface, instance...
    vkDestroyDevice(device, nullptr);
    // ...
}
```

```
VMA Cleanup Order:
  
  1. vkDeviceWaitIdle(device)          ← GPU must be done
  2. Destroy all VkImageViews          ← Views reference images
  3. vmaDestroyImage(...)              ← Frees image + memory
  4. vmaDestroyBuffer(...)             ← Frees buffer + memory
  5. vmaDestroyAllocator(allocator)    ← Frees all VMA internals
  6. vkDestroyDevice(device)           ← Destroys the device
  7. vkDestroyInstance(instance)       ← Destroys the instance
```

---

## Putting It All Together — VMA Conversion Checklist

Here's how to convert an existing codebase from manual memory management to VMA:

```
Step-by-step conversion:
  
  1. Add #include "vk_mem_alloc.h" (with VMA_IMPLEMENTATION in one .cpp)
  2. Add VmaAllocator allocator member
  3. Call vmaCreateAllocator() after device creation
  4. Replace VkDeviceMemory members with VmaAllocation
  5. Replace createBuffer() to use vmaCreateBuffer()
  6. Replace createImage() to use vmaCreateImage()
  7. Replace vkMapMemory/vkUnmapMemory with VMA_ALLOCATION_CREATE_MAPPED_BIT
  8. Replace vkFreeMemory + vkDestroyBuffer with vmaDestroyBuffer()
  9. Replace vkFreeMemory + vkDestroyImage with vmaDestroyImage()
  10. Call vmaDestroyAllocator() in cleanup (before vkDestroyDevice)
  11. Delete findMemoryType() — VMA handles it
```

---

## Complete Example: Vertex Buffer with VMA

Here's a complete, production-ready vertex buffer creation using VMA with a staging buffer:

```cpp
void createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    // Step 1: Create staging buffer (CPU-visible, for upload)
    VkBuffer stagingBuffer;
    VmaAllocation stagingAllocation;
    VmaAllocationInfo stagingAllocInfo;

    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = bufferSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocCreateInfo{};
    stagingAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    stagingAllocCreateInfo.flags =
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
        VMA_ALLOCATION_CREATE_MAPPED_BIT;

    vmaCreateBuffer(allocator, &stagingBufferInfo, &stagingAllocCreateInfo,
                    &stagingBuffer, &stagingAllocation, &stagingAllocInfo);

    // Step 2: Copy vertex data to staging buffer (already mapped)
    memcpy(stagingAllocInfo.pMappedData, vertices.data(), bufferSize);

    // Step 3: Create device-local vertex buffer (GPU-only, fast)
    VkBufferCreateInfo vertexBufferInfo{};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = bufferSize;
    vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                             VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VmaAllocationCreateInfo vertexAllocInfo{};
    vertexAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    vmaCreateBuffer(allocator, &vertexBufferInfo, &vertexAllocInfo,
                    &vertexBuffer, &vertexAllocation, nullptr);

    // Step 4: Copy staging → device-local via command buffer
    copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

    // Step 5: Destroy staging buffer (no longer needed)
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
}
```

---

## Try It Yourself

1. **Check your GPU's allocation limit**: Print `maxMemoryAllocationCount` from your physical device properties. On your hardware, how many naive per-resource allocations would it take to hit the wall?

2. **Integrate VMA into your project**: Download `vk_mem_alloc.h`, add it to your project, create a `VmaAllocator`, and convert your `createBuffer()` to use `vmaCreateBuffer()`. Verify the triangle still renders.

3. **Print VMA statistics**: After creating all your buffers and images, call `vmaCalculateStatistics` and print the block count vs allocation count. How many actual `vkAllocateMemory` calls did VMA make vs how many sub-allocations it created?

4. **Monitor memory budget**: Add a per-frame budget check using `vmaGetHeapBudgets`. Print a warning if any heap exceeds 80% usage. Observe how memory usage changes as you load textures and models.

5. **Experiment with memory types**: Create a buffer with `VMA_MEMORY_USAGE_AUTO` and inspect the resulting allocation with `vmaGetAllocationInfo`. Check which memory type index VMA chose and look up its flags. Try adding `HOST_ACCESS_SEQUENTIAL_WRITE_BIT` and see if VMA picks a different memory type.

---

## Key Takeaways

- Vulkan has a **hard allocation limit** (`maxMemoryAllocationCount` ~4096) — calling `vkAllocateMemory` per resource will eventually crash your application.
- **Sub-allocation** is the solution: allocate large memory blocks and carve them into smaller regions.
- **VMA (Vulkan Memory Allocator)** by AMD handles sub-allocation, alignment, memory type selection, and budget tracking for you — use it in every project.
- `vmaCreateBuffer` and `vmaCreateImage` replace ~50 lines of manual memory management with a single call.
- Use `VMA_MEMORY_USAGE_AUTO` with appropriate flags — VMA picks the best memory type automatically.
- `DEVICE_LOCAL` memory is fastest for GPU access; `HOST_VISIBLE` memory is needed when the CPU must read or write.
- **ReBAR** (Resizable BAR) allows CPU access to all VRAM, potentially eliminating staging buffers.
- Track your **memory budget** with `vmaGetHeapBudgets` to avoid out-of-memory crashes.
- Destroy allocations **before** destroying the VMA allocator, and destroy the allocator **before** `vkDestroyDevice`.

---

## What We've Built So Far

```
✅ Instance, Validation, Debug Messenger
✅ Physical Device, Logical Device, Queues
✅ Surface, Swap Chain, Image Views
✅ Render Pass, Graphics Pipeline, Shaders
✅ Framebuffers, Command Pool, Command Buffers
✅ Synchronization, Frames in Flight
✅ Vertex Buffers, Index Buffers, Staging Buffers
✅ Uniform Buffers, Descriptor Sets
✅ Texture Mapping, Depth Buffering
✅ Model Loading, Mipmaps, Multisampling
✅ Dynamic Rendering, Push Constants, Compute Shaders
✅ Advanced Render Passes
✅ Memory Management with VMA            ← NEW!
⬜ Debugging & Profiling
⬜ Best Practices & What's Next
```

---

[<< Previous: Advanced Render Passes](24-advanced-renderpasses.md) | [Next: Debugging & Profiling >>](26-debugging-profiling.md)
