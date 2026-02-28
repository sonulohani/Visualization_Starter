# Chapter 6: Logical Device and Queues

[<< Previous: Physical Devices](05-physical-devices.md) | [Next: Window Surface & Swap Chain >>](07-surface-swapchain.md)

---

## Physical Device vs Logical Device

You've picked a physical GPU. Now you need a **logical device** — your application's interface to that GPU.

### Analogy

Think of it like a **bank account**:
- The **physical device** is the bank itself (it exists whether you use it or not).
- The **logical device** is your **account** at that bank (your personal way to interact with it).

You can have multiple accounts (logical devices) at the same bank (physical device). Each account has its own settings, permissions, and access.

```
Physical Device (GPU)
├── Logical Device A (your app)
│   ├── Graphics Queue
│   └── Present Queue
│
└── Logical Device B (another app — or another subsystem)
    ├── Graphics Queue
    └── Compute Queue
```

### What the Logical Device Does

1. **Allocates queues** from the physical device's queue families.
2. **Enables features** you want to use (anisotropic filtering, geometry shaders, etc.).
3. **Enables device extensions** (swap chain, ray tracing, etc.).
4. **Provides the handle** for creating all other Vulkan objects (buffers, images, pipelines, etc.).

---

## Creating the Logical Device

### Step 1: Specify Which Queues You Want

```cpp
VkDevice device;                // The logical device handle
VkQueue graphicsQueue;          // Handle to the graphics queue
VkQueue presentQueue;           // Handle to the present queue

void createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    // We may need queues from multiple families
    // Use a set to avoid duplicates (graphics and present might be the same family)
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    // Queue priority (0.0 to 1.0) — influences scheduling
    // With a single queue per family, this doesn't matter much
    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;        // We only need 1 queue per family
        queueCreateInfo.pQueuePriorities = &queuePriority;

        queueCreateInfos.push_back(queueCreateInfo);
    }

    std::cout << "Requesting " << queueCreateInfos.size() << " queue(s).\n";
```

### Why Use a `std::set` for Queue Families?

On most GPUs, the graphics and present queue families are the **same family** (e.g., both family 0). If we request the same family twice, Vulkan will complain. The `std::set` automatically removes duplicates:

```
Scenario 1: Same family (most common)
  graphicsFamily = 0, presentFamily = 0
  set = {0}  →  Create 1 queue from family 0

Scenario 2: Different families
  graphicsFamily = 0, presentFamily = 2
  set = {0, 2}  →  Create 1 queue from family 0, 1 from family 2
```

### Step 2: Specify Features You Want

```cpp
    // Enable specific GPU features
    // Only enable what you actually need!
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;  // Better texture quality
    // deviceFeatures.fillModeNonSolid = VK_TRUE;  // Wireframe rendering
    // deviceFeatures.wideLines = VK_TRUE;          // Line width > 1.0
```

**Important**: If you enable a feature the GPU doesn't support, device creation will fail. Always check with `vkGetPhysicalDeviceFeatures` first (Chapter 5).

### Step 3: Specify Extensions

```cpp
    // Device extensions (swap chain is the most important one)
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
```

### Step 4: Create the Device

```cpp
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    // Queues
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    // Features
    createInfo.pEnabledFeatures = &deviceFeatures;

    // Extensions
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Validation layers on the device (deprecated but still set for compatibility)
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    // Create it!
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    std::cout << "Logical device created successfully!\n";
```

### Step 5: Retrieve Queue Handles

After creating the device, get handles to the queues we requested:

```cpp
    // Get the queue handles
    // Parameters: device, queueFamilyIndex, queueIndex (0 = first queue), output
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

    std::cout << "Queue handles retrieved.\n";
    std::cout << "  Graphics queue family: " << indices.graphicsFamily.value() << "\n";
    std::cout << "  Present queue family:  " << indices.presentFamily.value() << "\n";

    if (indices.graphicsFamily.value() == indices.presentFamily.value()) {
        std::cout << "  (Same family — this is normal on most GPUs)\n";
    }
}
```

