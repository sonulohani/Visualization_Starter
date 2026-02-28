# Chapter 18: Texture Mapping and Samplers

[<< Previous: Uniforms](17-uniforms-descriptors.md) | [Next: Depth Buffering >>](19-depth-buffering.md)

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
✅ Draw Loop          → Working rectangle with frames in flight!
✅ Vertex Buffer      → Vertex data on GPU
✅ Staging Buffers    → Optimized DEVICE_LOCAL memory
✅ Index Buffer       → Vertex reuse
✅ Uniform Buffers    → MVP matrices, spinning rectangle!
✅ Descriptor Sets    → Shader resource binding

🎯 Now: Wrap images onto geometry with texture mapping!
```

---

## What is Texture Mapping?

Right now our rectangle has colors that blend across vertices (per-vertex coloring). But real 3D objects need detailed surfaces — wood grain, brick walls, character faces. Painting every pixel with vertex colors is impossible for complex surfaces.

**Texture mapping** wraps a 2D image around 3D geometry, like wallpaper on a wall.

```
Without textures:                  With textures:

  ┌───────────────┐               ┌───────────────┐
  │               │               │ ╔═══╗ ╔═══╗   │
  │  solid color  │               │ ║   ║ ║   ║   │
  │  or gradient  │      ──→      │ ╠═══╬═╬═══╣   │
  │               │               │ ║   ║ ║   ║   │
  │               │               │ ╚═══╝ ╚═══╝   │
  └───────────────┘               └───────────────┘
     boring!                       brick texture!
```

### The Analogy: Gift Wrapping

Texture mapping is like wrapping a present:
- The **gift** (3D geometry) has a shape.
- The **wrapping paper** (texture image) has a pattern.
- You need **instructions** (texture coordinates) for how to place the paper on the gift.

### Texture Coordinates (UV Coordinates)

Every vertex gets a **texture coordinate** (also called **UV coordinate**) that specifies where on the texture image this vertex maps to. The texture image is addressed from (0,0) to (1,1):

```
Texture image (1024×1024 pixels):

  (0,0)──────────────────(1,0)        UV coordinate system:
    │                      │
    │    🧱🧱🧱🧱🧱     │          U = horizontal (0 to 1)
    │    🧱🧱🧱🧱🧱     │          V = vertical (0 to 1)
    │    🧱🧱🧱🧱🧱     │
    │    🧱🧱🧱🧱🧱     │          (0,0) = top-left
    │                      │          (1,1) = bottom-right
  (0,1)──────────────────(1,1)

Mapping onto rectangle vertices:

  vertex 0 (-0.5,-0.5)  uv=(0,0)  ←── top-left of texture
  vertex 1 ( 0.5,-0.5)  uv=(1,0)  ←── top-right of texture
  vertex 2 ( 0.5, 0.5)  uv=(1,1)  ←── bottom-right of texture
  vertex 3 (-0.5, 0.5)  uv=(0,1)  ←── bottom-left of texture

The GPU interpolates UV between vertices, sampling the texture for every pixel.
```

---

## Step 1: Loading an Image with stb_image

[stb_image](https://github.com/nothings/stb) is a single-header C library that can load PNG, JPG, BMP, and other formats:

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
```

Place this `#define` in exactly **one** `.cpp` file. It tells stb_image to include the implementation (not just declarations).

### Loading the Texture File

```cpp
void createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("textures/texture.jpg",
                                 &texWidth, &texHeight,
                                 &texChannels, STBI_rgb_alpha);

    VkDeviceSize imageSize = texWidth * texHeight * 4;  // 4 bytes per pixel (RGBA)

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }
```

| Parameter | Purpose |
|-----------|---------|
| `"textures/texture.jpg"` | Path to the image file |
| `&texWidth` | Filled with image width in pixels |
| `&texHeight` | Filled with image height in pixels |
| `&texChannels` | Filled with number of channels (3=RGB, 4=RGBA) |
| `STBI_rgb_alpha` | Force 4 channels (add alpha if missing) |

---

## Step 2: Staging Buffer → VkImage Pipeline

We can't just hand pixel data directly to a `VkImage`. The process mirrors what we did for vertex buffers with staging:

```
Image loading pipeline:

  Disk                CPU Memory            GPU Memory
  ────                ──────────            ──────────

  texture.jpg ──stb──→ pixels[]  ──memcpy──→ Staging Buffer
                        (RAM)                 (HOST_VISIBLE)
                                                  │
                                           vkCmdCopyBufferToImage
                                                  │
                                                  ↓
                                              VkImage
                                           (DEVICE_LOCAL)
                                           optimal layout
                                           GPU-fast access!
```

### Creating the Staging Buffer

```cpp
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    stbi_image_free(pixels);  // CPU copy no longer needed
```

---

## Step 3: Creating a VkImage

Unlike buffers (1D blobs of data), images are **2D/3D structures** with specific formats, tiling, and layouts that the GPU understands natively.

```cpp
VkImage textureImage;
VkDeviceMemory textureImageMemory;

void createImage(uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties,
                 VkImage& image, VkDeviceMemory& imageMemory) {

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
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                                properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}
```

### Key Image Fields

| Field | Value | Why |
|-------|-------|-----|
| `imageType` | `2D` | It's a flat texture (not 3D/volume) |
| `format` | `R8G8B8A8_SRGB` | 4 channels, 8 bits each, sRGB color space |
| `tiling` | `OPTIMAL` | GPU arranges pixels for fastest access |
| `initialLayout` | `UNDEFINED` | We don't care about existing contents |
| `usage` | `TRANSFER_DST \| SAMPLED` | Destination for copy AND readable by shaders |
| `samples` | `1_BIT` | No multisampling (single sample per pixel) |

### Tiling: LINEAR vs OPTIMAL

```
LINEAR tiling (row by row, like a bitmap):

  Row 0: [R G B A] [R G B A] [R G B A] [R G B A] ...
  Row 1: [R G B A] [R G B A] [R G B A] [R G B A] ...
  Row 2: [R G B A] [R G B A] [R G B A] [R G B A] ...

  ✅ CPU can read/write directly (predictable layout)
  ❌ Slow for GPU (poor cache locality for 2D access)

OPTIMAL tiling (GPU-optimized, swizzled/tiled):

  ┌────┬────┐
  │tile│tile│  The GPU rearranges pixels into small tiles
  │ 0  │ 1  │  (e.g., 4×4 or 8×8 blocks) for better
  ├────┼────┤  cache performance when sampling textures.
  │tile│tile│
  │ 2  │ 3  │  ✅ Fast for GPU (great cache locality)
  └────┴────┘  ❌ CPU can't directly read/write
```

We use `OPTIMAL` tiling for textures because the GPU samples them millions of times per frame.

### Using the Helper

Back in `createTextureImage`:

```cpp
    createImage(texWidth, texHeight,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                textureImage, textureImageMemory);
```

---

## Step 4: Image Layout Transitions

Images in Vulkan have a **layout** that describes how the pixel data is organized in memory. Different operations require different layouts. Before we can copy data into our image, we need to transition it from `UNDEFINED` to `TRANSFER_DST_OPTIMAL`.

```
Image layout transitions:

  UNDEFINED  ───transition──→  TRANSFER_DST_OPTIMAL  ───copy data──→  transition  ──→  SHADER_READ_ONLY_OPTIMAL
  (initial,                    (ready to receive                       (after copy)      (ready for shader
   don't care                   copied data)                                              to sample)
   about contents)
```

### Why Layouts?

The GPU can access the same image data in different ways. By declaring the layout, you tell the driver: "I'm done writing to this image; the next thing that will happen is shader reads." The driver can then optimize memory barriers, caches, and compression.

