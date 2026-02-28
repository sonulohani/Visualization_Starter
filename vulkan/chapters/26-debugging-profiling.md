# Chapter 26: Debugging and Profiling

[<< Previous: Memory Management](25-memory-management.md) | [Next: Best Practices >>](27-best-practices.md)

---

## Why Debugging Vulkan Is Hard

Vulkan gives you maximum control over the GPU — but that also means maximum opportunity for subtle bugs. A single wrong image layout, a missing barrier, or an off-by-one in a descriptor binding can produce:

- A completely black screen (no error, no crash — just... nothing)
- Flickering artifacts on some GPUs but not others
- Corruption that only appears on the 47th frame
- Crashes inside the driver with no helpful message

```
Debugging difficulty spectrum:

  Easy ◄──────────────────────────────────────────────► Hard

  printf()    GDB/LLDB    Validation    RenderDoc    GPU hang
  debugging   breakpoints  layers       frame        debugging
                                        capture      (driver crash,
                                                     no output)
```

This chapter gives you the tools and techniques to track down even the nastiest Vulkan bugs.

---

## Part 1: RenderDoc — Your GPU Debugger

### What Is RenderDoc?

**RenderDoc** is a free, open-source GPU frame debugger. It captures a single frame of your application and lets you inspect **every single GPU operation**: every draw call, every texture, every buffer, every pipeline state, every pixel.

```
┌────────────────────────────────────────────────────────────────────┐
│                        RenderDoc Workflow                          │
│                                                                    │
│   ┌──────────┐     ┌───────────┐     ┌───────────────────────┐   │
│   │ Your App │────→│ RenderDoc │────→│ Frame Capture (.rdc)  │   │
│   │ running  │     │ intercepts│     │                       │   │
│   │          │     │ all Vulkan│     │ Contains:             │   │
│   │          │ F12 │ calls     │     │ • Every API call      │   │
│   │          │────→│           │     │ • All buffer data     │   │
│   └──────────┘     └───────────┘     │ • All texture data    │   │
│                                      │ • Pipeline state      │   │
│                                      │ • Shader bytecode     │   │
│                                      └───────────────────────┘   │
│                                               │                   │
│                                               ↓                   │
│                                      ┌───────────────────────┐   │
│                                      │ RenderDoc UI          │   │
│                                      │                       │   │
│                                      │ • Event Browser       │   │
│                                      │ • Texture Viewer      │   │
│                                      │ • Pipeline State      │   │
│                                      │ • Mesh Viewer         │   │
│                                      │ • Shader Debugger     │   │
│                                      └───────────────────────┘   │
└────────────────────────────────────────────────────────────────────┘
```

### Analogy

RenderDoc is like a **DVR for your GPU**. Just as a DVR records a TV broadcast so you can pause, rewind, and examine each frame, RenderDoc records one frame of GPU work so you can step through every draw call and see exactly what happened.

### Installing RenderDoc

