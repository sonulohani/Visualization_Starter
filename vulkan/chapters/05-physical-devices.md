# Chapter 5: Physical Devices and Queue Families

[<< Previous: Validation Layers](04-validation-layers.md) | [Next: Logical Device >>](06-logical-device.md)

---

## What is a Physical Device?

A `VkPhysicalDevice` represents an **actual GPU** installed in your system. Unlike most Vulkan objects, you don't *create* physical devices — you **discover** them.

### Analogy

Imagine you walk into a computer shop. The GPUs on the shelves are **physical devices**. You look at them, compare specs, and **pick** the best one. You don't build a GPU — you choose one that's already there.

```
Your System
├── GPU 0: NVIDIA RTX 3080  (Discrete — dedicated graphics card)
├── GPU 1: Intel UHD 630    (Integrated — built into the CPU)
└── GPU 2: (maybe a second discrete card?)
```

---

## Discovering Physical Devices

```cpp
VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

void pickPhysicalDevice() {
    // ─── Step 1: How many GPUs? ───
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("No GPUs with Vulkan support found!");
    }

    std::cout << "Found " << deviceCount << " GPU(s) with Vulkan support.\n";

    // ─── Step 2: Get the GPU handles ───
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // ─── Step 3: Evaluate each GPU ───
    for (const auto& device : devices) {
        printDeviceInfo(device);
        if (isDeviceSuitable(device)) {
            physicalDevice = device;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    // Print the chosen GPU
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    std::cout << "\nSelected GPU: " << props.deviceName << "\n";
}
```

---

## Querying GPU Information

Each physical device has **properties** (what it is) and **features** (what it can do).

### Device Properties

```cpp
void printDeviceInfo(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    std::cout << "\n═══════════════════════════════════\n";
    std::cout << "GPU: " << properties.deviceName << "\n";
    std::cout << "═══════════════════════════════════\n";

    // Device type
    std::cout << "Type: ";
    switch (properties.deviceType) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            std::cout << "Discrete GPU (dedicated graphics card)";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            std::cout << "Integrated GPU (built into CPU)";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            std::cout << "Virtual GPU";
            break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            std::cout << "CPU (software rendering)";
            break;
        default:
            std::cout << "Other/Unknown";
            break;
    }
    std::cout << "\n";

    // API version
    std::cout << "Vulkan API: "
              << VK_API_VERSION_MAJOR(properties.apiVersion) << "."
              << VK_API_VERSION_MINOR(properties.apiVersion) << "."
              << VK_API_VERSION_PATCH(properties.apiVersion) << "\n";

    // Driver version
    std::cout << "Driver Version: " << properties.driverVersion << "\n";

    // Some useful limits
    std::cout << "Max Image Dimension 2D: " << properties.limits.maxImageDimension2D << "\n";
    std::cout << "Max Viewports: " << properties.limits.maxViewports << "\n";
    std::cout << "Max Push Constants Size: " << properties.limits.maxPushConstantsSize << " bytes\n";
    std::cout << "Max Memory Allocations: " << properties.limits.maxMemoryAllocationCount << "\n";
}
```

### Device Types Explained

| Type | What It Is | Performance | Example |
|------|-----------|-------------|---------|
| `DISCRETE_GPU` | Dedicated graphics card | Best | NVIDIA RTX 3080, AMD RX 6800 |
| `INTEGRATED_GPU` | Built into the CPU | Good enough | Intel UHD, AMD Radeon Vega |
| `VIRTUAL_GPU` | Virtual machine GPU | Varies | Cloud gaming GPUs |
| `CPU` | Software rendering | Slowest | SwiftShader, lavapipe |

### Device Features

Features are capabilities that must be explicitly enabled when creating the logical device:

```cpp
void printDeviceFeatures(VkPhysicalDevice device) {
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    std::cout << "\nKey Features:\n";
    std::cout << "  Geometry Shader:         " << (features.geometryShader ? "YES" : "NO") << "\n";
    std::cout << "  Tessellation Shader:     " << (features.tessellationShader ? "YES" : "NO") << "\n";
    std::cout << "  Multi Viewport:          " << (features.multiViewport ? "YES" : "NO") << "\n";
    std::cout << "  Sampler Anisotropy:      " << (features.samplerAnisotropy ? "YES" : "NO") << "\n";
    std::cout << "  Fill Mode Non-Solid:     " << (features.fillModeNonSolid ? "YES" : "NO") << "\n";
    std::cout << "  Wide Lines:              " << (features.wideLines ? "YES" : "NO") << "\n";
    std::cout << "  Depth Clamp:             " << (features.depthClamp ? "YES" : "NO") << "\n";
    std::cout << "  Sample Rate Shading:     " << (features.sampleRateShading ? "YES" : "NO") << "\n";
}
```

