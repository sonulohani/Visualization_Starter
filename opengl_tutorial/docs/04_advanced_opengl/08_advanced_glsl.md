# Advanced GLSL

By now you're quite comfortable writing GLSL shaders — vertex shaders that transform positions, fragment shaders that compute colors. But GLSL has several powerful features we haven't explored yet. In this chapter we'll look at built-in variables that give you access to useful rendering state, interface blocks for organizing shader inputs and outputs, and Uniform Buffer Objects (UBOs) that let you share uniform data across multiple shaders efficiently.

---

## Built-in Variables

GLSL provides a number of built-in variables that expose information about the current rendering state. We've already used `gl_Position` extensively, but there are several more worth knowing.

### Vertex Shader Built-ins

#### gl_Position

We've set this in every vertex shader — it's the clip-space position of the current vertex. Nothing new here, but it's worth noting that this is the *only* required output of a vertex shader.

#### gl_PointSize

When rendering with `GL_POINTS`, each vertex is drawn as a single point. By default, each point is 1 pixel. You can control the size from within the vertex shader:

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    gl_PointSize = 10.0;  // each point is 10 pixels wide
}
```

For this to take effect, you need to enable programmable point sizes on the C++ side:

```cpp
glEnable(GL_PROGRAM_POINT_SIZE);
```

You can make point sizes vary — for example, scale them by distance for a simple particle effect:

```glsl
gl_PointSize = 50.0 / length(gl_Position.xyz);
```

Points farther from the camera appear smaller. This is a quick and dirty way to add depth perception to point-based rendering.

#### gl_VertexID

This read-only variable contains the index of the vertex currently being processed. When using `glDrawElements`, it holds the index from the element buffer. When using `glDrawArrays`, it's the index of the vertex since the start of the draw call (0, 1, 2, ...).

This can be useful for procedural effects — for instance, coloring vertices differently based on their index:

```glsl
float hue = float(gl_VertexID) / totalVertices;
```

### Fragment Shader Built-ins

#### gl_FragCoord

This is one of the most useful built-in variables. `gl_FragCoord` is a `vec4` containing the window-space (screen-space) coordinates of the current fragment:

- `gl_FragCoord.x`: horizontal pixel position (0.0 at left edge)
- `gl_FragCoord.y`: vertical pixel position (0.0 at bottom edge)
- `gl_FragCoord.z`: the depth value of the fragment (0.0 to 1.0)

Since `gl_FragCoord.x` and `gl_FragCoord.y` give you the pixel position on screen, you can use them for all kinds of screen-space effects.

**Example: Split-screen effect**

Let's make the left half of the screen tinted red and the right half tinted blue:

```glsl
#version 330 core
out vec4 FragColor;

in vec3 ourColor;

void main()
{
    if (gl_FragCoord.x < 400.0)
        FragColor = vec4(ourColor * vec3(1.5, 0.5, 0.5), 1.0);  // warm tint
    else
        FragColor = vec4(ourColor * vec3(0.5, 0.5, 1.5), 1.0);  // cool tint
}
```

```
Screen (800 × 600):
┌──────────────────┬──────────────────┐
│                  │                  │
│   Warm (red)     │   Cool (blue)    │
│   tinted         │   tinted         │
│                  │                  │
│  gl_FragCoord.x  │  gl_FragCoord.x  │
│   < 400          │   >= 400         │
│                  │                  │
└──────────────────┴──────────────────┘
       x = 0              x = 800
```

#### gl_FrontFacing

This is a `bool` that tells you whether the current fragment belongs to a **front face** or a **back face** of a triangle. It's `true` for front-facing and `false` for back-facing primitives.

Which face is "front" depends on the winding order of the triangle's vertices. By default, OpenGL considers counter-clockwise vertices as front-facing (you can change this with `glFrontFace`).

This is useful for rendering different materials on each side of a surface — for example, a playing card with different front and back images, or a leaf with different colors on each side:

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
uniform sampler2D frontTexture;
uniform sampler2D backTexture;

void main()
{
    if (gl_FrontFacing)
        FragColor = texture(frontTexture, TexCoords);
    else
        FragColor = texture(backTexture, TexCoords);
}
```

