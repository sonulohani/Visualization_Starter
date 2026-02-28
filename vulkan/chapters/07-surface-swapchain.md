# Chapter 7: Window Surface and Swap Chain

[<< Previous: Logical Device](06-logical-device.md) | [Next: Image Views >>](08-image-views.md)

---

## Part 1: Window Surface

### What is a Surface?

Vulkan is platform-agnostic — it doesn't know about windows, screens, or operating systems. To display rendered images, we need a **surface** that connects Vulkan to the window system.

### Analogy

Think of the surface as a **TV screen** in a control room:
- Vulkan renders images in its own world (the GPU).
- The surface is the TV screen where those images are displayed.
- Without the TV, Vulkan still renders — but nobody sees the result.

```
          Vulkan World              Window System
┌─────────────────────┐      ┌──────────────────┐
│                     │      │                  │
│  Rendered Images ───┼──→ VkSurfaceKHR ──→ Window
│                     │      │                  │
└─────────────────────┘      └──────────────────┘
```

### Creating the Surface

GLFW handles all the platform-specific code for us:

```cpp
VkSurfaceKHR surface;

void createSurface() {
    // GLFW creates the right surface for the current OS:
    //   - VK_KHR_win32_surface on Windows
    //   - VK_KHR_xcb_surface or VK_KHR_xlib_surface on Linux
    //   - VK_MVK_macos_surface on macOS (through MoltenVK)

    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface!");
    }

    std::cout << "Window surface created!\n";
}
```

That's only 3 lines! Behind the scenes, GLFW does something like this on Linux:

```cpp
// What GLFW does internally (Linux/XCB example):
VkXcbSurfaceCreateInfoKHR surfaceInfo{};
surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
surfaceInfo.connection = xcbConnection;
surfaceInfo.window = xcbWindow;
vkCreateXcbSurfaceKHR(instance, &surfaceInfo, nullptr, &surface);
```

### Critical: Create Surface BEFORE Picking Physical Device

The surface is needed to check present support. The correct initialization order is:

```
1. createInstance()
2. setupDebugMessenger()
3. createSurface()           ← CREATE SURFACE HERE
4. pickPhysicalDevice()      ← Needs surface for present check
5. createLogicalDevice()
```

### Cleanup

Destroy the surface before the instance, after the device:

```cpp
void cleanup() {
    // ... destroy swap chain, device, etc. ...

    vkDestroySurfaceKHR(instance, surface, nullptr);   // After device
    // ... destroy debug messenger ...
    vkDestroyInstance(instance, nullptr);               // Last Vulkan object
    glfwDestroyWindow(window);
    glfwTerminate();
}
```

---

## Part 2: Swap Chain

### What is a Swap Chain?

The swap chain is a **queue of images** that take turns being displayed on screen. While one image is shown, another is being rendered to.

### Analogy: The Painter with Multiple Canvases

Imagine a painter in a gallery:

```
┌─────────────────────────────────────────────────────────┐
│                    Double Buffering                       │
│                                                          │
│  Canvas A: 🎨 Currently being painted (back buffer)     │
│  Canvas B: 🖼️  Currently displayed (front buffer)        │
│                                                          │
│  When Canvas A is done:                                  │
│    Canvas A → front (displayed)                          │
│    Canvas B → back (painter works on this now)           │
│                                                          │
│  This prevents visitors from seeing half-finished work!  │
└─────────────────────────────────────────────────────────┘
```

Triple buffering adds a third canvas for even smoother results:

```
┌─────────────────────────────────────────────────────────┐
│                    Triple Buffering                       │
│                                                          │
│  Canvas A: 🎨 Being painted                              │
│  Canvas B: ⏳ Finished, waiting to be displayed          │
│  Canvas C: 🖼️  Currently displayed                       │
│                                                          │
│  The painter never waits — always has a canvas to use!   │
└─────────────────────────────────────────────────────────┘
```

### Querying Swap Chain Support

Before creating a swap chain, we need to know what the GPU and surface support:

