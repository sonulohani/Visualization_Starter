# Chapter 10: Render Passes and Framebuffers

[<< Previous: Pipeline & Shaders](09-pipeline-shaders.md) | [Next: Command Buffers >>](11-command-buffers.md)

---

## What is a Render Pass?

A render pass describes the **structure** of a rendering operation — not the commands themselves, but the "blueprint."

### Analogy: A Recipe Card

Think of a render pass as a **recipe card**:

```
Recipe: "Chocolate Cake"
─────────────────────────
Ingredients needed:        → Attachments (images you'll use)
  - Flour (modified)         - Color buffer (written to)
  - Eggs (consumed)          - Depth buffer (used then discarded)

Steps:                     → Subpasses
  1. Mix ingredients         1. Geometry pass
  2. Bake                    2. Lighting pass

Step dependencies:         → Subpass Dependencies
  "Step 2 needs Step 1       "Lighting needs geometry
   to be complete"            results to be written"
```

The recipe card doesn't make the cake — it describes how to make it. Similarly, a render pass describes how rendering works but doesn't actually render anything.

---

## Attachments

An **attachment** is an image that participates in rendering. There are three types:

| Attachment Type | Purpose | Example |
|----------------|---------|---------|
| **Color** | The image you render to (what you see on screen) | Swap chain image |
| **Depth/Stencil** | Stores depth info for 3D occlusion | Depth buffer |
| **Resolve** | Multisampled image resolved to single-sample | MSAA resolve target |

### Attachment Operations

For each attachment, you specify what happens at the **start** and **end** of the render pass:

```
Start of render pass (loadOp):
  CLEAR      → Fill with a clear color (most common for first pass)
  LOAD       → Keep whatever was there before
  DONT_CARE  → Contents are undefined (fastest — when you'll overwrite everything)

End of render pass (storeOp):
  STORE      → Keep the results (you want to display or use them later)
  DONT_CARE  → Discard results (e.g., depth buffer after rendering)
```

Visual example:

```
loadOp = CLEAR:          loadOp = LOAD:           loadOp = DONT_CARE:
┌─────────────┐          ┌─────────────┐          ┌─────────────┐
│             │          │ Previous    │          │ ??????????  │
│  Black/     │          │ frame's     │          │ Garbage     │
│  Clear color│          │ contents    │          │ data        │
│             │          │             │          │             │
└─────────────┘          └─────────────┘          └─────────────┘
```

---

## Creating a Simple Render Pass

For our triangle, we need one color attachment (the swap chain image):

```cpp
VkRenderPass renderPass;

void createRenderPass() {
    // ═══════════════════════════════════════
    //  Describe the color attachment
    // ═══════════════════════════════════════
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainImageFormat;    // Must match swap chain
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;  // No multisampling

    // What to do at the start: clear to a solid color
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

    // What to do at the end: store (we want to display it!)
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    // We don't use stencil
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    // Layout transitions
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;       // Don't care about previous content
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;   // Ready for display

    // ═══════════════════════════════════════
    //  Create a reference to use in our subpass
    // ═══════════════════════════════════════
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;  // Index in the attachments array
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // ═══════════════════════════════════════
    //  Define the subpass
    // ═══════════════════════════════════════
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    // ═══════════════════════════════════════
    //  Subpass dependency
    // ═══════════════════════════════════════
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;  // Before the render pass
    dependency.dstSubpass = 0;                     // Our subpass (index 0)
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // ═══════════════════════════════════════
    //  Create the render pass
    // ═══════════════════════════════════════
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }

    std::cout << "Render pass created!\n";
}
```

---

## Image Layouts Explained

Images must be in the right **layout** for each operation. Think of it as "configuring" the memory for optimal access:

```
┌───────────────────────────────────────────────────────┐
│               Image Layout Transitions                 │
│                                                        │
│  UNDEFINED                                             │
│    │  (contents don't matter — starting fresh)        │
│    ↓                                                   │
│  COLOR_ATTACHMENT_OPTIMAL                              │
│    │  (optimized for rendering to)                    │
│    ↓                                                   │
│  PRESENT_SRC_KHR                                       │
│    │  (optimized for display on screen)               │
│    ↓                                                   │
│  Screen! 🖥️                                            │
└───────────────────────────────────────────────────────┘
```