Without face culling enabled, both faces of each triangle are rendered, and `gl_FrontFacing` lets you distinguish between them.

#### gl_FragDepth

While `gl_FragCoord.z` lets you *read* the depth value of the current fragment, `gl_FragDepth` lets you *write* a custom depth value:

```glsl
gl_FragDepth = 0.0;  // force this fragment to the near plane
```

If you don't write to `gl_FragDepth`, OpenGL uses the default value from `gl_FragCoord.z`. But if you write to it *anywhere* in the shader, you **must** write to it in every execution path — otherwise the depth value is undefined for fragments that don't reach your assignment.

> **Warning:** Writing to `gl_FragDepth` disables **early depth testing**. Normally, OpenGL can skip fragment shader execution for fragments that would fail the depth test. But if your shader might change the depth, OpenGL can't know in advance whether the fragment will pass, so it must run the shader for every fragment. This can significantly hurt performance.

In OpenGL 4.2+, you can mitigate this with a depth condition layout:

```glsl
layout (depth_greater) out float gl_FragDepth;
```

This promises the driver that you'll only increase the depth (never decrease it), which allows some early depth test optimizations to remain active. Options include `depth_any`, `depth_greater`, `depth_less`, and `depth_unchanged`.

---

## Interface Blocks

As shaders grow complex, you end up with many `in`/`out` variables passed between stages. Interface blocks let you group them into named structs, making code cleaner and more organized:

```glsl
// --- Vertex Shader ---
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} vs_out;

void main()
{
    vs_out.FragPos = vec3(model * vec4(aPos, 1.0));
    vs_out.Normal = mat3(transpose(inverse(model))) * aNormal;
    vs_out.TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
}
```

```glsl
// --- Fragment Shader ---
#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

void main()
{
    // Use fs_in.FragPos, fs_in.Normal, fs_in.TexCoords
    FragColor = vec4(fs_in.Normal * 0.5 + 0.5, 1.0);
}
```

The **block name** (`VS_OUT`) must match between stages, but the **instance name** (`vs_out`, `fs_in`) can differ. The instance name is what you use to access the members.

Interface blocks have several advantages:

