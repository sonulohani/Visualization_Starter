# Instancing

Imagine you're building a space scene with thousands of asteroids orbiting a planet. Each asteroid uses the same mesh, just positioned and rotated differently. The naive approach — looping in C++ and issuing a separate draw call for each asteroid — works, but it's painfully slow. Even though each draw call may only render a few hundred triangles, the CPU-side overhead of *setting up* each call (binding buffers, uploading uniforms, calling `glDrawArrays`) adds up quickly. With 10,000 asteroids, your CPU becomes the bottleneck long before the GPU breaks a sweat.

This is the problem **instancing** solves. Instead of 10,000 draw calls, you make *one* — and tell OpenGL to render 10,000 copies of the same mesh, each with its own transformation. The GPU handles the repetition at hardware speed.

---

## Why Draw Calls Are Expensive

A draw call like `glDrawArrays` or `glDrawElements` doesn't just tell the GPU "draw these triangles." It triggers a chain of CPU-side work:

```
Your code                    OpenGL Driver                GPU
─────────                    ─────────────                ───
glUseProgram(shader)    →    Validate shader state
glUniform*(...)         →    Copy uniform data
glBindVertexArray(VAO)  →    Validate vertex format
glDrawArrays(...)       →    Build command buffer    →    Execute draw
```

Each step involves validation, state changes, and potentially synchronization. Do this 10,000 times per frame and your CPU spends most of its time talking to the driver instead of doing useful work. The GPU, meanwhile, finishes each tiny draw almost instantly and sits idle waiting for the next command.

```
CPU timeline:  [setup][draw][setup][draw][setup][draw]...  (bottleneck!)
GPU timeline:  ......[████].......[████].......[████]...   (mostly idle)

With instancing:
CPU timeline:  [setup][draw─────────────────────────────]
GPU timeline:  .......[████████████████████████████████]    (fully utilized)
```

The rule of thumb: **minimize draw calls.** Instancing is one of the most effective ways to do this.

---

## glDrawArraysInstanced and glDrawElementsInstanced

OpenGL provides instanced versions of the standard draw calls:

```cpp
glDrawArraysInstanced(GL_TRIANGLES, 0, vertexCount, instanceCount);
glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, instanceCount);
```

These render the same mesh `instanceCount` times in a single call. Inside the vertex shader, the built-in variable `gl_InstanceID` tells you which instance is being processed (0, 1, 2, ..., instanceCount - 1). You use this to apply per-instance transformations.

---

## Simple Example: 100 Quads with Uniform Offsets

Let's start simple. We'll render 100 small quads, each at a different position on screen. The first approach stores per-instance offsets in a **uniform array** and indexes into it with `gl_InstanceID`.

### The Shaders

```glsl
// Vertex shader
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out vec3 fColor;

uniform vec2 offsets[100];

void main()
{
    vec2 offset = offsets[gl_InstanceID];
    gl_Position = vec4(aPos + offset, 0.0, 1.0);
    fColor = aColor;
}
```

```glsl
// Fragment shader
#version 330 core
out vec4 FragColor;
in vec3 fColor;

void main()
{
    FragColor = vec4(fColor, 1.0);
}
```

### The C++ Code

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <sstream>
#include <string>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out vec3 fColor;

uniform vec2 offsets[100];

