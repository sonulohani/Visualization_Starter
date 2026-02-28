# Hello Triangle

This is arguably the most important chapter in this entire tutorial. Drawing a single triangle in modern OpenGL requires understanding several fundamental concepts: how vertex data is organized, how it gets sent to the GPU, how shaders process it, and how OpenGL keeps track of all the configuration. Once you truly understand everything in this chapter, every subsequent topic builds naturally on top of it.

Take your time here. Read it twice if you need to.

---

## Normalized Device Coordinates (NDC)

Before we define any vertex data, we need to understand the coordinate system OpenGL uses *after* the vertex shader runs.

OpenGL expects all visible coordinates to fall within a specific range called **Normalized Device Coordinates (NDC)**:

```
          Y
          ▲
     1.0  │
          │
          │    Visible Area
          │
   ───────┼───────▶ X
  -1.0    │    1.0
          │
          │
    -1.0  │
```

- **X axis**: -1.0 (left) to 1.0 (right)
- **Y axis**: -1.0 (bottom) to 1.0 (top)
- **Z axis**: -1.0 (near) to 1.0 (far)

Anything outside this range is **clipped** — discarded and not rendered.

These NDC coordinates are then mapped to your screen via `glViewport`. For a 800×600 viewport:

| NDC          | Screen Pixel   |
|--------------|----------------|
| (-1.0, -1.0) | (0, 0)        |
| (1.0, 1.0)   | (800, 600)    |
| (0.0, 0.0)   | (400, 300)    |

> In later chapters, we'll use transformation matrices to go from "world coordinates" (whatever units make sense for your scene) to NDC. For now, we define vertices directly in NDC.

## Defining Vertex Data

Let's define a triangle using three vertices in NDC:

```cpp
float vertices[] = {
    -0.5f, -0.5f, 0.0f,  // bottom-left
     0.5f, -0.5f, 0.0f,  // bottom-right
     0.0f,  0.5f, 0.0f   // top-center
};
```

Each vertex has 3 floats (x, y, z). The z is 0 for all three — we're working in 2D for now.

Visualized in NDC space:

```
        (0.0, 0.5)
            /\
           /  \
          /    \
         /      \
        /________\
  (-0.5,-0.5)  (0.5,-0.5)
```

## The Graphics Pipeline — In Detail

When you issue a draw call, your vertex data passes through the **graphics pipeline**. In the previous chapter we saw the overview; now let's examine each stage in detail for what we're about to build.

```
  ┌──────────────────────────────────────────────────────────────┐
  │                    GRAPHICS PIPELINE                         │
  │                                                              │
  │   Vertex Data    ┌───────────────┐   ┌──────────────────┐   │
  │   (our array) ──▶│ VERTEX SHADER │──▶│ SHAPE ASSEMBLY   │   │
  │                  │ (per vertex)  │   │ (form triangles) │   │
  │                  └───────────────┘   └──────────────────┘   │
  │                         ▲                     │              │
  │                   YOU WRITE THIS              ▼              │
  │                                     ┌──────────────────┐    │
  │                                     │ RASTERIZATION    │    │
  │                                     │ (to fragments)   │    │
  │                                     └──────────────────┘    │
  │                                              │               │
  │                                              ▼               │
  │                  ┌───────────────┐   ┌──────────────────┐   │
  │                  │FRAGMENT SHADER│◀──│ (interpolated    │   │
  │                  │ (per pixel)   │   │  data per frag.) │   │
  │                  └───────────────┘   └──────────────────┘   │
  │                         ▲                     │              │
  │                   YOU WRITE THIS              ▼              │
  │                                     ┌──────────────────┐    │
  │                                     │ TESTS & BLENDING │    │
  │                                     │ (depth, stencil) │    │
  │                                     └──────────────────┘    │
  │                                              │               │
  │                                              ▼               │
  │                                        Framebuffer           │
  └──────────────────────────────────────────────────────────────┘
```

### Vertex Shader

The vertex shader runs **once per vertex**. It receives the raw vertex data (position, color, texture coordinates, normals — whatever you send) and outputs a transformed position.

At minimum, a vertex shader must set `gl_Position` — a special built-in variable that tells OpenGL where this vertex is in clip space (which gets mapped to NDC after perspective division).