1. **Organization** — Related variables are grouped together instead of scattered as individual declarations.
2. **Matching** — It's easier to ensure that the vertex shader's outputs match the fragment shader's inputs when they're in a named block.
3. **Required for geometry shaders** — When a geometry shader sits between the vertex and fragment stages, interface blocks are essential for passing data through cleanly (as we'll see in the next chapter).

---

## Uniform Buffer Objects (UBOs)

### The Problem

Consider a typical scene with multiple objects, each using a different shader. Every shader needs the same `projection` and `view` matrices:

```cpp
// For every shader in the scene:
glUseProgram(shaderA);
glUniformMatrix4fv(glGetUniformLocation(shaderA, "projection"), 1, GL_FALSE,
                   glm::value_ptr(projection));
glUniformMatrix4fv(glGetUniformLocation(shaderA, "view"), 1, GL_FALSE,
                   glm::value_ptr(view));

glUseProgram(shaderB);
glUniformMatrix4fv(glGetUniformLocation(shaderB, "projection"), 1, GL_FALSE,
                   glm::value_ptr(projection));
glUniformMatrix4fv(glGetUniformLocation(shaderB, "view"), 1, GL_FALSE,
                   glm::value_ptr(view));

// ... repeat for shaderC, shaderD, ...
```

This is repetitive and error-prone. With 10 shaders, you're making 20 nearly identical calls. Wouldn't it be nice to set these values once and have all shaders read from the same place?

### The Solution: Uniform Buffer Objects

A **Uniform Buffer Object (UBO)** is a buffer that stores uniform data. You bind it to a **binding point**, and any shader that declares a matching uniform block will read from that buffer automatically.

```
┌──────────────┐       Binding Point 0      ┌──────────────────┐
│  Shader A    │──────────────────────────▶  │  UBO (Matrices)  │
│  Shader B    │──────────────────────────▶  │  ┌────────────┐  │
│  Shader C    │──────────────────────────▶  │  │ projection │  │
│  Shader D    │──────────────────────────▶  │  │ view       │  │
└──────────────┘                             │  └────────────┘  │
                                             └──────────────────┘
```

All four shaders read `projection` and `view` from the same buffer. You update the buffer once, and every shader sees the new values.

### Step 1: Declare a Uniform Block in GLSL

In each shader that needs the shared matrices, add a uniform block:

```glsl
#version 330 core

layout (std140) uniform Matrices
{
    mat4 projection;
    mat4 view;
};

uniform mat4 model;  // still a regular uniform — different per object

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

The `layout (std140)` qualifier specifies the **memory layout** of the block. More on this below.

### Step 2: Understanding std140 Layout

The `std140` layout defines exactly how data is arranged in memory. Without it, the driver might pack members in any order, making it impossible to fill the buffer correctly from C++.

The key rules for std140:

| Type | Base Alignment | Size |
|---|---|---|
| `float`, `int`, `bool` | 4 bytes | 4 bytes |
| `vec2` | 8 bytes | 8 bytes |
| `vec3` | 16 bytes | 12 bytes |
| `vec4` | 16 bytes | 16 bytes |
| `mat4` | 16 bytes per column | 64 bytes (4 × vec4) |
| Array of `T` | Round up to `vec4` alignment (16 bytes) per element | — |

Each member's offset must be a **multiple of its base alignment**. If it isn't, padding bytes are inserted before it.

Let's trace through an example uniform block:

```glsl
layout (std140) uniform ExampleBlock
{                        // Base Alignment   // Offset
    float value;         // 4                // 0
    vec3 vector;         // 16               // 16  (not 4! padded to multiple of 16)
    mat4 matrix;         // 16               // 32
    float values[3];     // 16 (per element) // 96  (each element rounded to 16)
    bool boolean;        // 4                // 144 (3*16=48, 96+48=144)
    int integer;         // 4                // 148
};                       // Total: 152 bytes
```

Let's walk through why each offset is what it is:

- `float value` has base alignment 4. It starts at offset 0. Size: 4 bytes. Next free offset: 4.
- `vec3 vector` has base alignment 16. The next available multiple of 16 after offset 4 is **16**. So 12 bytes of padding are inserted. Size: 12 bytes. Next free offset: 28.
- `mat4 matrix` is treated as 4 `vec4` columns, each with base alignment 16. Next multiple of 16 after 28 is **32**. Size: 64 bytes. Next free offset: 96.
- `float values[3]` — each array element is rounded up to `vec4` alignment (16 bytes per element). Starts at 96 (already aligned). Size: 3 × 16 = 48 bytes. Next free offset: 144.
- `bool boolean` has alignment 4. 144 is a multiple of 4. Size: 4 bytes. Next free: 148.
- `int integer` has alignment 4. 148 is... wait, 148 is a multiple of 4. Size: 4. Next free: 152.

> **Note:** The `vec3` alignment is the most common gotcha. Even though a `vec3` is only 12 bytes, its alignment is 16 bytes. This means you almost always want to use `vec4` instead of `vec3` in uniform blocks to avoid confusion.

### Step 3: Create and Fill the UBO

On the C++ side:

```cpp
unsigned int uboMatrices;
glGenBuffers(1, &uboMatrices);
glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
glBindBuffer(GL_UNIFORM_BUFFER, 0);

// Bind the UBO to binding point 0
glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatrices, 0, 2 * sizeof(glm::mat4));
```

`glBindBufferRange` binds the buffer to binding point 0, specifying the offset and size of the range we want to expose. `glBindBufferBase` is a simpler version that binds the entire buffer:

```cpp
glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboMatrices);
```

Now fill in the data:

```cpp
// Set projection (offset 0)
glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                        800.0f / 600.0f, 0.1f, 100.0f);
glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4),
                glm::value_ptr(projection));

