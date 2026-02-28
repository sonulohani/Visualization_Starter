# Chapter 19: Depth Buffering

[<< Previous: Textures](18-textures-samplers.md) | [Next: Loading 3D Models >>](20-loading-models.md)

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
✅ Texture Mapping    → Images on geometry!

🎯 Now: Depth buffering so closer objects correctly hide farther ones!
```

---

## The Problem: No Depth Awareness

Right now, if we draw two overlapping objects, the **last one drawn** always appears on top — regardless of which one is actually closer to the camera. This is called the **painter's algorithm problem**.

```
Without depth testing:

  Draw order determines visibility, NOT distance!

  Step 1: Draw far square (blue)      Step 2: Draw near square (red)
  ┌────────────────────┐              ┌────────────────────┐
  │                    │              │                    │
  │    ┌──────────┐   │              │    ┌──────────┐   │
  │    │  BLUE    │   │              │    │  RED ┌───┼──┐│
  │    │  (far)   │   │              │    │      │BLU│  ││
  │    │          │   │              │    │      │(fa│  ││  ← Blue should be
  │    └──────────┘   │              │    └──────┤r) │  ││    hidden but it's
  │                    │              │           └───┼──┘│    partly visible
  └────────────────────┘              └────────────────────┘    because draw
                                                                order matters

  If we swap draw order, the result CHANGES:

  Step 1: Draw near square (red)      Step 2: Draw far square (blue)
  ┌────────────────────┐              ┌────────────────────┐
  │                    │              │                    │
  │    ┌──────────┐   │              │    ┌──────────┐   │
  │    │  RED     │   │              │    │  BLUE────┼──┐│
  │    │  (near)  │   │              │    │  overwri │  ││  ← Blue OVERWRITES
  │    │          │   │              │    │  tes red!│  ││    red even though
  │    └──────────┘   │              │    └──────────┼──┘│    it's farther!
  │                    │              │               └───┘│
  └────────────────────┘              └────────────────────┘
                                       WRONG! 💥
```

### The Real-World Analogy

Imagine painting on a canvas:
- Without depth testing: you paint in order, and later paint covers earlier paint. A mountain painted last would cover a nearby tree painted first.
- With depth testing: you have a "depth ruler" at every pixel. Before painting a pixel, you check: "Is this new paint closer than what's already there?" Only paint if yes.

---

## What is a Depth Buffer?

A **depth buffer** (also called a **Z-buffer**) is an image the same size as the screen that stores the **depth (Z value)** of the closest fragment at each pixel.

```
Color Buffer (what you see):        Depth Buffer (hidden, per-pixel Z):

  ┌────────────────────┐             ┌────────────────────┐
  │ sky sky sky sky sky │             │ 1.0 1.0 1.0 1.0   │  ← far (sky)
  │ sky sky sky sky sky │             │ 1.0 1.0 1.0 1.0   │
  │ sky ┌──────┐ sky   │             │ 1.0 ┌──────┐ 1.0  │
  │ sky │ cube │ sky   │             │ 1.0 │ 0.3  │ 1.0  │  ← near (cube)
  │ sky │ face │ sky   │             │ 1.0 │ 0.3  │ 1.0  │
  │ ground ground grnd │             │ 0.8 0.8 0.8 0.8   │  ← mid (ground)
  └────────────────────┘             └────────────────────┘

For each pixel, the depth buffer stores how far away the surface is:
  0.0 = at the near plane (closest possible)
  1.0 = at the far plane (farthest possible)
