# Geometry Shader

So far our shader pipeline has consisted of two programmable stages: the vertex shader transforms each vertex, and the fragment shader colors each pixel. OpenGL has a third programmable stage that sits between these two — the **geometry shader**. This optional stage receives entire primitives (points, lines, or triangles) from the vertex shader and can generate *new* geometry on the fly: transforming, discarding, or even creating additional primitives before they reach the fragment shader.

Geometry shaders are a powerful tool for effects like exploding objects, visualizing normals, generating shadow volumes, and creating billboards from single points. In this chapter we'll build up from the basics to two practical effects: exploding meshes and normal visualization.

---

## Where the Geometry Shader Fits

The geometry shader sits between the vertex shader and the rasterizer:

```
Vertex Data
    │
    ▼
┌──────────────────┐
│  Vertex Shader   │  Per-vertex: transforms positions, passes attributes
└──────────────────┘
    │
    ▼
┌──────────────────┐
│ Geometry Shader  │  Per-primitive: receives assembled primitives,
│   (optional)     │  can emit 0 or more new primitives
└──────────────────┘
    │
    ▼
┌──────────────────┐
│   Rasterizer     │  Converts primitives to fragments
└──────────────────┘
    │
    ▼
┌──────────────────┐
│ Fragment Shader  │  Per-fragment: computes final color
└──────────────────┘
```

The vertex shader processes vertices one at a time — it has no knowledge of the primitive a vertex belongs to. The geometry shader, by contrast, receives all vertices of a complete primitive at once and can reason about the shape as a whole.

---

## Basic Syntax

A geometry shader always starts with two layout qualifiers:

1. **Input layout** — what kind of primitive you're receiving
2. **Output layout** — what kind of primitive you're emitting, plus the maximum number of vertices

```glsl
#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

void main()
{
    // Process the input primitive and emit output vertices
}
```

### Input Layout Qualifiers

The input layout must match (or be compatible with) the draw mode used in your `glDrawArrays` / `glDrawElements` call:

| Layout Qualifier | Draw Mode | Vertices per Primitive |
|---|---|---|
| `points` | `GL_POINTS` | 1 |
| `lines` | `GL_LINES`, `GL_LINE_STRIP`, `GL_LINE_LIST` | 2 |
| `lines_adjacency` | `GL_LINES_ADJACENCY` | 4 |
| `triangles` | `GL_TRIANGLES`, `GL_TRIANGLE_STRIP`, `GL_TRIANGLE_FAN` | 3 |
| `triangles_adjacency` | `GL_TRIANGLES_ADJACENCY` | 6 |

### Output Layout Qualifiers

| Layout Qualifier | Description |
|---|---|
| `points` | Emit individual points |
| `line_strip` | Emit connected line segments |
| `triangle_strip` | Emit connected triangles |

The `max_vertices` parameter is required and tells OpenGL the maximum number of vertices this shader will ever emit in a single invocation. Keep this as small as possible — the driver allocates output space based on it.

### Accessing Input Vertices

Inside the geometry shader, input vertices are accessed through the built-in array `gl_in`:

```glsl
in gl_PerVertex
{
    vec4 gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[];
} gl_in[];
```

The size of `gl_in` depends on the input layout: 1 for `points`, 2 for `lines`, 3 for `triangles`, etc.

### Emitting Vertices

To produce output, you set `gl_Position` (and any `out` variables), call `EmitVertex()`, and repeat. When you've finished a primitive, call `EndPrimitive()`:

```glsl
gl_Position = gl_in[0].gl_Position;
EmitVertex();

gl_Position = gl_in[1].gl_Position;
EmitVertex();

gl_Position = gl_in[2].gl_Position;
EmitVertex();

EndPrimitive();
```

Each call to `EmitVertex()` records the current values of `gl_Position` and all output variables. After `EndPrimitive()`, the next `EmitVertex()` starts a new primitive.

---

## A Simple Pass-Through Geometry Shader

The simplest possible geometry shader receives a triangle and re-emits it unchanged:

```glsl
#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

void main()
{
    for (int i = 0; i < 3; i++)
    {
        gl_Position = gl_in[i].gl_Position;
        EmitVertex();
    }
    EndPrimitive();
}
```

This is the geometry shader equivalent of "hello world." The output is identical to having no geometry shader at all. But it confirms the pipeline is wired up correctly and serves as a template for more interesting effects.

---

## Drawing Houses from Points

Let's do something more creative: input a set of `GL_POINTS`, and for each point, emit a house shape (a square with a triangular roof) using a triangle strip.

