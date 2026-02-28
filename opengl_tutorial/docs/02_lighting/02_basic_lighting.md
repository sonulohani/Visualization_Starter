# Basic Lighting

Now that we understand colors, let's bring our scene to life with a real lighting model. In this chapter we'll implement the **Phong lighting model**, the most commonly used lighting approximation in real-time graphics. By the end, our flat coral cube will have realistic shading with bright highlights and soft shadows.

## The Phong Lighting Model

Real-world lighting is extremely complex and depends on far too many factors for us to compute in real time. The **Phong lighting model** (named after Bui Tuong Phong) is a simplified approximation that produces convincing results by breaking lighting down into three components:

```
  ┌─────────────────────────────────────────────────────┐
  │              Phong Lighting Model                    │
  │                                                     │
  │   Final Color = Ambient + Diffuse + Specular        │
  │                                                     │
  │   ┌─────────┐  ┌──────────┐  ┌──────────────┐      │
  │   │ Ambient │ +│ Diffuse  │ +│  Specular    │      │
  │   │         │  │          │  │              │      │
  │   │ constant│  │ angle of │  │ shiny        │      │
  │   │ base    │  │ light on │  │ highlight    │      │
  │   │ light   │  │ surface  │  │ reflection   │      │
  │   └─────────┘  └──────────┘  └──────────────┘      │
  └─────────────────────────────────────────────────────┘
```

Let's explore each component in detail.

---

## 1. Ambient Lighting

Even in the darkest room, there is almost always *some* light. Light bounces off walls, floors, and other objects, filling the room with a low level of indirect illumination. Simulating this properly (called **global illumination**) is very expensive, so we fake it with a simple constant: **ambient lighting**.

Ambient light is a small, constant amount of color applied to every fragment regardless of its position or orientation:

```glsl
float ambientStrength = 0.1;
vec3 ambient = ambientStrength * lightColor;
```

```
  Ambient Lighting
  ────────────────
  Every surface gets a small, equal amount of light.

  ┌───────────┐
  │ ░░░░░░░░░ │  All faces uniformly lit
  │ ░░░░░░░░░ │  (very dim, like being in a
  │ ░░░░░░░░░ │   dark room with faint light)
  └───────────┘
```

Without ambient light, surfaces facing away from the light would be completely black — unrealistically so. Ambient gives us a baseline.

---

## 2. Diffuse Lighting

Diffuse lighting is the most visually significant component. It gives an object its characteristic "lit" appearance — faces pointing toward the light are bright, and faces angled away are dark.

The amount of diffuse light depends on the **angle** between the surface normal and the direction to the light source:

```
  Diffuse Lighting — The Angle Matters
  ─────────────────────────────────────

  Light Source
      ☀
      │
      │  light ray
      │
      ▼
  ────────────  surface
      ↑
      │ Normal (perpendicular to surface)

  When the light ray is directly aligned with the
  normal, the surface receives maximum light.

  Light Source
      ☀
       \
        \  light ray        θ = angle between
         \                      normal and light dir
          \  θ
  ──────────────  surface
           ↑
           │ Normal

  As the angle θ increases, less light reaches
  the surface (cos θ decreases toward 0).

  When θ ≥ 90°, the surface faces away from the
  light and receives no diffuse light (cos θ ≤ 0).
```

### What We Need

To calculate diffuse lighting we need two things:
1. **A normal vector** — the direction perpendicular to each surface.
2. **The direction from the fragment to the light** — computed from the fragment's world position and the light's position.

### Normal Vectors

A **normal vector** is a vector perpendicular to the surface at a given point. For our cube, every vertex on a face shares the same normal (since the faces are flat):

```
  Normal Vectors on a Cube
  ────────────────────────
           ↑ (0,1,0)
     ┌─────────────┐
     │   top face   │
     └─────────────┘
  ←  │             │  →
(−1,0,0)         (1,0,0)
     │  front face │
     │      ↗      │
     │  (0,0,1)    │
     └─────────────┘
```

We add normal vectors to our vertex data. Each vertex now has 6 floats: 3 for position and 3 for the normal.

### Vertex Data with Normals

```cpp
float vertices[] = {
    // positions          // normals
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
};
```

### Updating the VAO

Since we added normals, we need to update our vertex attribute configuration. The stride changes from `3 * sizeof(float)` to `6 * sizeof(float)`:

```cpp
// Position attribute
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);

// Normal attribute
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);
```

> **Important:** The light cube VAO also needs updating since the VBO data layout changed. Even though the light doesn't use normals, the stride must match. Set up attribute 0 with the new stride of `6 * sizeof(float)`.