For our simple triangle, the vertex shader just passes the position through:

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
```

In later chapters, this is where we'll apply model, view, and projection matrices.

### Shape Assembly (Primitive Assembly)

The primitive assembly stage takes the vertices output by the vertex shader and groups them into the primitive you specified (triangles, lines, or points). For `GL_TRIANGLES`, it takes every 3 consecutive vertices and forms a triangle.

### Rasterization

The rasterizer takes each assembled primitive (triangle) and determines which pixels on the screen it covers. For each covered pixel, it generates a **fragment**. A fragment contains:

- Interpolated position
- Interpolated color (if the vertex shader outputs one)
- Interpolated texture coordinates
- Depth value
- And any other interpolated per-vertex outputs

```
                  Vertex A (red)
                     /\
                    /  \
  Rasterizer      / ···\       ··· = fragments (potential pixels)
  generates──▶   /······\          each fragment gets interpolated
  fragments     /········\         color between A, B, C
               /__________\
         Vertex B (green)  Vertex C (blue)
```

### Fragment Shader

The fragment shader runs **once per fragment** (potential pixel). It receives the interpolated data from the vertex shader outputs and determines the final color of that pixel.

Our simple fragment shader outputs a solid orange color:

```glsl
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);  // orange
}
```

### Tests and Blending

After the fragment shader, several tests are applied:

- **Depth test**: Is this fragment behind another already-drawn fragment? If so, discard it.
- **Stencil test**: Does this fragment pass the stencil mask?
- **Alpha blending**: For transparent objects, blend the fragment's color with what's already in the framebuffer.

Fragments that survive all tests are written to the framebuffer and eventually displayed on screen.

---

## Vertex Buffer Objects (VBO)

Now we need to get our vertex data from CPU memory onto the GPU. This is done with a **Vertex Buffer Object (VBO)** — a chunk of GPU memory that stores vertex data.

Why not just send vertices one at a time? Because the GPU is *massively* parallel but has a slow connection to the CPU. Sending data in bulk via buffers is orders of magnitude faster than sending individual vertices.

### Creating and Filling a VBO

```cpp
unsigned int VBO;
glGenBuffers(1, &VBO);
```

`glGenBuffers` generates buffer object names (IDs). The first parameter is how many to generate; the second is where to store the ID(s).

```cpp
glBindBuffer(GL_ARRAY_BUFFER, VBO);
```

`glBindBuffer` binds our buffer to the `GL_ARRAY_BUFFER` target. From now on, any operations on `GL_ARRAY_BUFFER` affect our `VBO`.

```cpp
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
```

`glBufferData` copies data into the currently bound buffer. Parameters:

| Parameter | Value | Meaning |
|-----------|-------|---------|
| target | `GL_ARRAY_BUFFER` | The buffer target (where VBO is bound) |
| size | `sizeof(vertices)` | Size in bytes (9 floats × 4 bytes = 36 bytes) |
| data | `vertices` | Pointer to the data |
| usage | `GL_STATIC_DRAW` | Usage hint for the driver |

Usage hints tell the driver how the data will be used so it can optimize storage:

| Hint | Meaning |
|------|---------|
| `GL_STATIC_DRAW` | Data set once, used many times (most geometry) |
| `GL_DYNAMIC_DRAW` | Data changed often, used many times (animations) |
| `GL_STREAM_DRAW` | Data set once, used at most a few times |

## Writing and Compiling Shaders

OpenGL shaders are written in **GLSL** (OpenGL Shading Language) and compiled at runtime. Yes, your application compiles shader source code every time it starts.

### Vertex Shader

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}
```

Let's break this down:

