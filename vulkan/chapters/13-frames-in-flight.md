# Chapter 13: Frames in Flight

[<< Previous: Drawing & Sync](12-drawing-sync.md) | [Next: Vertex Buffers >>](14-vertex-buffers.md)

---

## What We've Built So Far

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
✅ Sync Objects       → Fences + semaphores
✅ Draw Loop          → Working triangle!

🎯 Now: Multiple frames in flight + swap chain recreation
```

---

## The Problem: CPU and GPU Take Turns

In Chapter 12, our draw loop works — but it's **painfully inefficient**. Here's why:

```
CURRENT BEHAVIOR (1 frame in flight):

CPU:  [record F0]  [wait........]  [record F1]  [wait........]  [record F2]
GPU:               [render F0   ]               [render F1   ]               [render F2]
                   ↑                             ↑
              CPU is idle here!             CPU is idle here!
              (wasted time)                 (wasted time)
```

The CPU records commands, submits them, then **sits idle waiting** for the GPU to finish before it can record the next frame. Only one side is working at a time.

### A Restaurant Analogy

Think of a restaurant with **one waiter** and **one chef**:

- **Current approach**: The waiter takes an order, hands it to the chef, then **stands idle** until the food is ready. Only then does the waiter go take the next order. The waiter is doing nothing most of the time.

- **Better approach**: While the chef cooks order #1, the waiter is **already taking order #2**. By the time order #1 is plated, order #2 is ready for the kitchen. Both are always busy.

This is exactly what **frames in flight** gives us.

---

## Frames in Flight — Overlapping CPU and GPU Work

The idea is simple: while the GPU renders frame N, the CPU prepares frame N+1. To do this, we need **multiple sets** of synchronization objects and command buffers — one per frame in flight.

```
WITH 2 FRAMES IN FLIGHT:

CPU:  [record F0]  [record F1]  [record F2]  [record F3]  [record F4]
GPU:               [render F0]  [render F1]  [render F2]  [render F3]
                                                          
       ↑            ↑            ↑            ↑
       Frame 0      Frame 1      Frame 0      Frame 1
       resources    resources    resources    resources
       (set 0)      (set 1)      (set 0)      (set 1)

CPU and GPU work simultaneously! Much better throughput.
```

### Why MAX_FRAMES_IN_FLIGHT = 2?

```cpp
const int MAX_FRAMES_IN_FLIGHT = 2;
```

| Value | Behavior |
|-------|----------|
| **1** | CPU and GPU take turns (current behavior, wasteful) |
| **2** | Good overlap; CPU prepares next frame while GPU renders current |
| **3+** | More latency (frames are further ahead of display), marginal throughput gain |

**2 is the sweet spot** — good parallelism with low input latency. Most engines and tutorials use this value.

```
MAX_FRAMES_IN_FLIGHT = 2 means:

  ┌─────────────────────────────────────────────────┐
  │  Set 0                    Set 1                  │
  │  ┌──────────────┐        ┌──────────────┐       │
  │  │ commandBuf[0]│        │ commandBuf[1]│       │
  │  │ imgAvail[0]  │        │ imgAvail[1]  │       │
  │  │ renDone[0]   │        │ renDone[1]   │       │
  │  │ fence[0]     │        │ fence[1]     │       │
  │  └──────────────┘        └──────────────┘       │
  │                                                  │
  │  Frame 0, 2, 4, 6...     Frame 1, 3, 5, 7...   │
  └─────────────────────────────────────────────────┘

We alternate: set 0 → set 1 → set 0 → set 1 → ...
```

---

## Upgrading to Multiple Sync Objects

Previously, we had one of each:

```cpp
// OLD: single set (Chapter 12)
VkSemaphore imageAvailableSemaphore;
VkSemaphore renderFinishedSemaphore;
VkFence inFlightFence;
VkCommandBuffer commandBuffer;
```

Now we need **arrays** — one per frame in flight:

```cpp
// NEW: per-frame sets
std::vector<VkCommandBuffer> commandBuffers;
std::vector<VkSemaphore> imageAvailableSemaphores;
std::vector<VkSemaphore> renderFinishedSemaphores;
std::vector<VkFence> inFlightFences;

