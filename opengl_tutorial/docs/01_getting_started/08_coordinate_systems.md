# Coordinate Systems

[Previous: Transformations](07_transformations.md) | [Next: Camera](09_camera.md)

---

In the previous chapter, we learned to transform objects with matrices. But a single transformation matrix isn't enough to go from a 3D model to a 2D pixel on your screen. That journey passes through **five distinct coordinate systems**, each serving a specific purpose. Understanding this pipeline is the key to truly "getting" how 3D graphics work.

## The Five Coordinate Systems

A vertex starts its life in **local space** and ends up as a pixel in **screen space**. Here's the full pipeline:

```
Local Space ──Model Matrix──▶ World Space ──View Matrix──▶ View Space
     │
     ▼
Clip Space ◀──Projection Matrix── View Space
     │
     │  (Perspective Division + Viewport Transform)
     ▼
Screen Space
```

Or as a single equation:

```
gl_Position = Projection × View × Model × vec4(aPos, 1.0)
                  ▲          ▲       ▲
                  │          │       └── Local → World
                  │          └────────── World → View
                  └───────────────────── View  → Clip
```

Let's walk through each space.

### 1. Local (Object) Space

This is the coordinate system where your model is authored. A cube centered at the origin in Blender, for example, has vertices ranging from `(-0.5, -0.5, -0.5)` to `(0.5, 0.5, 0.5)`. All coordinates are relative to the object's own center.

### 2. World Space

World space is the global coordinate system of your entire scene. Each object has a **model matrix** that positions, rotates, and scales it from local space into world space.

For example, if you have two cubes, one at `(0, 0, 0)` and one at `(5, 0, 0)`, each has the same local-space vertices but different model matrices.

### 3. View (Eye/Camera) Space

View space is the world as seen from the camera's perspective. The **view matrix** transforms world coordinates so the camera is at the origin, looking down the negative Z axis. We'll build this matrix in the Camera chapter using `glm::lookAt`.

### 4. Clip Space

After the view transformation, the **projection matrix** transforms view-space coordinates into **clip space**. Clip space is a special coordinate system where:

- Anything outside the range `[-w, +w]` in X, Y, Z is **clipped** (not drawn)
- The `w` component becomes meaningful for perspective

The projection matrix defines the **viewing frustum** — the visible volume of the scene.

### 5. Screen Space

OpenGL performs **perspective division** (dividing by `w`) to go from clip space to **Normalized Device Coordinates (NDC)**, where everything visible is in `[-1, 1]` on all axes. Then the **viewport transform** (`glViewport`) maps NDC to actual pixel coordinates on your window.

```
Full pipeline for one vertex:

  Local       Model        World       View         Eye        Projection      Clip
  (x,y,z) ──────────▶ (x',y',z') ──────────▶ (x'',y'',z'') ──────────▶ (x,y,z,w)
                                                                              │
                                                           Perspective Division (÷w)
                                                                              │
                                                                              ▼
                                                                    NDC: (-1 to 1)
                                                                              │
                                                                   Viewport Transform
                                                                              │
                                                                              ▼
                                                                Screen: (pixel x, pixel y)
```

## The Model Matrix

The model matrix transforms vertices from local space to world space. It's the same transformation matrix we built in the previous chapter — a combination of scaling, rotation, and translation:

```cpp
glm::mat4 model = glm::mat4(1.0f);
model = glm::translate(model, glm::vec3(2.0f, 0.0f, 0.0f));
model = glm::rotate(model, glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
model = glm::scale(model, glm::vec3(1.5f, 1.5f, 1.5f));
```

Each object in the scene gets its own model matrix.

## The View Matrix

The view matrix simulates a camera. Instead of moving the camera, we move the **entire world** in the opposite direction. If the camera moves right, the world moves left.

For now, we'll place the camera slightly behind the scene (along positive Z, looking toward negative Z):

```cpp
glm::mat4 view = glm::mat4(1.0f);
view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));
```

We'll replace this with `glm::lookAt` in the Camera chapter.

## The Projection Matrix

The projection matrix defines *how* 3D coordinates map to the 2D screen. There are two types.

### Orthographic Projection

In orthographic projection, all parallel lines remain parallel. There's no sense of depth — objects far away appear the same size as objects close up. This is used for 2D games, UI rendering, and technical drawings.