```
    /\
   /  \         ◀── roof (triangle)
  /    \
 ┌──────┐
 │      │      ◀── walls (square from triangle strip)
 │      │
 └──────┘
```

A triangle strip can form this shape with 5 vertices:

```
      4
     / \
    /   \
   3─────5
   │     │
   │     │
   1─────2

Triangle strip order: 1, 2, 3, 4, 5
  → Triangle 1: 1-2-3 (bottom-left)
  → Triangle 2: 2-3-4 (connects to roof)
  → Triangle 3: 3-4-5 (roof peak)
  Wait — that's not quite right for a house.
```

Actually, let's be precise. We want a square base plus a triangular roof. One way:

```
Vertex order for triangle strip:
  1 (bottom-left) → 2 (bottom-right) → 3 (top-left) → 4 (top-right) → 5 (roof peak)

Triangles formed:
  1-2-3  (lower-left triangle of square)
  2-3-4  (upper-right triangle of square)
  3-4-5  (roof triangle)
```

```
   5 (roof peak)
   /\
  3──4
  │  │
  1──2
```

Here's the geometry shader:

```glsl
#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 5) out;

out vec3 gColor;

void main()
{
    vec4 pos = gl_in[0].gl_Position;

    // Bottom-left
    gColor = vec3(1.0, 0.0, 0.0);
    gl_Position = pos + vec4(-0.1, -0.1, 0.0, 0.0);
    EmitVertex();

    // Bottom-right
    gColor = vec3(0.0, 1.0, 0.0);
    gl_Position = pos + vec4( 0.1, -0.1, 0.0, 0.0);
    EmitVertex();

    // Top-left
    gColor = vec3(1.0, 1.0, 0.0);
    gl_Position = pos + vec4(-0.1,  0.1, 0.0, 0.0);
    EmitVertex();

    // Top-right
    gColor = vec3(0.0, 1.0, 1.0);
    gl_Position = pos + vec4( 0.1,  0.1, 0.0, 0.0);
    EmitVertex();

    // Roof peak
    gColor = vec3(1.0, 1.0, 1.0);
    gl_Position = pos + vec4( 0.0,  0.2, 0.0, 0.0);
    EmitVertex();

    EndPrimitive();
}
```

### Complete House-Drawing Program

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

out VS_OUT {
    vec3 color;
} vs_out;

void main()
{
    gl_Position = vec4(aPos, 0.0, 1.0);
    vs_out.color = aColor;
}
)";

const char* geometryShaderSource = R"(
#version 330 core
layout (points) in;
layout (triangle_strip, max_vertices = 5) out;

in VS_OUT {
    vec3 color;
} gs_in[];

out vec3 fColor;

void build_house(vec4 position)
{
    fColor = gs_in[0].color;

    // Bottom-left
    gl_Position = position + vec4(-0.1, -0.1, 0.0, 0.0);
    EmitVertex();
    // Bottom-right
    gl_Position = position + vec4( 0.1, -0.1, 0.0, 0.0);
    EmitVertex();
    // Top-left
    gl_Position = position + vec4(-0.1,  0.1, 0.0, 0.0);
    EmitVertex();
    // Top-right
    gl_Position = position + vec4( 0.1,  0.1, 0.0, 0.0);
    EmitVertex();
    // Roof peak (white)
    fColor = vec3(1.0, 1.0, 1.0);
    gl_Position = position + vec4( 0.0,  0.2, 0.0, 0.0);
    EmitVertex();
    EndPrimitive();
}

void main()
{
    build_house(gl_in[0].gl_Position);
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

unsigned int compileShader(unsigned int type, const char* source)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        std::cout << "Shader compilation failed:\n" << log << std::endl;
    }
    return shader;
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
                                          "Geometry Shader - Houses", NULL, NULL);
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

    // Compile and link shaders (vertex + geometry + fragment)
    unsigned int vertShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int geomShader = compileShader(GL_GEOMETRY_SHADER, geometryShaderSource);
    unsigned int fragShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertShader);
    glAttachShader(shaderProgram, geomShader);
    glAttachShader(shaderProgram, fragShader);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, log);
        std::cout << "Program linking failed:\n" << log << std::endl;
    }

    glDeleteShader(vertShader);
    glDeleteShader(geomShader);
    glDeleteShader(fragShader);

    // Point data: position (x,y) + color (r,g,b)
    float points[] = {
        // pos          // color
        -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,   // top-left (red)
         0.5f,  0.5f,  0.0f, 1.0f, 0.0f,   // top-right (green)
         0.5f, -0.5f,  0.0f, 0.0f, 1.0f,   // bottom-right (blue)
        -0.5f, -0.5f,  1.0f, 1.0f, 0.0f    // bottom-left (yellow)
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_POINTS, 0, 4);

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