---

## Queue Families — The Key Concept

### What Are Queues?

A **queue** is a channel through which you submit work to the GPU. Think of it as a **conveyor belt**:

```
Your App ──[commands]──→ Queue ──[commands]──→ GPU Processing
```

### What Are Queue Families?

Queues are grouped into **families**. All queues in a family have the same capabilities, but the family determines *what type of work* can be submitted.

```
GPU Queue Families:
┌────────────────────────────────────────────────┐
│                                                │
│  Family 0: Graphics + Compute + Transfer       │
│    ├── Queue 0 (we'll use this)                │
│    └── Queue 1                                 │
│                                                │
│  Family 1: Transfer Only (DMA engine)          │
│    └── Queue 0 (dedicated for copying data)    │
│                                                │
│  Family 2: Compute Only                        │
│    └── Queue 0 (async compute)                 │
│                                                │
└────────────────────────────────────────────────┘
```

### Queue Types

| Queue Flag | What It Does | Example Use |
|-----------|-------------|-------------|
| `VK_QUEUE_GRAPHICS_BIT` | Drawing triangles, running vertex/fragment shaders | Rendering your scene |
| `VK_QUEUE_COMPUTE_BIT` | Running compute shaders | Physics simulation, particle effects |
| `VK_QUEUE_TRANSFER_BIT` | Copying buffers and images | Uploading textures to GPU |
| `VK_QUEUE_SPARSE_BINDING_BIT` | Sparse resource management | Virtual textures |
| `VK_QUEUE_VIDEO_DECODE_BIT_KHR` | Hardware video decoding | Playing video |
| `VK_QUEUE_VIDEO_ENCODE_BIT_KHR` | Hardware video encoding | Recording video |

### Why Multiple Families?

Different GPU hardware units handle different tasks. A dedicated **transfer queue** uses the GPU's DMA (Direct Memory Access) engine, which can copy data *simultaneously* with rendering on the graphics queue:

```
Timeline:
Graphics Queue: ████ render scene ████ render scene ████
Transfer Queue: ░░░░░ upload texture ░░░░░ upload model ░░░░░
                ↑ Both run in parallel!
```

---

## Finding Queue Families

We need to find queue families that support:
1. **Graphics** — for rendering
2. **Present** — for displaying to the screen (not all graphics queues can present!)

```cpp
#include <optional>

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};
```

### Why `std::optional`?

We can't use a default value like `-1` or `0` because `0` is a valid queue family index. `std::optional` clearly distinguishes "not found" from "found at index 0".

```cpp
// Without optional:
int graphicsFamily = -1;
if (graphicsFamily == -1) { /* not found */ }
// But -1 requires casting from uint32_t — messy!

// With optional:
std::optional<uint32_t> graphicsFamily;
if (!graphicsFamily.has_value()) { /* not found */ }
if (graphicsFamily.has_value()) { /* use graphicsFamily.value() */ }
```

### Finding the Families

```cpp
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    // Get queue family count
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    // Get queue family properties
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    std::cout << "\nQueue Families (" << queueFamilyCount << "):\n";

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        std::cout << "  Family " << i << ": "
                  << queueFamily.queueCount << " queue(s) | ";

        // Print capabilities
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            std::cout << "Graphics ";
        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
            std::cout << "Compute ";
        if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
            std::cout << "Transfer ";
        if (queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
            std::cout << "Sparse ";
        std::cout << "\n";

        // Check for graphics support
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // Check for present support
        // NOTE: surface must be created BEFORE calling this (Chapter 7)
        // For now, we'll assume graphics == present (true on most hardware)
        VkBool32 presentSupport = false;
        // vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        // if (presentSupport) {
        //     indices.presentFamily = i;
        // }

        // Temporary: assume graphics queue also supports present
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) break;
        i++;
    }

    return indices;
}
```

### Typical Output

```
Queue Families (3):
  Family 0: 16 queue(s) | Graphics Compute Transfer
  Family 1: 2 queue(s) | Transfer
  Family 2: 8 queue(s) | Compute Transfer
```

**Reading this**: Family 0 can do everything (graphics, compute, transfer) and has 16 queues. Family 1 is transfer-only (a dedicated DMA engine). Family 2 is compute-only (for async compute).

---

## Scoring and Picking the Best GPU

If you have multiple GPUs, you might want to pick the best one:

```cpp
int rateDeviceSuitability(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum texture size affects graphics quality
    score += properties.limits.maxImageDimension2D;

    // Application can't function without geometry shaders (example)
    if (!features.geometryShader) {
        return 0;  // Not suitable at all
    }

    // Must have required queue families
    QueueFamilyIndices indices = findQueueFamilies(device);
    if (!indices.isComplete()) {
        return 0;  // Not suitable
    }

    return score;
}

void pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Score each device and pick the best
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    int bestScore = 0;

    for (const auto& device : devices) {
        int score = rateDeviceSuitability(device);

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        std::cout << props.deviceName << " → score: " << score << "\n";

        if (score > bestScore) {
            bestScore = score;
            bestDevice = device;
        }
    }

    if (bestDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    physicalDevice = bestDevice;

    VkPhysicalDeviceProperties finalProps;
    vkGetPhysicalDeviceProperties(physicalDevice, &finalProps);
    std::cout << "\nChosen: " << finalProps.deviceName << "\n";
}
```

---

## Checking Device Extension Support

We'll need certain device extensions (like the swap chain). Check availability:

```cpp
const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME   // Required for displaying to screen
};

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                          availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                              deviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();  // All required extensions found
}
```

Add this check to `isDeviceSuitable`:

```cpp
bool isDeviceSuitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    return indices.isComplete() && extensionsSupported;
}
```

---

## GPU Memory Types

The GPU has different types of memory. Understanding these is crucial for later chapters:

```cpp
void printMemoryInfo(VkPhysicalDevice device) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(device, &memProps);

    std::cout << "\nMemory Heaps (" << memProps.memoryHeapCount << "):\n";
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        float sizeMB = memProps.memoryHeaps[i].size / (1024.0f * 1024.0f);
        std::cout << "  Heap " << i << ": " << sizeMB << " MB";
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            std::cout << " [GPU VRAM]";
        } else {
            std::cout << " [System RAM]";
        }
        std::cout << "\n";
    }

    std::cout << "\nMemory Types (" << memProps.memoryTypeCount << "):\n";
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        std::cout << "  Type " << i << " (Heap "
                  << memProps.memoryTypes[i].heapIndex << "): ";

        VkMemoryPropertyFlags flags = memProps.memoryTypes[i].propertyFlags;
        if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)  std::cout << "DeviceLocal ";
        if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)  std::cout << "HostVisible ";
        if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) std::cout << "HostCoherent ";
        if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)   std::cout << "HostCached ";
        std::cout << "\n";
    }
}
```

### Typical Output (NVIDIA RTX 3080)

```
Memory Heaps (2):
  Heap 0: 10240.00 MB [GPU VRAM]     ← 10GB of dedicated GPU memory
  Heap 1: 32768.00 MB [System RAM]   ← Shared with your regular RAM

Memory Types (11):
  Type 0 (Heap 1): (none)
  Type 7 (Heap 0): DeviceLocal                      ← Best for GPU data
  Type 8 (Heap 1): HostVisible HostCoherent          ← CPU can write, GPU can read
  Type 9 (Heap 0): DeviceLocal HostVisible HostCoherent  ← Best of both (ReBAR)
```

**Key memory types**:

| Type | Best For | CPU Access | GPU Speed |
|------|---------|-----------|-----------|
| `DeviceLocal` | Textures, vertex buffers | No | Fastest |
| `HostVisible + HostCoherent` | Staging buffers, uniform buffers | Yes (read/write) | Slow |
| `DeviceLocal + HostVisible` | When available (ReBAR/SAM) | Yes | Fast |

---

## Try It Yourself

1. **Run the code and list all GPUs** on your system. What type are they?

2. **Print all queue families** for your GPU. How many graphics queues are available?

3. **Check memory**: How much VRAM does your GPU have? How many memory types?

4. **Multi-GPU test**: If you have both a discrete and integrated GPU, modify the scoring function to always prefer the discrete one.

5. **Extension list**: Print all device extensions available on your GPU. Can you find `VK_KHR_swapchain`?

---

## Key Takeaways

- Physical devices are **discovered**, not created.
- **Queue families** determine what types of work a queue can do (graphics, compute, transfer).
- Not all queues can **present** to the screen — you must check separately.
- Use **device properties** to identify the GPU (name, type, limits).
- Use **device features** to check capabilities (geometry shaders, anisotropy, etc.).
- **Device extensions** (like swap chain) must be verified before use.
- **GPU memory** has multiple types optimized for different use cases.

---

## What We've Built So Far

```
✅ Instance
✅ Validation Layers
✅ Physical Device (discovered and selected)
⬜ Logical Device
⬜ Surface
⬜ Swap Chain
⬜ Pipeline
⬜ Drawing!
```

---

[<< Previous: Validation Layers](04-validation-layers.md) | [Next: Logical Device >>](06-logical-device.md)
