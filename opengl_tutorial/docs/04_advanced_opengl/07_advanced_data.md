# Advanced Data

Throughout this tutorial series we've been using `glBufferData` to fill our buffer objects with data — and it's served us well. But OpenGL provides several more flexible and powerful ways to manage buffer data. In this chapter we'll explore alternative methods for filling buffers, mapping buffer memory directly, batching vertex attributes differently, and copying data between buffers. Understanding these techniques gives you much finer control over how your GPU memory is managed.

---

## Filling Buffers with glBufferSubData

We've been using `glBufferData` to allocate *and* fill a buffer in one call:

```cpp
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
```

This call does two things: it allocates GPU memory of the given size and copies data into it. Every time you call `glBufferData`, the old buffer storage is destroyed and replaced.

But what if you want to fill only *part* of a buffer? Maybe you want to allocate a large buffer upfront and populate different regions at different times. That's what `glBufferSubData` is for:

```cpp
glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
```

This copies `size` bytes from `data` into the buffer currently bound to `GL_ARRAY_BUFFER`, starting at byte `offset`. Critically, the buffer must already be allocated (via `glBufferData`), and `offset + size` must not exceed the buffer's total size.

### Example: Uploading Data in Parts

Imagine you have position data and color data stored separately, and you want to place them sequentially in a single buffer:

```cpp
float positions[] = {
    -0.5f, -0.5f, 0.0f,
     0.5f, -0.5f, 0.0f,
     0.0f,  0.5f, 0.0f
};
float colors[] = {
    1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 1.0f
};

unsigned int VBO;
glGenBuffers(1, &VBO);
glBindBuffer(GL_ARRAY_BUFFER, VBO);

// Allocate enough space for both arrays, but don't fill it yet
glBufferData(GL_ARRAY_BUFFER,
             sizeof(positions) + sizeof(colors),
             NULL,                   // no initial data
             GL_STATIC_DRAW);

// Upload positions starting at offset 0
glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(positions), positions);

// Upload colors starting right after the positions
glBufferSubData(GL_ARRAY_BUFFER, sizeof(positions), sizeof(colors), colors);
```

By passing `NULL` as the data pointer to `glBufferData`, we allocate the memory without filling it. Then we use `glBufferSubData` to populate each region independently. This pattern is particularly useful when you receive data in stages or from multiple sources.

---

## Mapping Buffers

`glBufferSubData` still requires you to have the data in a C++ array first. But sometimes you want to write data *directly* into the GPU buffer's memory — for example, when reading from a file or generating data procedurally. OpenGL lets you do this by **mapping** the buffer: you obtain a raw pointer to the buffer's memory, write to it, and then unmap it.

```cpp
glBindBuffer(GL_ARRAY_BUFFER, VBO);

// Get a pointer to the buffer's memory
void* ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

// Write data directly
float* floatPtr = (float*)ptr;
floatPtr[0] = -0.5f;  // x1
floatPtr[1] = -0.5f;  // y1
floatPtr[2] =  0.0f;  // z1
// ... fill in more vertices ...

// Tell OpenGL we're done writing
glUnmapBuffer(GL_ARRAY_BUFFER);
```

The second parameter to `glMapBuffer` specifies the access pattern:

| Access Flag | Meaning |
|---|---|
| `GL_READ_ONLY` | You will only read from the pointer |
| `GL_WRITE_ONLY` | You will only write to the pointer |
| `GL_READ_WRITE` | You will both read and write |

Choosing the correct flag lets the driver optimize memory transfers. If you only need to write, use `GL_WRITE_ONLY` — the driver may be able to avoid copying current buffer contents to a CPU-accessible location.

> **Important:** You *must* call `glUnmapBuffer` before using the buffer for rendering. While a buffer is mapped, OpenGL cannot use it. `glUnmapBuffer` returns `GL_TRUE` on success and `GL_FALSE` if the buffer's contents were corrupted (e.g., the display resolution changed). Always check the return value in production code.

### Practical Use: Streaming Particle Data