void main()
{
    vec2 offset = offsets[gl_InstanceID];
    gl_Position = vec4(aPos + offset, 0.0, 1.0);
    fColor = aColor;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 fColor;

void main()
{
    FragColor = vec4(fColor, 1.0);
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
                                          "Instancing - Uniform Offsets", NULL, NULL);
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

    // Compile shaders
    unsigned int vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertShader);

    unsigned int fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertShader);
    glAttachShader(shaderProgram, fragShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    // Generate a 10x10 grid of offsets
    glm::vec2 offsets[100];
    int index = 0;
    float offsetStep = 0.2f;
    for (int y = -5; y < 5; y++)
    {
        for (int x = -5; x < 5; x++)
        {
            offsets[index].x = (float)x * offsetStep + offsetStep / 2.0f;
            offsets[index].y = (float)y * offsetStep + offsetStep / 2.0f;
            index++;
        }
    }

    // Upload offsets as uniforms
    glUseProgram(shaderProgram);
    for (int i = 0; i < 100; i++)
    {
        std::ostringstream ss;
        ss << "offsets[" << i << "]";
        glUniform2fv(glGetUniformLocation(shaderProgram, ss.str().c_str()),
                     1, &offsets[i][0]);
    }

    // Quad vertices: position (2D) + color (RGB)
    float quadVertices[] = {
        // positions     // colors
        -0.05f,  0.05f,  1.0f, 0.0f, 0.0f,
         0.05f, -0.05f,  0.0f, 1.0f, 0.0f,
        -0.05f, -0.05f,  0.0f, 0.0f, 1.0f,

        -0.05f,  0.05f,  1.0f, 0.0f, 0.0f,
         0.05f, -0.05f,  0.0f, 1.0f, 0.0f,
         0.05f,  0.05f,  0.0f, 1.0f, 1.0f
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 100);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

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

You'll see a 10×10 grid of colorful quads — all rendered with a single draw call.

### The Limitation

This works, but the uniform array has a hard size limit. OpenGL guarantees at least 1024 `vec4` uniforms per shader stage, which means roughly 512 `vec2` values. For 100 quads, that's fine. For 10,000 asteroids? Not a chance. We need a better approach.

---

## Instanced Arrays: The Scalable Solution

Instead of cramming per-instance data into uniforms, we can store it in a **vertex buffer** and tell OpenGL to advance that attribute once per instance instead of once per vertex. This is called an **instanced array**, and it has no practical size limit — it's just a regular VBO.

The key function is `glVertexAttribDivisor`:

```cpp
glVertexAttribDivisor(attributeIndex, divisor);
```

| Divisor Value | Meaning |
|---|---|
| 0 | Advance per vertex (default behavior) |
| 1 | Advance per instance |
| 2 | Advance every 2 instances |
| N | Advance every N instances |

```
Per-vertex attribute (divisor = 0):
  Instance 0: v0 v1 v2 v3 v4 v5    ← reads vertices 0-5
  Instance 1: v0 v1 v2 v3 v4 v5    ← reads vertices 0-5 again
  Instance 2: v0 v1 v2 v3 v4 v5    ← same vertices again

Per-instance attribute (divisor = 1):
  Instance 0: offset[0] offset[0] offset[0] ...  ← same value for all vertices
  Instance 1: offset[1] offset[1] offset[1] ...  ← next value
  Instance 2: offset[2] offset[2] offset[2] ...  ← next value
```

### Refactored Example with Instanced Arrays

Let's redo the 100-quad example using instanced arrays instead of uniform offsets:

```glsl
// Vertex shader — now uses an attribute instead of a uniform array
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aOffset;    // per-instance!

out vec3 fColor;

void main()
{
    gl_Position = vec4(aPos + aOffset, 0.0, 1.0);
    fColor = aColor;
}
```

On the C++ side, we create a second VBO for the offsets and set its divisor to 1:

```cpp
// Create and fill the instance VBO
unsigned int instanceVBO;
glGenBuffers(1, &instanceVBO);
glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * 100, &offsets[0],
             GL_STATIC_DRAW);

// Configure the instanced attribute (location = 2)
glBindVertexArray(VAO);
glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
glEnableVertexAttribArray(2);
glVertexAttribDivisor(2, 1);    // advance once per instance
glBindVertexArray(0);
```

The draw call is unchanged:

```cpp
glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 100);
```

This is cleaner, simpler, and scales to any instance count. You could have 100,000 offsets in that VBO with no trouble.

---

## Asteroid Field: Instanced Rendering at Scale

Let's build a more impressive demo: a field of thousands of asteroids orbiting a planet. We'll use the `Model` class from the model loading chapters to load a rock mesh and render it thousands of times with instancing.

### The Plan

1. Load a rock model and a planet model
2. Generate thousands of random model matrices (position in a ring around the planet, with random rotation and scale)
3. Store all model matrices in a VBO
4. Configure the VAO to read `mat4` attributes with divisor 1
5. Render with `glDrawElementsInstanced`

### Generating the Instance Matrices

```cpp
unsigned int amount = 10000;
glm::mat4* modelMatrices = new glm::mat4[amount];

srand(static_cast<unsigned int>(glfwGetTime()));
float radius = 50.0f;
float offset = 10.0f;

for (unsigned int i = 0; i < amount; i++)
{
    glm::mat4 model = glm::mat4(1.0f);

    // Position: distribute in a ring
    float angle = (float)i / (float)amount * 360.0f;
    float displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
    float x = sin(glm::radians(angle)) * radius + displacement;
    displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
    float y = displacement * 0.2f;   // keep asteroids mostly in a flat ring
    displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
    float z = cos(glm::radians(angle)) * radius + displacement;
    model = glm::translate(model, glm::vec3(x, y, z));

    // Scale: random between 0.05 and 0.25
    float scale = static_cast<float>((rand() % 20) / 100.0 + 0.05);
    model = glm::scale(model, glm::vec3(scale));

    // Rotation: random axis and angle
    float rotAngle = static_cast<float>(rand() % 360);
    model = glm::rotate(model, glm::radians(rotAngle),
                        glm::vec3(0.4f, 0.6f, 0.8f));

    modelMatrices[i] = model;
}
```

### Without Instancing (The Slow Way)

For comparison, here's the non-instanced approach:

```cpp
for (unsigned int i = 0; i < amount; i++)
{
    shader.use();
    shader.setMat4("model", modelMatrices[i]);
    rock.Draw(shader);
}
```

With 10,000 asteroids, this is 10,000 draw calls per frame. You'll likely get single-digit FPS.

### With Instancing: Setting Up mat4 Attributes

A `mat4` is 4 columns of `vec4`. Since a single vertex attribute can be at most `vec4` (4 floats), a `mat4` requires **4 consecutive vertex attributes**:

```
Attribute location 3: column 0 (vec4)
Attribute location 4: column 1 (vec4)
Attribute location 5: column 2 (vec4)
Attribute location 6: column 3 (vec4)
```

```
Memory layout of one mat4 (64 bytes):
┌──────────┬──────────┬──────────┬──────────┐
│  col 0   │  col 1   │  col 2   │  col 3   │
│  (vec4)  │  (vec4)  │  (vec4)  │  (vec4)  │
│ loc = 3  │ loc = 4  │ loc = 5  │ loc = 6  │
└──────────┴──────────┴──────────┴──────────┘
    16 B       16 B       16 B       16 B
```

Here's how to set it up:

```cpp
// Store all model matrices in a VBO
unsigned int instanceVBO;
glGenBuffers(1, &instanceVBO);
glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
glBufferData(GL_ARRAY_BUFFER, amount * sizeof(glm::mat4), &modelMatrices[0],
             GL_STATIC_DRAW);

// Configure instanced mat4 attributes for each mesh in the model.
// We need to do this for every mesh's VAO.
for (unsigned int i = 0; i < rock.meshes.size(); i++)
{
    unsigned int VAO = rock.meshes[i].VAO;
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

    std::size_t vec4Size = sizeof(glm::vec4);

    // Attribute 3: first column of the mat4
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                          (void*)0);
    glVertexAttribDivisor(3, 1);

    // Attribute 4: second column
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                          (void*)(1 * vec4Size));
    glVertexAttribDivisor(4, 1);

    // Attribute 5: third column
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                          (void*)(2 * vec4Size));
    glVertexAttribDivisor(5, 1);

    // Attribute 6: fourth column
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                          (void*)(3 * vec4Size));
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);
}
```

### The Instanced Vertex Shader

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in mat4 aInstanceMatrix;   // occupies locations 3-6

out vec2 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = projection * view * aInstanceMatrix * vec4(aPos, 1.0);
}
```

When you declare `in mat4 aInstanceMatrix` at location 3, GLSL automatically assigns locations 3, 4, 5, and 6 to the four columns — matching exactly what we configured with `glVertexAttribPointer`.

### The Draw Call

With everything configured, the draw call is beautifully simple:

```cpp
for (unsigned int i = 0; i < rock.meshes.size(); i++)
{
    glBindVertexArray(rock.meshes[i].VAO);
    glDrawElementsInstanced(GL_TRIANGLES,
                            rock.meshes[i].indices.size(),
                            GL_UNSIGNED_INT, 0,
                            amount);
    glBindVertexArray(0);
}
```

One draw call per mesh, regardless of how many instances. With a simple rock model (one mesh), this is literally one draw call for 10,000 asteroids.

### Complete Asteroid Field Program

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

Camera camera(glm::vec3(0.0f, 15.0f, 80.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

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
                                          "Instancing - Asteroid Field", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // Shaders
    const char* planetVS = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 2) in vec2 aTexCoords;
    out vec2 TexCoords;
    uniform mat4 projection;
    uniform mat4 view;
    uniform mat4 model;
    void main()
    {
        TexCoords = aTexCoords;
        gl_Position = projection * view * model * vec4(aPos, 1.0);
    }
    )";

    const char* asteroidVS = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 2) in vec2 aTexCoords;
    layout (location = 3) in mat4 aInstanceMatrix;
    out vec2 TexCoords;
    uniform mat4 projection;
    uniform mat4 view;
    void main()
    {
        TexCoords = aTexCoords;
        gl_Position = projection * view * aInstanceMatrix * vec4(aPos, 1.0);
    }
    )";

    const char* fragmentSource = R"(
    #version 330 core
    out vec4 FragColor;
    in vec2 TexCoords;
    uniform sampler2D texture_diffuse1;
    void main()
    {
        FragColor = texture(texture_diffuse1, TexCoords);
    }
    )";

    auto compileShaderProgram = [](const char* vs, const char* fs) {
        unsigned int v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vs, NULL);
        glCompileShader(v);

        unsigned int f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fs, NULL);
        glCompileShader(f);

        unsigned int prog = glCreateProgram();
        glAttachShader(prog, v);
        glAttachShader(prog, f);
        glLinkProgram(prog);
        glDeleteShader(v);
        glDeleteShader(f);
        return prog;
    };

    unsigned int planetShader = compileShaderProgram(planetVS, fragmentSource);
    unsigned int asteroidShader = compileShaderProgram(asteroidVS, fragmentSource);

    Model rock("resources/objects/rock/rock.obj");
    Model planet("resources/objects/planet/planet.obj");

    // Generate random model matrices for asteroids
    unsigned int amount = 10000;
    glm::mat4* modelMatrices = new glm::mat4[amount];

    srand(static_cast<unsigned int>(glfwGetTime()));
    float radius = 50.0f;
    float offsetRange = 10.0f;

    for (unsigned int i = 0; i < amount; i++)
    {
        glm::mat4 model = glm::mat4(1.0f);

        float angle = (float)i / (float)amount * 360.0f;
        float displacement;

        displacement = (rand() % (int)(2 * offsetRange * 100)) / 100.0f - offsetRange;
        float x = sin(glm::radians(angle)) * radius + displacement;

        displacement = (rand() % (int)(2 * offsetRange * 100)) / 100.0f - offsetRange;
        float y = displacement * 0.2f;

        displacement = (rand() % (int)(2 * offsetRange * 100)) / 100.0f - offsetRange;
        float z = cos(glm::radians(angle)) * radius + displacement;

        model = glm::translate(model, glm::vec3(x, y, z));

        float scale = static_cast<float>((rand() % 20) / 100.0 + 0.05);
        model = glm::scale(model, glm::vec3(scale));

        float rotAngle = static_cast<float>(rand() % 360);
        model = glm::rotate(model, glm::radians(rotAngle),
                            glm::vec3(0.4f, 0.6f, 0.8f));

        modelMatrices[i] = model;
    }

    // Store instance matrices in a VBO
    unsigned int instanceVBO;
    glGenBuffers(1, &instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, amount * sizeof(glm::mat4),
                 &modelMatrices[0], GL_STATIC_DRAW);

    // Configure instanced attributes for each mesh in the rock model
    for (unsigned int i = 0; i < rock.meshes.size(); i++)
    {
        unsigned int VAO = rock.meshes[i].VAO;
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

        std::size_t vec4Size = sizeof(glm::vec4);
        for (int col = 0; col < 4; col++)
        {
            glEnableVertexAttribArray(3 + col);
            glVertexAttribPointer(3 + col, 4, GL_FLOAT, GL_FALSE,
                                  sizeof(glm::mat4),
                                  (void*)(col * vec4Size));
            glVertexAttribDivisor(3 + col, 1);
        }
        glBindVertexArray(0);
    }

    delete[] modelMatrices;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 200.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // Draw planet
        glUseProgram(planetShader);
        glUniformMatrix4fv(glGetUniformLocation(planetShader, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(planetShader, "view"),
                           1, GL_FALSE, glm::value_ptr(view));
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(4.0f));
        glUniformMatrix4fv(glGetUniformLocation(planetShader, "model"),
                           1, GL_FALSE, glm::value_ptr(model));
        planet.Draw(planetShader);

        // Draw asteroids (instanced)
        glUseProgram(asteroidShader);
        glUniformMatrix4fv(glGetUniformLocation(asteroidShader, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(asteroidShader, "view"),
                           1, GL_FALSE, glm::value_ptr(view));

        for (unsigned int i = 0; i < rock.meshes.size(); i++)
        {
            glBindVertexArray(rock.meshes[i].VAO);
            glDrawElementsInstanced(GL_TRIANGLES,
                                    static_cast<unsigned int>(
                                        rock.meshes[i].indices.size()),
                                    GL_UNSIGNED_INT, 0, amount);
            glBindVertexArray(0);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(1, &instanceVBO);
    glDeleteProgram(planetShader);
    glDeleteProgram(asteroidShader);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) { lastX = xpos; lastY = ypos; firstMouse = false; }
    camera.ProcessMouseMovement(xpos - lastX, lastY - ypos);
    lastX = xpos;
    lastY = ypos;
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
```