uint32_t currentFrame = 0;  // Alternates: 0, 1, 0, 1, ...
```

### Updated Command Buffer Allocation

```cpp
void createCommandBuffers() {
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}
```

### Updated Sync Object Creation

```cpp
void createSyncObjects() {
    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled!

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create synchronization objects for frame " + std::to_string(i));
        }
    }
}
```

---

## Updated drawFrame()

The key change: use `currentFrame` as the index into our arrays, and advance it each frame.

```cpp
void drawFrame() {
    // ═══════════════════════════════════════════════════════
    //  STEP 1: Wait for THIS frame's previous work to finish
    // ═══════════════════════════════════════════════════════
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    // ═══════════════════════════════════════════════════════
    //  STEP 2: Acquire the next swap chain image
    // ═══════════════════════════════════════════════════════
    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
                          imageAvailableSemaphores[currentFrame],
                          VK_NULL_HANDLE, &imageIndex);

    // ═══════════════════════════════════════════════════════
    //  STEP 3: Record commands into THIS frame's command buffer
    // ═══════════════════════════════════════════════════════
    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    // ═══════════════════════════════════════════════════════
    //  STEP 4: Submit
    // ═══════════════════════════════════════════════════════
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // ═══════════════════════════════════════════════════════
    //  STEP 5: Present
    // ═══════════════════════════════════════════════════════
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);

    // ═══════════════════════════════════════════════════════
    //  STEP 6: Advance to the next frame-in-flight index
    // ═══════════════════════════════════════════════════════
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

### Detailed Timeline With 2 Frames in Flight

```
Time ──────────────────────────────────────────────────────────────────►

currentFrame:  0              1              0              1
               │              │              │              │
CPU:    ┌──────┴──────┐┌──────┴──────┐┌──────┴──────┐┌──────┴──────┐
        │ wait fence 0││ wait fence 1││ wait fence 0││ wait fence 1│
        │ acquire img ││ acquire img ││ acquire img ││ acquire img │
        │ record cb 0 ││ record cb 1 ││ record cb 0 ││ record cb 1 │
        │ submit      ││ submit      ││ submit      ││ submit      │
        │ present     ││ present     ││ present     ││ present     │
        └─────────────┘└─────────────┘└─────────────┘└─────────────┘

GPU:           ┌─────────────┐┌─────────────┐┌─────────────┐
               │ render F0   ││ render F1   ││ render F2   │
               │(uses cb 0)  ││(uses cb 1)  ││(uses cb 0)  │
               └──────┬──────┘└──────┬──────┘└──────┬──────┘
                      │              │              │
               signal fence 0  signal fence 1  signal fence 0

         ├── Frame 0 ──┤── Frame 1 ──┤── Frame 2 ──┤── ...

Notice: CPU prepares frame 1 while GPU renders frame 0!
        CPU prepares frame 2 while GPU renders frame 1!
        Much better utilization of both CPU and GPU.
```

---

## Swap Chain Recreation — Handling Window Resizes

Everything above works great until the user **resizes the window**. When that happens, the swap chain images no longer match the window size, and we need to **destroy and recreate** the swap chain and everything that depends on it.

### What Can Trigger Recreation?

| Trigger | How We Detect It |
|---------|-----------------|
| Window resize | `VK_ERROR_OUT_OF_DATE_KHR` from `vkAcquireNextImageKHR` or `vkQueuePresentKHR` |
| Window minimize (size 0) | Framebuffer width/height becomes 0 |
| Suboptimal swap chain | `VK_SUBOPTIMAL_KHR` from present (image still usable but not ideal) |

### The Return Values

```cpp
// vkAcquireNextImageKHR and vkQueuePresentKHR can return:

VK_SUCCESS              // Everything is fine
VK_SUBOPTIMAL_KHR       // Image works, but swap chain doesn't perfectly match surface
VK_ERROR_OUT_OF_DATE_KHR // Swap chain is incompatible — MUST recreate
```

Think of it like wearing clothes:

```
VK_SUCCESS              →  Clothes fit perfectly
VK_SUBOPTIMAL_KHR       →  Clothes still fit but they're a bit tight
                            (works but should update when convenient)
VK_ERROR_OUT_OF_DATE_KHR → Clothes don't fit at all — you grew out of them
                            (MUST get new clothes before going out)
```

---

## The GLFW Framebuffer Resize Callback

GLFW can tell us when the window is resized. We set a flag that our draw function checks:

```cpp
bool framebufferResized = false;

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = reinterpret_cast<HelloTriangleApplication*>(
        glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}
```

Register this callback during window creation:

```cpp
void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Note: we no longer disable resizing!
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  ← REMOVED

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}
```

### Why `glfwSetWindowUserPointer`?

The callback is a static/free function — it doesn't have a `this` pointer. `glfwSetWindowUserPointer` stores our application pointer inside GLFW's window, and `glfwGetWindowUserPointer` retrieves it in the callback. It's like writing your name on a package so anyone who picks it up knows who it belongs to.

```
┌──────────────────────┐
│   GLFW Window        │
│                      │
│  userPointer ────────────► HelloTriangleApplication*
│                      │         (our app instance)
│  resizeCallback ─────────► framebufferResizeCallback()
│                      │         ↓ looks up userPointer
└──────────────────────┘         ↓ sets framebufferResized = true
```

---

## cleanupSwapChain()

Before recreating, we must destroy the old swap chain resources:

```cpp
void cleanupSwapChain() {
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(device, swapChain, nullptr);
}
```

### What Gets Destroyed and Why

```
cleanupSwapChain() destroys:
  ┌─────────────────────────────────────────────────────┐
  │                                                     │
  │  Framebuffers ─────► Depend on swap chain images    │
  │       ↓                                             │
  │  Image Views  ─────► Interpret swap chain images    │
  │       ↓                                             │
  │  Swap Chain   ─────► The images themselves          │
  │                                                     │
  └─────────────────────────────────────────────────────┘

  NOT destroyed (reusable):
  ┌─────────────────────────────────────────────────────┐
  │  Render Pass         → Same rendering process       │
  │  Pipeline            → Same shader/state config     │
  │  Command Pool        → Same allocation pool         │
  │  Sync Objects        → Same synchronization logic   │
  │  Device, Surface     → Same GPU and window          │
  └─────────────────────────────────────────────────────┘
```

---

## recreateSwapChain()

This builds everything back up with the new window dimensions:

```cpp
void recreateSwapChain() {
    // Handle minimization — wait until window has a non-zero size
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();  // Sleep until there's an event (un-minimize)
    }

    // Wait for the GPU to finish using the old swap chain
    vkDeviceWaitIdle(device);

    // Tear down old resources
    cleanupSwapChain();

    // Build new ones with the updated window size
    createSwapChain();
    createImageViews();
    createFramebuffers();
}
```

### The Flow

```
Window resized
      │
      ▼
framebufferResized = true  (GLFW callback sets this)
      │
      ▼
drawFrame() detects it OR gets VK_ERROR_OUT_OF_DATE_KHR
      │
      ▼
recreateSwapChain()
      │
      ├── Handle minimize (wait for non-zero size)
      ├── vkDeviceWaitIdle (GPU finishes current work)
      ├── cleanupSwapChain (destroy old framebuffers, image views, swap chain)
      ├── createSwapChain (new swap chain at new resolution)
      ├── createImageViews (new views for new images)
      └── createFramebuffers (new framebuffers for new image views)
      │
      ▼
Resume rendering at new size!
```

---

## Updated drawFrame() With Swap Chain Recreation

```cpp
void drawFrame() {
    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // Acquire — might fail if swap chain is out of date
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        device, swapChain, UINT64_MAX,
        imageAvailableSemaphores[currentFrame],
        VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swap chain no longer matches window — recreate immediately
        recreateSwapChain();
        return;  // Try again next frame
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    // Only reset the fence if we're actually going to submit work
    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(commandBuffers[currentFrame], 0);
    recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

    // ... submit info setup (same as before) ...
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // Present — also check for out-of-date
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
```