Buffer mapping shines when you need to update data every frame, such as a particle system. Instead of rebuilding a C++ array and copying it with `glBufferSubData`, you map the buffer, write particle positions directly, and unmap:

```cpp
struct Particle {
    glm::vec3 position;
    glm::vec4 color;
    float life;
};

// Each frame:
glBindBuffer(GL_ARRAY_BUFFER, particleVBO);

// Orphan the old buffer storage to avoid synchronization stalls
glBufferData(GL_ARRAY_BUFFER, maxParticles * sizeof(Particle), NULL, GL_STREAM_DRAW);

Particle* particles = (Particle*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
for (unsigned int i = 0; i < activeParticleCount; i++)
{
    particles[i].position = computePosition(i, deltaTime);
    particles[i].color    = computeColor(i);
    particles[i].life     = particleLives[i];
}
glUnmapBuffer(GL_ARRAY_BUFFER);
```

The `glBufferData` call with `NULL` before mapping is a technique called **buffer orphaning**: it tells the driver "I don't need the old data anymore, give me fresh storage." This avoids GPU stalls where the driver would otherwise need to wait for the previous frame's rendering to finish before you can safely write new data.

---

## Batching Vertex Attributes

So far we've been storing vertex data in an **interleaved** format. Each vertex's attributes are grouped together:

```
Buffer: [pos1 norm1 tex1 | pos2 norm2 tex2 | pos3 norm3 tex3 | ...]
```

This is what it looks like in memory:

```
Interleaved layout:
┌────────┬────────┬────────┬────────┬────────┬────────┬─────┐
│ pos[0] │norm[0] │ tex[0] │ pos[1] │norm[1] │ tex[1] │ ... │
└────────┴────────┴────────┴────────┴────────┴────────┴─────┘
  vertex 0 data             vertex 1 data
```

There is an alternative: **batched** (also called **non-interleaved** or **struct-of-arrays**) layout, where all positions come first, then all normals, then all texture coordinates:

```
Batched layout:
┌────────┬────────┬────────┬────────┬────────┬────────┬────────┬────────┬─────┐
│ pos[0] │ pos[1] │ pos[2] │norm[0] │norm[1] │norm[2] │ tex[0] │ tex[1] │ ... │
└────────┴────────┴────────┴────────┴────────┴────────┴────────┴────────┴─────┘
  all positions              all normals              all texcoords
```

### Setting Up a Batched Layout

With `glBufferSubData`, setting up a batched layout is straightforward. We allocate one big buffer and upload each attribute array at a different offset:

```cpp
float positions[] = { /* all vertex positions */ };
float normals[]   = { /* all vertex normals   */ };
float texCoords[] = { /* all vertex texcoords */ };

unsigned int VBO;
glGenBuffers(1, &VBO);
glBindBuffer(GL_ARRAY_BUFFER, VBO);

GLsizeiptr totalSize = sizeof(positions) + sizeof(normals) + sizeof(texCoords);
glBufferData(GL_ARRAY_BUFFER, totalSize, NULL, GL_STATIC_DRAW);

GLintptr offsetPositions = 0;
GLintptr offsetNormals   = sizeof(positions);
GLintptr offsetTexCoords = sizeof(positions) + sizeof(normals);

glBufferSubData(GL_ARRAY_BUFFER, offsetPositions, sizeof(positions), positions);
glBufferSubData(GL_ARRAY_BUFFER, offsetNormals,   sizeof(normals),   normals);
glBufferSubData(GL_ARRAY_BUFFER, offsetTexCoords,  sizeof(texCoords), texCoords);
```

Now when configuring vertex attributes, the stride and offset change compared to interleaved:

```cpp
// Position attribute: 3 floats, tightly packed, starting at offsetPositions
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                      (void*)offsetPositions);
glEnableVertexAttribArray(0);

// Normal attribute: 3 floats, tightly packed, starting at offsetNormals
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                      (void*)offsetNormals);
glEnableVertexAttribArray(1);

// TexCoord attribute: 2 floats, tightly packed, starting at offsetTexCoords
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                      (void*)offsetTexCoords);
glEnableVertexAttribArray(2);
```