// Set view (offset = sizeof(mat4) = 64 bytes)
glm::mat4 view = camera.GetViewMatrix();
glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4),
                glm::value_ptr(view));
glBindBuffer(GL_UNIFORM_BUFFER, 0);
```

### Step 4: Link Shader Blocks to the Binding Point

Each shader needs to know which binding point its `Matrices` block maps to:

```cpp
// Get the index of the uniform block in each shader
unsigned int blockIndexA = glGetUniformBlockIndex(shaderA, "Matrices");
unsigned int blockIndexB = glGetUniformBlockIndex(shaderB, "Matrices");
unsigned int blockIndexC = glGetUniformBlockIndex(shaderC, "Matrices");
unsigned int blockIndexD = glGetUniformBlockIndex(shaderD, "Matrices");

// Link each shader's block to binding point 0
glUniformBlockBinding(shaderA, blockIndexA, 0);
glUniformBlockBinding(shaderB, blockIndexB, 0);
glUniformBlockBinding(shaderC, blockIndexC, 0);
glUniformBlockBinding(shaderD, blockIndexD, 0);
```

Now all four shaders read `projection` and `view` from the same UBO at binding point 0.

### The Full Picture

Here's how the pieces connect:

```
C++ Side                          GLSL Side
─────────                         ─────────
                                  layout (std140) uniform Matrices {
  UBO Buffer (128 bytes)              mat4 projection;  ◄── offset 0
  ┌────────────────────┐              mat4 view;        ◄── offset 64
  │ projection (64 B)  │          };
  │ view       (64 B)  │
  └────────────────────┘
         │
  glBindBufferBase(..., 0, ubo)
         │
    Binding Point 0  ◄──────────  glUniformBlockBinding(shader, blockIdx, 0)
```

---

## Complete Example: Four Cubes, Four Shaders, One UBO

Let's create a program that renders four cubes, each with a different shader (different solid colors), but all sharing the same projection and view matrices via a UBO.

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// All shaders share this vertex shader structure with a Matrices uniform block
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

layout (std140) uniform Matrices
{
    mat4 projection;
    mat4 view;
};

uniform mat4 model;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

// Four fragment shaders, each outputting a different color
const char* fragShaderRed = R"(
#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(1.0, 0.2, 0.2, 1.0); }
)";

const char* fragShaderGreen = R"(
#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(0.2, 1.0, 0.2, 1.0); }
)";

const char* fragShaderBlue = R"(
#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(0.2, 0.2, 1.0, 1.0); }
)";

const char* fragShaderYellow = R"(
#version 330 core
out vec4 FragColor;
void main() { FragColor = vec4(1.0, 1.0, 0.2, 1.0); }
)";

unsigned int compileShader(const char* vertSrc, const char* fragSrc)
{
    unsigned int vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertSrc, NULL);
    glCompileShader(vert);

    unsigned int frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fragSrc, NULL);
    glCompileShader(frag);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    glDeleteShader(vert);
    glDeleteShader(frag);
    return prog;
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "Advanced GLSL - UBO Demo", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Compile four shader programs
    unsigned int shaders[4];
    const char* fragSources[] = {
        fragShaderRed, fragShaderGreen, fragShaderBlue, fragShaderYellow
    };
    for (int i = 0; i < 4; i++)
        shaders[i] = compileShader(vertexShaderSource, fragSources[i]);

    // Link each shader's Matrices uniform block to binding point 0
    for (int i = 0; i < 4; i++)
    {
        unsigned int blockIndex = glGetUniformBlockIndex(shaders[i], "Matrices");
        glUniformBlockBinding(shaders[i], blockIndex, 0);
    }

    // Create the UBO
    unsigned int uboMatrices;
    glGenBuffers(1, &uboMatrices);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Bind UBO to binding point 0
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboMatrices);

    // Set up cube VAO
    float cubeVertices[] = {
        // positions (we only need positions for solid-color cubes)
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,   0.5f,  0.5f, -0.5f,   0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,   0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,   0.5f, -0.5f, -0.5f,   0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,  -0.5f, -0.5f,  0.5f,  -0.5f, -0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,   0.5f,  0.5f, -0.5f,   0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,  -0.5f,  0.5f,  0.5f,  -0.5f,  0.5f, -0.5f
    };

    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices,
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Cube positions: 2×2 grid
    glm::vec3 cubePositions[] = {
        glm::vec3(-1.2f,  1.2f, 0.0f),  // top-left (red)
        glm::vec3( 1.2f,  1.2f, 0.0f),  // top-right (green)
        glm::vec3(-1.2f, -1.2f, 0.0f),  // bottom-left (blue)
        glm::vec3( 1.2f, -1.2f, 0.0f)   // bottom-right (yellow)
    };

    // Set projection once (it rarely changes)
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4),
                    glm::value_ptr(projection));
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Update view matrix each frame (orbiting camera)
        float time = (float)glfwGetTime();
        float radius = 8.0f;
        glm::mat4 view = glm::lookAt(
            glm::vec3(sin(time) * radius, 2.0f, cos(time) * radius),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        // Update view in the UBO (offset = sizeof(mat4))
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4),
                        glm::value_ptr(view));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        // Draw each cube with its own shader
        glBindVertexArray(cubeVAO);
        for (int i = 0; i < 4; i++)
        {
            glUseProgram(shaders[i]);

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);
            model = glm::rotate(model, time * glm::radians(45.0f),
                                glm::vec3(0.5f, 1.0f, 0.0f));

            int modelLoc = glGetUniformLocation(shaders[i], "model");
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &uboMatrices);
    for (int i = 0; i < 4; i++)
        glDeleteProgram(shaders[i]);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
```