### Why Reset Fence After Acquire (Not Before)?

Notice we moved `vkResetFences` **after** the acquire check. Here's why:

```
WRONG — reset fence first:
  vkWaitForFences → vkResetFences → vkAcquireNextImage returns OUT_OF_DATE
       ↓                                    ↓
  Fence is now UNSIGNALED             We return without submitting
       ↓                                    ↓
  Next frame: vkWaitForFences → DEADLOCK! (fence never gets signaled)

RIGHT — reset fence after successful acquire:
  vkWaitForFences → vkAcquireNextImage returns OUT_OF_DATE → return
       ↓                                    ↓
  Fence is still SIGNALED             We recreate swap chain
       ↓                                    ↓
  Next frame: vkWaitForFences → passes immediately (still signaled)
```

---

## Updated Cleanup

Don't forget to destroy all the per-frame sync objects:

```cpp
void cleanup() {
    cleanupSwapChain();

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyRenderPass(device, renderPass, nullptr);

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

## Complete Synchronization Diagram

Here's how everything connects with 2 frames in flight:

```
┌───────────── FRAME 0 (currentFrame=0) ──────────────┐
│                                                      │
│  CPU:  wait fence[0]                                 │
│             │                                        │
│             ▼                                        │
│        acquire image ──► imageAvailableSemaphore[0]  │
│             │                   │                    │
│             ▼                   │ (GPU waits)        │
│        record cb[0]             │                    │
│             │                   ▼                    │
│        submit cb[0] ──► GPU renders ──► renderFinishedSemaphore[0]
│             │                   │               │    │
│             │                   ▼               │    │
│             │           signal fence[0]         │    │
│             │                                   ▼    │
│             └──────────────────────────── present     │
│                                                      │
└──────────────────────────────────────────────────────┘
            │ (CPU immediately starts frame 1
            │  because it waits on fence[1], not fence[0])
            ▼
┌───────────── FRAME 1 (currentFrame=1) ──────────────┐
│                                                      │
│  CPU:  wait fence[1]  ← already signaled from init!  │
│             │           (or from 2 frames ago)       │
│             ▼                                        │
│        acquire image ──► imageAvailableSemaphore[1]  │
│        ...same pattern...                            │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## Try It Yourself

1. **Resize the window** while the triangle is rendering. It should seamlessly adapt to the new size without crashing.

2. **Minimize the window**, then restore it. The app should pause rendering during minimize and resume smoothly.

3. **Add a frame counter**: Print `currentFrame` in `drawFrame()` and observe it alternating between 0 and 1.

4. **Experiment with MAX_FRAMES_IN_FLIGHT = 1**: Change the value and observe the performance difference. With a simple triangle, you won't see much — but try adding a `std::this_thread::sleep_for` in `drawFrame()` to simulate expensive CPU work and see the difference.

5. **Thought exercise**: What would happen if you set `MAX_FRAMES_IN_FLIGHT = 5`? What about `MAX_FRAMES_IN_FLIGHT = 100`? What's the trade-off between throughput and input latency?

---

## Key Takeaways

- **Frames in flight** let the CPU and GPU work simultaneously by having multiple sets of resources.
- **MAX_FRAMES_IN_FLIGHT = 2** is the standard choice — good overlap without excessive latency.
- Each frame in flight has its own **command buffer**, **semaphores**, and **fence**.
- `currentFrame` alternates between 0 and 1 to select which set of resources to use.
- The **swap chain must be recreated** when the window is resized (or minimized/restored).
- `VK_ERROR_OUT_OF_DATE_KHR` means the swap chain **must** be recreated.
- `VK_SUBOPTIMAL_KHR` means the swap chain still works but **should** be recreated.
- **Don't reset the fence before acquire** — if acquire fails, you'd deadlock.
- `cleanupSwapChain()` destroys framebuffers, image views, and the swap chain.
- `recreateSwapChain()` rebuilds them at the new window resolution.

---

[<< Previous: Drawing & Sync](12-drawing-sync.md) | [Next: Vertex Buffers >>](14-vertex-buffers.md)