Notice the key difference: with interleaved data, the **stride** equals the full vertex size (e.g., `8 * sizeof(float)` for pos+normal+tex), and the offset is the byte offset *within* a single vertex. With batched data, the stride equals just the size of one attribute (e.g., `3 * sizeof(float)` for positions), and the offset is the byte position of that attribute block *within the buffer*.

### Which Layout Is Better?

In general, **interleaved is preferred** for rendering. When the GPU processes a vertex, it needs all of that vertex's attributes. With interleaved data, these attributes sit next to each other in memory, which is cache-friendly. With batched data, the GPU has to fetch from three separate memory regions for each vertex, which can lead to more cache misses.

However, batched layouts are useful when:

- You only update one attribute (e.g., positions in a physics simulation) while others stay static
- You receive data from different sources at different times
- You want to share part of a buffer between multiple objects

---

## Copying Between Buffers: glCopyBufferSubData

Sometimes you need to copy data from one buffer to another. You might be consolidating multiple small buffers into one large buffer, or creating a snapshot of a buffer for comparison.

OpenGL provides `glCopyBufferSubData` for this:

```cpp
glCopyBufferSubData(readTarget, writeTarget,
                    readOffset, writeOffset, size);
```

This copies `size` bytes from the buffer bound to `readTarget` (starting at `readOffset`) to the buffer bound to `writeTarget` (starting at `writeOffset`).

There's a subtlety: if both buffers are the same type (e.g., both `GL_ARRAY_BUFFER`), you can't bind them both at the same time to the same target. OpenGL provides two "scratch" targets specifically for this purpose:

- `GL_COPY_READ_BUFFER`
- `GL_COPY_WRITE_BUFFER`

These targets have no special GPU meaning — they exist solely to allow you to bind two buffers simultaneously for copying:

```cpp
unsigned int bufA, bufB;
glGenBuffers(1, &bufA);
glGenBuffers(1, &bufB);

// Set up bufA with some data
glBindBuffer(GL_ARRAY_BUFFER, bufA);
glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

// Allocate bufB with same size
glBindBuffer(GL_ARRAY_BUFFER, bufB);
glBufferData(GL_ARRAY_BUFFER, sizeof(data), NULL, GL_STATIC_DRAW);

// Bind to copy targets
glBindBuffer(GL_COPY_READ_BUFFER, bufA);
glBindBuffer(GL_COPY_WRITE_BUFFER, bufB);

// Copy entire contents
glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                    0, 0, sizeof(data));
```

You could also bind one buffer to `GL_ARRAY_BUFFER` and the other to `GL_COPY_WRITE_BUFFER` (or vice versa), since they're different targets. The copy targets are just a convenient way to avoid disturbing your existing bindings.

---

## Complete Example: All Techniques in Action

Let's put everything together in a program that demonstrates `glBufferSubData`, buffer mapping, batched attributes, and buffer copying. We'll draw a colorful triangle using batched vertex attributes, with the color data populated via buffer mapping:

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 ourColor;