### Diffuse Calculation in the Fragment Shader

```glsl
// Normalize the normal (interpolation can de-normalize it)
vec3 norm = normalize(Normal);

// Direction from fragment to light
vec3 lightDir = normalize(lightPos - FragPos);

// Diffuse impact: cosine of angle between normal and light direction
float diff = max(dot(norm, lightDir), 0.0);
vec3 diffuse = diff * lightColor;
```

The `max(dot(...), 0.0)` ensures we don't get negative light — if the surface faces away from the light, the dot product is negative, and we clamp it to zero.

---

## 3. Specular Lighting

Specular lighting simulates the bright spot of light that appears on shiny objects when light reflects directly toward the viewer. Think of the bright glare on a polished metal surface or a wet floor.

```
  Specular Reflection
  ───────────────────
                    ☀ Light
                   /
                  / incident ray
                 /
  surface  ─────●───────
                 \  ↑
         reflect  \ │ Normal
            ray    \│
                    ●
                   👁 Viewer

  Specular highlight is brightest when the reflected
  ray points directly at the viewer.
```

### What We Need

1. **View direction** — from the fragment to the camera.
2. **Reflect direction** — the light direction reflected around the normal.

```glsl
float specularStrength = 0.5;

vec3 viewDir = normalize(viewPos - FragPos);
vec3 reflectDir = reflect(-lightDir, norm);

float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
vec3 specular = specularStrength * spec * lightColor;
```

The **`reflect`** function takes the incident light direction (pointing *from* the light *to* the surface, hence `-lightDir`) and the normal, and returns the reflected direction.

The **shininess** value (`32` in this example) controls how focused the highlight is:

```
  Shininess Values
  ────────────────
  Low (2-8):    ████████████████  Large, spread-out highlight (rubber, cloth)
  Medium (32):  ████████          Medium highlight (plastic, painted surface)
  High (256):   ██                Tiny, focused highlight (polished metal, glass)
```

Higher shininess values create a smaller, more concentrated highlight, simulating shinier materials.

---

## Combining the Components

Now we add all three components together and multiply by the object's color:

```glsl
vec3 result = (ambient + diffuse + specular) * objectColor;
FragColor = vec4(result, 1.0);
```

```
  Phong Components Combined
  ─────────────────────────

  ┌──────────┐   ┌──────────┐   ┌──────────┐   ┌──────────┐
  │ Ambient  │ + │ Diffuse  │ + │ Specular │ = │  Final   │
  │          │   │          │   │          │   │          │
  │ ░░░░░░░░ │   │ ░░░████░ │   │ ░░░░●░░░ │   │ ░░░████░ │
  │ ░░░░░░░░ │   │ ░░██████ │   │ ░░░░░░░░ │   │ ░░██●███ │
  │ ░░░░░░░░ │   │ ░░░████░ │   │ ░░░░░░░░ │   │ ░░░████░ │
  │          │   │          │   │          │   │          │
  └──────────┘   └──────────┘   └──────────┘   └──────────┘
    uniform         shading         glare         realistic
    dim base      from light      highlight       result
```

---

## Fragment Position

To compute `lightDir` and `viewDir`, we need the fragment's position in **world space**. We compute this in the vertex shader by transforming the vertex position by the model matrix, and pass it to the fragment shader:

**Vertex shader:**
```glsl
FragPos = vec3(model * vec4(aPos, 1.0));
```

We pass `FragPos` as an `out` variable from the vertex shader, and it's interpolated across the triangle for each fragment.

---

## The Normal Matrix

There's a subtle problem. If we apply a **non-uniform scale** to our model (e.g., stretching along one axis), the normal vectors get distorted:

```
  Non-Uniform Scaling Distorts Normals
  ─────────────────────────────────────

  Before scaling:        After scale(2,1,1):

       ↑ normal               ↗ WRONG normal
       │                     /
  ─────┼─────           ───────────────
  flat surface          stretched surface
                             ↑ correct normal
                             │
```

Simply multiplying normals by the model matrix doesn't work correctly under non-uniform scaling. The fix is to use the **normal matrix**, defined as the transpose of the inverse of the upper-left 3×3 part of the model matrix:

```glsl
Normal = mat3(transpose(inverse(model))) * aNormal;
```

> **Performance note:** Computing `inverse()` in the shader is expensive. For production code, calculate the normal matrix on the CPU and send it as a uniform. For learning purposes, doing it in the shader is fine.

---

## Complete Shaders

