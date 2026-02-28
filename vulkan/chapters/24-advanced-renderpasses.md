# Chapter 24: Advanced Render Passes

[<< Previous: Compute Shaders](23-compute-shaders.md) | [Next: Memory Management >>](25-memory-management.md)

---

**Progress Tracker:**
```
[████████████████████████░] Chapter 24 of 30
Topics: Multiple Subpasses | Deferred Rendering | Input Attachments | Dynamic Rendering
```

---

## Table of Contents

1. [Why Multiple Subpasses?](#why-multiple-subpasses)
2. [Deferred Rendering Concept](#deferred-rendering-concept)
3. [Input Attachments](#input-attachments)
4. [Defining Attachments](#defining-attachments)
5. [Subpass Descriptions](#subpass-descriptions)
6. [Subpass Dependencies](#subpass-dependencies)
7. [Creating the G-Buffer Images](#creating-the-g-buffer-images)
8. [Descriptor Set for Input Attachments](#descriptor-set-for-input-attachments)
9. [G-Buffer Write Shader](#g-buffer-write-shader)
10. [Lighting Read Shader](#lighting-read-shader)
11. [Putting It All Together](#putting-it-all-together)
12. [Dynamic Rendering (Vulkan 1.3)](#dynamic-rendering-vulkan-13)
13. [Try It Yourself](#try-it-yourself)
14. [Key Takeaways](#key-takeaways)

---

## Why Multiple Subpasses?

So far, our render pass has had a single subpass. But Vulkan supports **multiple subpasses** within a single render pass, and this is more than just organizational — it's a **performance optimization**, especially on tile-based GPUs (mobile, Apple Silicon, some Intel).

**Analogy:** Imagine you're painting a mural. With separate render passes, you paint the background, carry the canvas to a different room (write to memory), bring it back (read from memory), and paint the foreground. With subpasses, you stay in the same spot and paint both layers without ever moving the canvas — everything stays in your hands (tile memory).

```
Separate render passes (desktop-style):

  Subpass 1            Main Memory            Subpass 2
  ┌──────────┐        ┌──────────┐          ┌──────────┐
  │ G-Buffer │──write─►│ Textures │──read───►│ Lighting │
  │  pass    │        │          │          │   pass   │
  └──────────┘        └──────────┘          └──────────┘

  Expensive: full framebuffer written to VRAM, then read back.

Multiple subpasses (tile-based optimization):

  ┌─────── Single Render Pass (stays in tile memory) ───────┐
  │                                                          │
  │  Subpass 1          input attachment        Subpass 2    │
  │  ┌──────────┐    (tile-local read)       ┌──────────┐   │
  │  │ G-Buffer │────────────────────────────►│ Lighting │   │
  │  │  pass    │                             │   pass   │   │
  │  └──────────┘                             └──────────┘   │
  │                                                          │
  │  Data never leaves tile memory! Much faster on mobile.   │
  └──────────────────────────────────────────────────────────┘
```

**On tile-based GPUs** (ARM Mali, Qualcomm Adreno, Apple GPU, Intel Gen), the framebuffer is divided into tiles (~16×16 or 32×32 pixels). Each tile is processed entirely in fast on-chip SRAM. If data stays within a render pass via subpasses and input attachments, it never touches slow external memory.

**On desktop GPUs** (NVIDIA, AMD discrete), the benefit is smaller but still exists: the driver can optimize memory access patterns and avoid unnecessary format conversions.

---

## Deferred Rendering Concept

Deferred rendering splits the rendering into two phases:

1. **Geometry pass (G-Buffer):** Render all geometry, storing position, normal, albedo, and material properties into multiple render targets.
2. **Lighting pass:** Read the G-buffer data and compute lighting for each pixel. Only one lighting calculation per visible pixel, regardless of how many objects overlap.

```
Forward rendering:          Deferred rendering:

For each object:            Pass 1 (Geometry): For each object:
  For each light:             Write position, normal, albedo to G-buffer
    shade pixel
                             Pass 2 (Lighting): For each pixel:
Cost: O(objects × lights)     For each light:
                                shade using G-buffer data
                             Cost: O(objects + pixels × lights)
```

When you have many lights, deferred rendering wins dramatically:

| Lights | Objects | Forward (ops) | Deferred (ops) |
|---|---|---|---|
| 10 | 100 | 1,000 | 100 + pixels×10 |
| 100 | 100 | 10,000 | 100 + pixels×100 |
| 1000 | 100 | 100,000 | 100 + pixels×1000 |

```
G-Buffer layout (what gets stored per pixel):

┌────────────────────┐ ┌────────────────────┐ ┌────────────────────┐
│ Position (RGB32F)  │ │  Normal (RGB32F)   │ │ Albedo (RGBA8)     │
│                    │ │                    │ │                    │
│ World-space X,Y,Z  │ │ World-space Nx,Ny,Nz│ │ Diffuse color      │
│ per pixel          │ │ per pixel          │ │ + specular in alpha │
└────────────────────┘ └────────────────────┘ └────────────────────┘
        ▼                      ▼                      ▼
    ┌──────────────────────────────────────────────────────┐
    │                  Lighting Pass                        │
    │  Read all three buffers per pixel                     │
    │  For each light: compute diffuse + specular           │
    │  Output: final lit color                              │
    └──────────────────────────────────────────────────────┘
```

---

## Input Attachments

Input attachments are the mechanism that lets a later subpass **read** data written by an earlier subpass — without the data ever leaving tile memory on tile-based GPUs.

In GLSL, you access them with:

```glsl
layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput inPosition;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inNormal;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform subpassInput inAlbedo;

// Read the value at the current fragment's position
vec3 position = subpassLoad(inPosition).rgb;
vec3 normal   = subpassLoad(inNormal).rgb;
vec4 albedo   = subpassLoad(inAlbedo);
```

**Key constraints:**
- `subpassLoad` can **only** read the value at the current fragment position (no random access)
- No filtering — you get the exact texel value
- The `input_attachment_index` corresponds to the order in `pInputAttachments` of the subpass description
- Input attachments must be declared as `VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT`

**Comparison with regular textures:**

| Feature | Input Attachment | Sampled Image (Texture) |
|---|---|---|
| Random access | No (current pixel only) | Yes (any UV coordinate) |
| Filtering | None | Bilinear, trilinear, etc. |
| Mip levels | N/A | Supported |
| Tile memory | Stays on-chip (tile-based GPU) | Must be in VRAM |
| Descriptor type | `INPUT_ATTACHMENT` | `COMBINED_IMAGE_SAMPLER` |
| GLSL access | `subpassLoad(att)` | `texture(sampler, uv)` |

---

## Defining Attachments

Our deferred render pass needs these attachments:

| Index | Purpose | Format | Usage |
|---|---|---|---|
| 0 | Final color output | Swapchain format | Written by subpass 1 (lighting) |
| 1 | G-Buffer: Position | `R16G16B16A16_SFLOAT` | Written by subpass 0, input to subpass 1 |
| 2 | G-Buffer: Normal | `R16G16B16A16_SFLOAT` | Written by subpass 0, input to subpass 1 |
| 3 | G-Buffer: Albedo | `R8G8B8A8_UNORM` | Written by subpass 0, input to subpass 1 |
| 4 | Depth | Depth format | Used by subpass 0 |

```cpp
void createDeferredRenderPass() {
    // Attachment 0: Final color (swapchain image)
    VkAttachmentDescription finalColorAttachment{};
    finalColorAttachment.format = swapChainImageFormat;
    finalColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    finalColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    finalColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    finalColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    finalColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    finalColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    finalColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Attachment 1: G-Buffer Position
    VkAttachmentDescription positionAttachment{};
    positionAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    positionAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    positionAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    positionAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    positionAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    positionAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    positionAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    positionAttachment.finalLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Attachment 2: G-Buffer Normal
    VkAttachmentDescription normalAttachment{};
    normalAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalAttachment.finalLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Attachment 3: G-Buffer Albedo
    VkAttachmentDescription albedoAttachment{};
    albedoAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    albedoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    albedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    albedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    albedoAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    albedoAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    albedoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    albedoAttachment.finalLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Attachment 4: Depth
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

    std::array<VkAttachmentDescription, 5> attachments = {
        finalColorAttachment, positionAttachment, normalAttachment,
        albedoAttachment, depthAttachment
    };
```

Notice that G-buffer attachments use `storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE`. After the lighting pass reads them, we don't need them anymore — telling the driver this allows it to skip writing them to main memory entirely.

---

## Subpass Descriptions

Define two subpasses and how they reference attachments:

```cpp
    // --- Subpass 0: Geometry (G-Buffer write) ---
    VkAttachmentReference gBufferColorRefs[3];

    // Write position to attachment 1
    gBufferColorRefs[0].attachment = 1;
    gBufferColorRefs[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Write normal to attachment 2
    gBufferColorRefs[1].attachment = 2;
    gBufferColorRefs[1].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Write albedo to attachment 3
    gBufferColorRefs[2].attachment = 3;
    gBufferColorRefs[2].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 4;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription geometrySubpass{};
    geometrySubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    geometrySubpass.colorAttachmentCount = 3;
    geometrySubpass.pColorAttachments = gBufferColorRefs;
    geometrySubpass.pDepthStencilAttachment = &depthRef;

    // --- Subpass 1: Lighting (read G-Buffer, write final color) ---
    VkAttachmentReference finalColorRef{};
    finalColorRef.attachment = 0;
    finalColorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference inputRefs[3];
    inputRefs[0].attachment = 1;  // position
    inputRefs[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputRefs[1].attachment = 2;  // normal
    inputRefs[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    inputRefs[2].attachment = 3;  // albedo
    inputRefs[2].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkSubpassDescription lightingSubpass{};
    lightingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    lightingSubpass.colorAttachmentCount = 1;
    lightingSubpass.pColorAttachments = &finalColorRef;
    lightingSubpass.inputAttachmentCount = 3;
    lightingSubpass.pInputAttachments = inputRefs;

    std::array<VkSubpassDescription, 2> subpasses = {
        geometrySubpass, lightingSubpass
    };
```

```
Attachment flow between subpasses:

Subpass 0 (Geometry):                  Subpass 1 (Lighting):

  color[0] → Att 1 (Position) ──input──► inputAttachment[0]
  color[1] → Att 2 (Normal)   ──input──► inputAttachment[1]
  color[2] → Att 3 (Albedo)   ──input──► inputAttachment[2]
  depth    → Att 4 (Depth)
                                          color[0] → Att 0 (Final → Present)
```

---

## Subpass Dependencies

Dependencies tell Vulkan the execution and memory ordering between subpasses:

```cpp
    std::array<VkSubpassDependency, 3> dependencies{};

    // External → Subpass 0: wait for swapchain image to be available
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask =
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Subpass 0 → Subpass 1: G-buffer writes must complete before reads
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask =
        VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Subpass 1 → External: final color must be written before present
    dependencies[2].srcSubpass = 1;
    dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[2].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[2].dstStageMask =
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[2].srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
```

### VK_DEPENDENCY_BY_REGION_BIT

This flag is **critical for tile-based GPUs**. It tells the driver that the dependency is per-tile: subpass 1 only needs data from the *same screen region* that subpass 0 wrote. This means the GPU can process each tile through both subpasses before moving to the next tile, keeping everything in tile memory.

```
Without BY_REGION_BIT:                With BY_REGION_BIT:

  Subpass 0: Process ALL tiles        Tile 0: Subpass 0 → Subpass 1
             ↓ flush all to VRAM      Tile 1: Subpass 0 → Subpass 1
  Subpass 1: Read ALL from VRAM       Tile 2: Subpass 0 → Subpass 1
                                      ...
  Slow: full framebuffer roundtrip    Fast: data stays in tile SRAM
```

Now create the render pass:

```cpp
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount =
        static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount =
        static_cast<uint32_t>(subpasses.size());
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount =
        static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr,
                           &deferredRenderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create deferred render pass!");
    }
}
```

---

## Creating the G-Buffer Images

Each G-buffer attachment needs its own image, memory, and image view:

```cpp
struct GBuffer {
    VkImage positionImage;
    VkDeviceMemory positionMemory;
    VkImageView positionView;

    VkImage normalImage;
    VkDeviceMemory normalMemory;
    VkImageView normalView;

    VkImage albedoImage;
    VkDeviceMemory albedoMemory;
    VkImageView albedoView;
} gBuffer;

void createGBufferImages() {
    auto createGBufferAttachment = [&](VkFormat format,
                                       VkImageUsageFlags usage,
                                       VkImage& image,
                                       VkDeviceMemory& memory,
                                       VkImageView& view) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = {swapChainExtent.width,
                           swapChainExtent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = usage |
                         VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(device, &imageInfo, nullptr, &image)
            != VK_SUCCESS) {
            throw std::runtime_error("failed to create G-buffer image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &memory)
            != VK_SUCCESS) {
            throw std::runtime_error(
                "failed to allocate G-buffer image memory!");
        }

        vkBindImageMemory(device, image, memory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask =
            VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &view)
            != VK_SUCCESS) {
            throw std::runtime_error(
                "failed to create G-buffer image view!");
        }
    };

    createGBufferAttachment(
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        gBuffer.positionImage, gBuffer.positionMemory,
        gBuffer.positionView);

    createGBufferAttachment(
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        gBuffer.normalImage, gBuffer.normalMemory,
        gBuffer.normalView);

    createGBufferAttachment(
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        gBuffer.albedoImage, gBuffer.albedoMemory,
        gBuffer.albedoView);
}
```

The framebuffer for the deferred render pass includes all five attachments:

```cpp
void createDeferredFramebuffers() {
    deferredFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 5> attachments = {
            swapChainImageViews[i],     // 0: Final color
            gBuffer.positionView,       // 1: Position
            gBuffer.normalView,         // 2: Normal
            gBuffer.albedoView,         // 3: Albedo
            depthImageView              // 4: Depth
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType =
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = deferredRenderPass;
        framebufferInfo.attachmentCount =
            static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                                &deferredFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error(
                "failed to create deferred framebuffer!");
        }
    }
}
```

---

## Descriptor Set for Input Attachments

The lighting subpass needs a descriptor set to access the G-buffer attachments as input attachments:

```cpp
void createInputAttachmentDescriptors() {
    // Layout: 3 input attachments
    std::array<VkDescriptorSetLayoutBinding, 3> bindings{};

    for (uint32_t i = 0; i < 3; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType =
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType =
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                    &inputAttachmentLayout)
        != VK_SUCCESS) {
        throw std::runtime_error(
            "failed to create input attachment descriptor set layout!");
    }

    // Allocate and write descriptor sets
    // ... allocate from pool ...

    VkDescriptorImageInfo posInfo{};
    posInfo.imageView = gBuffer.positionView;
    posInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    posInfo.sampler = VK_NULL_HANDLE;  // not needed for input attachments

    VkDescriptorImageInfo normalInfo{};
    normalInfo.imageView = gBuffer.normalView;
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    normalInfo.sampler = VK_NULL_HANDLE;

    VkDescriptorImageInfo albedoInfo{};
    albedoInfo.imageView = gBuffer.albedoView;
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    albedoInfo.sampler = VK_NULL_HANDLE;

    std::array<VkWriteDescriptorSet, 3> writes{};
    for (uint32_t i = 0; i < 3; i++) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = inputAttachmentDescriptorSet;
        writes[i].dstBinding = i;
        writes[i].descriptorType =
            VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        writes[i].descriptorCount = 1;
    }
    writes[0].pImageInfo = &posInfo;
    writes[1].pImageInfo = &normalInfo;
    writes[2].pImageInfo = &albedoInfo;

    vkUpdateDescriptorSets(device,
        static_cast<uint32_t>(writes.size()),
        writes.data(), 0, nullptr);
}
```

---

## G-Buffer Write Shader

**Vertex shader (`gbuffer.vert`):**

```glsl
#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;

void main() {
    vec4 worldPos = ubo.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    fragNormal = mat3(transpose(inverse(ubo.model))) * inNormal;
    fragTexCoord = inTexCoord;

    gl_Position = ubo.proj * ubo.view * worldPos;
}
```

**Fragment shader (`gbuffer.frag`):**

The fragment shader writes to **multiple render targets** (MRT). Each `layout(location = N)` output corresponds to the Nth color attachment of the subpass.

```glsl
#version 450

layout(set = 1, binding = 0) uniform sampler2D albedoMap;

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;

// Multiple Render Targets — one per G-buffer attachment
layout(location = 0) out vec4 outPosition;   // → Attachment 1
layout(location = 1) out vec4 outNormal;     // → Attachment 2
layout(location = 2) out vec4 outAlbedo;     // → Attachment 3

void main() {
    outPosition = vec4(fragWorldPos, 1.0);
    outNormal   = vec4(normalize(fragNormal), 0.0);
    outAlbedo   = texture(albedoMap, fragTexCoord);
}
```

```
Fragment shader output mapping:

Shader output           Subpass color attachment     Render pass attachment
─────────────           ───────────────────────     ──────────────────────
location = 0  ────────► colorAttachments[0]  ─────► Attachment 1 (Position)
location = 1  ────────► colorAttachments[1]  ─────► Attachment 2 (Normal)
location = 2  ────────► colorAttachments[2]  ─────► Attachment 3 (Albedo)
```

---

## Lighting Read Shader

**Vertex shader (`lighting.vert`):**

The lighting pass renders a full-screen triangle (or quad). No vertex buffer needed:

```glsl
#version 450

layout(location = 0) out vec2 fragTexCoord;

void main() {
    // Full-screen triangle trick: 3 vertices, no vertex buffer
    fragTexCoord = vec2((gl_VertexIndex << 1) & 2,
                        gl_VertexIndex & 2);
    gl_Position = vec4(fragTexCoord * 2.0 - 1.0, 0.0, 1.0);
}
```

```
Full-screen triangle (oversized, clipped to screen):

     (-1,3)
       ╱│
      ╱ │
     ╱  │
    ╱   │         Only the part inside the
   ╱    │         NDC cube [-1,1] is rasterized.
  ╱─────┤(1,1)
 ╱  ██  │
╱  ████ │         ██ = visible screen area
───████─┤
   ████ │
(-1,-1) (3,-1)

Vertices: (−1,−1), (3,−1), (−1,3)
Produces UV coordinates (0,0), (2,0), (0,2) → clipped to [0,1]
```

**Fragment shader (`lighting.frag`):**

```glsl
#version 450

layout(input_attachment_index = 0, set = 0, binding = 0)
    uniform subpassInput inPosition;
layout(input_attachment_index = 1, set = 0, binding = 1)
    uniform subpassInput inNormal;
layout(input_attachment_index = 2, set = 0, binding = 2)
    uniform subpassInput inAlbedo;

struct Light {
    vec4 position;
    vec4 color;       // rgb = color, a = intensity
    float radius;
};

layout(std140, set = 1, binding = 0) uniform LightData {
    Light lights[64];
    int numLights;
    vec3 viewPos;
} lightUBO;

layout(location = 0) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 position = subpassLoad(inPosition).rgb;
    vec3 normal   = normalize(subpassLoad(inNormal).rgb);
    vec4 albedo   = subpassLoad(inAlbedo);

    vec3 ambient = 0.05 * albedo.rgb;
    vec3 result = ambient;

    for (int i = 0; i < lightUBO.numLights; i++) {
        Light light = lightUBO.lights[i];

        vec3 lightDir = light.position.xyz - position;
        float dist = length(lightDir);

        if (dist > light.radius) continue;

        lightDir = normalize(lightDir);
        float attenuation = 1.0 / (1.0 + dist * dist);
        attenuation *= smoothstep(light.radius, 0.0, dist);

        // Diffuse
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diff * albedo.rgb * light.color.rgb *
                       light.color.a;

        // Specular (Blinn-Phong)
        vec3 viewDir = normalize(lightUBO.viewPos - position);
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(normal, halfDir), 0.0), 32.0);
        vec3 specular = spec * light.color.rgb * light.color.a;

        result += (diffuse + specular) * attenuation;
    }

    outColor = vec4(result, 1.0);
}
```

---

## Putting It All Together

Recording the command buffer with both subpasses:

```cpp
void recordDeferredCommandBuffer(VkCommandBuffer cmd,
                                 uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    std::array<VkClearValue, 5> clearValues{};
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};  // final color
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}};  // position
    clearValues[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}};  // normal
    clearValues[3].color = {{0.0f, 0.0f, 0.0f, 0.0f}};  // albedo
    clearValues[4].depthStencil = {1.0f, 0};              // depth

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = deferredRenderPass;
    renderPassInfo.framebuffer = deferredFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;
    renderPassInfo.clearValueCount =
        static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    // === Subpass 0: Geometry pass (write G-buffer) ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      gBufferPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            gBufferPipelineLayout, 0, 1,
                            &sceneDescriptorSet, 0, nullptr);

    // Draw all scene geometry
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

    // === Transition to Subpass 1 ===
    vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);

    // === Subpass 1: Lighting pass (read G-buffer, write final color) ===
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      lightingPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            lightingPipelineLayout, 0, 1,
                            &inputAttachmentDescriptorSet, 0, nullptr);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            lightingPipelineLayout, 1, 1,
                            &lightDescriptorSet, 0, nullptr);

    // Full-screen triangle (3 vertices, no vertex buffer)
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}
```

```
Command buffer timeline:

  BeginRenderPass (5 attachments, 5 clear values)
  │
  ├── Subpass 0 (Geometry)
  │     Bind gBufferPipeline
  │     Bind scene descriptors (UBO, textures)
  │     Bind vertex/index buffers
  │     DrawIndexed (all scene geometry)
  │     Writes: Position, Normal, Albedo, Depth
  │
  ├── NextSubpass
  │     (driver inserts implicit barriers per dependencies)
  │
  ├── Subpass 1 (Lighting)
  │     Bind lightingPipeline
  │     Bind input attachment descriptors (G-buffer)
  │     Bind light data descriptors
  │     Draw(3) — full-screen triangle
  │     Reads: Position, Normal, Albedo
  │     Writes: Final color → swapchain image
  │
  EndRenderPass
```

---

## Dynamic Rendering (Vulkan 1.3)

Vulkan 1.3 introduced **Dynamic Rendering** (`VK_KHR_dynamic_rendering`), which eliminates the need for `VkRenderPass` and `VkFramebuffer` objects entirely. Instead, you specify attachments inline when beginning a render pass.

```cpp
// No VkRenderPass or VkFramebuffer needed!
VkRenderingAttachmentInfo colorAttachmentInfo{};
colorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
colorAttachmentInfo.imageView = swapChainImageView;
colorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
colorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
colorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
colorAttachmentInfo.clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

VkRenderingAttachmentInfo depthAttachmentInfo{};
depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
depthAttachmentInfo.imageView = depthImageView;
depthAttachmentInfo.imageLayout =
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
depthAttachmentInfo.clearValue.depthStencil = {1.0f, 0};

VkRenderingInfo renderingInfo{};
renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
renderingInfo.renderArea = {{0, 0}, swapChainExtent};
renderingInfo.layerCount = 1;
renderingInfo.colorAttachmentCount = 1;
renderingInfo.pColorAttachments = &colorAttachmentInfo;
renderingInfo.pDepthAttachment = &depthAttachmentInfo;

vkCmdBeginRendering(commandBuffer, &renderingInfo);
// ... draw commands ...
vkCmdEndRendering(commandBuffer);
```

**Advantages of Dynamic Rendering:**

| Aspect | Traditional Render Pass | Dynamic Rendering |
|---|---|---|
| Setup complexity | High (VkRenderPass, VkFramebuffer) | Low (inline attachment info) |
| Flexibility | Fixed at creation time | Can change per frame |
| Subpasses | Supported | Not supported (use barriers) |
| Tile-based optimization | Automatic via subpasses | Manual (driver may still optimize) |
| API version | Vulkan 1.0+ | Vulkan 1.3+ (or extension) |

**Trade-offs:**
- Dynamic rendering is simpler for basic cases and forward rendering
- For deferred rendering on mobile, traditional render passes with subpasses are still preferred for tile-based optimization
- Dynamic rendering cannot express subpass dependencies — you use explicit pipeline barriers instead
- Most modern desktop engines use dynamic rendering for simplicity, with explicit barriers for synchronization

```
Traditional:                         Dynamic Rendering:

  CreateRenderPass(...)              // No setup needed!
  CreateFramebuffer(...)
  CreateFramebuffer(...)
  CreateFramebuffer(...)             vkCmdBeginRendering(info)
                                       ... draw ...
  vkCmdBeginRenderPass(info)         vkCmdEndRendering()
    ... draw ...
  vkCmdEndRenderPass()

  DestroyFramebuffer(...)            // No cleanup needed!
  DestroyFramebuffer(...)
  DestroyFramebuffer(...)
  DestroyRenderPass(...)
```

---

## Try It Yourself

1. **Visualize G-Buffer:** Add a debug mode that skips the lighting pass and instead outputs one G-buffer channel directly. Toggle between position (as color), normal (as color), and albedo views with a push constant.

2. **Many Lights:** Create 100+ point lights with random positions and colors. Observe how deferred rendering handles this efficiently compared to forward rendering (which would need 100+ light calculations per fragment per object).

3. **Add a Third Subpass:** Insert a post-processing subpass (subpass 2) after the lighting pass. Use it to apply a simple tone mapping or vignette effect. The lighting output becomes an input attachment for the post-processing subpass.

4. **Switch to Dynamic Rendering:** Rewrite the forward rendering path using `vkCmdBeginRendering` / `vkCmdEndRendering` (requires Vulkan 1.3 or `VK_KHR_dynamic_rendering`). Compare the code complexity and verify identical output.

5. **Profile Tile Optimization:** If you have access to a mobile GPU (or emulator), compare the performance of deferred rendering using subpasses (with `VK_DEPENDENCY_BY_REGION_BIT`) versus separate render passes. Measure bandwidth using GPU profiling tools.

---

## Key Takeaways

- **Multiple subpasses** within a single render pass allow tile-based GPUs to keep data in fast on-chip memory, avoiding expensive framebuffer roundtrips.
- **Deferred rendering** splits work into a geometry pass (G-buffer) and a lighting pass, making per-pixel lighting cost independent of scene complexity.
- **Input attachments** (`subpassInput`, `subpassLoad`) let a later subpass read data written by an earlier subpass at the current fragment position — tile-local on mobile GPUs.
- Define attachments with appropriate formats: `R16G16B16A16_SFLOAT` for position/normal (need precision), `R8G8B8A8_UNORM` for albedo (8-bit is enough).
- Use `storeOp = DONT_CARE` for intermediate attachments that are only needed within the render pass.
- **Subpass dependencies** define execution and memory ordering. Always use `VK_DEPENDENCY_BY_REGION_BIT` when the consuming subpass only reads its own pixel location.
- `vkCmdNextSubpass` transitions between subpasses — the driver inserts appropriate barriers based on your dependency declarations.
- **Dynamic Rendering** (Vulkan 1.3) simplifies the API by removing `VkRenderPass` and `VkFramebuffer`, but loses the subpass abstraction that enables automatic tile-based optimization.
- For mobile-targeted applications, prefer subpasses. For desktop-focused applications, dynamic rendering with explicit barriers is often simpler and equally performant.

---

[<< Previous: Compute Shaders](23-compute-shaders.md) | [Next: Memory Management >>](25-memory-management.md)