```cpp
glm::mat4 projection = glm::ortho(left, right, bottom, top, near, far);
```

```
Orthographic frustum (a box):

         top
    ┌─────────────┐
    │             │
    │             │
left│    Scene    │right
    │             │
    │             │
    └─────────────┘
       bottom

Near plane ◄──────────► Far plane
(objects at any distance appear the same size)
```

Example:

```cpp
glm::mat4 projection = glm::ortho(0.0f, 800.0f, 0.0f, 600.0f, 0.1f, 100.0f);
```

### Perspective Projection

Perspective projection mimics how human eyes see: objects farther away appear smaller. This is what makes 3D scenes look realistic.

```cpp
glm::mat4 projection = glm::perspective(fov, aspectRatio, near, far);
```

```
Perspective frustum (a truncated pyramid):

            Near plane
             ┌───┐
            /     \
           /       \
          /         \
         /   Scene   \
        /             \
       /               \
      └─────────────────┘
            Far plane

Things farther away → appear smaller
```

Parameters:

- **fov**: Field of view angle (usually `45.0f` degrees), vertical
- **aspectRatio**: Window width / height (e.g., `800.0f / 600.0f`)
- **near**: Distance to the near clipping plane (e.g., `0.1f`) — must be > 0
- **far**: Distance to the far clipping plane (e.g., `100.0f`)

```cpp
glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f,
                                         0.1f, 100.0f);
```

### The w Component and Perspective Division

The perspective projection matrix manipulates the `w` component of the output vector. After projection, OpenGL automatically performs **perspective division**: dividing `x`, `y`, and `z` by `w`. This is what makes distant objects smaller — they get larger `w` values, so dividing shrinks their screen coordinates.

```
Before perspective division:  (x, y, z, w)
After perspective division:   (x/w, y/w, z/w)  →  NDC coordinates
```

## Putting It All Together: MVP

In the vertex shader, we apply all three matrices:

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
```

In C++:

```cpp
// Model: rotate the plane to lie on the "ground"
glm::mat4 model = glm::mat4(1.0f);
model = glm::rotate(model, glm::radians(-55.0f), glm::vec3(1.0f, 0.0f, 0.0f));

// View: move the camera back
glm::mat4 view = glm::mat4(1.0f);
view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

// Projection: perspective
glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                         800.0f / 600.0f, 0.1f, 100.0f);

// Send to shader
glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),
                   1, GL_FALSE, glm::value_ptr(model));
glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"),
                   1, GL_FALSE, glm::value_ptr(view));
glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"),
                   1, GL_FALSE, glm::value_ptr(projection));
```

Now our flat rectangle appears to recede into the screen — true 3D perspective!

## Going 3D: Depth Testing

There's a problem. When we draw 3D geometry, fragments from objects behind other objects can overwrite what's in front. We need **depth testing** (also called **Z-buffering**).

The **depth buffer** stores a depth value for every pixel on screen. When a fragment is about to be drawn, OpenGL compares its depth to the stored value. If the new fragment is closer, it overwrites both the color and depth; if it's farther, it's discarded.

Enable it:

```cpp
glEnable(GL_DEPTH_TEST);
```

And clear the depth buffer each frame alongside the color buffer:

```cpp
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
```

Without `GL_DEPTH_TEST`, faces drawn later always overwrite earlier ones regardless of depth, creating bizarre visual glitches.

## Drawing a Cube

Let's upgrade from a flat rectangle to a full 3D cube. A cube has 6 faces, each made of 2 triangles, totaling **36 vertices** (we use non-indexed drawing for simplicity):

```cpp
float vertices[] = {
    // positions          // texture coords
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
};
```

Vertex attribute setup (position + texCoord only, stride = 5 floats):

```cpp
// Position attribute
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);
// Texture coordinate attribute
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                      (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);
```

Draw with:

```cpp
glDrawArrays(GL_TRIANGLES, 0, 36);
```

To make the cube spin, use time-based rotation around a diagonal axis:

```cpp
glm::mat4 model = glm::mat4(1.0f);
model = glm::rotate(model, (float)glfwGetTime() * glm::radians(50.0f),
                    glm::vec3(0.5f, 1.0f, 0.0f));
