# Transformations

[Previous: Textures](07_textures.md) | [Next: Coordinate Systems](09_coordinate_systems.md)

---

So far, all our geometry has been static — a triangle or rectangle sitting at fixed coordinates. To make objects move, rotate, and scale, we need **transformations**. And transformations are built on top of **linear algebra**: vectors and matrices.

Don't worry if math isn't your favorite subject. We'll cover exactly what you need, with clear explanations and diagrams, and then immediately put it into practice.

## Vectors

A **vector** is a quantity with both a **magnitude** (length) and a **direction**. In graphics, we use vectors constantly to represent positions, directions, velocities, and more.

A 3D vector has three components:

```
    ┌   ┐
v = │ x │
    │ y │
    │ z │
    └   ┘
```

Visually, think of a vector as an arrow pointing from the origin to the point `(x, y, z)`:

```
        y
        ↑
        |   / v = (2, 3)
        |  /
        | /
        |/________→ x
```

### Vector Addition and Subtraction

Adding two vectors combines their components:

```
a + b = (a.x + b.x, a.y + b.y, a.z + b.z)

         b
        ──→
       /    \
  a   /      ↘ a + b
 ──→ /
```

Subtraction gives you the vector *from* `b` *to* `a`:

```
a - b = (a.x - b.x, a.y - b.y, a.z - b.z)
```

This is how you compute the direction from one point to another.

### Vector Length (Magnitude)

The length of a vector is computed with the Pythagorean theorem:

```
|v| = sqrt(x² + y² + z²)
```

For example, `|(3, 4, 0)| = sqrt(9 + 16 + 0) = sqrt(25) = 5`.

### Normalization

A **unit vector** has length 1. Normalizing a vector scales it to unit length while preserving direction:

```
v̂ = v / |v|
```

Unit vectors are used for directions (e.g., "which way is the camera facing?") where magnitude doesn't matter.

### Dot Product

The **dot product** of two vectors yields a **scalar**:

```
a · b = a.x*b.x + a.y*b.y + a.z*b.z
```

It's also related to the angle between the vectors:

```
a · b = |a| * |b| * cos(θ)
```

This makes the dot product extremely useful:

- If `a` and `b` are unit vectors: `a · b = cos(θ)`, directly giving the cosine of the angle between them
- `a · b > 0`: vectors point in roughly the same direction (angle < 90°)
- `a · b = 0`: vectors are perpendicular (angle = 90°)
- `a · b < 0`: vectors point in roughly opposite directions (angle > 90°)

```
    a · b > 0          a · b = 0          a · b < 0

      a                   a                   a
     ↗                    ↑                  ↗
    /  θ < 90°            | θ = 90°         /
   /                      |                / θ > 90°
  ·────→ b                ·────→ b        ·
                                           \
                                            ↘ b
```

### Cross Product

The **cross product** of two 3D vectors produces a **new vector** that is **perpendicular** to both inputs:

```
a × b = (a.y*b.z - a.z*b.y,
         a.z*b.x - a.x*b.z,
         a.x*b.y - a.y*b.x)
```

The direction follows the **right-hand rule**: point your index finger along `a`, curl your fingers toward `b`, and your thumb points in the direction of `a × b`.

```
        a × b
          ↑
          |
          |
    a ────·────→ b
         /
        /
```

