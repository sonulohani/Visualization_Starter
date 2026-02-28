# Chapter 9: Graphics Pipeline and Shaders

[<< Previous: Image Views](08-image-views.md) | [Next: Render Passes & Framebuffers >>](10-renderpasses-framebuffers.md)

---

## The Graphics Pipeline — Big Picture

The graphics pipeline is a sequence of stages that transform **vertices** (3D points) into **pixels** on screen. This is the heart of all real-time rendering.

```
Input Data              Pipeline Stages                    Output
(vertices)                                              (pixels)

 v0 ─┐
 v1 ─┤    ┌───────────┐   ┌─────────┐   ┌──────────┐   ┌───────┐
 v2 ─┼──→ │  Vertex   │──→│ Raster- │──→│ Fragment │──→│ Color │──→ Screen
 v3 ─┤    │  Shader   │   │  izer   │   │  Shader  │   │ Blend │
 v4 ─┤    └───────────┘   └─────────┘   └──────────┘   └───────┘
 v5 ─┘    (per vertex)    (triangles    (per pixel)    (combine
           transform      to fragments)   compute      with existing)
           positions       /pixels        color)
```

### All Pipeline Stages (In Order)

```
┌──────────────────────────────────────────────────────────────────┐
│                     Graphics Pipeline Stages                      │
│                                                                   │
│  ┌─────────────────┐                                             │
│  │ Input Assembler  │  Collects vertex data                      │
│  └────────┬────────┘  (FIXED — you configure it)                │
│           ↓                                                       │
│  ┌─────────────────┐                                             │
│  │  Vertex Shader   │  Transforms each vertex                    │
│  └────────┬────────┘  (PROGRAMMABLE — you write code)           │
│           ↓                                                       │
│  ┌─────────────────┐                                             │
│  │  Tessellation    │  Subdivides geometry                       │
│  └────────┬────────┘  (OPTIONAL, PROGRAMMABLE)                  │
│           ↓                                                       │
│  ┌─────────────────┐                                             │
│  │ Geometry Shader  │  Can add/remove vertices                   │
│  └────────┬────────┘  (OPTIONAL, PROGRAMMABLE, rarely used)     │
│           ↓                                                       │
│  ┌─────────────────┐                                             │
│  │  Rasterization   │  Converts triangles to fragments           │
│  └────────┬────────┘  (FIXED — you configure it)                │
│           ↓                                                       │
│  ┌─────────────────┐                                             │
│  │ Fragment Shader  │  Computes color for each fragment          │
│  └────────┬────────┘  (PROGRAMMABLE — you write code)           │
│           ↓                                                       │
│  ┌─────────────────┐                                             │
│  │  Color Blending  │  Combines new color with existing          │
│  └─────────────────┘  (FIXED — you configure it)                │
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

### Fixed vs Programmable Stages

| Stage | Type | What You Do |
|-------|------|-------------|
| Input Assembler | Fixed | Configure topology (triangles, lines, points) |
| **Vertex Shader** | **Programmable** | **Write GLSL code** to transform vertices |
| Tessellation | Programmable | Write GLSL code (optional, advanced) |
| Geometry Shader | Programmable | Write GLSL code (optional, rarely used) |
| Rasterization | Fixed | Configure culling, polygon mode, depth bias |
| **Fragment Shader** | **Programmable** | **Write GLSL code** to compute pixel colors |
| Color Blending | Fixed | Configure blending equations |

For a basic triangle, we only need: **Vertex Shader** + **Fragment Shader**.

---

## Part 1: Writing Shaders

### What is a Shader?

A shader is a small program that runs on the **GPU** — once per vertex (vertex shader) or once per pixel (fragment shader). Unlike CPU code that runs sequentially, shaders run **massively in parallel**:

```
CPU: Process items one at a time (or a few with threads)
  Item 1 → Item 2 → Item 3 → Item 4

GPU: Process ALL items simultaneously
  Item 1 ─┐
  Item 2 ─┤
  Item 3 ─┼──→ All processed at the same time!
  Item 4 ─┤
  ...      │
  Item 1000┘
```

### GLSL — The Shader Language

We write shaders in **GLSL** (OpenGL Shading Language), then compile them to **SPIR-V** (binary) for Vulkan.

### Vertex Shader (shader.vert)

The vertex shader runs **once per vertex**. Its main job is to output the final screen position:

```glsl
#version 450

// ─── Input: data from our vertex buffer ───
// (For now, we'll hardcode vertices — Chapter 14 adds real vertex buffers)

// Hardcoded triangle positions (in clip space: -1 to +1)
vec2 positions[3] = vec2[](
    vec2( 0.0, -0.5),   // Top center
    vec2( 0.5,  0.5),   // Bottom right
    vec2(-0.5,  0.5)    // Bottom left
);

// Hardcoded colors
vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),   // Red
    vec3(0.0, 1.0, 0.0),   // Green
    vec3(0.0, 0.0, 1.0)    // Blue
);

