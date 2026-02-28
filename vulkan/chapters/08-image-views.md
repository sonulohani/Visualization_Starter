# Chapter 8: Image Views

[<< Previous: Surface & Swap Chain](07-surface-swapchain.md) | [Next: Graphics Pipeline & Shaders >>](09-pipeline-shaders.md)

---

## Images vs Image Views

### What's the Difference?

A `VkImage` is **raw image data** — just bytes in GPU memory. It doesn't specify *how* to interpret those bytes.

A `VkImageView` is a **lens** that tells Vulkan how to read an image:

```
VkImage (raw data):
  ┌─────────────────────────────────────────────┐
  │ Raw bytes: 0xFF00FF00 0x00FF0000 0xABCD1234 │
  │ Could be: 2D texture? 3D texture? Cube map? │
  │ How many mip levels? Which layer?            │
  └─────────────────────────────────────────────┘

VkImageView (interpretation):
  "Treat this as a 2D image, read all mip levels,
   use the first array layer, format is B8G8R8A8_SRGB,
   don't swizzle any channels."
```

### Analogy

Think of it like a **photograph**:
- The `VkImage` is the negative film — raw data, not directly viewable.
- The `VkImageView` is the printed photograph — processed for a specific use.

You can create multiple views of the same image (e.g., view just the red channel, or just mip level 3).

---

## Why Do We Need Image Views?

Every place you use an image in Vulkan requires an image view:

| Use Case | Needs Image View? |
|----------|------------------|
| Rendering to a swap chain image | Yes (in framebuffer) |
| Sampling a texture in a shader | Yes (in descriptor set) |
| Depth/stencil testing | Yes (in framebuffer) |
| Copying between images | No (uses VkImage directly) |

---

## Creating Image Views for the Swap Chain

We need one image view per swap chain image:

```cpp
std::vector<VkImageView> swapChainImageViews;

void createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];

        // ─── View type: 2D image ───
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        // ─── Format: same as the swap chain ───
        createInfo.format = swapChainImageFormat;

        // ─── Component swizzling: identity (no remapping) ───
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // ─── Subresource range: which part of the image to view ───
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;    // Start at mip 0
        createInfo.subresourceRange.levelCount = 1;      // Just 1 mip level
        createInfo.subresourceRange.baseArrayLayer = 0;  // First layer
        createInfo.subresourceRange.layerCount = 1;      // Just 1 layer

        if (vkCreateImageView(device, &createInfo, nullptr,
                              &swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image view!");
        }
    }

    std::cout << "Created " << swapChainImageViews.size() << " image views.\n";
}
```

---

## Understanding Each Field

### View Type

| Type | Dimensions | Use Case |
|------|-----------|----------|
| `VK_IMAGE_VIEW_TYPE_1D` | Width only | Gradient lookup tables |
| `VK_IMAGE_VIEW_TYPE_2D` | Width + Height | Standard textures, render targets |
| `VK_IMAGE_VIEW_TYPE_3D` | Width + Height + Depth | Volume textures (fog, clouds) |
| `VK_IMAGE_VIEW_TYPE_CUBE` | 6 faces | Skyboxes, environment maps |
| `VK_IMAGE_VIEW_TYPE_1D_ARRAY` | 1D × N layers | Array of gradients |
| `VK_IMAGE_VIEW_TYPE_2D_ARRAY` | 2D × N layers | Texture arrays |
| `VK_IMAGE_VIEW_TYPE_CUBE_ARRAY` | Cube × N | Multiple skyboxes |

### Component Swizzling

Swizzling lets you remap color channels. `IDENTITY` means "use as-is":

```
Original pixel: R=0.8  G=0.2  B=0.5  A=1.0

IDENTITY:       R=0.8  G=0.2  B=0.5  A=1.0  (no change)

If we set components.r = VK_COMPONENT_SWIZZLE_B:
Swizzled:       R=0.5  G=0.2  B=0.5  A=1.0  (R now reads B)

If we set all to VK_COMPONENT_SWIZZLE_R:
Swizzled:       R=0.8  G=0.8  B=0.8  A=0.8  (grayscale from red channel)
```