| Layout | Optimized For |
|--------|--------------|
| `UNDEFINED` | Nothing — image contents may be discarded |
| `COLOR_ATTACHMENT_OPTIMAL` | Being rendered to as a color target |
| `DEPTH_STENCIL_ATTACHMENT_OPTIMAL` | Being used as a depth/stencil buffer |
| `SHADER_READ_ONLY_OPTIMAL` | Being sampled in a shader (texture) |
| `TRANSFER_SRC_OPTIMAL` | Being copied FROM |
| `TRANSFER_DST_OPTIMAL` | Being copied TO |
| `PRESENT_SRC_KHR` | Being shown on screen |

Layout transitions happen automatically within render passes (via `initialLayout` and `finalLayout`).

---

## Subpass Dependencies

The dependency tells Vulkan **when** it's safe to start the render pass:

```
VK_SUBPASS_EXTERNAL (operations before the render pass)
  │
  │  srcStageMask:  COLOR_ATTACHMENT_OUTPUT_BIT
  │  srcAccessMask: 0 (nothing to wait for)
  │
  ↓  "Don't start writing to the color attachment..."
  
Subpass 0 (our rendering)
  │
  │  dstStageMask:  COLOR_ATTACHMENT_OUTPUT_BIT
  │  dstAccessMask: COLOR_ATTACHMENT_WRITE_BIT
  │
  ↓  "...until the swap chain image is ready for writing."
```

Without this dependency, Vulkan might try to render to the swap chain image before it's been acquired from the presentation engine.

---

## What is a Framebuffer?

A **framebuffer** connects **specific images** to the attachment slots defined in the render pass.

### Analogy

If the render pass is a recipe card that says "needs a bowl," the framebuffer is "this specific blue bowl on the counter."

```
Render Pass: "I need 1 color attachment"
                     │
Framebuffer 0: "Use swap chain image view 0"
Framebuffer 1: "Use swap chain image view 1"
Framebuffer 2: "Use swap chain image view 2"
```

We need **one framebuffer per swap chain image** because each frame renders to a different image.

---

## Creating Framebuffers

```cpp
std::vector<VkFramebuffer> swapChainFramebuffers;

void createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {
            swapChainImageViews[i]    // This goes into attachment slot 0
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;     // Must match the render pass
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;  // Single layer (not stereo)

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr,
                                &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }

    std::cout << "Created " << swapChainFramebuffers.size() << " framebuffers.\n";
}
```

### Framebuffer → Render Pass Compatibility

The framebuffer must be **compatible** with the render pass:
- Same number of attachments
- Matching formats
- Matching sample counts

---

## Updated Initialization Order

```cpp
void initVulkan() {
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();          // Before pipeline (pipeline needs render pass)
    createGraphicsPipeline();    // After render pass
    createFramebuffers();        // After render pass + image views
}
```

## Cleanup Order

```cpp
void cleanup() {
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
    vkDestroyDevice(device, nullptr);
    // ... debug messenger, surface, instance, GLFW ...
}
```

---

## Try It Yourself

1. **Create the render pass and framebuffers**. Verify the counts match.

2. **Change the clear color**: We'll set this when recording commands. For now, think about what colors to try (black, cornflower blue, dark green).

3. **Think ahead**: We now have a pipeline and framebuffers. What's the last piece we need before we can actually render? (Command buffers!)

---

## Key Takeaways

- A **render pass** is a blueprint describing what images are used and how.
- **Attachments** are the images involved (color, depth, stencil).
- **Load/store operations** control what happens at the start/end of rendering.
- **Image layouts** must match the operation being performed — transitions happen automatically in render passes.
- **Subpass dependencies** ensure operations happen in the right order.
- A **framebuffer** binds specific image views to attachment slots.
- One framebuffer per swap chain image (they all use the same render pass).

---

[<< Previous: Pipeline & Shaders](09-pipeline-shaders.md) | [Next: Command Buffers >>](11-command-buffers.md)
