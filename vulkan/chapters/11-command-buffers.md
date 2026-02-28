# Chapter 11: Command Buffers

[<< Previous: Render Passes](10-renderpasses-framebuffers.md) | [Next: Drawing & Synchronization >>](12-drawing-sync.md)

---

## The Command Model

In Vulkan, you don't call GPU functions directly. Instead, you **record** commands into a **command buffer**, then **submit** the entire buffer to a queue.

### Analogy: The Shopping List

```
OpenGL (shopping one item at a time):
  Drive to store → buy milk → drive home
  Drive to store → buy eggs → drive home
  Drive to store → buy bread → drive home
  (3 trips! Inefficient!)

Vulkan (make a list, one trip):
  Write list: [milk, eggs, bread]
  Drive to store → buy everything → drive home
  (1 trip! Efficient!)
```

The GPU works the same way. Sending commands one at a time has overhead (like driving to the store each time). Batching them into a command buffer and submitting them all at once is much faster.

---

## Command Pools

A **command pool** is a memory manager for command buffers. It allocates the memory that command buffers use to store their commands.

### Rules

- Each command pool is tied to a **single queue family**.
- Command buffers from a pool can only be submitted to queues of that family.
- Pools are **not thread-safe** by default — use one pool per thread.

```
Thread 0                    Thread 1
┌─────────────┐            ┌─────────────┐
│ Command     │            │ Command     │
│ Pool A      │            │ Pool B      │
│  ├── CmdBuf0│            │  ├── CmdBuf0│
│  └── CmdBuf1│            │  └── CmdBuf1│
└──────┬──────┘            └──────┬──────┘
       │                          │
       └──────── Both submit to ──┘
                 Graphics Queue
```

### Creating a Command Pool

```cpp
VkCommandPool commandPool;

void createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    // Allow individual command buffers to be re-recorded
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    // This pool allocates command buffers for the graphics queue
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    std::cout << "Command pool created for queue family "
              << queueFamilyIndices.graphicsFamily.value() << "\n";
}
```

### Pool Flags Explained

| Flag | Effect |
|------|--------|
| `RESET_COMMAND_BUFFER_BIT` | Command buffers can be individually re-recorded |
| `TRANSIENT_BIT` | Command buffers are short-lived (hints for memory optimization) |
| (none) | Command buffers cannot be reset individually (reset the whole pool instead) |

---

## Allocating Command Buffers

Command buffers are **allocated** from a pool (not created — the pool manages their memory):

```cpp
VkCommandBuffer commandBuffer;

void createCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffer!");
    }

    std::cout << "Command buffer allocated.\n";
}
```

### Primary vs Secondary Command Buffers

| Level | Can Submit to Queue? | Called From | Use Case |
|-------|---------------------|------------|----------|
| **Primary** | Yes | Queue directly | Main command recording |
| **Secondary** | No | Primary buffer only | Multi-threaded recording |

Secondary buffers are used for parallel recording:

```
Thread 0: Record Secondary Buffer A (shadows)
Thread 1: Record Secondary Buffer B (geometry)
Thread 2: Record Secondary Buffer C (particles)

Main Thread: Primary Buffer
  ├── vkCmdExecuteCommands(A)
  ├── vkCmdExecuteCommands(B)
  └── vkCmdExecuteCommands(C)
  Then submit Primary Buffer to queue
```

For now, we'll use only primary buffers.

---

## Recording Commands

Recording is the process of writing GPU commands into the buffer:

```cpp
void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // ═══════════════════════════════════════
    //  Begin recording
    // ═══════════════════════════════════════
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;       // Optional flags
    beginInfo.pInheritanceInfo = nullptr;  // Only for secondary buffers

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    // ═══════════════════════════════════════
    //  Begin the render pass
    // ═══════════════════════════════════════
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];

    // Render area (usually the full framebuffer)
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapChainExtent;

    // Clear color (dark blue — looks nice!)
    VkClearValue clearColor = {{{0.01f, 0.01f, 0.03f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // ═══════════════════════════════════════
    //  Bind the graphics pipeline
    // ═══════════════════════════════════════
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    // ═══════════════════════════════════════
    //  Set dynamic viewport and scissor
    // ═══════════════════════════════════════
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapChainExtent.width);
    viewport.height = static_cast<float>(swapChainExtent.height);
    viewport.minDepth = 0.0f;  // Depth range
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // ═══════════════════════════════════════
    //  DRAW!
    // ═══════════════════════════════════════
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    //                       │  │  │  │
    //         vertexCount ──┘  │  │  │
    //         instanceCount ───┘  │  │
    //         firstVertex ────────┘  │
    //         firstInstance ─────────┘

    // ═══════════════════════════════════════
    //  End render pass and recording
    // ═══════════════════════════════════════
    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}
```