When you run this, you'll see four cubes — red, green, blue, and yellow — orbiting gently as the camera circles them. The key insight: we only set the projection and view matrices **once** (in the UBO), and all four shaders pick them up automatically. The only per-object uniform is the model matrix.

---

## Key Takeaways

- **gl_PointSize** controls point sprite size in the vertex shader. Enable it with `glEnable(GL_PROGRAM_POINT_SIZE)`.
- **gl_FragCoord** gives the screen-space position and depth of a fragment — useful for screen-space effects.
- **gl_FrontFacing** distinguishes front faces from back faces, letting you apply different materials to each side.
- **gl_FragDepth** lets you override the depth value, but disables early depth testing (use depth layout qualifiers in OpenGL 4.2+ to mitigate).
- **Interface blocks** (`out VS_OUT { ... }`) group in/out variables for cleaner shader code and are required when using geometry shaders.
- **Uniform Buffer Objects** store shared uniform data in a GPU buffer. All shaders bound to the same binding point read from the same data.
- **std140 layout** defines precise memory alignment rules. Remember: `vec3` aligns to 16 bytes, and array elements are padded to 16-byte boundaries.
- A single UBO update replaces N redundant `glUniform` calls across N shaders.

## Exercises

1. Use `gl_FragCoord` to create a **checkerboard** pattern overlay: darken pixels where `(floor(gl_FragCoord.x / 20.0) + floor(gl_FragCoord.y / 20.0))` is odd. Apply this to the 4-cube demo.
2. Modify the demo to disable face culling and use `gl_FrontFacing` to render front faces with the assigned color and back faces in a darker shade.
3. Add a third matrix to the UBO — a `mat4 lightSpaceMatrix`. Update the GLSL block, the buffer allocation, and the `glBufferSubData` calls. Verify the std140 offset manually.
4. Experiment with `gl_PointSize`: render a field of `GL_POINTS` where each point's size varies based on `sin(gl_VertexID * 0.1)`. Share projection/view via UBO.
5. (Challenge) Create a uniform block with mixed types (`float`, `vec3`, `mat4`, `float[4]`, `bool`) and manually calculate the std140 offsets. Verify by reading back the buffer with `glMapBuffer(GL_UNIFORM_BUFFER, GL_READ_ONLY)`.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Advanced Data](07_advanced_data.md) | | [Next: Geometry Shader →](09_geometry_shader.md) |