Download from [renderdoc.org](https://renderdoc.org). Available for Windows, Linux, and Android.

On Ubuntu/Debian:

```bash
sudo apt install renderdoc
```

### Capturing a Frame

**Method 1: Launch through RenderDoc**

1. Open RenderDoc
2. Go to **File → Launch Application**
3. Set the **Executable Path** to your Vulkan app
4. Set the **Working Directory** to your project folder
5. Click **Launch**
6. Press **F12** (or **Print Screen**) to capture a frame

**Method 2: Programmatic capture (more control)**

```cpp
#include "renderdoc_app.h"

RENDERDOC_API_1_1_2* rdoc_api = nullptr;

void initRenderDoc() {
    if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rdoc_api);
    }
}

void captureFrame() {
    if (rdoc_api) {
        rdoc_api->StartFrameCapture(nullptr, nullptr);
        // ... render one frame ...
        rdoc_api->EndFrameCapture(nullptr, nullptr);
    }
}
```

### Navigating a RenderDoc Capture

Once you have a capture, here's what you can inspect:

```
┌─────────────────────────────────────────────────────────────────────────┐
│ RenderDoc UI Layout                                                     │
│                                                                         │
│ ┌─────────────────────┐  ┌──────────────────────────────────────────┐  │
│ │  Event Browser      │  │  Texture Viewer                          │  │
│ │  (left panel)       │  │  (center panel)                          │  │
│ │                     │  │                                           │  │
│ │  ▼ Frame Start      │  │  ┌─────────────────────────────────────┐ │  │
│ │  ▼ Begin Render Pass│  │  │                                     │ │  │
│ │    ├─ vkCmdBind...  │  │  │  Shows the render target at the    │ │  │
│ │    ├─ vkCmdDraw(3)  │  │  │  selected event — you can see      │ │  │
│ │    ├─ vkCmdBind...  │──│──│  exactly what each draw call adds  │ │  │
│ │    ├─ vkCmdDraw(36) │  │  │  to the image!                     │ │  │
│ │    └─ vkCmdDraw(12) │  │  │                                     │ │  │
│ │  ▼ End Render Pass  │  │  └─────────────────────────────────────┘ │  │
│ │  ▼ Present          │  │                                           │  │
│ └─────────────────────┘  └──────────────────────────────────────────┘  │
│                                                                         │
│ ┌─────────────────────────────────────────────────────────────────────┐ │
│ │  Pipeline State (bottom panel)                                      │ │
│ │                                                                     │ │
│ │  Vertex Input │ Vertex Shader │ Rasterizer │ Fragment Shader │ ...  │ │
│ │  ─────────────┼───────────────┼────────────┼─────────────────┼───  │ │
│ │  Shows every pipeline setting for the selected draw call            │ │
│ └─────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```

### Key RenderDoc Panels

| Panel | What It Shows | Use It When |
|-------|--------------|-------------|
| **Event Browser** | Every Vulkan command in the frame | Finding which draw call renders (or doesn't render) something |
| **Texture Viewer** | Any image/attachment at any point in the frame | Checking if a texture loaded correctly, if depth is correct |
| **Pipeline State** | Complete pipeline configuration for selected draw | Verifying blend state, depth test, cull mode, shaders |
| **Mesh Viewer** | Vertex data in/out of vertex shader | Debugging vertex positions, checking transforms |
| **Shader Debugger** | Step through shader code line by line | Finding why a pixel is the wrong color |
| **Resource Inspector** | All buffers, images, samplers | Checking buffer contents, image formats, sizes |

### Common RenderDoc Investigations

**"My screen is black"** — Step through events in the Event Browser. Click each draw call and watch the Texture Viewer. Find where rendering stops or never starts.

**"Wrong colors"** — Click the pixel in the Texture Viewer, then use the Shader Debugger to step through the fragment shader and see what values it computed.

**"Objects in wrong position"** — Use the Mesh Viewer to check vertex positions before and after the vertex shader. Check if the MVP matrix in the uniform buffer has correct values.

**"Texture looks wrong"** — Use the Resource Inspector to view the texture data. Check format, dimensions, mip levels, and actual pixel values.

---

## Part 2: Debug Labels and Object Names

Validation layer messages and RenderDoc captures are much more useful when your Vulkan objects have **human-readable names**.

### Naming Objects

Without names, a validation error looks like:

```
VUID-vkCmdDraw-None-02699: VkPipeline 0x00000000000000ab is not
compatible with VkRenderPass 0x00000000000000cd, subpass index 0.
```

With names, the same error becomes:

```
VUID-vkCmdDraw-None-02699: VkPipeline "Main Scene Pipeline" is not
compatible with VkRenderPass "Shadow Map Pass", subpass index 0.
```

Use `vkSetDebugUtilsObjectNameEXT` to name any Vulkan object:

```cpp
void setDebugName(VkDevice device, VkObjectType objectType,
                  uint64_t objectHandle, const char* name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = objectType;
    nameInfo.objectHandle = objectHandle;
    nameInfo.pObjectName = name;

    vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
}
```

Call it right after creating each object:

```cpp
vkCreateGraphicsPipelines(device, cache, 1, &pipelineInfo, nullptr, &pipeline);
setDebugName(device, VK_OBJECT_TYPE_PIPELINE,
             (uint64_t)pipeline, "Main Scene Pipeline");

vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
setDebugName(device, VK_OBJECT_TYPE_RENDER_PASS,
             (uint64_t)renderPass, "Forward Render Pass");

vkCreateImageView(device, &viewInfo, nullptr, &depthImageView);
setDebugName(device, VK_OBJECT_TYPE_IMAGE_VIEW,
             (uint64_t)depthImageView, "Depth Buffer View");

vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
for (size_t i = 0; i < commandBuffers.size(); i++) {
    std::string name = "Frame " + std::to_string(i) + " Command Buffer";
    setDebugName(device, VK_OBJECT_TYPE_COMMAND_BUFFER,
                 (uint64_t)commandBuffers[i], name.c_str());
}
```

### Common Object Types to Name

| Object | VkObjectType | Suggested Name Format |
|--------|-------------|----------------------|
| VkPipeline | `VK_OBJECT_TYPE_PIPELINE` | `"PBR Forward Pipeline"` |
| VkRenderPass | `VK_OBJECT_TYPE_RENDER_PASS` | `"Shadow Map Pass"` |
| VkBuffer | `VK_OBJECT_TYPE_BUFFER` | `"Vertex Buffer - Character"` |
| VkImage | `VK_OBJECT_TYPE_IMAGE` | `"Albedo Texture - Wood"` |
| VkDescriptorSet | `VK_OBJECT_TYPE_DESCRIPTOR_SET` | `"Material Set #3"` |
| VkCommandBuffer | `VK_OBJECT_TYPE_COMMAND_BUFFER` | `"Frame 0 Cmd Buffer"` |
| VkFramebuffer | `VK_OBJECT_TYPE_FRAMEBUFFER` | `"Swapchain FB #2"` |
| VkSampler | `VK_OBJECT_TYPE_SAMPLER` | `"Linear Repeat Sampler"` |

### Debug Labels (Command Buffer Regions)

You can group commands inside a command buffer into labeled **regions** that appear in RenderDoc:

```cpp
void beginDebugLabel(VkCommandBuffer cmd, const char* name,
                     float r, float g, float b)
{
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = r;
    label.color[1] = g;
    label.color[2] = b;
    label.color[3] = 1.0f;

    vkCmdBeginDebugUtilsLabelEXT(cmd, &label);
}

void endDebugLabel(VkCommandBuffer cmd) {
    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void insertDebugLabel(VkCommandBuffer cmd, const char* name) {
    VkDebugUtilsLabelEXT label{};
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName = name;
    label.color[0] = 1.0f;
    label.color[1] = 1.0f;
    label.color[2] = 1.0f;
    label.color[3] = 1.0f;

    vkCmdInsertDebugUtilsLabelEXT(cmd, &label);
}
```

Use them to structure your render loop:

```cpp
void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    beginDebugLabel(cmd, "Shadow Pass", 0.5f, 0.0f, 0.5f);
    {
        // ... render shadow map ...
    }
    endDebugLabel(cmd);

    beginDebugLabel(cmd, "Main Scene", 0.0f, 0.5f, 0.0f);
    {
        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        beginDebugLabel(cmd, "Opaque Objects", 0.0f, 0.8f, 0.0f);
        {
            // ... draw opaque geometry ...
            insertDebugLabel(cmd, "Character Draw");
            vkCmdDraw(cmd, vertexCount, 1, 0, 0);
        }
        endDebugLabel(cmd);

        beginDebugLabel(cmd, "Transparent Objects", 0.0f, 0.0f, 0.8f);
        {
            // ... draw transparent geometry ...
        }
        endDebugLabel(cmd);

        vkCmdEndRenderPass(cmd);
    }
    endDebugLabel(cmd);

    beginDebugLabel(cmd, "Post Processing", 1.0f, 0.5f, 0.0f);
    {
        // ... bloom, tone mapping, etc. ...
    }
    endDebugLabel(cmd);

    vkEndCommandBuffer(cmd);
}
```

In RenderDoc, this produces a beautiful hierarchical view:

```
Event Browser (with debug labels):
  
  ▼ Shadow Pass                    ← purple
  │   vkCmdBindPipeline
  │   vkCmdBindDescriptorSets
  │   vkCmdDraw(1024)
  │
  ▼ Main Scene                     ← green
  │  ▼ Opaque Objects              ← bright green
  │  │   vkCmdBindPipeline
  │  │   • Character Draw          ← label marker
  │  │   vkCmdDraw(3672)
  │  │   vkCmdDraw(5184)
  │  │
  │  ▼ Transparent Objects         ← blue
  │      vkCmdBindPipeline
  │      vkCmdDraw(128)
  │
  ▼ Post Processing                ← orange
      vkCmdBindPipeline
      vkCmdDraw(6)
```

Without labels, you'd see a flat list of hundreds of unlabeled Vulkan calls — trying to find your bug in that is like searching a phone book with no names.

---

## Part 3: GPU Timestamps — Measuring GPU Performance

CPU-side timing (`std::chrono`) tells you how long the CPU spent recording commands — but **not how long the GPU spent executing them**. For GPU timing, you need **timestamp queries**.

### Analogy

Imagine you're a manager sending work orders to a factory (GPU). Measuring how long it takes you to *write* the orders (CPU time) doesn't tell you how long the factory takes to *complete* them. You need the factory to stamp each order with its completion time.

### Setting Up a Timestamp Query Pool

```cpp
VkQueryPool timestampQueryPool;
const uint32_t TIMESTAMP_COUNT = 16;

void createTimestampQueryPool() {
    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolInfo.queryCount = TIMESTAMP_COUNT;

    if (vkCreateQueryPool(device, &queryPoolInfo, nullptr,
                          &timestampQueryPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create timestamp query pool!");
    }
}
```

### Recording Timestamps

Place timestamps at the beginning and end of the GPU work you want to measure:

```cpp
void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Reset queries for this frame
    vkCmdResetQueryPool(cmd, timestampQueryPool, 0, TIMESTAMP_COUNT);

    // Timestamp 0: start of frame
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        timestampQueryPool, 0);

    // --- Shadow pass ---
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        timestampQueryPool, 1);
    // ... render shadow map ...
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        timestampQueryPool, 2);

    // --- Main scene ---
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        timestampQueryPool, 3);
    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    // ... draw scene ...
    vkCmdEndRenderPass(cmd);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        timestampQueryPool, 4);

    // Timestamp 5: end of frame
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                        timestampQueryPool, 5);

    vkEndCommandBuffer(cmd);
}
```

```
Timeline of timestamp placement:

  Query Index:    0        1              2    3              4        5
                  │        │              │    │              │        │
  GPU Timeline:  ─┼────────┼──────────────┼────┼──────────────┼────────┼──
                  │        │ Shadow Pass  │    │  Main Scene  │        │
                  │        │◄────────────►│    │◄────────────►│        │
                  │        │              │    │              │        │
                  │◄──────────────── Total Frame ──────────────────────►│
```

### Reading Timestamp Results

Read back the timestamps **the next frame** (the GPU is still executing the current frame's commands):

```cpp
void readTimestamps() {
    uint64_t timestamps[TIMESTAMP_COUNT];

    VkResult result = vkGetQueryPoolResults(
        device, timestampQueryPool,
        0, 6,
        sizeof(timestamps), timestamps,
        sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
    );

    if (result != VK_SUCCESS) return;

    // Get the timestamp period (nanoseconds per tick)
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    float timestampPeriod = props.limits.timestampPeriod;

    auto ticksToMs = [&](uint64_t start, uint64_t end) -> float {
        return (end - start) * timestampPeriod / 1'000'000.0f;
    };

    float shadowMs = ticksToMs(timestamps[1], timestamps[2]);
    float sceneMs  = ticksToMs(timestamps[3], timestamps[4]);
    float totalMs  = ticksToMs(timestamps[0], timestamps[5]);

    std::cout << "GPU Timing:\n";
    std::cout << "  Shadow pass: " << shadowMs << " ms\n";
    std::cout << "  Main scene:  " << sceneMs << " ms\n";
    std::cout << "  Total frame: " << totalMs << " ms\n";

    if (totalMs > 16.67f) {
        std::cout << "  ⚠ Frame exceeds 16.67ms budget (60 FPS target)\n";
    }
}
```

```
Example output:
  GPU Timing:
    Shadow pass: 0.42 ms
    Main scene:  2.18 ms
    Total frame: 3.05 ms
```

### Important Caveats

| Caveat | Details |
|--------|---------|
| **timestampPeriod varies** | NVIDIA: 1.0 ns/tick, AMD: varies, Intel: varies — always use the device property |
| **Not all queues support timestamps** | Check `VkQueueFamilyProperties::timestampValidBits > 0` |
| **Results are from the previous frame** | The GPU hasn't finished the current frame yet when you read |
| **TOP_OF_PIPE vs BOTTOM_OF_PIPE** | `TOP_OF_PIPE` = before work begins, `BOTTOM_OF_PIPE` = after all work completes |
| **Timestamp overflow** | 64-bit timestamps can overflow — check `timestampValidBits` for the valid range |

### Building a Simple GPU Profiler

Here's a reusable profiler class that wraps the boilerplate:

```cpp
class GpuProfiler {
public:
    void init(VkDevice device, VkPhysicalDevice physDevice, uint32_t maxTimers) {
        this->device = device;
        this->maxTimers = maxTimers;

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physDevice, &props);
        timestampPeriod = props.limits.timestampPeriod;

        VkQueryPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        poolInfo.queryCount = maxTimers * 2;
        vkCreateQueryPool(device, &poolInfo, nullptr, &queryPool);

        results.resize(maxTimers * 2);
        names.resize(maxTimers);
    }

    void reset(VkCommandBuffer cmd) {
        vkCmdResetQueryPool(cmd, queryPool, 0, maxTimers * 2);
        currentTimer = 0;
    }

    void beginTimer(VkCommandBuffer cmd, const std::string& name) {
        names[currentTimer] = name;
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            queryPool, currentTimer * 2);
    }

    void endTimer(VkCommandBuffer cmd) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                            queryPool, currentTimer * 2 + 1);
        currentTimer++;
    }

    void readResults() {
        if (currentTimer == 0) return;

        vkGetQueryPoolResults(device, queryPool, 0, currentTimer * 2,
                              results.size() * sizeof(uint64_t), results.data(),
                              sizeof(uint64_t),
                              VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

        for (uint32_t i = 0; i < currentTimer; i++) {
            float ms = (results[i*2+1] - results[i*2]) * timestampPeriod / 1e6f;
            timings[names[i]] = ms;
        }
    }

    float getMs(const std::string& name) const {
        auto it = timings.find(name);
        return it != timings.end() ? it->second : 0.0f;
    }

    void destroy() {
        vkDestroyQueryPool(device, queryPool, nullptr);
    }

private:
    VkDevice device;
    VkQueryPool queryPool;
    float timestampPeriod;
    uint32_t maxTimers;
    uint32_t currentTimer = 0;
    std::vector<uint64_t> results;
    std::vector<std::string> names;
    std::unordered_map<std::string, float> timings;
};
```

Usage:

```cpp
GpuProfiler profiler;
profiler.init(device, physicalDevice, 8);

// In recordCommandBuffer:
profiler.reset(cmd);

profiler.beginTimer(cmd, "Shadow");
// ... shadow pass ...
profiler.endTimer(cmd);

profiler.beginTimer(cmd, "Scene");
// ... main scene ...
profiler.endTimer(cmd);

// After frame completes (next frame):
profiler.readResults();
std::cout << "Shadow: " << profiler.getMs("Shadow") << " ms\n";
std::cout << "Scene: "  << profiler.getMs("Scene") << " ms\n";
```

---

## Part 4: Common Debugging Strategies

### Strategy 1: Decoding Validation Layer Messages

Validation messages have a standard format. Learning to read them saves hours:

```
validation layer: Validation Error:
  [ VUID-vkCmdDraw-renderPass-02684 ]
  Object 0: handle = 0x55a4c8d37e40, type = VK_OBJECT_TYPE_COMMAND_BUFFER;
  Object 1: handle = 0x9a00000000009a, type = VK_OBJECT_TYPE_PIPELINE;
  | MessageID = 0x45f5abc5 |
  vkCmdDraw(): The current render pass is not compatible with the
  renderPass used to create VkPipeline 0x9a00000000009a.
```

Breaking it down:

```
┌─────────────────────────────────────────────────────────────────────┐
│ Anatomy of a Validation Error                                       │
│                                                                     │
│ VUID-vkCmdDraw-renderPass-02684                                    │
│ ────  ──────── ──────────  ─────                                   │
│  │       │         │         │                                      │
│  │       │         │         └── Unique error number                │
│  │       │         └── Related parameter                            │
│  │       └── Function that triggered the error                      │
│  └── "Validation Usage ID" — searchable in the Vulkan spec          │
│                                                                     │
│ Search this VUID at:                                                │
│   https://registry.khronos.org/vulkan/specs/latest/html/            │
│                                                                     │
│ The message text tells you what's wrong:                            │
│   "render pass is not compatible with renderPass used to create     │
│    VkPipeline" → You're using a pipeline created for a different    │
│    render pass than the one currently active.                       │
└─────────────────────────────────────────────────────────────────────┘
```

### Strategy 2: Systematic Isolation

When something is wrong and you don't know where, **isolate each piece**:

```
The "Binary Search" debugging method:

  Full frame broken? Is it...
  │
  ├── Geometry problem?
  │     → Replace complex mesh with a single triangle
  │     → If triangle renders, the mesh data is wrong
  │
  ├── Shader problem?
  │     → Output solid red: fragColor = vec4(1,0,0,1)
  │     → If red appears, vertex positions are OK
  │
  ├── Descriptor problem?
  │     → Remove all descriptor bindings
  │     → Use push constants for the MVP matrix
  │     → If it works, descriptors were misconfigured
  │
  ├── Synchronization problem?
  │     → Add vkDeviceWaitIdle() after every submit
  │     → If it fixes flickering, you're missing synchronization
  │
  └── Render pass problem?
        → Simplify to single subpass, single attachment
        → If it works, your multi-pass setup has an issue
```

### Strategy 3: Clear Color Debugging

Change the clear color to verify that rendering is happening at all:

```cpp
VkClearValue clearValues[2]{};

// Bright magenta — impossible to miss!
clearValues[0].color = {{1.0f, 0.0f, 1.0f, 1.0f}};
clearValues[1].depthStencil = {1.0f, 0};
```

If you see the clear color: the render pass is working, the problem is in your draw calls.

If the screen is black: the render pass or swap chain presentation is broken.

### Strategy 4: Debug Fragment Shader Outputs

Temporarily modify your fragment shader to output diagnostic information:

```glsl
// Output the normal as a color (should look like a colorful sphere)
fragColor = vec4(normalize(fragNormal) * 0.5 + 0.5, 1.0);

// Output the UV coordinates (should be a red-green gradient)
fragColor = vec4(fragTexCoord, 0.0, 1.0);

// Output the depth (should be a grayscale gradient)
float d = gl_FragCoord.z;
fragColor = vec4(d, d, d, 1.0);

// Output a checkerboard to verify UVs and screen coordinates
float checker = mod(floor(fragTexCoord.x * 10.0) +
                    floor(fragTexCoord.y * 10.0), 2.0);
fragColor = vec4(vec3(checker), 1.0);
```

```
What each debug output should look like:

  Normal output:        UV output:          Depth output:
  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
  │  ╭──────╮    │    │ ■■■■■■■■▓▓▓▓│    │ ▒▒▒▒▒▒░░░░░░│
  │ ╭┤Purple│Green   │ ■■■■■■■■▓▓▓▓│    │ ▒▒▒▒░░░░░░░░│
  │ │╰──────╯    │    │ ▓▓▓▓▓▓▓▓████│    │ ▒▒░░░░░░    │
  │ ╰──Blue──╯   │    │ ▓▓▓▓▓▓▓▓████│    │ ░░░░        │
  └──────────────┘    └──────────────┘    └──────────────┘
  Smooth color         Red/green          Black=near,
  = normals OK         gradient = UVs OK  White=far
```

### Strategy 5: GPU Crash Debugging

When the GPU hangs or crashes, you get no error message. Here are techniques:

```cpp
// Technique 1: Breadcrumb markers
// Write incrementing values to a buffer from the GPU.
// After a crash, read the buffer to see the last marker that was written.

// Technique 2: Device lost diagnostics (VK_EXT_device_fault)
// If supported, provides info about what the GPU was doing when it crashed.

// Technique 3: Reduce workload
// Comment out draw calls one by one until the crash goes away.
// The last draw call you commented out is the culprit.

// Technique 4: Enable GPU-assisted validation
VkValidationFeaturesEXT validationFeatures{};
validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;

VkValidationFeatureEnableEXT enables[] = {
    VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
    VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_RESERVE_BINDING_SLOT_EXT
};
validationFeatures.enabledValidationFeatureCount = 2;
validationFeatures.pEnabledValidationFeatures = enables;

// Chain into VkInstanceCreateInfo:
instanceCreateInfo.pNext = &validationFeatures;
```

---

## Debugging Checklist

When something goes wrong, work through this systematically:

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Vulkan Debugging Checklist                        │
│                                                                     │
│  □ 1. Check validation layer output                                 │
│       Any errors? Any warnings? Fix ALL of them.                    │
│                                                                     │
│  □ 2. Verify clear color is visible                                 │
│       Set clear to bright magenta. See it? Render pass works.       │
│                                                                     │
│  □ 3. Check vertex positions                                        │
│       Output fragColor = vec4(1,0,0,1). See red? Geometry is OK.   │
│                                                                     │
│  □ 4. Check viewport and scissor                                    │
│       Are they set? Do they match the swapchain extent?             │
│                                                                     │
│  □ 5. Check descriptor sets                                         │
│       Are they bound? Correct set/binding? Correct buffer/image?    │
│                                                                     │
│  □ 6. Capture in RenderDoc                                          │
│       Inspect the draw call. Check Pipeline State tab.              │
│                                                                     │
│  □ 7. Check synchronization                                         │
│       Add vkDeviceWaitIdle after submit. Does it fix the issue?     │
│                                                                     │
│  □ 8. Check image layouts                                           │
│       Transitions must happen before use. Check barriers.           │
│                                                                     │
│  □ 9. Simplify                                                      │
│       Remove features until it works. Add back one at a time.       │
│                                                                     │
│  □ 10. Check the Vulkan spec for the VUID                           │
│        Search the exact VUID string for the rule you violated.      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Try It Yourself

1. **Name all your Vulkan objects**: Write a helper function `setDebugName` and call it after every `vkCreate*` call. Trigger a validation error intentionally and see the improved error message with object names.

2. **Add debug labels to your command buffer**: Group your render pass commands into labeled regions (e.g., "Clear", "Draw Triangle", "Present"). Capture a frame in RenderDoc and verify the labels appear in the Event Browser.

3. **Implement GPU timestamps**: Create a query pool with 4 timestamps. Measure the time before and after your draw calls. Print the result in milliseconds. Is your frame under the 16.67ms budget for 60 FPS?

4. **Debug a black screen**: Intentionally break something (e.g., set the viewport width to 0, or bind the wrong descriptor set). Then use the debugging checklist above to systematically find and fix the bug.

5. **Capture and explore in RenderDoc**: Launch your application through RenderDoc. Capture a frame. Find your triangle in the Event Browser, inspect its vertex data in the Mesh Viewer, and check the pipeline state. Take a screenshot of the Texture Viewer showing your rendered output.

---

## Key Takeaways

- **RenderDoc** is the essential GPU debugging tool — it captures every draw call, buffer, texture, and pipeline state in a frame. Learn it well; it will save you countless hours.
- **Name every Vulkan object** with `vkSetDebugUtilsObjectNameEXT` — it transforms cryptic hex handles into readable names in validation messages and RenderDoc.
- **Debug labels** (`vkCmdBeginDebugUtilsLabelEXT` / `vkCmdEndDebugUtilsLabelEXT`) organize your command buffers into a hierarchy visible in RenderDoc.
- **GPU timestamps** with `VkQueryPool` measure actual GPU execution time — essential for profiling since CPU timing doesn't reflect GPU work.
- Convert timestamp ticks to milliseconds using `timestampPeriod` from the physical device properties.
- **Validation layer messages** contain a searchable VUID that links directly to the Vulkan spec rule you violated.
- When debugging, use **systematic isolation**: simplify everything, verify each piece works, then add complexity back one piece at a time.
- **Clear color debugging** (bright magenta) instantly tells you whether your render pass is executing at all.
- **Fragment shader debug outputs** (normals-as-color, UV visualization, depth visualization) help diagnose rendering issues without any external tools.

---

## What We've Built So Far

```
✅ Instance, Validation, Debug Messenger
✅ Physical Device, Logical Device, Queues
✅ Surface, Swap Chain, Image Views
✅ Render Pass, Graphics Pipeline, Shaders
✅ Framebuffers, Command Pool, Command Buffers
✅ Synchronization, Frames in Flight
✅ Vertex Buffers, Index Buffers, Staging Buffers
✅ Uniform Buffers, Descriptor Sets
✅ Texture Mapping, Depth Buffering
✅ Model Loading, Mipmaps, Multisampling
✅ Dynamic Rendering, Push Constants, Compute Shaders
✅ Advanced Render Passes
✅ Memory Management with VMA
✅ Debugging & Profiling                  ← NEW!
⬜ Best Practices & What's Next
```

---

[<< Previous: Memory Management](25-memory-management.md) | [Next: Best Practices >>](27-best-practices.md)