- `#version 330 core` — Use GLSL version 3.30, core profile (matches OpenGL 3.3).
- `layout (location = 0)` — This vertex attribute will be at attribute location 0 (we'll reference this when configuring vertex attributes).
- `in vec3 aPos` — Input variable: a 3-component vector named `aPos`. Each vertex will receive its position through this variable.
- `gl_Position` — Built-in output: the final clip-space position of this vertex. It's a `vec4` — the fourth component `w` is used for perspective division (set to 1.0 for now).

### Fragment Shader

```glsl
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
}
```

- `out vec4 FragColor` — Output variable: the color of this fragment (RGBA).
- We hardcode an orange color for every pixel of the triangle.

### Compiling a Shader in C++

Shader compilation follows a fixed pattern:

```cpp
// 1. Store shader source as a C string
const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";

// 2. Create a shader object
unsigned int vertexShader;
vertexShader = glCreateShader(GL_VERTEX_SHADER);

// 3. Attach the source code
glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);

// 4. Compile
glCompileShader(vertexShader);

// 5. Check for compilation errors
int success;
char infoLog[512];
glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
if (!success)
{
    glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
              << infoLog << std::endl;
}
```

Do the same for the fragment shader, using `GL_FRAGMENT_SHADER`:

```cpp
const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\0";

unsigned int fragmentShader;
fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
glCompileShader(fragmentShader);

glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
if (!success)
{
    glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
              << infoLog << std::endl;
}
```

**Always check for compilation errors.** A typo in GLSL will silently produce a broken shader, and nothing will render. The info log tells you exactly what went wrong.

## Shader Program

Individual shaders can't be used directly. They must be **linked** into a **shader program** — an object that chains the vertex shader output to the fragment shader input.

```cpp
unsigned int shaderProgram;
shaderProgram = glCreateProgram();

glAttachShader(shaderProgram, vertexShader);
glAttachShader(shaderProgram, fragmentShader);
glLinkProgram(shaderProgram);

// Check for linking errors
glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
if (!success)
{
    glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
    std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
              << infoLog << std::endl;
}

// Delete individual shader objects after linking (no longer needed)
glDeleteShader(vertexShader);
glDeleteShader(fragmentShader);
```

To use the program for rendering:

```cpp
glUseProgram(shaderProgram);
```

After this call, every draw call will use our vertex + fragment shaders.

## Linking Vertex Attributes

We've uploaded vertex data to the GPU (VBO) and compiled our shaders. But there's a critical missing piece: **OpenGL doesn't know how to interpret the raw bytes in the VBO.**

Our buffer is just a flat array of floats:

```
  Buffer contents (bytes):
  ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┐
  │ -0.5  │ -0.5  │  0.0  │  0.5  │ -0.5  │  0.0  │  0.0  │  0.5  │  0.0  │
  └───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────┘
  │◄── Vertex 0 ──▶│◄── Vertex 1 ──▶│◄── Vertex 2 ──▶│
  │  x    y    z   │  x    y    z   │  x    y    z   │
```

We need to tell OpenGL:
- How many components per attribute (3 for xyz)
- What data type (float)
- The stride — how many bytes between consecutive vertices
- The offset — where this attribute starts within each vertex

This is done with `glVertexAttribPointer`:

```cpp
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);
```

Let's examine each parameter:

```
  glVertexAttribPointer(
      0,                  // attribute location (layout = 0 in shader)
      3,                  // number of components (vec3 = 3)
      GL_FLOAT,           // data type of each component
      GL_FALSE,           // should data be normalized?
      3 * sizeof(float),  // stride: bytes between consecutive vertices
      (void*)0            // offset: where this attribute starts in the buffer
  );
```

The **stride** and **offset** diagram for our simple case:

```
  Byte offset:  0       4       8      12      16      20      24      28      32
                ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┐
                │  x0   │  y0   │  z0   │  x1   │  y1   │  z1   │  x2   │  y2   │  z2   │
                └───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────┘
                │◄──── stride (12 bytes) ────▶│
                │                             │
  offset = 0 ──┘                              └── next vertex starts here
```

Each float is 4 bytes. Each vertex has 3 floats = 12 bytes. So the stride is `3 * sizeof(float)` = 12 bytes.

> **Tip**: You can pass `0` as the stride when your vertex attributes are *tightly packed* (no gaps). OpenGL then calculates the stride automatically from the component count and type. But it's clearer to be explicit.

`glEnableVertexAttribArray(0)` enables attribute location 0. Vertex attributes are disabled by default.

## Vertex Array Objects (VAO)

Every time you want to draw, you'd have to:
1. Bind the VBO
2. Call `glVertexAttribPointer` for each attribute
3. Enable each attribute

For a complex scene with hundreds of objects, this gets tedious and error-prone. Enter the **Vertex Array Object (VAO)**.

A VAO stores all the vertex attribute configuration so you can restore it with a single `glBindVertexArray` call. Think of it as a "saved configuration snapshot."

What a VAO remembers:

```
  ┌──────────────────────────────────────────────┐
  │               Vertex Array Object             │
  ├──────────────────────────────────────────────-┤
  │ Attribute 0:                                  │
  │   • Source buffer: VBO #1                     │
  │   • Components: 3 (vec3)                      │
  │   • Type: GL_FLOAT                            │
  │   • Stride: 12 bytes                          │
  │   • Offset: 0 bytes                           │
  │   • Enabled: YES                              │
  ├───────────────────────────────────────────────┤
  │ Attribute 1:                                  │
  │   • (not configured)                          │
  ├───────────────────────────────────────────────┤
  │ Element Buffer (EBO): (none yet)              │
  └───────────────────────────────────────────────┘
```

### Using a VAO

```cpp
// Generate and bind a VAO
unsigned int VAO;
glGenVertexArrays(1, &VAO);
glBindVertexArray(VAO);

// Now configure vertex attributes — the VAO records everything
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);

// Unbind the VAO (optional, but saves you from accidental modifications)
glBindVertexArray(0);
```

Later, when drawing:

```cpp
glUseProgram(shaderProgram);
glBindVertexArray(VAO);    // restores all the attribute configuration
glDrawArrays(GL_TRIANGLES, 0, 3);
```

> **In OpenGL 3.3 core profile, a VAO is required.** Without one bound, draw calls will fail silently or produce errors. Always create and bind a VAO before configuring vertex attributes.

## Drawing the Triangle

Finally! With the shader program active and the VAO bound:

```cpp
glUseProgram(shaderProgram);
glBindVertexArray(VAO);
glDrawArrays(GL_TRIANGLES, 0, 3);
```

`glDrawArrays` parameters:

| Parameter | Value | Meaning |
|-----------|-------|---------|
| mode | `GL_TRIANGLES` | Draw triangles (every 3 vertices = 1 triangle) |
| first | `0` | Start at index 0 in the vertex array |
| count | `3` | Draw 3 vertices (= 1 triangle) |

Other primitive modes:

| Mode | Description |
|------|-------------|
| `GL_POINTS` | Each vertex is a single point |
| `GL_LINES` | Every 2 vertices form a line |
| `GL_LINE_STRIP` | Connected line segments |
| `GL_TRIANGLES` | Every 3 vertices form a triangle |
| `GL_TRIANGLE_STRIP` | Connected triangles sharing edges |
| `GL_TRIANGLE_FAN` | Triangles sharing a common center vertex |

## Complete Triangle Code

Here is the full, compilable program that draws an orange triangle:

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";

const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\0";

int main()
{
    // --- GLFW initialization ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "LearnOpenGL", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // --- GLAD ---
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // --- Build and compile the shader program ---

    // Vertex shader
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    // Fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    // Link shaders into a program
    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                  << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // --- Set up vertex data and buffers ---

    float vertices[] = {
        -0.5f, -0.5f, 0.0f,  // bottom-left
         0.5f, -0.5f, 0.0f,  // bottom-right
         0.0f,  0.5f, 0.0f   // top-center
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    // 1. Bind the VAO first
    glBindVertexArray(VAO);

    // 2. Bind and fill the VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 3. Configure vertex attribute(s)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Unbind VBO and VAO (safe since VAO recorded the VBO binding)
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // --- Render loop ---
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        // Clear screen
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Draw the triangle
        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- Cleanup ---
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

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

### What You Should See

An orange triangle on a dark teal background:

```
  ┌────────────────────────────────────────┐
  │ LearnOpenGL                      [—][×]│
  ├────────────────────────────────────────┤
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓/\▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓/:::::\▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓/:::::::::\▓▓▓▓▓▓▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓▓▓/:::(orange):::\▓▓▓▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓/::::::::::::::::::::\▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  └────────────────────────────────────────┘
            ▓ = teal background
            : = orange triangle
```

---

## Element Buffer Objects (EBO) — Indexed Drawing

What if we want to draw a **rectangle**? A rectangle is made of two triangles:

```
  3 ────── 2        Triangle 1: vertices 0, 1, 2
  │ ╲      │        Triangle 2: vertices 0, 2, 3
  │   ╲    │
  │     ╲  │
  0 ────── 1
```

Without indexed drawing, we'd need 6 vertices (3 per triangle):

```cpp
float vertices[] = {
    // First triangle
     0.5f,  0.5f, 0.0f,  // top-right     (vertex 2)
     0.5f, -0.5f, 0.0f,  // bottom-right  (vertex 1)
    -0.5f, -0.5f, 0.0f,  // bottom-left   (vertex 0)
    // Second triangle
    -0.5f, -0.5f, 0.0f,  // bottom-left   (vertex 0) — DUPLICATE
    -0.5f,  0.5f, 0.0f,  // top-left      (vertex 3)
     0.5f,  0.5f, 0.0f,  // top-right     (vertex 2) — DUPLICATE
};
```

Vertices 0 and 2 are duplicated. For a rectangle that's only 2 extra vertices, but for a complex 3D model with millions of vertices, duplication wastes massive amounts of GPU memory.

**Element Buffer Objects (EBOs)**, also called **Index Buffer Objects (IBOs)**, solve this. We define each unique vertex once and then use an array of indices to specify which vertices form each triangle:

```cpp
float vertices[] = {
     0.5f,  0.5f, 0.0f,  // 0: top-right
     0.5f, -0.5f, 0.0f,  // 1: bottom-right
    -0.5f, -0.5f, 0.0f,  // 2: bottom-left
    -0.5f,  0.5f, 0.0f   // 3: top-left
};

unsigned int indices[] = {
    0, 1, 3,  // first triangle
    1, 2, 3   // second triangle
};
```

Four vertices, six indices — and the saving grows dramatically with complex geometry.

### Memory comparison

```
  Without EBO:          With EBO:
  6 vertices × 3 floats = 18 floats     4 vertices × 3 floats = 12 floats
  = 72 bytes                             + 6 indices × 4 bytes = 24 bytes
                                         = 72 bytes total
                                         (but vertices are reusable for
                                          normals, UVs — real savings
                                          come from complex meshes)
```

### Setting Up the EBO

An EBO is just another buffer object, bound to `GL_ELEMENT_ARRAY_BUFFER`:

```cpp
unsigned int EBO;
glGenBuffers(1, &EBO);

glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
```

**Important**: The EBO binding is stored *inside* the currently bound VAO. So the setup order matters:

```
  1. Bind VAO
  2. Bind VBO, upload vertex data, configure attributes
  3. Bind EBO, upload index data
  4. Unbind VAO (this "saves" the EBO binding too)
```

> **Warning**: Do NOT unbind the EBO while the VAO is still bound! Unlike the VBO (which is recorded when `glVertexAttribPointer` is called), the EBO binding is part of the VAO state directly.

What the VAO now stores:

```
  ┌──────────────────────────────────────────────┐
  │               Vertex Array Object             │
  ├──────────────────────────────────────────────-┤
  │ Attribute 0:                                  │
  │   • Source buffer: VBO                        │
  │   • Components: 3 (vec3)                      │
  │   • Type: GL_FLOAT                            │
  │   • Stride: 12 bytes                          │
  │   • Offset: 0 bytes                           │
  │   • Enabled: YES                              │
  ├──────────────────────────────────────────────-┤
  │ Element Buffer (EBO): EBO                     │
  └──────────────────────────────────────────────-┘
```

### Drawing with glDrawElements

Instead of `glDrawArrays`, we use `glDrawElements`:

```cpp
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
```

Parameters:

| Parameter | Value | Meaning |
|-----------|-------|---------|
| mode | `GL_TRIANGLES` | Draw triangles |
| count | `6` | Number of indices to draw |
| type | `GL_UNSIGNED_INT` | Data type of the indices |
| indices | `0` | Offset into the EBO (0 = start) |

## Complete Rectangle Code (with EBO)

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";

const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\0";

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
                                          "LearnOpenGL", nullptr, nullptr);
    if (window == nullptr)
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

    // --- Build and compile shaders ---
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                  << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // --- Vertex data with indices ---
    float vertices[] = {
         0.5f,  0.5f, 0.0f,  // 0: top-right
         0.5f, -0.5f, 0.0f,  // 1: bottom-right
        -0.5f, -0.5f, 0.0f,  // 2: bottom-left
        -0.5f,  0.5f, 0.0f   // 3: top-left
    };
    unsigned int indices[] = {
        0, 1, 3,  // first triangle
        1, 2, 3   // second triangle
    };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // 1. Bind VAO
    glBindVertexArray(VAO);

    // 2. Bind and fill VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 3. Bind and fill EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    // 4. Configure vertex attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Unbind VBO (safe), then unbind VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    // Do NOT unbind EBO while VAO is bound — it's part of VAO state

    // --- Render loop ---
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

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

## Wireframe Mode

To see the triangle edges instead of filled polygons, switch to wireframe mode:

```cpp
glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
```

This is extremely useful for debugging geometry. To switch back to filled mode:

```cpp
glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
```

With our rectangle in wireframe mode, you'll clearly see the two triangles:

```
  ┌──────┐         ┌──────┐
  │ ╲    │         │╲     │
  │   ╲  │   ──▶   │  ╲   │  (wireframe shows
  │     ╲│         │    ╲  │   the triangle edges)
  └──────┘         └──────┘
    Filled          Wireframe
```

---

## Putting It All Together — The Object Setup Flow

Here is the complete sequence of operations when setting up geometry for rendering. Understanding this flow is crucial:

```
  ┌─────────────────────────────────────────────┐
  │           SETUP (once per object)            │
  │                                              │
  │  1. glGenVertexArrays → create VAO           │
  │  2. glBindVertexArray → bind VAO             │
  │     ┌─────────────────────────────────────┐  │
  │     │  3. glGenBuffers → create VBO       │  │
  │     │  4. glBindBuffer(ARRAY) → bind VBO  │  │
  │     │  5. glBufferData → upload vertices  │  │
  │     │                                     │  │
  │     │  6. glGenBuffers → create EBO       │  │
  │     │  7. glBindBuffer(ELEMENT) → bind EBO│  │
  │     │  8. glBufferData → upload indices   │  │
  │     │                                     │  │
  │     │  9. glVertexAttribPointer → config  │  │
  │     │ 10. glEnableVertexAttribArray       │  │
  │     └─────────────────────────────────────┘  │
  │ 11. glBindVertexArray(0) → unbind VAO        │
  └─────────────────────────────────────────────-┘

  ┌─────────────────────────────────────────────┐
  │        DRAW (every frame, per object)        │
  │                                              │
  │  1. glUseProgram(shader)                     │
  │  2. glBindVertexArray(VAO)                   │
  │  3. glDrawElements (or glDrawArrays)         │
  └─────────────────────────────────────────────-┘
```

---

## Key Takeaways

- Vertex coordinates in OpenGL range from **-1 to 1** in all three axes (Normalized Device Coordinates).
- **VBOs** (Vertex Buffer Objects) store vertex data on the GPU for fast access.
- **Shaders** are small programs that run on the GPU. You *must* write at least a vertex shader and a fragment shader in modern OpenGL.
- Shaders are written in **GLSL**, compiled at runtime, and linked into a **shader program**.
- **`glVertexAttribPointer`** tells OpenGL how to interpret the raw bytes in a VBO — specifying the stride and offset of each vertex attribute.
- **VAOs** (Vertex Array Objects) save all the vertex attribute configuration so you can restore it with a single bind call.
- **EBOs** (Element Buffer Objects) enable indexed drawing: define each vertex once and reuse it via indices. This saves memory and is how real 3D models work.
- Use `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` for wireframe debugging.

## Exercises

1. **Two triangles with separate VAOs**: Draw two triangles side by side, each using its own VAO and VBO. This practices the setup flow twice.

2. **Two triangles, different colors**: Create two different shader programs (with different fragment shaders) and draw one triangle with each. This shows that `glUseProgram` can switch shaders between draw calls.

3. **Experiment with `glDrawArrays`**: Try changing the first parameter to `GL_LINE_LOOP` or `GL_POINTS` and see what happens. Try `glPointSize(10.0f)` before drawing with `GL_POINTS`.

4. **Move a vertex**: Change one of the vertex positions and rebuild. Confirm it works as you expect.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Hello Window](03_hello_window.md) | | [Next: Shaders →](05_shaders.md) |