---

## Queue Priority Explained

When you request multiple queues from the same family, priorities influence scheduling:

```cpp
float priorities[] = {1.0f, 0.5f};  // Queue 0 gets priority over Queue 1

VkDeviceQueueCreateInfo queueInfo{};
queueInfo.queueCount = 2;
queueInfo.pQueuePriorities = priorities;
```

| Priority | Meaning |
|----------|---------|
| `1.0` | Highest priority — GPU processes this first |
| `0.5` | Medium priority |
| `0.0` | Lowest priority — processed when nothing else is waiting |

In practice, with a single queue per family (which is what we're doing), priority doesn't matter.

---

## Instance Extensions vs Device Extensions

This is a common point of confusion:

| | Instance Extensions | Device Extensions |
|---|---|---|
| **Scope** | Global (system-wide) | Per-GPU |
| **Set during** | `vkCreateInstance()` | `vkCreateDevice()` |
| **Examples** | `VK_EXT_debug_utils`, `VK_KHR_surface` | `VK_KHR_swapchain`, `VK_KHR_ray_tracing_pipeline` |
| **Purpose** | System-level features | GPU-specific features |

```
Instance Extensions:
  VK_KHR_surface          ← "I want to render to a window"
  VK_KHR_xcb_surface      ← "Specifically, an XCB window on Linux"
  VK_EXT_debug_utils      ← "I want debug messages"

Device Extensions:
  VK_KHR_swapchain        ← "This GPU should manage presentation images"
  VK_KHR_ray_tracing_pipeline  ← "This GPU should do ray tracing"
```

---

## Device Layers (Deprecated but Important)

You might notice we still pass validation layers to `VkDeviceCreateInfo`. This is technically **deprecated** in modern Vulkan — instance layers now apply to all devices. But older implementations may still need it, so we include it for compatibility:

```cpp
// Deprecated but harmless — include for older drivers
if (enableValidationLayers) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
}
```

---

## Cleanup

The logical device must be destroyed before the instance:

```cpp
void cleanup() {
    // ─── Destroy in reverse creation order ───

    vkDestroyDevice(device, nullptr);  // Queues are cleaned up automatically

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}
```

**Note**: You don't destroy queues — they're automatically cleaned up when the device is destroyed.

---

## Updated initVulkan

```cpp
void initVulkan() {
    createInstance();         // 1
    setupDebugMessenger();    // 2
    // createSurface();       // 3 — Chapter 7
    pickPhysicalDevice();     // 4
    createLogicalDevice();    // 5
}
```

---

## The Complete Device Creation Example

```cpp
void createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create logical device!");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}
```

---

## Try It Yourself

1. **Create the logical device** and verify it succeeds.

2. **Enable a feature**: Enable `fillModeNonSolid` (for wireframe rendering later). Check that your GPU supports it first.

3. **Request 2 queues** from the graphics family (if available — check `queueCount`):
   ```cpp
   queueCreateInfo.queueCount = 2;
   float priorities[] = {1.0f, 0.5f};
   queueCreateInfo.pQueuePriorities = priorities;
   ```

4. **Test error handling**: Try requesting a queue from a non-existent family index (e.g., index 99). What error do you get from validation layers?

---

## Key Takeaways

- The **logical device** is your application's handle to the GPU.
- You request **queues** from specific queue families during device creation.
- **Features** must be explicitly enabled (and must be supported by the GPU).
- **Device extensions** (like swap chain) are per-GPU capabilities.
- Use `std::set` to avoid requesting duplicate queue families.
- Queue **priority** (0.0–1.0) influences scheduling when multiple queues compete.
- Destroy the device **before** the instance in cleanup.

---

## What We've Built So Far

```
✅ Instance
✅ Validation Layers
✅ Physical Device
✅ Logical Device + Queues
⬜ Surface
⬜ Swap Chain
⬜ Pipeline
⬜ Drawing!
```

---

[<< Previous: Physical Devices](05-physical-devices.md) | [Next: Window Surface & Swap Chain >>](07-surface-swapchain.md)