```

### How Depth Testing Works

For every fragment the rasterizer generates:

```
1. Fragment arrives with depth value Z_new
2. Read existing depth at this pixel: Z_old
3. Compare: Z_new < Z_old?  (is the new fragment closer?)
   YES → Write new color AND new depth (closer fragment wins)
   NO  → Discard the fragment (it's behind something)

Example at pixel (100, 200):

  Frame starts: depth buffer = 1.0 (cleared to farthest)

  Fragment 1: blue cube at Z=0.5
    0.5 < 1.0? YES → write blue, depth = 0.5

  Fragment 2: red sphere at Z=0.3
    0.3 < 0.5? YES → write red, depth = 0.3  (sphere is closer!)

  Fragment 3: green wall at Z=0.8
    0.8 < 0.3? NO  → discard  (wall is behind the sphere)

  Final pixel: RED (the closest fragment)
```

---

## Step 1: Finding a Supported Depth Format

Different GPUs support different depth buffer formats. We need to query the device for a format it supports:

```
Common depth formats:

  Format                    Bits    Stencil?   Notes
  ─────────────────────────────────────────────────────────
  VK_FORMAT_D32_SFLOAT       32      No         Best precision
  VK_FORMAT_D32_SFLOAT_S8_UINT 32+8  Yes        Best precision + stencil
  VK_FORMAT_D24_UNORM_S8_UINT 24+8   Yes        Good precision + stencil
  VK_FORMAT_D16_UNORM        16      No         Low precision (mobile)
```

| Format | Precision | Stencil | Typical Use |
|--------|-----------|---------|-------------|
| `D32_SFLOAT` | 32-bit float | No | Desktop, best quality |
| `D32_SFLOAT_S8_UINT` | 32-bit float + 8-bit | Yes | Desktop + stencil effects |
| `D24_UNORM_S8_UINT` | 24-bit fixed + 8-bit | Yes | Most common compromise |
| `D16_UNORM` | 16-bit fixed | No | Mobile, limited range |

### Format Finder Helper

```cpp
VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
                             VkImageTiling tiling,
                             VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                   (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

VkFormat findDepthFormat() {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT,
         VK_FORMAT_D32_SFLOAT_S8_UINT,
         VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}
```

The candidates are listed in preference order — 32-bit float first (best precision), 24-bit as fallback.

### Checking for Stencil Component

Some depth formats include a stencil component. We'll need to know this later:

```cpp
bool hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           format == VK_FORMAT_D24_UNORM_S8_UINT;
}
```

---

## Step 2: Creating the Depth Image and View

The depth buffer is just another `VkImage` — same as our texture, but with a depth format:

```cpp
VkImage depthImage;
VkDeviceMemory depthImageMemory;
VkImageView depthImageView;

void createDepthResources() {
    VkFormat depthFormat = findDepthFormat();

    createImage(swapChainExtent.width, swapChainExtent.height,
                depthFormat,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depthImage, depthImageMemory);

    depthImageView = createImageView(depthImage, depthFormat);
}
```

We need to update `createImageView` to handle depth formats by passing the aspect mask:

```cpp
VkImageView createImageView(VkImage image, VkFormat format,
                             VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
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

Update the calls:

```cpp
// For swap chain image views (color):
createImageView(swapChainImages[i], swapChainImageFormat,
                VK_IMAGE_ASPECT_COLOR_BIT);

// For texture image view (color):
createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_ASPECT_COLOR_BIT);

// For depth image view (depth):
createImageView(depthImage, depthFormat,
                VK_IMAGE_ASPECT_DEPTH_BIT);
```

```
Image types comparison:

  Color Image (swap chain):          Depth Image:
  ┌─────────────────────┐           ┌─────────────────────┐
  │ R G B A  R G B A .. │           │ D D D D D D D D ... │
  │ per pixel: 4 bytes  │           │ per pixel: 4 bytes  │
  │ format: B8G8R8A8    │           │ format: D32_SFLOAT  │
  │ usage: COLOR_ATTACH │           │ usage: DEPTH_ATTACH │
  │ aspect: COLOR       │           │ aspect: DEPTH       │
  └─────────────────────┘           └─────────────────────┘
```

### Do We Need a Layout Transition?

You might expect we need to transition the depth image to `DEPTH_STENCIL_ATTACHMENT_OPTIMAL`. Actually, the render pass handles this automatically — the `initialLayout` and `finalLayout` in the attachment description take care of it.

---

## Step 3: Adding Depth Attachment to Render Pass

The render pass needs to know about the depth attachment:

```cpp
void createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;  // NEW

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {
        colorAttachment, depthAttachment
    };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr,
                            &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}
```

### Depth Attachment Settings Explained

| Field | Value | Why |
|-------|-------|-----|
| `loadOp` | `CLEAR` | Clear depth to 1.0 (farthest) at start of each frame |
| `storeOp` | `DONT_CARE` | We don't read depth after rendering (not needed for presentation) |
| `initialLayout` | `UNDEFINED` | We clear it anyway, so previous contents don't matter |
| `finalLayout` | `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` | Stays as depth attachment |
| `attachment = 1` | Second attachment | Index 0 is color, index 1 is depth |

```
Render pass attachments:

  Index 0: Color attachment          Index 1: Depth attachment
  ┌─────────────────────┐           ┌─────────────────────┐
  │ format: B8G8R8A8    │           │ format: D32_SFLOAT  │
  │ load: CLEAR (black) │           │ load: CLEAR (1.0)   │
  │ store: STORE        │           │ store: DONT_CARE    │
  │ final: PRESENT_SRC  │           │ final: DEPTH_ATTACH │
  └─────────────────────┘           └─────────────────────┘
         ↑                                   ↑
    colorAttachmentRef              depthAttachmentRef
    (attachment = 0)                (attachment = 1)
```

---

## Step 4: Updating Framebuffers

Each framebuffer now needs **two** image views — color and depth:

```cpp
void createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            depthImageView              // Same depth image for all framebuffers
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
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
Framebuffer structure (before vs after):

BEFORE (color only):                AFTER (color + depth):

  Framebuffer 0:                    Framebuffer 0:
  ┌─────────────────┐              ┌─────────────────┐
  │ attachment 0:   │              │ attachment 0:   │
  │ swapImage[0]    │              │ swapImage[0]    │
  └─────────────────┘              │ attachment 1:   │
                                   │ depthImageView  │
  Framebuffer 1:                   └─────────────────┘
  ┌─────────────────┐
  │ attachment 0:   │              Framebuffer 1:
  │ swapImage[1]    │              ┌─────────────────┐
  └─────────────────┘              │ attachment 0:   │
                                   │ swapImage[1]    │
                                   │ attachment 1:   │
                                   │ depthImageView  │  ← SAME depth image
                                   └─────────────────┘    (shared across all)
```

Why is the depth image shared? Because we only render one frame at a time — the depth buffer is cleared at the start of each render pass. We don't need a separate depth image per swap chain image.

---

## Step 5: Enabling Depth Testing in the Pipeline

The graphics pipeline needs to be told to actually **perform** depth testing. This is done with `VkPipelineDepthStencilStateCreateInfo`:

```cpp
void createGraphicsPipeline() {
    // ... shader stages, vertex input, input assembly,
    //     viewport, rasterizer, multisampling ...

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    // ... then pass it to the pipeline create info:

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    // ... other state ...
    pipelineInfo.pDepthStencilState = &depthStencil;

    // ... create pipeline ...
}
```

### Depth Stencil Fields

| Field | Value | Meaning |
|-------|-------|---------|
| `depthTestEnable` | `TRUE` | Enable depth comparison for each fragment |
| `depthWriteEnable` | `TRUE` | Passing fragments update the depth buffer |
| `depthCompareOp` | `LESS` | Fragment passes if its depth < stored depth (closer wins) |
| `depthBoundsTestEnable` | `FALSE` | Don't restrict to a depth range (advanced) |
| `stencilTestEnable` | `FALSE` | No stencil operations (for now) |

### Compare Operations

The `depthCompareOp` determines when a fragment "passes" the depth test:

```
Compare operations (Z_new is the incoming fragment, Z_old is stored):

  VK_COMPARE_OP_LESS           Z_new < Z_old     (default, closest wins)
  VK_COMPARE_OP_LESS_OR_EQUAL  Z_new ≤ Z_old     (closest wins, ties pass)
  VK_COMPARE_OP_GREATER        Z_new > Z_old     (farthest wins — unusual)
  VK_COMPARE_OP_GREATER_OR_EQUAL Z_new ≥ Z_old
  VK_COMPARE_OP_EQUAL          Z_new = Z_old     (only exact depth match)
  VK_COMPARE_OP_NOT_EQUAL      Z_new ≠ Z_old
  VK_COMPARE_OP_ALWAYS         always pass       (no depth test)
  VK_COMPARE_OP_NEVER          never pass        (nothing renders)

Most common: LESS or LESS_OR_EQUAL

Special uses:
  GREATER: "reverse-Z" technique for better precision at distance
  EQUAL: second-pass rendering (only draw where first pass wrote)
  ALWAYS: disable depth test without recreating pipeline
```

---

## Step 6: Updating Clear Values

When beginning the render pass, we now clear **two** attachments — color and depth:

```cpp
void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // ...

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};   // Black background
    clearValues[1].depthStencil = {1.0f, 0};                // Depth = 1.0 (far)

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // ...
}
```

### Why Clear Depth to 1.0?

```
Depth range in Vulkan: 0.0 (nearest) to 1.0 (farthest)

If we clear to 1.0:
  Every fragment starts closer than the cleared value.
  First fragment at any pixel always passes (its Z < 1.0).

If we clear to 0.0:
  Every fragment would fail (its Z > 0.0, LESS comparison fails).
  Nothing would render! ❌

Clear value must match your compare operation:
  LESS compare → clear to 1.0 (maximum depth)
  GREATER compare → clear to 0.0 (minimum depth)
```

The clear values array order must match the attachment indices: index 0 = color, index 1 = depth.

---

## Step 7: Swap Chain Recreation

When the swap chain is recreated (window resize), the depth resources must also be recreated since they depend on the swap chain extent:

```cpp
void cleanupSwapChain() {
    vkDestroyImageView(device, depthImageView, nullptr);
    vkDestroyImage(device, depthImage, nullptr);
    vkFreeMemory(device, depthImageMemory, nullptr);

    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapChain, nullptr);
}

void recreateSwapChain() {
    // ... handle minimization ...

    vkDeviceWaitIdle(device);
    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();   // Recreate depth with new extent
    createFramebuffers();
}
```

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
    createDepthResources();         // NEW — before framebuffers!
    createFramebuffers();
    createCommandPool();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}
```

`createDepthResources` must come **before** `createFramebuffers` because the framebuffers reference the depth image view.

---

## Visualizing the Depth Pipeline

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        DEPTH TESTING PIPELINE                           │
│                                                                         │
│  1. Render pass begins                                                  │
│     ├─ Color buffer cleared to black (0,0,0,1)                         │
│     └─ Depth buffer cleared to 1.0                                     │
│                                                                         │
│  2. For each triangle:                                                  │
│     ├─ Vertex shader runs → outputs gl_Position (includes Z)           │
│     ├─ Rasterizer creates fragments with interpolated Z                │
│     │                                                                   │
│     │  3. For each fragment:                                            │
│     │     ├─ Read depth buffer at (x, y) → Z_old                      │
│     │     ├─ Compare: fragment Z < Z_old?                              │
│     │     │   YES ──→ Run fragment shader                              │
│     │     │         ──→ Write color to color buffer                    │
│     │     │         ──→ Write Z to depth buffer                        │
│     │     │   NO  ──→ Discard fragment (behind existing surface)       │
│     │     └─────────────────────────────────                           │
│     └───────────────────────────────────────                           │
│                                                                         │
│  4. Render pass ends                                                    │
│     └─ Color buffer presented to screen                                │
│        (depth buffer is discarded — DONT_CARE store op)                │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## Testing It: Overlapping Geometry

To verify depth buffering works, you can render two overlapping rectangles at different depths by using 3D positions in the vertex data. Update the vertex positions to use `glm::vec3`:

```cpp
const std::vector<Vertex> vertices = {
    // First rectangle (at Z = 0.0)
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

    // Second rectangle (at Z = -0.5, shifted right)
    {{-0.5f + 0.3f, -0.5f + 0.3f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f + 0.3f, -0.5f + 0.3f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f + 0.3f,  0.5f + 0.3f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f + 0.3f,  0.5f + 0.3f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0,   // First rectangle
    4, 5, 6, 6, 7, 4    // Second rectangle
};
```

Remember to update the vertex shader to use `vec3` for position:

```glsl
layout(location = 0) in vec3 inPosition;  // was vec2

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    // ...                                           ^^^^^^^^^^^^^^^^^^^
    // Now using all 3 components directly (no more 0.0 for Z)
}
```

And update the Vertex struct's position attribute format:

```cpp
// was VK_FORMAT_R32G32_SFLOAT (vec2)
attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;  // vec3
```

---

## Try It Yourself

1. **Run with two overlapping rectangles** — verify the closer one correctly occludes the farther one, regardless of draw order. Swap the order of the rectangles in the vertex/index data and confirm the result stays the same.

2. **Disable depth testing**: Set `depthTestEnable = VK_FALSE`. Notice that draw order now matters again — the second rectangle always appears on top.

3. **Disable depth writing**: Set `depthWriteEnable = VK_FALSE` (but keep `depthTestEnable = VK_TRUE`). Nothing passes because the depth buffer stays at 1.0 — wait, actually everything passes but nothing updates depth, so the last draw always wins. Think about why.

4. **Try GREATER compare**: Change to `VK_COMPARE_OP_GREATER` and clear depth to `0.0f`. This is the "reverse-Z" technique. Everything should still render correctly but with better precision at far distances.

5. **Visualize the depth buffer**: In your fragment shader, output depth as color: `outColor = vec4(vec3(gl_FragCoord.z), 1.0);`. Closer objects appear darker, farther objects appear lighter.

---

## Key Takeaways

- **Without depth testing**, the last drawn fragment always wins — causing incorrect occlusion for 3D scenes.
- A **depth buffer** stores per-pixel depth values (0.0 = near, 1.0 = far), tested against each incoming fragment.
- Use `findDepthFormat()` to query the GPU for a supported depth format — prefer `D32_SFLOAT` for best precision.
- The depth image is a `VkImage` like any other, but with a depth format and `DEPTH_STENCIL_ATTACHMENT` usage.
- The **render pass** gains a second attachment (depth), and the **subpass** references it via `pDepthStencilAttachment`.
- **Framebuffers** now include the depth image view alongside each swap chain image view.
- `VkPipelineDepthStencilStateCreateInfo` enables depth testing in the pipeline with a **compare operation** (`LESS` = closest wins).
- Clear the depth buffer to **1.0** (farthest) at the start of each frame so all geometry can pass the test.
- Depth resources must be **recreated** when the swap chain is recreated (window resize).
- With depth buffering enabled, we're ready for **real 3D scenes** with multiple overlapping objects!

---

[<< Previous: Textures](18-textures-samplers.md) | [Next: Loading 3D Models >>](20-loading-models.md)