Common uses:
- Convert single-channel textures (grayscale) to a visible format.
- Swap BGR ↔ RGB for different format conventions.

### Subresource Range

This defines **which subset** of the image to view:

```
Full Image:
┌─────────────────────────────┐
│ Mip 0 (full resolution)     │
│ ┌───────────────────────┐   │
│ │ Layer 0 │ Layer 1     │   │   ← Array layers (for texture arrays)
│ └───────────────────────┘   │
│ Mip 1 (half resolution)     │
│ ┌──────────────┐            │
│ │ Layer 0 │ L1 │            │
│ └──────────────┘            │
│ Mip 2 (quarter resolution)  │
│ ┌───────┐                   │
│ │ L0 │L1│                   │
│ └───────┘                   │
└─────────────────────────────┘

Our view: baseMipLevel=0, levelCount=1, baseArrayLayer=0, layerCount=1
→ Just the top-left rectangle (Mip 0, Layer 0)
```

### Aspect Mask

| Aspect | When to Use |
|--------|------------|
| `VK_IMAGE_ASPECT_COLOR_BIT` | Color images (textures, render targets) |
| `VK_IMAGE_ASPECT_DEPTH_BIT` | Depth buffer images |
| `VK_IMAGE_ASPECT_STENCIL_BIT` | Stencil buffer images |

---

## A Reusable createImageView Helper

Since we'll create many image views later (for textures, depth buffers, etc.), let's make a helper:

```cpp
VkImageView createImageView(VkImage image, VkFormat format,
                             VkImageAspectFlags aspectFlags,
                             uint32_t mipLevels = 1)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;

    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view!");
    }

    return imageView;
}

// Usage examples:
VkImageView colorView = createImageView(colorImage, VK_FORMAT_B8G8R8A8_SRGB,
                                         VK_IMAGE_ASPECT_COLOR_BIT);

VkImageView depthView = createImageView(depthImage, VK_FORMAT_D32_SFLOAT,
                                         VK_IMAGE_ASPECT_DEPTH_BIT);

VkImageView textureView = createImageView(texture, VK_FORMAT_R8G8B8A8_SRGB,
                                           VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
```

---

## Cleanup

Image views must be destroyed before the swap chain (because they reference swap chain images):

```cpp
void cleanup() {
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
    // ... rest of cleanup ...
}
```

**Note**: We don't destroy `swapChainImages` — they're owned by the swap chain and destroyed automatically when the swap chain is destroyed.

---

## Updated Initialization

```cpp
void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();        // NEW
}
```

---

## Try It Yourself

1. **Create the image views** and verify the count matches the swap chain image count.

2. **Experiment with swizzling**: Change the red component to `VK_COMPONENT_SWIZZLE_ZERO`. What would this do when rendering? (The red channel would always be 0.)

3. **Think ahead**: Why can't we render yet? What objects are we still missing? (Pipeline, render pass, framebuffers, command buffers, synchronization.)

---

## Key Takeaways

- `VkImage` is raw data; `VkImageView` is how you interpret it.
- Every use of an image in the pipeline requires an image view.
- **View type** determines dimensionality (2D, 3D, cube map).
- **Component swizzling** remaps color channels.
- **Subresource range** selects mip levels and array layers.
- **Aspect mask** distinguishes color, depth, and stencil data.
- Create a helper function — you'll create many image views throughout the tutorial.

---

## What We've Built So Far

```
✅ Instance
✅ Validation Layers
✅ Surface
✅ Physical Device
✅ Logical Device + Queues
✅ Swap Chain
✅ Image Views
⬜ Graphics Pipeline ← Next!
⬜ Render Pass
⬜ Framebuffers
⬜ Command Buffers
⬜ Drawing!
```

---

[<< Previous: Surface & Swap Chain](07-surface-swapchain.md) | [Next: Graphics Pipeline & Shaders >>](09-pipeline-shaders.md)