With instancing, 10,000 asteroids render comfortably at high frame rates. Without it, you'd be lucky to get 10 FPS. The difference is dramatic.

---

## Performance Comparison

Here's roughly what you'd see on a mid-range GPU:

| Method | Asteroids | Draw Calls | Approx. FPS |
|---|---|---|---|
| Individual draw calls | 1,000 | 1,000 | ~30 |
| Individual draw calls | 10,000 | 10,000 | ~3 |
| Instanced rendering | 10,000 | 1 | ~60+ |
| Instanced rendering | 100,000 | 1 | ~30+ |

The numbers vary by hardware, but the pattern is clear: instancing moves the bottleneck from CPU overhead to actual GPU rendering work, which is where it should be.

---

## Key Takeaways

- **Draw calls are expensive** because of CPU-side overhead (state validation, uniform uploads, driver communication). Minimizing draw calls is one of the most impactful optimizations.
- **`glDrawArraysInstanced`** and **`glDrawElementsInstanced`** render the same mesh multiple times in a single call.
- **`gl_InstanceID`** is a built-in vertex shader variable that uniquely identifies each instance (0 to instanceCount - 1).
- **Uniform arrays** work for small instance counts but hit size limits quickly (~1024 vec4s).
- **Instanced arrays** (vertex attributes with `glVertexAttribDivisor(index, 1)`) store per-instance data in a VBO with no practical size limit.
- A **`mat4` requires 4 vertex attribute slots** (locations N through N+3) since the maximum attribute size is `vec4`. Configure each column separately with `glVertexAttribPointer`.
- For maximum performance, configure instanced attributes in each mesh's VAO and draw with `glDrawElementsInstanced`.

## Exercises

1. Modify the 100-quad example to use **instanced arrays** (if you followed along with the uniform version). Then increase to 10,000 quads and verify it still runs smoothly.
2. Add per-instance **color** as an additional instanced attribute. Give each quad (or asteroid) a unique tint by storing a `vec3` color per instance.
3. Make the asteroid field **rotate** over time. Each frame, update the instance VBO with modified model matrices (multiply each by a rotation around the Y axis). Compare performance when using `glBufferData` (orphaning) vs. `glBufferSubData`.
4. Implement instanced rendering for a **forest scene**: load a tree model and render thousands of trees scattered on a flat plane. Add random Y-axis rotation and slight scale variation.
5. (Challenge) Implement **frustum culling** for instances. Before uploading the instance buffer each frame, check which asteroids are in the camera's view frustum. Only include visible ones. Measure the FPS improvement.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Geometry Shader](09_geometry_shader.md) | | [Next: Anti-Aliasing →](11_anti_aliasing.md) |