The cross product is essential for computing surface normals and building coordinate frames (e.g., the camera's right vector).

## Matrices

A **matrix** is a rectangular grid of numbers. In graphics, we mostly use **4×4 matrices** to encode transformations.

```
         ┌                 ┐
    M =  │ m00  m01  m02  m03 │
         │ m10  m11  m12  m13 │
         │ m20  m21  m22  m23 │
         │ m30  m31  m32  m33 │
         └                 ┘
```

### Matrix Multiplication

To transform a vector by a matrix, we multiply them. The key rules:

- You can multiply an `m×n` matrix by an `n×p` matrix to get an `m×p` result
- **Order matters**: `A * B ≠ B * A` in general
- The **identity matrix** `I` is the matrix equivalent of multiplying by 1: `I * v = v`

```
Identity matrix:

    ┌             ┐
I = │ 1  0  0  0  │
    │ 0  1  0  0  │
    │ 0  0  1  0  │
    │ 0  0  0  1  │
    └             ┘
```

## Transformation Matrices

Now for the exciting part: encoding **scaling**, **rotation**, and **translation** as matrices.

### Scaling

A scaling matrix multiplies each component by a scale factor:

```
    ┌              ┐   ┌   ┐     ┌        ┐
    │ Sx  0   0  0 │   │ x │     │ Sx * x │
    │ 0   Sy  0  0 │ × │ y │  =  │ Sy * y │
    │ 0   0   Sz 0 │   │ z │     │ Sz * z │
    │ 0   0   0  1 │   │ 1 │     │   1    │
    └              ┘   └   ┘     └        ┘
```

- `Sx = Sy = Sz = 2`: double the size in all dimensions
- `Sx = 0.5, Sy = 1, Sz = 1`: compress horizontally by half

### Translation

To move (translate) an object, we add an offset to its position. This is why we need **4×4 matrices** and **homogeneous coordinates** (adding a `w = 1` component to our 3D vectors):

```
    ┌              ┐   ┌   ┐     ┌        ┐
    │ 1  0  0  Tx  │   │ x │     │ x + Tx │
    │ 0  1  0  Ty │ × │ y │  =  │ y + Ty │
    │ 0  0  1  Tz  │   │ z │     │ z + Tz │
    │ 0  0  0  1   │   │ 1 │     │   1    │
    └              ┘   └   ┘     └        ┘
```

With a 3×3 matrix, there's no way to represent translation — this is precisely why homogeneous coordinates (4D) are used throughout graphics.

> **Homogeneous Coordinates:** A 3D point `(x, y, z)` is represented as `(x, y, z, 1)`. The `w` component of `1` enables translation. Vectors (directions without position) use `w = 0`, which makes them immune to translation — exactly what we want for directions.

### Rotation

Rotation is more complex. Here are the matrices for rotating by angle `θ` around each axis:

**Rotation around the X axis:**

```
    ┌                    ┐
    │ 1    0       0   0 │
    │ 0   cos θ  -sin θ 0 │
    │ 0   sin θ   cos θ 0 │
    │ 0    0       0   1 │
    └                    ┘
```

**Rotation around the Y axis:**

```
    ┌                     ┐
    │  cos θ  0   sin θ  0 │
    │   0     1    0     0 │
    │ -sin θ  0   cos θ  0 │
    │   0     0    0     1 │
    └                     ┘
```

**Rotation around the Z axis:**

```
    ┌                    ┐
    │ cos θ  -sin θ  0  0 │
    │ sin θ   cos θ  0  0 │
    │  0       0     1  0 │
    │  0       0     0  1 │
    └                    ┘
```

For rotation around an **arbitrary axis** `(Rx, Ry, Rz)`, the formula is more involved — but in practice we let libraries handle it.

### Combining Transformations

You combine transformations by multiplying their matrices. Since matrix multiplication is read right-to-left, the transformation applied **first** goes on the **right**:

```
Final = Translation * Rotation * Scale
```

This means: first scale, then rotate, then translate.

```
Typical order:  Scale  →  Rotate  →  Translate

   Original        Scaled         Rotated        Translated
   ┌──┐            ┌────┐          ╱╲              ╱╲
   │  │     →      │    │    →    ╱  ╲      →    ╱  ╲  ← now at
   └──┘            └────┘        ╱────╲         ╱────╲   new position
```

> **Warning:** Order matters! Translating then rotating moves the object to a new position and then rotates it *around the origin* (creating an orbital motion). Rotating then translating rotates the object in place and then moves it. Try both in the exercises to see the difference.

## GLM (OpenGL Mathematics)

Writing matrix math by hand is error-prone and tedious. **GLM** (OpenGL Mathematics) is a header-only C++ library designed to mirror GLSL math types and functions. It's the de facto standard for OpenGL math in C++.

### Setup

GLM is header-only — just include the headers:

```cpp
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
```

- `glm/glm.hpp` — core types: `glm::vec2`, `glm::vec3`, `glm::vec4`, `glm::mat4`
- `glm/gtc/matrix_transform.hpp` — `glm::translate`, `glm::rotate`, `glm::scale`, `glm::perspective`, `glm::lookAt`
- `glm/gtc/type_ptr.hpp` — `glm::value_ptr()` to get a raw float pointer for OpenGL

### Basic Usage

```cpp
glm::vec4 vec(1.0f, 0.0f, 0.0f, 1.0f);

// Start with identity matrix
glm::mat4 trans = glm::mat4(1.0f);

// Apply a translation of (1, 1, 0)
trans = glm::translate(trans, glm::vec3(1.0f, 1.0f, 0.0f));

// Transform the vector
vec = trans * vec;
// vec is now (2, 1, 0, 1)
```

Each GLM transformation function takes an existing matrix and returns a new matrix with the transformation applied. This makes it easy to chain:

```cpp
glm::mat4 trans = glm::mat4(1.0f);
trans = glm::translate(trans, glm::vec3(0.5f, -0.5f, 0.0f));
trans = glm::rotate(trans, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
trans = glm::scale(trans, glm::vec3(0.5f, 0.5f, 0.5f));
```

Remember, the last transformation in code (`scale`) is applied **first** to the vertices. Reading bottom-up: scale → rotate → translate.

> **Note:** `glm::rotate` takes the angle in **radians**. Use `glm::radians()` to convert from degrees.

### Passing Matrices to OpenGL

To send a `glm::mat4` to a shader uniform:

```cpp
unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));
```

- `1`: number of matrices to upload
- `GL_FALSE`: don't transpose the matrix (GLM already uses column-major order, matching OpenGL)
- `glm::value_ptr(trans)`: pointer to the first element of the matrix data

## Applying Transformations to Our Textured Rectangle

Let's modify the project from the Textures chapter to add transformations.

### Updated Vertex Shader

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec3 ourColor;
out vec2 TexCoord;

uniform mat4 transform;

void main() {
    gl_Position = transform * vec4(aPos, 1.0);
    ourColor = aColor;
    TexCoord = aTexCoord;
}
```

The key change: instead of `gl_Position = vec4(aPos, 1.0)`, we multiply by our `transform` matrix.

### Creating the Transformation in C++

```cpp
glm::mat4 trans = glm::mat4(1.0f);
trans = glm::translate(trans, glm::vec3(0.5f, -0.5f, 0.0f));
trans = glm::rotate(trans, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));
```

This translates the rectangle to the bottom-right and rotates it 90 degrees around the Z axis.

## Rotating Over Time

For a continuously spinning rectangle, use `glfwGetTime()` inside the render loop:

```cpp
while (!glfwWindowShouldClose(window)) {
    processInput(window);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Create transformation with time-based rotation
    glm::mat4 trans = glm::mat4(1.0f);
    trans = glm::translate(trans, glm::vec3(0.5f, -0.5f, 0.0f));
    trans = glm::rotate(trans, (float)glfwGetTime(), glm::vec3(0.0f, 0.0f, 1.0f));

    unsigned int transformLoc = glGetUniformLocation(shaderProgram, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

    // Bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture2);

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glfwSwapBuffers(window);
    glfwPollEvents();
}
```

`glfwGetTime()` returns seconds since GLFW was initialized, providing a smoothly increasing angle in radians.

## Complete Source Code

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec3 ourColor;
out vec2 TexCoord;

uniform mat4 transform;

void main() {
    gl_Position = transform * vec4(aPos, 1.0);
    ourColor = aColor;
    TexCoord = aTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

uniform sampler2D texture1;
uniform sampler2D texture2;

void main() {
    FragColor = mix(texture(texture1, TexCoord),
                    texture(texture2, TexCoord),
                    0.2);
}
)";

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "Transformations",
                                          NULL, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Compile shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "Vertex shader error:\n" << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "Fragment shader error:\n" << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "Linking error:\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Vertex data
    float vertices[] = {
        // positions          // colors           // texture coords
         0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f
    };
    unsigned int indices[] = {
        0, 1, 3,
        1, 2, 3
    };

    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
                 GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Load textures
    unsigned int texture1, texture2;

    glGenTextures(1, &texture1);
    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load("container.jpg", &width, &height,
                                    &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    stbi_image_free(data);

    glGenTextures(1, &texture2);
    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    data = stbi_load("awesomeface.png", &width, &height, &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    stbi_image_free(data);

    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture2"), 1);

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture2);

        // Continuously rotating container in the bottom-right
        glm::mat4 trans = glm::mat4(1.0f);
        trans = glm::translate(trans, glm::vec3(0.5f, -0.5f, 0.0f));
        trans = glm::rotate(trans, (float)glfwGetTime(),
                            glm::vec3(0.0f, 0.0f, 1.0f));

        glUseProgram(shaderProgram);
        unsigned int transformLoc = glGetUniformLocation(shaderProgram,
                                                         "transform");
        glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(trans));

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
```

## Key Takeaways

- **Vectors** represent positions and directions; key operations are dot product (angles/projection) and cross product (perpendicular vectors).
- **Matrices** encode transformations; 4×4 matrices with homogeneous coordinates allow scaling, rotation, *and* translation.
- **Transformation order matters:** The rightmost transformation in a matrix product is applied first. The typical order is Scale → Rotate → Translate.
- **GLM** provides GLSL-compatible math types and functions for C++.
- Pass matrices to shaders with `glUniformMatrix4fv` and `glm::value_ptr()`.
- Use `glfwGetTime()` for time-based animations.

## Exercises

1. **Translate then rotate:** Instead of translating first and then rotating, try rotating first and then translating. What happens? The container should orbit around the origin instead of spinning in place. Understand *why* by thinking about what each transformation does.

2. **Second container:** Draw a second container in the top-left corner using another transformation matrix. Make this one scale over time using `sin(glfwGetTime())` to pulse between small and large.

3. **Manual matrix:** Try constructing a translation matrix manually (as a `glm::mat4` with values filled in) without using `glm::translate`, and verify it produces the same result. This builds intuition for what the library does under the hood.

---

[Previous: Textures](07_textures.md) | [Next: Coordinate Systems](09_coordinate_systems.md)