We draw 4 `GL_POINTS`, but on screen you see 4 colorful houses — each one generated entirely by the geometry shader from a single point!

---

## Passing Data Through the Geometry Shader

When a geometry shader is present, data flows: **Vertex → Geometry → Fragment**. Interface blocks make this clean. The vertex shader outputs to a block, the geometry shader receives it as an array of blocks (one per input vertex), and the geometry shader outputs to its own block that the fragment shader reads.

```glsl
// --- Vertex Shader ---
out VS_OUT {
    vec3 Normal;
    vec2 TexCoords;
} vs_out;

// --- Geometry Shader ---
in VS_OUT {
    vec3 Normal;
    vec2 TexCoords;
} gs_in[];    // array: one per input vertex

out GS_OUT {
    vec3 Normal;
    vec2 TexCoords;
} gs_out;

// Before each EmitVertex(), set gs_out members:
gs_out.Normal = gs_in[i].Normal;
gs_out.TexCoords = gs_in[i].TexCoords;
EmitVertex();

// --- Fragment Shader ---
in GS_OUT {
    vec3 Normal;
    vec2 TexCoords;
} fs_in;
```

Remember: the **block name** (e.g., `VS_OUT`) must match between stages, but the **instance name** (e.g., `gs_in`, `fs_in`) can differ. In the geometry shader, inputs are arrays because each invocation receives multiple vertices (the whole primitive).

---

## Practical Effect: Exploding Objects

Now let's build something dramatic. We'll make a 3D model "explode" — each triangle flies outward along its face normal. The geometry shader calculates each triangle's face normal and translates all three vertices along that direction, with the displacement increasing over time.

```
Normal state:              Exploding:
    ┌───────┐               ╲   ╱
    │       │                 ╲ ╱
    │  ☺    │              ──  ☺  ──
    │       │                 ╱ ╲
    └───────┘               ╱   ╲
                    Triangles fly outward along face normals
```

### The Math

For each triangle with vertices `p0`, `p1`, `p2`:

1. Compute two edge vectors: `e1 = p1 - p0`, `e2 = p2 - p0`
2. Face normal: `n = normalize(cross(e1, e2))`
3. Translate each vertex: `p_new = p + n * magnitude`

The `magnitude` increases over time, making the triangles fly apart.

### Exploding Geometry Shader

```glsl
#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in VS_OUT {
    vec2 texCoords;
} gs_in[];

out vec2 TexCoords;

uniform float time;

vec3 GetNormal()
{
    vec3 a = vec3(gl_in[0].gl_Position) - vec3(gl_in[1].gl_Position);
    vec3 b = vec3(gl_in[2].gl_Position) - vec3(gl_in[1].gl_Position);
    return normalize(cross(a, b));
}

vec4 explode(vec4 position, vec3 normal)
{
    float magnitude = 2.0;
    vec3 direction = normal * ((sin(time) + 1.0) / 2.0) * magnitude;
    return position + vec4(direction, 0.0);
}

void main()
{
    vec3 normal = GetNormal();

    gl_Position = explode(gl_in[0].gl_Position, normal);
    TexCoords = gs_in[0].texCoords;
    EmitVertex();

    gl_Position = explode(gl_in[1].gl_Position, normal);
    TexCoords = gs_in[1].texCoords;
    EmitVertex();

    gl_Position = explode(gl_in[2].gl_Position, normal);
    TexCoords = gs_in[2].texCoords;
    EmitVertex();

    EndPrimitive();
}
```

The `sin(time)` creates a pulsing effect — the triangles spread outward and then come back together, oscillating.

### Supporting Shaders

