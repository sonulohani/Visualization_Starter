# Chapter 21: Mipmaps and Multisampling

[<< Previous: Loading Models](20-loading-models.md) | [Next: Push Constants >>](22-push-constants.md)

---

**Progress Tracker:**
```
[████████████████████░░░░] Chapter 21 of 30
Topics: Mipmaps | MSAA | Sample Shading
```

---

## Table of Contents

1. [What Are Mipmaps?](#what-are-mipmaps)
2. [Why Mipmaps Matter](#why-mipmaps-matter)
3. [Calculating Mip Level Count](#calculating-mip-level-count)
4. [Generating Mipmaps](#generating-mipmaps)
5. [Updating Image Creation](#updating-image-creation)
6. [Configuring the Sampler](#configuring-the-sampler)
7. [Format Support Check](#format-support-check)
8. [What Is Multisampling (MSAA)?](#what-is-multisampling-msaa)
9. [Getting Max Sample Count](#getting-max-sample-count)
10. [Creating the MSAA Color Image](#creating-the-msaa-color-image)
11. [Updating the Render Pass](#updating-the-render-pass)
12. [Updating Framebuffers](#updating-framebuffers)
13. [Updating the Pipeline](#updating-the-pipeline)
14. [Sample Shading](#sample-shading)
15. [Try It Yourself](#try-it-yourself)
16. [Key Takeaways](#key-takeaways)

---

## What Are Mipmaps?

Mipmaps are **pre-calculated, progressively downscaled versions** of a texture. The word "mipmap" comes from the Latin *multum in parvo* — "much in little."

Think of it like a set of photographs of the same painting, taken from different distances. Up close, you need the full-resolution photo. From across the room, a smaller version looks just as good — and your eyes don't have to strain to filter out detail you can't perceive anyway.

```
Mip Level Chain (256×256 base texture):

Level 0: 256×256  ████████████████
Level 1: 128×128  ████████
Level 2:  64×64   ████
Level 3:  32×32   ██
Level 4:  16×16   █
Level 5:   8×8    ▪
Level 6:   4×4    ·
Level 7:   2×2    .
Level 8:   1×1    .

Each level is exactly half the width and height of the previous one.
The GPU selects the appropriate level based on screen-space size.
```

When a textured surface is far from the camera, using the full-resolution texture causes **aliasing** — shimmering, moiré patterns, and flickering. The GPU would sample wildly different texels from frame to frame. Mipmaps solve this by providing a pre-filtered smaller version that the GPU can sample smoothly.

---

## Why Mipmaps Matter

| Problem Without Mipmaps | Solution With Mipmaps |
|---|---|
| Aliasing and shimmering at distance | Pre-filtered textures eliminate noise |
| Wasted memory bandwidth reading huge textures for tiny on-screen objects | Smaller mip levels mean fewer bytes fetched |
| Texture cache thrashing | Smaller textures fit better in GPU cache |
| No smooth LOD transitions | Trilinear filtering blends between mip levels |

**Analogy:** Imagine reading a giant billboard from a mile away versus reading a business card. Your eyes don't need all those giant letters — a summary would do. Mipmaps are that summary, pre-computed so the GPU doesn't waste time shrinking textures on the fly.

**Memory cost:** A full mipmap chain adds only ~33% extra memory. For a 256×256 RGBA texture (256 KB at level 0), the total with all mip levels is about 341 KB — a small price for dramatically improved quality and performance.

```
Memory usage per mip level (256×256 RGBA8):

Level 0: 256×256 = 262,144 bytes  ████████████████████████████████
Level 1: 128×128 =  65,536 bytes  ████████
Level 2:  64×64  =  16,384 bytes  ██
Level 3:  32×32  =   4,096 bytes  ▪
Level 4:  16×16  =   1,024 bytes  .
Level 5:   8×8   =     256 bytes  .
Level 6:   4×4   =      64 bytes  .
Level 7:   2×2   =      16 bytes  .
Level 8:   1×1   =       4 bytes  .
                   ─────────────
Total:             349,524 bytes (~1.33× base)
```

---

## Calculating Mip Level Count

The number of mip levels for a 2D image is determined by how many times you can halve the largest dimension before reaching 1:

```cpp
uint32_t mipLevels = static_cast<uint32_t>(
    std::floor(std::log2(std::max(texWidth, texHeight)))
) + 1;
```

The `+1` accounts for the base level (level 0, the original image).

| Texture Size | log2(max dim) | Mip Levels |
|---|---|---|
| 256×256 | 8 | 9 |
| 512×512 | 9 | 10 |
| 1024×768 | 10 | 11 |
| 1920×1080 | ~10.9 → 10 | 11 |
| 4096×4096 | 12 | 13 |

Store this value as a member variable — you'll use it in several places:

```cpp
class VulkanApp {
    // ...
    uint32_t mipLevels;
    VkImage textureImage;
    VkDeviceMemory textureImageMemory;
};
```

Compute it right after loading the texture pixels:

```cpp
int texWidth, texHeight, texChannels;
stbi_uc* pixels = stbi_load("textures/statue.jpg", &texWidth, &texHeight,
                            &texChannels, STBI_rgb_alpha);

mipLevels = static_cast<uint32_t>(
    std::floor(std::log2(std::max(texWidth, texHeight)))
) + 1;
```

---

## Updating Image Creation

When creating the texture image, pass the mip level count instead of `1`:

```cpp
void createTextureImage() {
    // ... load pixels, create staging buffer ...

    createImage(texWidth, texHeight, mipLevels,
                VK_SAMPLE_COUNT_1_BIT,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                textureImage, textureImageMemory);

    // Transition the whole image to TRANSFER_DST
    transitionImageLayout(textureImage,
                          VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          mipLevels);

    // Copy staging buffer to mip level 0
    copyBufferToImage(stagingBuffer, textureImage,
                      static_cast<uint32_t>(texWidth),
                      static_cast<uint32_t>(texHeight));

    // Generate remaining mip levels (this also transitions to SHADER_READ_ONLY)
    generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                    texWidth, texHeight, mipLevels);

    // Cleanup staging buffer...
}
```

Notice `VK_IMAGE_USAGE_TRANSFER_SRC_BIT` — each mip level acts as a blit **source** for the next smaller level. The `createImage` helper needs the `mipLevels` parameter:

```cpp
void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                 VkSampleCountFlagBits numSamples, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties,
                 VkImage& image, VkDeviceMemory& imageMemory) {

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;  // was hardcoded to 1
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = numSamples;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // ... allocate memory ...
}
```

Also update the image view to cover all mip levels:

```cpp
VkImageViewCreateInfo viewInfo{};
viewInfo.subresourceRange.levelCount = mipLevels;  // was 1
```

---

## Generating Mipmaps

This is the heart of mipmap creation. We use `vkCmdBlitImage` in a loop, blitting each level to the next smaller one:

```
Blit chain:

Level 0 (256×256) ──blit──► Level 1 (128×128) ──blit──► Level 2 (64×64)
                                                            │
                                                            blit
                                                            ▼
Level 5 (8×8) ◄──blit── Level 4 (16×16) ◄──blit── Level 3 (32×32)
    │
    blit
    ▼
Level 6 (4×4) ──blit──► Level 7 (2×2) ──blit──► Level 8 (1×1)

Each level is transitioned:
  TRANSFER_DST → TRANSFER_SRC (after being written, before being read)
  TRANSFER_SRC → SHADER_READ_ONLY (after all blits done)
```

Here is the full implementation:

```cpp
void generateMipmaps(VkImage image, VkFormat imageFormat,
                     int32_t texWidth, int32_t texHeight,
                     uint32_t mipLevels) {

    // Check if the format supports linear blitting
    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat,
                                        &formatProperties);

    if (!(formatProperties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        throw std::runtime_error(
            "texture image format does not support linear blitting!");
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        // Transition level i-1 from TRANSFER_DST to TRANSFER_SRC
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        // Define the blit region
        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;

        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {
            mipWidth > 1 ? mipWidth / 2 : 1,
            mipHeight > 1 ? mipHeight / 2 : 1,
            1
        };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &blit,
            VK_FILTER_LINEAR);

        // Transition level i-1 to SHADER_READ_ONLY (done with it)
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        // Halve dimensions for next iteration
        if (mipWidth > 1) mipWidth /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    // Transition the last mip level (never used as blit source)
    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    endSingleTimeCommands(commandBuffer);
}
```

**Why not just `vkCmdCopyImage`?** Because `vkCmdBlitImage` performs **filtering** (scaling) during the copy. It's essentially a GPU-side resize with bilinear interpolation, which produces properly filtered mip levels. `vkCmdCopyImage` only does 1:1 copies with no scaling.

---

## Configuring the Sampler

The texture sampler needs to know about mipmaps. Update the sampler creation:

```cpp
void createTextureSampler() {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    // Mipmap settings
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;  // trilinear
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels);

    if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler)
        != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}
```

**Filtering modes explained:**

| Setting | Value | Effect |
|---|---|---|
| `magFilter` | `LINEAR` | Smooth when texture is magnified (close up) |
| `minFilter` | `LINEAR` | Smooth when texture is minified (far away) |
| `mipmapMode` | `LINEAR` | Blend between two nearest mip levels (trilinear) |
| `minLod` | `0.0f` | Allow sampling from the highest-res mip |
| `maxLod` | `mipLevels` | Allow sampling down to the smallest mip |
| `mipLodBias` | `0.0f` | No bias (positive = blurrier, negative = sharper) |

```
Filtering hierarchy:

Bilinear:   Sample 4 texels in ONE mip level, interpolate
Trilinear:  Bilinear in TWO adjacent mip levels, blend results
Anisotropic: Multiple samples along the axis of greatest compression

Quality:  Nearest < Bilinear < Trilinear < Anisotropic
Cost:     Cheapest                          Most expensive
```

---

## Format Support Check

Not all GPUs support `vkCmdBlitImage` with linear filtering for all formats. You **must** check:

```cpp
VkFormatProperties formatProperties;
vkGetPhysicalDeviceFormatProperties(physicalDevice,
                                    VK_FORMAT_R8G8B8A8_SRGB,
                                    &formatProperties);

if (!(formatProperties.optimalTilingFeatures &
      VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    // Fall back to software mipmap generation or use a format that supports it
    throw std::runtime_error("format does not support linear blit!");
}
```

In practice, `VK_FORMAT_R8G8B8A8_SRGB` is universally supported for linear blitting on all desktop GPUs. But if you use exotic formats (like compressed BC/ASTC formats), you'll need to generate mipmaps on the CPU or use a compute shader.

**Alternatives when linear blit is unsupported:**
1. Generate mipmaps on the CPU before uploading
2. Use a compute shader to downsample
3. Use a different image format that supports blitting

---

## What Is Multisampling (MSAA)?

**Multisample Anti-Aliasing (MSAA)** renders each pixel with multiple samples to smooth jagged edges. Instead of determining a pixel's color from a single point, the GPU evaluates coverage at several sub-pixel locations.

**Analogy:** Imagine you're painting a checkerboard pattern onto a grid of large tiles. If each tile just checks its center point, you get harsh stair-step edges. But if each tile checks 4 points spread across its area and averages the result, the edges become much smoother.

```
Without MSAA (jagged edges):          With 4× MSAA (smooth edges):

    ████                                  ▓████
    ████                                 ▓█████
   ████                                 ▓█████
   ████                                ▓█████
  ████                                ▓█████
  ████                               ▓█████
 ████                               ▓█████
 ████                              ▓█████

Legend: █ = fully covered pixel
        ▓ = partially covered pixel (blended)
```

```
How MSAA works for a single pixel on a triangle edge:

  ┌─────────────┐
  │ ×     ×     │   × = sample point
  │   ╲         │   ╲ = triangle edge
  │     ╲       │
  │ ×     ╲  ×  │   2 of 4 samples inside triangle
  └─────────────┘   → pixel gets 50% triangle color

  Without MSAA: center point is inside → 100% triangle color
  With 4× MSAA: 2/4 samples inside → 50% blend → smooth edge
```

**MSAA does NOT:**
- Anti-alias texture interiors (use mipmaps for that)
- Anti-alias shader-computed patterns (use sample shading for that)
- Cost as much as supersampling (SSAA), because it only runs the fragment shader once per pixel

---

## Getting Max Sample Count

Different GPUs support different maximum sample counts. Query the physical device limits:

```cpp
VkSampleCountFlagBits getMaxUsableSampleCount() {
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts =
        physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) return VK_SAMPLE_COUNT_64_BIT;
    if (counts & VK_SAMPLE_COUNT_32_BIT) return VK_SAMPLE_COUNT_32_BIT;
    if (counts & VK_SAMPLE_COUNT_16_BIT) return VK_SAMPLE_COUNT_16_BIT;
    if (counts & VK_SAMPLE_COUNT_8_BIT)  return VK_SAMPLE_COUNT_8_BIT;
    if (counts & VK_SAMPLE_COUNT_4_BIT)  return VK_SAMPLE_COUNT_4_BIT;
    if (counts & VK_SAMPLE_COUNT_2_BIT)  return VK_SAMPLE_COUNT_2_BIT;

    return VK_SAMPLE_COUNT_1_BIT;
}
```

We take the **intersection** of color and depth sample counts because we use both attachments. Store the result:

```cpp
VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

void pickPhysicalDevice() {
    // ... pick device ...
    msaaSamples = getMaxUsableSampleCount();
    std::cout << "Using " << msaaSamples << "x MSAA\n";
}
```

| Common Sample Counts | Notes |
|---|---|
| 1× | No MSAA (default) |
| 2× | Minimal smoothing |
| 4× | Good balance of quality and performance |
| 8× | High quality, noticeable cost |
| 16×+ | Rarely needed, diminishing returns |

---

## Creating the MSAA Color Image

MSAA requires a **multisampled color image** that acts as the rendering target. The final single-sampled result is resolved to the swapchain image.

```
Rendering flow with MSAA:

┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│ MSAA Color   │     │ Depth        │     │ Swapchain    │
│ (4× samples) │────►│ (4× samples) │     │ (1× sample)  │
│ Render here  │     │ Depth test   │     │ Present this │
└──────┬───────┘     └──────────────┘     └──────▲───────┘
       │                                         │
       └────── resolve (average samples) ────────┘
```

```cpp
VkImage colorImage;
VkDeviceMemory colorImageMemory;
VkImageView colorImageView;

void createColorResources() {
    VkFormat colorFormat = swapChainImageFormat;

    createImage(swapChainExtent.width, swapChainExtent.height,
                1,                          // no mipmaps for MSAA target
                msaaSamples,                // multi-sampled!
                colorFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                colorImage, colorImageMemory);

    colorImageView = createImageView(colorImage, colorFormat,
                                     VK_IMAGE_ASPECT_COLOR_BIT, 1);
}
```

**Why `VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT`?** This tells the driver that the image contents are temporary — they exist only during rendering and are discarded after the resolve. On tile-based GPUs (mobile), this allows the driver to keep the data entirely in on-chip tile memory, never writing it to main memory. Huge performance win.

Also update the depth image to be multisampled:

```cpp
void createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    createImage(swapChainExtent.width, swapChainExtent.height,
                1, msaaSamples,  // multi-sampled depth!
                depthFormat, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depthImage, depthImageMemory);

    depthImageView = createImageView(depthImage, depthFormat,
                                     VK_IMAGE_ASPECT_DEPTH_BIT, 1);
}
```

---

## Updating the Render Pass

The render pass now has **three** attachments:

1. **MSAA color** — multisampled, where rendering happens
2. **Depth** — multisampled, for depth testing
3. **Resolve** — single-sampled swapchain image, the final output

```cpp
void createRenderPass() {
    // Attachment 0: MSAA color
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = msaaSamples;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // resolved, not stored
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Attachment 1: Depth
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = msaaSamples;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Attachment 2: Resolve target (swapchain image)
    VkAttachmentDescription colorAttachmentResolve{};
    colorAttachmentResolve.format = swapChainImageFormat;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentResolveRef{};
    colorAttachmentResolveRef.attachment = 2;
    colorAttachmentResolveRef.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;
    subpass.pResolveAttachments = &colorAttachmentResolveRef;

    // ... subpass dependency and render pass creation ...

    std::array<VkAttachmentDescription, 3> attachments = {
        colorAttachment, depthAttachment, colorAttachmentResolve
    };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount =
        static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    // ... dependencies ...

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass)
        != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}
```

The key insight: `pResolveAttachments` tells Vulkan to automatically resolve the multisampled color attachment into the single-sampled swapchain image at the end of the subpass.

---

## Updating Framebuffers

Each framebuffer now needs three image views in the correct order:

```cpp
void createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 3> attachments = {
            colorImageView,          // 0: MSAA color (shared)
            depthImageView,          // 1: Depth (shared)
            swapChainImageViews[i]   // 2: Resolve target (per-swapchain)
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType =
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount =
            static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                                &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}
```

```
Framebuffer attachment layout:

Framebuffer[0]           Framebuffer[1]           Framebuffer[2]
┌──────────────┐         ┌──────────────┐         ┌──────────────┐
│ [0] MSAA ────┼─shared──┼─[0] MSAA ────┼─shared──┼─[0] MSAA     │
│ [1] Depth ───┼─shared──┼─[1] Depth ───┼─shared──┼─[1] Depth    │
│ [2] Swap 0   │         │ [2] Swap 1   │         │ [2] Swap 2   │
└──────────────┘         └──────────────┘         └──────────────┘

MSAA and depth images are shared (only one of each needed).
Each framebuffer resolves to its own swapchain image.
```

Also update `vkCmdBeginRenderPass` to clear all three attachments:

```cpp
std::array<VkClearValue, 3> clearValues{};      // was 2
clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
clearValues[1].depthStencil = {1.0f, 0};
clearValues[2].color = {{0.0f, 0.0f, 0.0f, 1.0f}}; // resolve target

renderPassInfo.clearValueCount =
    static_cast<uint32_t>(clearValues.size());
renderPassInfo.pClearValues = clearValues.data();
```

---

## Updating the Pipeline

Set the rasterization sample count in the pipeline multisampling state:

```cpp
VkPipelineMultisampleStateCreateInfo multisampling{};
multisampling.sType =
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
multisampling.sampleShadingEnable = VK_FALSE;
multisampling.rasterizationSamples = msaaSamples;  // was VK_SAMPLE_COUNT_1_BIT
multisampling.minSampleShading = 1.0f;
multisampling.pSampleMask = nullptr;
multisampling.alphaToCoverageEnable = VK_FALSE;
multisampling.alphaToOneEnable = VK_FALSE;
```

That's it for basic MSAA! The pipeline now renders with multiple samples per pixel.

---

## Sample Shading

Standard MSAA only smooths **geometry edges**. If you have a texture with high-frequency detail or a fragment shader that produces sharp patterns, MSAA won't help with aliasing *inside* triangles.

**Sample shading** runs the fragment shader at **every sample point** instead of once per pixel, then averages the results. This catches all types of aliasing but costs proportionally more.

```cpp
VkPipelineMultisampleStateCreateInfo multisampling{};
multisampling.sType =
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
multisampling.sampleShadingEnable = VK_TRUE;      // enable!
multisampling.rasterizationSamples = msaaSamples;
multisampling.minSampleShading = 0.2f;             // 20% of samples
multisampling.pSampleMask = nullptr;
multisampling.alphaToCoverageEnable = VK_FALSE;
multisampling.alphaToOneEnable = VK_FALSE;
```

Also enable the device feature during logical device creation:

```cpp
VkPhysicalDeviceFeatures deviceFeatures{};
deviceFeatures.samplerAnisotropy = VK_TRUE;
deviceFeatures.sampleRateShading = VK_TRUE;  // required for sample shading
```

`minSampleShading` controls the fraction of samples that get unique shader evaluations:

| `minSampleShading` | Effect with 4× MSAA | Cost |
|---|---|---|
| 0.0 or disabled | 1 shader invocation per pixel | Cheapest |
| 0.25 | ~1 invocation per pixel | Minimal overhead |
| 0.5 | ~2 invocations per pixel | Moderate |
| 1.0 | 4 invocations per pixel (= SSAA) | Most expensive |

**Recommendation:** Use `0.2` to `0.5` for a good balance. Full sample shading (`1.0`) is essentially supersampling (SSAA) and very expensive.

---

## Try It Yourself

1. **Mip Level Visualization:** Modify the fragment shader to tint each mip level a different color (use `textureLod` with explicit levels). Verify that closer surfaces use level 0 and distant surfaces use higher levels.

2. **LOD Bias Experiment:** Change `mipLodBias` in the sampler to positive values (e.g., `2.0`) and negative values (e.g., `-1.0`). Observe how positive bias makes textures blurrier and negative bias makes them sharper (but aliased).

3. **MSAA Comparison:** Create a toggle that switches between `VK_SAMPLE_COUNT_1_BIT`, `VK_SAMPLE_COUNT_2_BIT`, `VK_SAMPLE_COUNT_4_BIT`, and `VK_SAMPLE_COUNT_8_BIT`. Screenshot each and zoom in on triangle edges to compare quality.

4. **Sample Shading Test:** Render a procedural checkerboard pattern in the fragment shader (using `sin` or `step` on texture coordinates). Compare the result with MSAA only versus MSAA + sample shading. The interior aliasing should be dramatically reduced with sample shading.

5. **Performance Profiling:** Use `VK_EXT_debug_utils` timestamps or a GPU profiler to measure the render time difference between no MSAA, 4× MSAA, 4× MSAA + sample shading, and 8× MSAA. Create a table of your results.

---

## Key Takeaways

- **Mipmaps** are pre-computed downscaled texture versions that eliminate distance aliasing and improve cache performance at a ~33% memory cost.
- Calculate mip count with `floor(log2(max(width, height))) + 1`.
- **Generate mipmaps** by blitting each level to the next smaller one in a loop using `vkCmdBlitImage` with `VK_FILTER_LINEAR`.
- Always **check format support** for linear blitting with `vkGetPhysicalDeviceFormatProperties`.
- Configure the sampler with `mipmapMode = LINEAR`, `minLod = 0`, `maxLod = mipLevels` for trilinear filtering.
- **MSAA** smooths geometry edges by evaluating pixel coverage at multiple sub-pixel sample points.
- Query `framebufferColorSampleCounts & framebufferDepthSampleCounts` for the maximum supported sample count.
- MSAA requires a **multisampled color image** and a **resolve attachment** (the swapchain image).
- Use `VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT` for the MSAA color image — the GPU may keep it entirely in tile memory.
- **Sample shading** catches interior aliasing that MSAA misses, at higher cost.
- Mipmaps + MSAA together give you comprehensive anti-aliasing: mipmaps for texture aliasing, MSAA for geometry edges.

---

[<< Previous: Loading Models](20-loading-models.md) | [Next: Push Constants >>](22-push-constants.md)