// ─── Output: sent to the fragment shader ───
layout(location = 0) out vec3 fragColor;

void main() {
    // gl_VertexIndex is a built-in variable: 0, 1, 2 for our 3 vertices
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
```

**Key concepts**:
- `gl_VertexIndex`: Built-in variable — the index of the current vertex (0, 1, 2, ...).
- `gl_Position`: Built-in output — the vertex position in **clip space** (x, y, z, w).
- `layout(location = 0) out`: Declares an output variable that the fragment shader will receive.

### Clip Space Coordinates

```
Clip space (what gl_Position uses):

        (-1, -1) ─────────── (1, -1)
           │                    │
           │     (0, 0)         │
           │       •            │
           │                    │
        (-1, 1) ──────────── (1, 1)

  x: -1 = left,  +1 = right
  y: -1 = top,   +1 = bottom  ← NOTE: Y is flipped vs math convention!
  z: 0 = near,   1 = far
  w: usually 1.0 (used for perspective division)
```

### Fragment Shader (shader.frag)

The fragment shader runs **once per pixel** (technically per "fragment"). It outputs the final color:

```glsl
#version 450

// ─── Input: received from the vertex shader ───
layout(location = 0) in vec3 fragColor;

// ─── Output: the final pixel color ───
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0);  // RGB from vertex shader, alpha = 1.0
}
```

**What happens between vertex and fragment shader?**

The rasterizer **interpolates** the vertex outputs across the triangle surface:

```
Vertex 0 (red)         Vertex 1 (green)
       ╲                  ╱
        ╲   interpolated ╱
         ╲  colors here ╱
          ╲            ╱
           ╲          ╱
            Vertex 2 (blue)

A pixel near vertex 0 → mostly red
A pixel near vertex 1 → mostly green
A pixel in the center → mix of all three (brownish)
```

This is why the classic Vulkan triangle has smooth color gradients!

---

## Part 2: Compiling Shaders to SPIR-V

Vulkan doesn't accept GLSL text — it only understands **SPIR-V** (Standard Portable Intermediate Representation):

```
 shader.vert  ──[glslc]──→  vert.spv
 (human-readable)           (binary, GPU-ready)

 shader.frag  ──[glslc]──→  frag.spv
```

### Compile with glslc

```bash
cd shaders/

# Compile vertex shader
glslc shader.vert -o vert.spv

# Compile fragment shader
glslc shader.frag -o frag.spv

# Verify the files were created
ls -la *.spv
```

### Common Compilation Errors

| Error | Cause | Fix |
|-------|-------|-----|
| `'gl_VertexIndex' undeclared` | Wrong GLSL version | Use `#version 450` |
| `'location' unknown` | Missing `layout` qualifier | Add `layout(location = N)` |
| `type mismatch` | Output/input types don't match | Ensure vert `out` matches frag `in` |

---

## Part 3: Loading Shaders in C++

### Reading SPIR-V Files

```cpp
#include <fstream>

static std::vector<char> readFile(const std::string& filename) {
    // Open at end (ate) in binary mode
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    // tellg() gives position (= file size since we opened at end)
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    // Seek back to beginning and read
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    std::cout << "Loaded shader: " << filename << " (" << fileSize << " bytes)\n";
    return buffer;
}
```

### Creating Shader Modules

A `VkShaderModule` wraps the SPIR-V bytecode:

```cpp
VkShaderModule createShaderModule(const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}
```

**Note**: `pCode` expects `uint32_t*` because SPIR-V is 32-bit word-aligned. Our `char` vector satisfies this because `std::vector` guarantees proper alignment for its element type, and the SPIR-V file itself is word-aligned.

---

## Part 4: Building the Graphics Pipeline

This is the longest piece of setup in Vulkan. The pipeline needs **every aspect** of rendering specified upfront.

