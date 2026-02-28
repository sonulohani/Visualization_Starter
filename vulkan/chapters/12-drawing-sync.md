# Chapter 12: Drawing and Synchronization

[<< Previous: Command Buffers](11-command-buffers.md) | [Next: Frames in Flight >>](13-frames-in-flight.md)

---

## We Can Finally Draw!

We have all the pieces. Let's put them together to render our first triangle.

### What We've Built

```
✅ Instance           → Connection to Vulkan
✅ Validation Layers  → Error checking
✅ Surface            → Window connection
✅ Physical Device    → GPU selected
✅ Logical Device     → GPU interface
✅ Swap Chain         → Presentation images
✅ Image Views        → Image interpretation
✅ Render Pass        → Rendering blueprint
✅ Pipeline           → Complete render configuration
✅ Framebuffers       → Image-to-renderpass bindings
✅ Command Pool       → Command memory manager
✅ Command Buffer     → Command recording

🎯 Now: Connect it all in a draw loop!
```

---

## Synchronization — The Missing Piece

Before we draw, we need to understand **synchronization**. The CPU and GPU run independently. Without explicit coordination, chaos ensues:

```
WITHOUT SYNCHRONIZATION:
  CPU: "Here's frame 1!" → "Here's frame 2!" → "Here's frame 3!"
  GPU: "Still working on frame 1..."  → CRASH (frame 1 data overwritten!)

WITH SYNCHRONIZATION:
  CPU: "Here's frame 1!" → [wait for GPU] → "Here's frame 2!" → [wait] → ...
  GPU: "Working on frame 1..." → "Done!" → "Working on frame 2..." → "Done!"
```

### Vulkan's Synchronization Tools

| Tool | What It Synchronizes | CPU Can Wait? |
|------|---------------------|---------------|
| **Semaphore** | GPU operation A with GPU operation B | No |
| **Fence** | GPU work with CPU code | Yes |

### Semaphores (GPU ↔ GPU)

Semaphores coordinate between GPU operations. The CPU doesn't wait — it just declares the dependency:

```
CPU says: "GPU, don't start rendering until the image is acquired."
          "And don't present until rendering is done."

   Acquire Image ──[imageAvailable semaphore]──→ Render
                                                    │
                                    [renderFinished semaphore]
                                                    │
                                                    ↓
                                                 Present
```

### Fences (GPU → CPU)

Fences let the CPU wait for the GPU:

```
CPU: "Submit work to GPU"
     "Wait on fence..."  (CPU blocks here)
     ...
GPU: "Working... working... done! Signal fence."
CPU: "Fence signaled! GPU is done. Safe to proceed."
```

---

## Creating Synchronization Objects

```cpp
VkSemaphore imageAvailableSemaphore;   // GPU: "image is ready for rendering"
VkSemaphore renderFinishedSemaphore;   // GPU: "rendering is done"
VkFence inFlightFence;                  // CPU: "GPU finished the frame"

void createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled!

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create synchronization objects!");
    }

    std::cout << "Synchronization objects created.\n";
}
```

### Why `VK_FENCE_CREATE_SIGNALED_BIT`?

The first call to `drawFrame()` waits on the fence. If it starts **unsignaled**, the CPU waits forever (deadlock!) because no previous frame has signaled it. Starting signaled avoids this.

```
Frame 1:                                Frame 1 (FIXED):
  vkWaitForFences (unsignaled)            vkWaitForFences (SIGNALED → passes immediately)
  ...waiting forever...                    vkResetFences
  DEADLOCK!                                ...render...
                                           vkQueueSubmit (signals fence)
```

---

## The Draw Frame Function

This is the main rendering function, called every frame:

```cpp
void drawFrame() {
    // ═══════════════════════════════════════════════
    //  STEP 1: Wait for the previous frame to finish
    // ═══════════════════════════════════════════════
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    //                                          │         │
    //                              wait ALL? ──┘         │
    //                              timeout (ns) ─────────┘
    //                              UINT64_MAX = wait forever

    vkResetFences(device, 1, &inFlightFence);  // Reset for next frame

    // ═══════════════════════════════════════════════
    //  STEP 2: Acquire the next swap chain image
    // ═══════════════════════════════════════════════
    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
                          imageAvailableSemaphore,  // Signal this when acquired
                          VK_NULL_HANDLE,           // No fence
                          &imageIndex);

    // imageIndex = which swap chain image to render to (0, 1, or 2)

    // ═══════════════════════════════════════════════
    //  STEP 3: Record commands for this frame
    // ═══════════════════════════════════════════════
    vkResetCommandBuffer(commandBuffer, 0);
    recordCommandBuffer(commandBuffer, imageIndex);

    // ═══════════════════════════════════════════════
    //  STEP 4: Submit the command buffer
    // ═══════════════════════════════════════════════
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // Wait for the image to be available before writing colors
    VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    // The command buffer to execute
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // Signal this semaphore when rendering is done
    VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // Submit! The fence signals when the GPU finishes
    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // ═══════════════════════════════════════════════
    //  STEP 5: Present the rendered image
    // ═══════════════════════════════════════════════
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    // Wait for rendering to finish before presenting
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);
}
```

### The Synchronization Timeline

```
Frame N:

CPU:     [wait fence]  acquire  record  submit        present
              │           │                │              │
GPU:          │           │        [wait semaphore]      [wait semaphore]
              │           │        render render         present
              │           │        render render           │
              │           ↓              │                 │
              │     imageAvailable       │           renderFinished
              │     semaphore            │           semaphore
              │     (GPU signals)        │           (GPU signals)
              │                          ↓
              │                     inFlightFence
              │                     (GPU signals → CPU can proceed)
              │                          │
Frame N+1:    └──────────────────────────┘
              [wait fence] ...
```

---

## The Main Loop

```cpp
void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();  // Handle keyboard/mouse/window events
        drawFrame();
    }

    // Wait for the GPU to finish before cleanup
    vkDeviceWaitIdle(device);
}
```

### Why `vkDeviceWaitIdle`?

When we exit the main loop, the GPU might still be processing the last frame. Destroying Vulkan objects while the GPU is using them causes undefined behavior. `vkDeviceWaitIdle` blocks until all GPU work is done.

---

## You Did It! 🎉

Run the program and you should see:

```
┌────────────────────────────┐
│                            │
│         ▲                  │
│        ╱ ╲                 │
│       ╱   ╲                │
│      ╱ RGB ╲               │
│     ╱  tri  ╲              │
│    ╱─────────╲             │
│                            │
│   (dark background)        │
└────────────────────────────┘
```

A colorful triangle with smooth color gradients! The red, green, and blue vertices blend across the surface.

---

## Cleanup — The Complete Order

```cpp
void cleanup() {
    vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
    vkDestroyFence(device, inFlightFence, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);

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

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}
```

---

## Try It Yourself

1. **Run the complete program!** See the triangle. You've earned it.

2. **Change the clear color** to something bright (bright red, bright blue).

3. **Change the triangle colors** in the shader (recompile!) — try all white, or a rainbow gradient.

4. **Observe the GPU waiting**: Add `std::cout << "Frame!\n";` in `drawFrame()`. How fast is it running?

---

## Key Takeaways

- **Semaphores** synchronize GPU-to-GPU operations (acquire → render, render → present).
- **Fences** synchronize GPU-to-CPU (CPU waits for GPU to finish).
- The draw loop: **Wait** → **Acquire** → **Record** → **Submit** → **Present**.
- Fences start **signaled** to prevent a deadlock on the first frame.
- `vkDeviceWaitIdle()` ensures the GPU is done before cleanup.
- **We have a working triangle!** Everything from here builds on this foundation.

---

[<< Previous: Command Buffers](11-command-buffers.md) | [Next: Frames in Flight >>](13-frames-in-flight.md)