### Viewport vs Scissor

```
Viewport (transforms coordinates):
┌─────────────────────────────┐
│                             │
│    ┌─────────────────┐      │
│    │  Rendered scene  │      │  Viewport defines WHERE and HOW BIG
│    │  (stretched to   │      │  the rendering appears
│    │   fill this area)│      │
│    └─────────────────┘      │
│                             │
└─────────────────────────────┘

Scissor (clips pixels):
┌─────────────────────────────┐
│  xxxxxxxxxxxxxxxxxxxxxxxx   │
│  x┌─────────────────┐xxx   │  Scissor defines which pixels
│  x│  Only these      │xxx   │  are actually written
│  x│  pixels are      │xxx   │  (everything outside is discarded)
│  x│  kept             │xxx   │
│  x└─────────────────┘xxx   │
│  xxxxxxxxxxxxxxxxxxxxxxxx   │
└─────────────────────────────┘

Usually both cover the full framebuffer.
```

### vkCmdDraw Parameters

```cpp
vkCmdDraw(commandBuffer,
    3,    // vertexCount — draw 3 vertices (our triangle)
    1,    // instanceCount — 1 instance (no instancing)
    0,    // firstVertex — start from vertex 0
    0     // firstInstance — start from instance 0
);
```

---

## Command Buffer Lifecycle

```
┌──────────────┐
│   Initial    │  Just allocated — can't record yet
└──────┬───────┘
       │ vkBeginCommandBuffer()
       ↓
┌──────────────┐
│  Recording   │  Recording commands (vkCmd* functions)
└──────┬───────┘
       │ vkEndCommandBuffer()
       ↓
┌──────────────┐
│  Executable  │  Ready to submit to a queue
└──────┬───────┘
       │ vkQueueSubmit()
       ↓
┌──────────────┐
│   Pending    │  GPU is processing it
└──────┬───────┘
       │ GPU finishes (fence signals)
       ↓
┌──────────────┐
│  Executable  │  Can submit again, or reset to record new commands
└──────┬───────┘
       │ vkResetCommandBuffer() or vkBeginCommandBuffer()
       ↓
┌──────────────┐
│   Initial    │  Ready to record again
└──────────────┘
```

---

## Common `vkCmd*` Functions

| Function | What It Does |
|----------|-------------|
| `vkCmdBeginRenderPass` | Start a render pass |
| `vkCmdEndRenderPass` | End the render pass |
| `vkCmdBindPipeline` | Set the active pipeline |
| `vkCmdSetViewport` | Set the viewport (dynamic state) |
| `vkCmdSetScissor` | Set the scissor rectangle (dynamic state) |
| `vkCmdDraw` | Draw vertices (no index buffer) |
| `vkCmdDrawIndexed` | Draw with an index buffer |
| `vkCmdBindVertexBuffers` | Bind vertex data |
| `vkCmdBindIndexBuffer` | Bind index data |
| `vkCmdBindDescriptorSets` | Bind uniforms/textures |
| `vkCmdPushConstants` | Push small data to shaders |
| `vkCmdCopyBuffer` | Copy between buffers |
| `vkCmdPipelineBarrier` | Synchronize within a command buffer |
| `vkCmdDispatch` | Launch compute shader work |

---

## Try It Yourself

1. **Create the command pool and buffer**. Verify they're created successfully.

2. **Try different clear colors**: Modify the `clearColor` in `recordCommandBuffer`:
   - Pure red: `{1.0f, 0.0f, 0.0f, 1.0f}`
   - Cornflower blue: `{0.39f, 0.58f, 0.93f, 1.0f}`
   - Dark green: `{0.0f, 0.2f, 0.0f, 1.0f}`

3. **Draw 6 vertices instead of 3**: What happens? (With our hardcoded shader, vertices 3-5 don't exist, so you'll get undefined behavior.)

---

## Key Takeaways

- Vulkan uses **command buffers** to batch GPU commands for efficient submission.
- **Command pools** manage memory for command buffers and are tied to a queue family.
- Commands are **recorded** (begin → commands → end), then **submitted** to a queue.
- **Viewport** transforms the rendering coordinates; **scissor** clips pixels.
- `vkCmdDraw(buffer, vertexCount, instanceCount, firstVertex, firstInstance)` is the draw call.
- We re-record command buffers **every frame** (with `RESET_COMMAND_BUFFER_BIT`).

---

[<< Previous: Render Passes](10-renderpasses-framebuffers.md) | [Next: Drawing & Synchronization >>](12-drawing-sync.md)