```cpp
VkPipelineLayout pipelineLayout;
VkPipeline graphicsPipeline;

void createGraphicsPipeline() {
    // ═══════════════════════════════════════
    //  STEP 1: Load shaders
    // ═══════════════════════════════════════
    auto vertShaderCode = readFile("shaders/vert.spv");
    auto fragShaderCode = readFile("shaders/frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    // Vertex shader stage
    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShaderModule;
    vertStageInfo.pName = "main";  // Entry point function name

    // Fragment shader stage
    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShaderModule;
    fragStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertStageInfo, fragStageInfo
    };

    // ═══════════════════════════════════════
    //  STEP 2: Vertex input (empty — hardcoded in shader for now)
    // ═══════════════════════════════════════
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    // ═══════════════════════════════════════
    //  STEP 3: Input assembly
    // ═══════════════════════════════════════
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ═══════════════════════════════════════
    //  STEP 4: Dynamic state (viewport + scissor change per frame)
    // ═══════════════════════════════════════
    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // ═══════════════════════════════════════
    //  STEP 5: Viewport state
    // ═══════════════════════════════════════
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;  // Set dynamically
    viewportState.scissorCount = 1;   // Set dynamically

    // ═══════════════════════════════════════
    //  STEP 6: Rasterizer
    // ═══════════════════════════════════════
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;       // Discard fragments beyond far plane
    rasterizer.rasterizerDiscardEnable = VK_FALSE; // Don't discard all (for compute-only)
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // Fill triangles (vs wireframe)
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;   // Cull back faces
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; // Which winding = front
    rasterizer.depthBiasEnable = VK_FALSE;          // No depth bias (for shadow mapping)

    // ═══════════════════════════════════════
    //  STEP 7: Multisampling (disabled for now)
    // ═══════════════════════════════════════
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ═══════════════════════════════════════
    //  STEP 8: Color blending
    // ═══════════════════════════════════════
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;  // No blending (overwrite)

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // ═══════════════════════════════════════
    //  STEP 9: Pipeline layout (uniforms, push constants — empty for now)
    // ═══════════════════════════════════════
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // ═══════════════════════════════════════
    //  STEP 10: Create the pipeline!
    // ═══════════════════════════════════════
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr;   // No depth testing yet
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;  // Created in Chapter 10
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                   nullptr, &graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create graphics pipeline!");
    }

    std::cout << "Graphics pipeline created!\n";

    // Shader modules can be destroyed after pipeline creation
    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);
}
```

---

## Understanding Key Pipeline Settings

### Primitive Topology

How vertices are assembled into shapes:

```
TRIANGLE_LIST (most common):
  v0, v1, v2 → triangle 1
  v3, v4, v5 → triangle 2

  v0──v1    v3──v4
   \ │       \ │
    \│        \│
    v2        v5

TRIANGLE_STRIP:
  v0, v1, v2 → triangle 1
  v1, v2, v3 → triangle 2 (reuses 2 vertices!)

  v0──v1
   \ │ \
    \│  \
    v2──v3

LINE_LIST:
  v0 → v1 (line 1)
  v2 → v3 (line 2)

POINT_LIST:
  Each vertex is a point
```

### Polygon Mode

```
FILL:      Solid triangles (normal rendering)
LINE:      Wireframe (just edges) — requires fillModeNonSolid feature
POINT:     Just vertices as dots
```

### Face Culling

Triangles have a **front face** and a **back face** based on vertex winding order:

```
Clockwise winding (front face with our setting):
  v0
  │ ↘
  │   v1
  │  ╱
  v2

Counter-clockwise (back face — culled!):
  v0
  ↙ │
v2  │
  ╲ │
   v1
```

Culling back faces saves ~50% of fragment processing for solid objects.

### Dynamic State

Some pipeline settings can change **without recreating the pipeline**:

```cpp
// These are set dynamically (per command buffer, per frame):
VK_DYNAMIC_STATE_VIEWPORT    // The rendering area
VK_DYNAMIC_STATE_SCISSOR     // The clipping rectangle
VK_DYNAMIC_STATE_LINE_WIDTH  // Line width for LINE polygon mode

// Everything else is baked into the pipeline (immutable)
```

Why make viewport/scissor dynamic? Because the window can be resized. Without dynamic state, you'd need to recreate the entire pipeline on every resize.

### Color Blending

Controls how new fragment colors combine with existing framebuffer colors:

```cpp
// No blending (default — overwrite):
colorBlendAttachment.blendEnable = VK_FALSE;
// Result: new color replaces old color

// Alpha blending (for transparency):
colorBlendAttachment.blendEnable = VK_TRUE;
colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
// Result: finalColor = srcAlpha * srcColor + (1 - srcAlpha) * dstColor
// (standard transparency formula)
```

---

## Cleanup

```cpp
void cleanup() {
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    // ... rest of cleanup ...
}
```

---

## Try It Yourself

1. **Write and compile the shaders**. Verify the `.spv` files are created.

2. **Change the triangle colors** in the vertex shader to yellow, cyan, and magenta. Recompile and see the difference.

3. **Change the triangle shape**: Move the vertices to form a right triangle or an equilateral triangle.

4. **Think about immutability**: Why does Vulkan use immutable pipelines? (Hint: the GPU can optimize when it knows the complete state upfront.)

---

## Key Takeaways

- The graphics pipeline has **fixed** stages (configured) and **programmable** stages (shaders).
- **Vertex shader**: Runs per-vertex, outputs position and data for the fragment shader.
- **Fragment shader**: Runs per-pixel, outputs the final color.
- Shaders are written in **GLSL** and compiled to **SPIR-V** binary.
- The pipeline is **immutable** — all state must be specified at creation time.
- **Dynamic state** (viewport, scissor) can change without recreating the pipeline.
- Shader modules are only needed during pipeline creation and can be destroyed after.

---

[<< Previous: Image Views](08-image-views.md) | [Next: Render Passes & Framebuffers >>](10-renderpasses-framebuffers.md)