```

## Drawing Multiple Cubes

To draw many cubes, define an array of positions and use a different model matrix for each:

```cpp
glm::vec3 cubePositions[] = {
    glm::vec3( 0.0f,  0.0f,   0.0f),
    glm::vec3( 2.0f,  5.0f, -15.0f),
    glm::vec3(-1.5f, -2.2f,  -2.5f),
    glm::vec3(-3.8f, -2.0f, -12.3f),
    glm::vec3( 2.4f, -0.4f,  -3.5f),
    glm::vec3(-1.7f,  3.0f,  -7.5f),
    glm::vec3( 1.3f, -2.0f,  -2.5f),
    glm::vec3( 1.5f,  2.0f,  -2.5f),
    glm::vec3( 1.5f,  0.2f,  -1.5f),
    glm::vec3(-1.3f,  1.0f,  -1.5f)
};
```

In the render loop:

```cpp
glBindVertexArray(VAO);
for (unsigned int i = 0; i < 10; i++) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, cubePositions[i]);
    float angle = 20.0f * i;
    model = glm::rotate(model, glm::radians(angle),
                        glm::vec3(1.0f, 0.3f, 0.5f));

    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),
                       1, GL_FALSE, glm::value_ptr(model));

    glDrawArrays(GL_TRIANGLES, 0, 36);
}
```

Each iteration builds a unique model matrix, uploads it, and draws the same cube geometry at a different position and rotation.

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
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D texture1;
uniform sampler2D texture2;

void main() {
    FragColor = mix(texture(texture1, TexCoord),
                    texture(texture2, TexCoord), 0.2);
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

    GLFWwindow* window = glfwCreateWindow(800, 600, "Coordinate Systems",
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

    glEnable(GL_DEPTH_TEST);

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

    // Cube vertices: position (3) + texCoord (2)
    float vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f
    };

    glm::vec3 cubePositions[] = {
        glm::vec3( 0.0f,  0.0f,   0.0f),
        glm::vec3( 2.0f,  5.0f, -15.0f),
        glm::vec3(-1.5f, -2.2f,  -2.5f),
        glm::vec3(-3.8f, -2.0f, -12.3f),
        glm::vec3( 2.4f, -0.4f,  -3.5f),
        glm::vec3(-1.7f,  3.0f,  -7.5f),
        glm::vec3( 1.3f, -2.0f,  -2.5f),
        glm::vec3( 1.5f,  2.0f,  -2.5f),
        glm::vec3( 1.5f,  0.2f,  -1.5f),
        glm::vec3(-1.3f,  1.0f,  -1.5f)
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

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
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture2);

        glUseProgram(shaderProgram);

        // View and projection (same for all cubes)
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::translate(view, glm::vec3(0.0f, 0.0f, -3.0f));

        glm::mat4 projection = glm::perspective(
            glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);

        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"),
                           1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));

        // Draw 10 cubes
        glBindVertexArray(VAO);
        for (unsigned int i = 0; i < 10; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);
            float angle = 20.0f * i;
            if (i % 3 == 0)
                angle = (float)glfwGetTime() * 25.0f;
            model = glm::rotate(model, glm::radians(angle),
                                glm::vec3(1.0f, 0.3f, 0.5f));
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"),
                               1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
```

## Key Takeaways

- A vertex passes through **5 coordinate spaces**: Local → World → View → Clip → Screen.
- The **model matrix** positions an object in the world. The **view matrix** represents the camera. The **projection matrix** defines how 3D maps to 2D.
- **Perspective projection** creates realistic depth; **orthographic projection** preserves parallel lines.
- The **depth buffer** (`GL_DEPTH_TEST`) prevents fragments behind other objects from being drawn on top.
- Drawing multiple objects means using different **model matrices** in a loop, reusing the same vertex data.

## Exercises

1. **Play with the FOV:** Change the field of view in `glm::perspective` from `45.0f` to `90.0f` and to `15.0f`. Observe how a wider FOV creates a fisheye effect and a narrow FOV creates a zoomed-in telescope effect.

2. **Orthographic vs. perspective:** Replace `glm::perspective` with `glm::ortho(-2.0f, 2.0f, -1.5f, 1.5f, 0.1f, 100.0f)` and observe how the cubes lose their depth perception.

3. **Animate all cubes:** Make every cube rotate over time (not just every 3rd one). Give each cube a unique rotation speed and axis.

---

[Previous: Transformations](07_transformations.md) | [Next: Camera](09_camera.md)