```cpp
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;       // Min/max image count, dimensions
    std::vector<VkSurfaceFormatKHR> formats;     // Pixel format + color space
    std::vector<VkPresentModeKHR> presentModes;  // How images are presented
};

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    // 1. Surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    std::cout << "Swap chain capabilities:\n";
    std::cout << "  Min images: " << details.capabilities.minImageCount << "\n";
    std::cout << "  Max images: " << details.capabilities.maxImageCount
              << (details.capabilities.maxImageCount == 0 ? " (unlimited)" : "") << "\n";
    std::cout << "  Current extent: "
              << details.capabilities.currentExtent.width << "x"
              << details.capabilities.currentExtent.height << "\n";

    // 2. Surface formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                              details.formats.data());
    }

    std::cout << "  Available formats: " << formatCount << "\n";

    // 3. Present modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                                   details.presentModes.data());
    }

    std::cout << "  Available present modes: " << presentModeCount << "\n";

    return details;
}
```

---

### Choosing the Surface Format

The surface format defines the **pixel layout** and **color space**:

```cpp
VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    // Prefer: 8-bit BGRA with sRGB color space
    // This is the most common format for desktop displays
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            std::cout << "  Chosen format: B8G8R8A8_SRGB (ideal)\n";
            return format;
        }
    }

    // If our preferred isn't available, just use the first one
    std::cout << "  Chosen format: first available (fallback)\n";
    return availableFormats[0];
}
```

**Why B8G8R8A8_SRGB?**
- **B8G8R8A8**: 8 bits per channel (Blue, Green, Red, Alpha) = 32 bits per pixel
- **SRGB**: sRGB color space — matches how monitors display colors. Colors look correct without manual gamma correction.

### Choosing the Present Mode

The present mode determines **how and when** images appear on screen:

```cpp
VkPresentModeKHR chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    // Print available modes
    std::cout << "  Available present modes:\n";
    for (const auto& mode : availablePresentModes) {
        switch (mode) {
            case VK_PRESENT_MODE_IMMEDIATE_KHR:
                std::cout << "    - IMMEDIATE (no vsync, may tear)\n";
                break;
            case VK_PRESENT_MODE_FIFO_KHR:
                std::cout << "    - FIFO (vsync, guaranteed available)\n";
                break;
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
                std::cout << "    - FIFO_RELAXED (vsync with late frame handling)\n";
                break;
            case VK_PRESENT_MODE_MAILBOX_KHR:
                std::cout << "    - MAILBOX (triple buffer, low latency)\n";
                break;
            default: break;
        }
    }

    // Prefer MAILBOX (triple buffering) — low latency without tearing
    for (const auto& mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            std::cout << "  Chosen: MAILBOX\n";
            return mode;
        }
    }

    // FIFO is always available
    std::cout << "  Chosen: FIFO (fallback)\n";
    return VK_PRESENT_MODE_FIFO_KHR;
}
```

### Present Modes Explained

```
IMMEDIATE:
  Frame 1 → Screen immediately (may tear if mid-refresh)
  Frame 2 → Screen immediately
  Lowest latency, but visible tearing

FIFO (First In, First Out):
  Frame 1 ──→ Queue ──→ Screen at next refresh
  Frame 2 ──→ Queue ──→ Screen at next refresh
  No tearing, but higher latency. Guaranteed available.

MAILBOX (Triple Buffering):
  Frame 1 ──→ Queue ──→ Screen at next refresh
  Frame 2 ──→ Replaces Frame 1 in queue (if not yet displayed)
  Frame 3 ──→ Replaces Frame 2 in queue
  Low latency, no tearing. Best option when available.

FIFO_RELAXED:
  Like FIFO, but if the app is late (slower than refresh rate),
  the frame is displayed immediately instead of waiting.
```

**Visual comparison at 60Hz monitor refresh:**

```
Monitor:    |──16ms──|──16ms──|──16ms──|──16ms──|
             refresh   refresh   refresh   refresh

FIFO:       F1──wait──F2──wait──F3──wait──F4──wait
             ↑ app must wait for vsync

MAILBOX:    F1─F2─F3──display F3──F4─F5──display F5
             ↑ app renders as fast as possible
               only latest frame is shown
```

### Choosing the Swap Extent (Resolution)

```cpp
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    // If the current extent is already set, use it
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        std::cout << "  Swap extent: " << capabilities.currentExtent.width
                  << "x" << capabilities.currentExtent.height << "\n";
        return capabilities.currentExtent;
    }

    // Otherwise, choose based on window size
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    // Clamp to supported range
    actualExtent.width = std::clamp(actualExtent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    std::cout << "  Swap extent: " << actualExtent.width
              << "x" << actualExtent.height << "\n";
    return actualExtent;
}
```