**Vertex shader** — transforms to clip space and passes data through:

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT {
    vec2 texCoords;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    vs_out.texCoords = aTexCoords;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

**Fragment shader** — samples the texture:

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
uniform sampler2D texture_diffuse1;

void main()
{
    FragColor = texture(texture_diffuse1, TexCoords);
}
```

### Complete Exploding Object Program

This example assumes you have the `Shader`, `Model`, and `Camera` classes from the model loading chapters. The key difference from a normal model rendering program is the addition of the geometry shader:

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

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
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
                                          "Exploding Objects", NULL, NULL);
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

    // The Shader class accepts an optional geometry shader path
    Shader explodeShader("explode.vs", "explode.fs", "explode.gs");

    Model backpack("resources/objects/backpack/backpack.obj");

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);

        explodeShader.use();
        explodeShader.setMat4("projection", projection);
        explodeShader.setMat4("view", view);
        explodeShader.setMat4("model", model);
        explodeShader.setFloat("time", currentFrame);

        backpack.Draw(explodeShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

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

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
```

The model oscillates between its normal shape and a fully exploded form. Each triangle moves along its own face normal, creating a satisfying "blow apart" effect.

---

## Practical Effect: Visualizing Normals

Debugging lighting issues is much easier when you can *see* the normals. We can use a geometry shader to draw a small line from each vertex in the direction of its normal.

```
              ↑ normal
              │
   ───────●───────
          vertex
```

The approach uses **two rendering passes**:

1. **Pass 1**: Render the model normally (standard vertex + fragment shader)
2. **Pass 2**: Render the model's normals as lines using a geometry shader

### Normal Visualization Shaders

**Vertex shader** (for the normal visualization pass):

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out VS_OUT {
    vec3 normal;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main()
{
    // Transform normal to view space (for correct line direction)
    mat3 normalMatrix = mat3(transpose(inverse(view * model)));
    vs_out.normal = normalize(vec3(vec4(normalMatrix * aNormal, 0.0)));

    gl_Position = view * model * vec4(aPos, 1.0);
}
```

Note that we output the position in *view space* (not clip space). The geometry shader will apply the projection after generating the line endpoints.

**Geometry shader** — generates a line for each vertex's normal:

```glsl
#version 330 core
layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

in VS_OUT {
    vec3 normal;
} gs_in[];

uniform mat4 projection;

const float MAGNITUDE = 0.2;

void GenerateLine(int index)
{
    gl_Position = projection * gl_in[index].gl_Position;
    EmitVertex();
    gl_Position = projection * (gl_in[index].gl_Position +
                                vec4(gs_in[index].normal, 0.0) * MAGNITUDE);
    EmitVertex();
    EndPrimitive();
}

void main()
{
    GenerateLine(0);
    GenerateLine(1);
    GenerateLine(2);
}
```

For each of the 3 vertices in the input triangle, we emit 2 vertices (a line segment): one at the vertex position and one at `position + normal * MAGNITUDE`. That's 3 line strips × 2 vertices = 6 total vertices — matching our `max_vertices`.

**Fragment shader** for the normals (simple solid color):

```glsl
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0, 1.0, 0.0, 1.0);  // bright yellow lines
}
```

### Two-Pass Rendering

In your render loop, draw the model twice:

```cpp
// Pass 1: render the model normally
normalShader.use();
normalShader.setMat4("projection", projection);
normalShader.setMat4("view", view);
normalShader.setMat4("model", model);
backpack.Draw(normalShader);