### Object Vertex Shader (`object.vert`)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;

    gl_Position = projection * view * vec4(FragPos, 1.0);
}
```

### Object Fragment Shader (`object.frag`)

```glsl
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

void main()
{
    // Ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    // Combine
    vec3 result = (ambient + diffuse + specular) * objectColor;
    FragColor = vec4(result, 1.0);
}
```

### Light Cube Vertex Shader (`light_cube.vert`)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

### Light Cube Fragment Shader (`light_cube.frag`)

```glsl
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0);
}
```

---

## Complete C++ Source Code

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

glm::vec3 lightPos(1.2f, 1.0f, 2.0f);

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
        "LearnOpenGL - Basic Lighting", NULL, NULL);
    if (window == NULL)
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

    Shader objectShader("shaders/object.vert", "shaders/object.frag");
    Shader lightCubeShader("shaders/light_cube.vert", "shaders/light_cube.frag");

    float vertices[] = {
        // positions          // normals
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
    };

    // Object cube
    unsigned int VBO, cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(cubeVAO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Light cube (same VBO, only need position attribute)
    unsigned int lightCubeVAO;
    glGenVertexArrays(1, &lightCubeVAO);
    glBindVertexArray(lightCubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Render loop
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Object shader
        objectShader.use();
        objectShader.setVec3("objectColor", 1.0f, 0.5f, 0.31f);
        objectShader.setVec3("lightColor",  1.0f, 1.0f, 1.0f);
        objectShader.setVec3("lightPos", lightPos);
        objectShader.setVec3("viewPos", camera.Position);

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f
        );
        glm::mat4 view = camera.GetViewMatrix();
        objectShader.setMat4("projection", projection);
        objectShader.setMat4("view", view);

        glm::mat4 model = glm::mat4(1.0f);
        objectShader.setMat4("model", model);

        glBindVertexArray(cubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Light cube
        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);

        model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.2f));
        lightCubeShader.setMat4("model", model);

        glBindVertexArray(lightCubeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &lightCubeVAO);
    glDeleteBuffers(1, &VBO);

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

---

## Phong vs. Gouraud Shading

What we've implemented above is called **Phong shading** — lighting calculations are performed **per fragment** in the fragment shader. There's an older (and cheaper) alternative called **Gouraud shading**, where lighting is calculated **per vertex** in the vertex shader, and the results are interpolated across the triangle.

```
  Phong Shading              Gouraud Shading
  (per-fragment)             (per-vertex)
  ──────────────             ────────────────
  ┌──────────┐               ┌──────────┐
  │ ░░██████ │               │ ░░████░░ │
  │ ░███●███ │               │ ░██████░ │
  │ ░░██████ │               │ ░░████░░ │
  └──────────┘               └──────────┘
  Smooth, accurate           Blocky specular
  specular highlight         highlight (lost
                             between vertices)
```

Gouraud shading is faster (fewer calculations) but produces visibly worse results, especially for specular highlights. With modern GPUs, the performance difference is negligible, so **Phong shading is the standard choice**.

---

## Key Takeaways

- The **Phong lighting model** approximates real lighting with three components: **ambient**, **diffuse**, and **specular**.
- **Ambient** provides a constant base illumination so nothing is completely black.
- **Diffuse** depends on the angle between the surface normal and the light direction (dot product).
- **Specular** depends on the angle between the reflected light and the view direction, with a shininess exponent controlling the size of the highlight.
- We need **normal vectors** in our vertex data and must update the VAO accordingly.
- The fragment's **world-space position** (`FragPos`) is needed for direction calculations.
- The **normal matrix** (`transpose(inverse(model))`) corrects normals under non-uniform scaling.
- **Phong shading** (per-fragment) produces better results than **Gouraud shading** (per-vertex).

## Exercises

1. **Move the light:** Currently the light is static. Orbit it around the cube using `sin` and `cos` on `glfwGetTime()`. Observe how the diffuse and specular highlights move.
2. **Gouraud shading:** Move the lighting calculations from the fragment shader to the vertex shader (compute the final color per-vertex and pass it to the fragment shader). Compare the visual quality — especially the specular highlight.
3. **Ambient experiment:** Set `ambientStrength` to `0.0` and observe the faces in shadow. Then set it to `1.0` — what happens? Experiment to find a good value.
4. **Shininess:** Try different shininess values (1, 2, 8, 32, 128, 256, 1024). How does the specular highlight change?

---

| [← Colors](01_colors.md) | [Next: Materials →](03_materials.md) |
|:---|---:|