**Why `glfwGetFramebufferSize` instead of the window size?** On high-DPI displays (Retina), the framebuffer size may be larger than the window size in screen coordinates. The swap chain needs the actual pixel count.

---

### Creating the Swap Chain

```cpp
VkSwapchainKHR swapChain;
std::vector<VkImage> swapChainImages;
VkFormat swapChainImageFormat;
VkExtent2D swapChainExtent;

void createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // Request one more image than minimum (for triple buffering)
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    // Don't exceed the maximum (0 means unlimited)
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    std::cout << "Requesting " << imageCount << " swap chain images.\n";

    // ─── Fill in the create info ───
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;    // Always 1 unless stereoscopic 3D
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // ─── Queue family sharing ───
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    if (indices.graphicsFamily != indices.presentFamily) {
        // Different families — images must be shared
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
        std::cout << "Image sharing: CONCURRENT (different queue families)\n";
    } else {
        // Same family — no sharing needed (faster)
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
        std::cout << "Image sharing: EXCLUSIVE (same queue family)\n";
    }

    // ─── Other settings ───
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;          // Don't render pixels behind other windows
    createInfo.oldSwapchain = VK_NULL_HANDLE;  // Used during swap chain recreation

    // ─── Create! ───
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    // ─── Retrieve the images ───
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    // Save for later use
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    std::cout << "Swap chain created with " << imageCount << " images ("
              << extent.width << "x" << extent.height << ")\n";
}
```

### Key Fields Explained

| Field | Value | Why |
|-------|-------|-----|
| `imageArrayLayers` | `1` | Each image has 1 layer. Use 2 for stereoscopic 3D (VR). |
| `imageUsage` | `COLOR_ATTACHMENT_BIT` | We render directly to these images. |
| `preTransform` | `currentTransform` | No rotation (on mobile, this handles screen orientation). |
| `compositeAlpha` | `OPAQUE_BIT` | Don't blend with other windows. |
| `clipped` | `VK_TRUE` | Don't waste time rendering pixels hidden by other windows. |
| `oldSwapchain` | `VK_NULL_HANDLE` | Used when recreating the swap chain (window resize). |

### Sharing Modes

| Mode | When | Performance |
|------|------|-------------|
| `EXCLUSIVE` | Graphics and present are the same queue family | Faster (no sharing overhead) |
| `CONCURRENT` | Different queue families need access | Slightly slower |

---

## Updated Initialization Order

```cpp
void initVulkan() {
    createInstance();          // 1. Root connection to Vulkan
    setupDebugMessenger();     // 2. Error reporting
    createSurface();           // 3. Window connection
    pickPhysicalDevice();      // 4. Choose GPU (needs surface!)
    createLogicalDevice();     // 5. Open GPU handle
    createSwapChain();         // 6. Presentation images
}
```

---

## Try It Yourself

1. **Create the swap chain** and print how many images were created.

2. **Test different present modes**: Force `VK_PRESENT_MODE_FIFO_KHR` (vsync) and `VK_PRESENT_MODE_IMMEDIATE_KHR` (no vsync). Can you feel a difference later when rendering?

3. **Change the image count**: Request `minImageCount` (double buffer) vs `minImageCount + 1` (triple buffer). What happens?

4. **Print all available formats** with their format and color space. How many does your GPU support?

---

## Key Takeaways

- A **surface** connects Vulkan to a window — GLFW creates it in one call.
- Create the surface **before** picking a physical device (needed for present support check).
- The **swap chain** manages a queue of images for display.
- **Surface format**: Prefer B8G8R8A8_SRGB for correct color display.
- **Present mode**: MAILBOX for low latency, FIFO for guaranteed availability.
- **Image sharing**: EXCLUSIVE (same family) is faster than CONCURRENT (different families).
- The swap chain gives you `VkImage` handles — but you need **image views** to use them.

---

## What We've Built So Far

```
✅ Instance
✅ Validation Layers
✅ Surface
✅ Physical Device
✅ Logical Device + Queues
✅ Swap Chain
⬜ Image Views
⬜ Pipeline
⬜ Drawing!
```

---

[<< Previous: Logical Device](06-logical-device.md) | [Next: Image Views >>](08-image-views.md)