// Pass 2: render the normals as lines
normalVisShader.use();
normalVisShader.setMat4("projection", projection);
normalVisShader.setMat4("view", view);
normalVisShader.setMat4("model", model);
backpack.Draw(normalVisShader);
```

### Complete Normal Visualization Program

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

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Vertex + Fragment for normal model rendering
const char* modelVS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
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

const char* modelFS = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D texture_diffuse1;

void main()
{
    FragColor = texture(texture_diffuse1, TexCoords);
}
)";

// Normal visualization shaders
const char* normalVS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out VS_OUT {
    vec3 normal;
} vs_out;

uniform mat4 view;
uniform mat4 model;

void main()
{
    mat3 normalMatrix = mat3(transpose(inverse(view * model)));
    vs_out.normal = normalize(vec3(vec4(normalMatrix * aNormal, 0.0)));
    gl_Position = view * model * vec4(aPos, 1.0);
}
)";

const char* normalGS = R"(
#version 330 core
layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

in VS_OUT {
    vec3 normal;
} gs_in[];

uniform mat4 projection;

const float MAGNITUDE = 0.2;

void GenerateLine(int index)
{
    gl_Position = projection * gl_in[index].gl_Position;
    EmitVertex();
    gl_Position = projection * (gl_in[index].gl_Position +
                                vec4(gs_in[index].normal, 0.0) * MAGNITUDE);
    EmitVertex();
    EndPrimitive();
}

void main()
{
    GenerateLine(0);
    GenerateLine(1);
    GenerateLine(2);
}
)";

const char* normalFS = R"(
#version 330 core
out vec4 FragColor;
void main()
{
    FragColor = vec4(1.0, 1.0, 0.0, 1.0);
}
)";

unsigned int buildShaderProgram(const char* vs, const char* fs,
                                const char* gs = nullptr)
{
    auto compile = [](unsigned int type, const char* src) {
        unsigned int s = glCreateShader(type);
        glShaderSource(s, 1, &src, NULL);
        glCompileShader(s);
        int ok;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[512];
            glGetShaderInfoLog(s, 512, NULL, log);
            std::cout << "Shader error:\n" << log << std::endl;
        }
        return s;
    };

    unsigned int v = compile(GL_VERTEX_SHADER, vs);
    unsigned int f = compile(GL_FRAGMENT_SHADER, fs);
    unsigned int g = 0;
    if (gs) g = compile(GL_GEOMETRY_SHADER, gs);

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, v);
    glAttachShader(prog, f);
    if (gs) glAttachShader(prog, g);
    glLinkProgram(prog);

    int ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetProgramInfoLog(prog, 512, NULL, log);
        std::cout << "Link error:\n" << log << std::endl;
    }

    glDeleteShader(v);
    glDeleteShader(f);
    if (gs) glDeleteShader(g);
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
                                          "Normal Visualization", NULL, NULL);
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

    unsigned int modelShader = buildShaderProgram(modelVS, modelFS);
    unsigned int normalVisShader = buildShaderProgram(normalVS, normalFS, normalGS);

    Model backpack("resources/objects/backpack/backpack.obj");

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 model = glm::mat4(1.0f);

        // Pass 1: draw the model normally
        glUseProgram(modelShader);
        glUniformMatrix4fv(glGetUniformLocation(modelShader, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(modelShader, "view"),
                           1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(modelShader, "model"),
                           1, GL_FALSE, glm::value_ptr(model));
        backpack.Draw(modelShader);

        // Pass 2: draw the normal lines
        glUseProgram(normalVisShader);
        glUniformMatrix4fv(glGetUniformLocation(normalVisShader, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(normalVisShader, "view"),
                           1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(normalVisShader, "model"),
                           1, GL_FALSE, glm::value_ptr(model));
        backpack.Draw(normalVisShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(modelShader);
    glDeleteProgram(normalVisShader);

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

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
```

You'll see the backpack model rendered normally, with bright yellow lines sticking out of every vertex showing the normal direction. This is an invaluable debugging tool when lighting doesn't look right — you can immediately see if normals are flipped, missing, or pointing in unexpected directions.

---

## Key Takeaways

- The **geometry shader** is an optional stage between vertex and fragment shaders that operates on complete primitives.
- It receives all vertices of a primitive at once via `gl_in[]` and can emit zero or more new primitives using `EmitVertex()` and `EndPrimitive()`.
- **Input layout** (`points`, `lines`, `triangles`, etc.) must match the draw mode. **Output layout** (`points`, `line_strip`, `triangle_strip`) plus `max_vertices` defines what you emit.
- Interface blocks pass data through the pipeline: vertex `out` → geometry `in[]` (array) → geometry `out` → fragment `in`.
- **Exploding objects**: calculate the face normal per triangle, translate vertices along it by a time-varying amount.
- **Normal visualization**: for each triangle, emit line segments from each vertex position to `position + normal * magnitude`. Use two rendering passes (model + normals).
- Geometry shaders can be expensive — they add processing for every primitive. Use them judiciously and keep `max_vertices` as low as possible.

## Exercises

1. Modify the house-drawing program to generate **4 houses per point** (one in each direction from the input point). You'll need to increase `max_vertices` and emit multiple `EndPrimitive()` calls.
2. Change the exploding geometry shader to also **shrink** each triangle as it moves outward. Scale each vertex toward the triangle's centroid as the explosion progresses.
3. Create a geometry shader that takes `GL_TRIANGLES` as input and outputs `line_strip` to render the model as a **wireframe**. For each triangle, emit 4 vertices (3 edges + close the loop).
4. Implement a geometry shader that **culls back-facing triangles** by checking the face normal against the view direction. Emit nothing for back-facing triangles.
5. (Challenge) Create a "grass" geometry shader: given a flat plane drawn with `GL_POINTS`, generate a grass blade (narrow triangle strip) at each point. Randomize the height and lean direction using the vertex position as a seed for a hash function.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Advanced GLSL](08_advanced_glsl.md) | | [Next: Instancing →](10_instancing.md) |