Think of it like a library book:
- **UNDEFINED**: Book is in a return bin (could be anywhere, no one's using it)
- **TRANSFER_DST**: Book is at the printer, receiving new pages
- **SHADER_READ_ONLY**: Book is on the shelf, available for reading

### The transitionImageLayout Helper

```cpp
void transitionImageLayout(VkImage image, VkFormat format,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}
```

### Pipeline Barrier Breakdown

```
Pipeline barrier for UNDEFINED → TRANSFER_DST:

  "Nothing needs to finish before this" (TOP_OF_PIPE, no src access)
  "Transfer writes must wait for this barrier" (TRANSFER stage, WRITE access)

  ┌─────────────┐          barrier          ┌───────────────────┐
  │ (nothing)   │ ─────────────────────────→│ Transfer write    │
  │ TOP_OF_PIPE │                            │ (copy to image)   │
  └─────────────┘                            └───────────────────┘

Pipeline barrier for TRANSFER_DST → SHADER_READ_ONLY:

  "Transfer writes must finish first" (TRANSFER stage, WRITE access)
  "Fragment shader reads must wait" (FRAGMENT stage, READ access)

  ┌─────────────────┐      barrier      ┌────────────────────────┐
  │ Transfer write   │ ────────────────→│ Fragment shader read   │
  │ (copy finished)  │                   │ (texture sampling)     │
  └─────────────────┘                   └────────────────────────┘
```

### Single-Time Command Helpers

These helpers submit a one-shot command buffer, used for operations like layout transitions and buffer copies:

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

---

## Step 5: Copying Buffer to Image

```cpp
void copyBufferToImage(VkBuffer buffer, VkImage image,
                       uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;    // 0 = tightly packed
    region.bufferImageHeight = 0;  // 0 = tightly packed
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &region);

    endSingleTimeCommands(commandBuffer);
}
```

### Completing createTextureImage

Putting it all together:

```cpp
void createTextureImage() {
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load("textures/texture.jpg",
                                 &texWidth, &texHeight,
                                 &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    // 1. Create staging buffer and copy pixels into it
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);
    stbi_image_free(pixels);

    // 2. Create the VkImage
    createImage(texWidth, texHeight,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                textureImage, textureImageMemory);

    // 3. Transition UNDEFINED → TRANSFER_DST
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // 4. Copy staging buffer → image
    copyBufferToImage(stagingBuffer, textureImage,
                      static_cast<uint32_t>(texWidth),
                      static_cast<uint32_t>(texHeight));

    // 5. Transition TRANSFER_DST → SHADER_READ_ONLY
    transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // 6. Clean up staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}
```

---

## Step 6: Texture Image View

Just like swap chain images need image views, our texture image needs one too. The image view tells Vulkan how to interpret the image data:

```cpp
VkImageView textureImageView;

void createTextureImageView() {
    textureImageView = createImageView(textureImage,
                                        VK_FORMAT_R8G8B8A8_SRGB);
}
```

If you haven't already, refactor image view creation into a reusable helper:

```cpp
VkImageView createImageView(VkImage image, VkFormat format) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }

    return imageView;
}
```

---

## Step 7: Texture Sampler

The **sampler** is a separate Vulkan object that defines **how** the texture is read — filtering, wrapping, and other sampling parameters. It's decoupled from the image itself, so you can reuse one sampler with many textures.

### Filtering Modes

When the GPU samples a texture, the UV coordinate rarely maps to an exact pixel. Filtering determines how to handle this.

```
Texture pixels (texels):        Fragment maps to position
                                 between texels:
  ┌────┬────┬────┬────┐
  │ A  │ B  │ C  │ D  │           ×  ← sample point
  ├────┼────┼────┼────┤
  │ E  │ F  │ G  │ H  │
  ├────┼────┼────┼────┤         NEAREST: picks closest texel (F)
  │ I  │ J  │ K  │ L  │         Result: sharp, pixelated
  ├────┼────┼────┼────┤
  │ M  │ N  │ O  │ P  │         LINEAR: blends 4 nearest texels (B,C,F,G)
  └────┴────┴────┴────┘         Result: smooth, blurry

NEAREST (VK_FILTER_NEAREST):          LINEAR (VK_FILTER_LINEAR):
  Sharp, blocky, retro look            Smooth, blended, realistic
  ┌──┬──┬──┬──┐                       ┌──────────────┐
  │██│░░│██│░░│                       │ smooth blend  │
  │██│░░│██│░░│  ← visible            │  of colors    │  ← no visible
  ├──┼──┼──┼──┤    pixel grid         │              │    pixel grid
  │░░│██│░░│██│                       │              │
  └──┴──┴──┴──┘                       └──────────────┘
```

### Address Modes (Wrapping)

What happens when UV coordinates go outside the [0, 1] range?

```
REPEAT:                          MIRRORED_REPEAT:
  The texture tiles infinitely     Tiles, but every other copy is flipped

  ┌────┬────┬────┐               ┌────┬────┬────┐
  │ AB │ AB │ AB │               │ AB │ BA │ AB │
  │ CD │ CD │ CD │               │ CD │ DC │ CD │
  ├────┼────┼────┤               ├────┼────┼────┤
  │ AB │ AB │ AB │               │ CD │ DC │ CD │
  │ CD │ CD │ CD │               │ AB │ BA │ AB │
  └────┴────┴────┘               └────┴────┴────┘

CLAMP_TO_EDGE:                   CLAMP_TO_BORDER:
  Edge pixels stretch forever      A solid border color is used

  ┌────┬────┬────┐               ┌────┬────┬────┐
  │ AB │ BB │ BB │               │ AB │ ■■ │ ■■ │
  │ CD │ DD │ DD │               │ CD │ ■■ │ ■■ │
  ├────┼────┼────┤               ├────┼────┼────┤
  │ CD │ DD │ DD │               │ ■■ │ ■■ │ ■■ │
  │ CD │ DD │ DD │               │ ■■ │ ■■ │ ■■ │
  └────┴────┴────┘               └────┴────┴────┘
```

### Anisotropic Filtering

When a texture is viewed at a steep angle (like a floor stretching into the distance), standard filtering produces blurriness. **Anisotropic filtering** samples along the stretched direction more intelligently.

```
Without anisotropic:              With anisotropic:

  ┌──────────────────┐           ┌──────────────────┐
  │ clear            │           │ clear             │
  │                  │           │                   │
  │  getting blurry  │           │  still clear      │
  │                  │           │                   │
  │   very blurry    │           │   still clear!    │
  │                  │           │                   │
  │    mush          │           │    slightly soft   │
  └──────────────────┘           └──────────────────┘
   (floor at an angle)            (same floor, sharper)
```

### Creating the Sampler

```cpp
VkSampler textureSampler;

void createTextureSampler() {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(device, &samplerInfo, nullptr,
                         &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}
```

### Sampler Fields Explained

| Field | Value | Meaning |
|-------|-------|---------|
| `magFilter` | `LINEAR` | When texels are larger than pixels (zoomed in), blend |
| `minFilter` | `LINEAR` | When texels are smaller than pixels (zoomed out), blend |
| `addressModeU/V/W` | `REPEAT` | Texture tiles when UVs exceed [0,1] |
| `anisotropyEnable` | `TRUE` | Enable anisotropic filtering |
| `maxAnisotropy` | Device max | Use best quality the GPU supports (typically 16) |
| `borderColor` | `OPAQUE_BLACK` | Only matters with CLAMP_TO_BORDER |
| `unnormalizedCoordinates` | `FALSE` | UVs range [0,1] (not [0,width]) |

---

## Step 8: Combined Image Sampler Descriptor

Now we need to make the texture accessible to shaders through the descriptor system. We add a second binding to our descriptor set layout:

### Update Descriptor Set Layout

```cpp
void createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType =
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
        uboLayoutBinding, samplerLayoutBinding
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                     &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}
```

```
Updated descriptor set layout:

  ┌───────────────────────────────────────────┐
  │  Descriptor Set Layout                     │
  │                                            │
  │  Binding 0: UNIFORM_BUFFER                │
  │    Stage: VERTEX                           │
  │    (MVP matrices)                          │
  │                                            │
  │  Binding 1: COMBINED_IMAGE_SAMPLER        │
  │    Stage: FRAGMENT                         │
  │    (texture + sampler)                     │
  └───────────────────────────────────────────┘
```

### Update Descriptor Pool

The pool needs to accommodate the new sampler descriptor type:

```cpp
void createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr,
                                &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}
```

### Update Descriptor Sets

Add a write for the image sampler in `createDescriptorSets`:

```cpp
void createDescriptorSets() {
    // ... allocation code (same as before) ...

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView;
        imageInfo.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType =
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device,
                               static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}
```

---

## Step 9: Adding Texture Coordinates to Vertices

The Vertex struct needs a `texCoord` field:

```cpp
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;
    glm::vec2 texCoord;   // NEW

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3>
    getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;       // NEW
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);    // NEW

        return attributeDescriptions;
    }
};
```

### Updated Vertex Data

```cpp
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
    {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
};
```

```
Vertex layout in memory (per vertex):

  Offset:  0        8           20       28
           │        │            │        │
           ▼        ▼            ▼        ▼
          ┌────────┬────────────┬────────┐
          │ pos    │ color      │texCoord│
          │ vec2   │ vec3       │ vec2   │
          │ 8 B    │ 12 B      │ 8 B    │
          └────────┴────────────┴────────┘
           location 0  location 1  location 2
                                    (NEW!)
```

---

## Step 10: Updated Shaders

### Vertex Shader

```glsl
#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;   // NEW

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord; // NEW

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord;              // pass to fragment shader
}
```

### Fragment Shader

```glsl
#version 450

layout(binding = 1) uniform sampler2D texSampler;  // Combined image sampler

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(texSampler, fragTexCoord);
}
```

The `texture()` function samples the texture at the interpolated UV coordinate. The result is an RGBA color from the texture image.

You can also **combine** texture color with vertex color:

```glsl
void main() {
    outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);
}
```

This multiplies the texture color by the vertex color, tinting the texture.

---

## Initialization Order

```cpp
void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createTextureImage();           // NEW
    createTextureImageView();       // NEW
    createTextureSampler();         // NEW
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}
```

---

## Cleanup

```cpp
void cleanup() {
    cleanupSwapChain();

    vkDestroySampler(device, textureSampler, nullptr);
    vkDestroyImageView(device, textureImageView, nullptr);
    vkDestroyImage(device, textureImage, nullptr);
    vkFreeMemory(device, textureImageMemory, nullptr);

    // ... uniform buffers, descriptor pool/layout, buffers, etc. ...
}
```

---

## Try It Yourself

1. **Run the program** — you should see the texture mapped onto the spinning rectangle instead of just vertex colors!

2. **Try NEAREST filtering**: Change `magFilter` and `minFilter` to `VK_FILTER_NEAREST`. Notice the pixelated look when you zoom in — great for pixel art games.

3. **Change address modes**: Set UV coordinates beyond [0,1] (e.g., `{2.0f, 0.0f}`) and try `REPEAT`, `MIRRORED_REPEAT`, `CLAMP_TO_EDGE`, and `CLAMP_TO_BORDER` to see each mode in action.

4. **Tint with vertex color**: Use `outColor = vec4(fragColor * texture(texSampler, fragTexCoord).rgb, 1.0);` in the fragment shader. Each corner of the rectangle will tint the texture a different color.

5. **Disable anisotropic filtering**: Set `anisotropyEnable = VK_FALSE` and rotate the rectangle to view it at a steep angle. Compare the blurriness with anisotropic filtering enabled.

---

## Key Takeaways

- **Texture mapping** wraps 2D images onto 3D geometry using **UV coordinates** (0 to 1 range per axis).
- The texture loading pipeline is: **disk → stb_image → staging buffer → layout transition → copy → layout transition → VkImage**.
- **Image layouts** must be transitioned with pipeline barriers before the image can be used differently (e.g., write target → shader read).
- `createImage` is a reusable helper, mirroring `createBuffer` for images.
- **Samplers** are separate objects that control filtering (NEAREST/LINEAR), address modes (REPEAT/CLAMP), and anisotropy.
- Textures are exposed to shaders via `COMBINED_IMAGE_SAMPLER` descriptors — bundling image view + sampler in one binding.
- The Vertex struct gains a `texCoord` field; the vertex shader passes it through, and the fragment shader calls `texture()`.
- **Anisotropic filtering** dramatically improves texture quality at steep viewing angles.
- Staging buffer → image copy follows the same "host-visible staging → device-local destination" pattern we used for vertex data.

---

[<< Previous: Uniforms](17-uniforms-descriptors.md) | [Next: Depth Buffering >>](19-depth-buffering.md)