void main()
{
    FragColor = vec4(ourColor, 1.0);
}
)";

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
                                          "Advanced Data", NULL, NULL);
    if (window == NULL)
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

    // --- Compile shaders ---
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // --- Vertex data (batched layout) ---
    float positions[] = {
        -0.5f, -0.5f, 0.0f,   // bottom-left
         0.5f, -0.5f, 0.0f,   // bottom-right
         0.0f,  0.5f, 0.0f    // top-center
    };

    // We'll fill colors via buffer mapping instead of a C++ array

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    GLsizeiptr posSize   = sizeof(positions);
    GLsizeiptr colorSize = 3 * 3 * sizeof(float);  // 3 vertices × 3 components
    GLsizeiptr totalSize = posSize + colorSize;

    // Allocate buffer with no initial data
    glBufferData(GL_ARRAY_BUFFER, totalSize, NULL, GL_STATIC_DRAW);

    // Upload positions using glBufferSubData
    glBufferSubData(GL_ARRAY_BUFFER, 0, posSize, positions);

    // Upload colors using buffer mapping
    // We map only the color region. First, map the entire buffer:
    float* ptr = (float*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
    if (ptr)
    {
        // Offset to the color region (after positions: 9 floats)
        float* colorPtr = ptr + (posSize / sizeof(float));
        // Vertex 0: red
        colorPtr[0] = 1.0f; colorPtr[1] = 0.0f; colorPtr[2] = 0.0f;
        // Vertex 1: green
        colorPtr[3] = 0.0f; colorPtr[4] = 1.0f; colorPtr[5] = 0.0f;
        // Vertex 2: blue
        colorPtr[6] = 0.0f; colorPtr[7] = 0.0f; colorPtr[8] = 1.0f;

        glUnmapBuffer(GL_ARRAY_BUFFER);
    }

    // Configure vertex attributes (batched: positions then colors)
    // Position: attribute 0, starts at offset 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color: attribute 1, starts at offset posSize
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          (void*)posSize);
    glEnableVertexAttribArray(1);

    // --- Demonstrate buffer copying ---
    // Create a backup of our VBO
    unsigned int backupVBO;
    glGenBuffers(1, &backupVBO);
    glBindBuffer(GL_COPY_WRITE_BUFFER, backupVBO);
    glBufferData(GL_COPY_WRITE_BUFFER, totalSize, NULL, GL_STATIC_DRAW);

    // Copy from VBO (bound to ARRAY_BUFFER) to backupVBO (bound to COPY_WRITE)
    glCopyBufferSubData(GL_ARRAY_BUFFER, GL_COPY_WRITE_BUFFER,
                        0, 0, totalSize);

    glBindVertexArray(0);

    // --- Render loop ---
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &backupVBO);
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

---

## Key Takeaways

- **`glBufferData`** allocates *and* fills a buffer; calling it again destroys and replaces the old storage. Pass `NULL` to allocate without filling.
- **`glBufferSubData`** writes data into a portion of an already-allocated buffer without reallocating. Use it to populate regions independently.
- **`glMapBuffer`** gives you a direct pointer to buffer memory. Write to it freely, then call `glUnmapBuffer` before rendering. Ideal for streaming data.
- **Buffer orphaning** (calling `glBufferData` with `NULL` before mapping) avoids GPU synchronization stalls when updating every frame.
- **Interleaved** vertex attributes (pos-norm-tex per vertex) are generally preferred for rendering performance due to cache locality.
- **Batched** attributes (all positions, then all normals, etc.) are useful when attributes are updated independently or come from different sources.
- **`glCopyBufferSubData`** copies data between buffers. Use `GL_COPY_READ_BUFFER` and `GL_COPY_WRITE_BUFFER` as scratch targets to avoid binding conflicts.

## Exercises

1. Modify the complete example to use an **interleaved** layout instead of batched. Compare how `glVertexAttribPointer` calls differ (stride and offset parameters).
2. Create a simple animation by mapping the position buffer every frame and moving the vertices. Use `GL_STREAM_DRAW` and buffer orphaning for best performance.
3. Set up a buffer with batched attributes (positions, normals, texcoords) for a cube. Render the cube and then use `glCopyBufferSubData` to clone the buffer into a second VBO. Render a second cube using the copied buffer.
4. Write a mini particle system: allocate a buffer for 1000 particles, map it every frame, update each particle's position (simple gravity simulation), and render as `GL_POINTS`. Experiment with `GL_STREAM_DRAW` vs `GL_DYNAMIC_DRAW`.
5. (Research) Look up `glMapBufferRange` — a more flexible version of `glMapBuffer` that lets you map a specific byte range and provides flags like `GL_MAP_INVALIDATE_BUFFER_BIT`. How does it improve over `glMapBuffer`?

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Framebuffers](06_framebuffers.md) | | [Next: Advanced GLSL →](08_advanced_glsl.md) |
