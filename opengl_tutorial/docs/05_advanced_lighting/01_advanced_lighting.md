# Advanced Lighting (Blinn-Phong)

In the lighting section we implemented the Phong lighting model — a combination of ambient, diffuse, and specular components that gives objects a convincing sense of three-dimensionality. While Phong works well in many cases, it has a noticeable flaw in how it calculates specular highlights. In this chapter, we'll examine that flaw and introduce the **Blinn-Phong** model, a simple modification that produces more realistic results and is the de facto standard in real-time rendering.

---

## Recap: The Phong Specular Model

Recall how Phong calculates the specular component:

1. Compute the **reflection direction** `R` by reflecting the light direction around the surface normal.
2. Measure the angle between `R` and the **view direction** `V`.
3. Raise the cosine of that angle to a shininess exponent.

In GLSL:

```glsl
vec3 reflectDir = reflect(-lightDir, normal);
float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
```

This works well when the viewer is roughly in line with the reflected light. But there's a problem lurking at certain angles.

## The Problem with Phong

Consider what happens when the angle between the view direction `V` and the reflect direction `R` exceeds 90°:

```
                    N (normal)
                    │
                    │
            R ╱     │     ╲ V (view)
             ╱      │      ╲
            ╱  >90° │       ╲
           ╱        │        ╲
  ─────────────────────────────────── surface
                    │
                    L (light, below surface)
```

When the angle between `V` and `R` is greater than 90°, `dot(viewDir, reflectDir)` becomes negative. Our `max(..., 0.0)` clamps it to zero, meaning the specular contribution drops to exactly zero — instantly.

This causes a **harsh cutoff** in the specular highlight. Instead of a smooth, natural falloff, you see the highlight abruptly vanish at certain viewing angles. The artifact is most visible with low shininess exponents (wide, soft highlights), because those broad highlights are more likely to extend past the 90° boundary.

```
  Phong Specular Intensity vs. Angle
  
  intensity
  1.0 │  ╲
      │   ╲         high shininess (32):
      │    ╲        narrow highlight,
      │     ╲       cutoff rarely visible
  0.5 │      ╲
      │       ╲
      │        ╲    low shininess (4):
      │         ╲   wide highlight,
  0.0 │──────────╲──────── cutoff visible!
      └────────────────── angle (V, R)
      0°        90°
                 ↑
           abrupt cutoff here
```

## The Blinn-Phong Solution

In 1977, Jim Blinn proposed a simple modification: instead of measuring the angle between the view direction and the reflect direction, measure the angle between the **surface normal** and the **halfway vector**.

### The Halfway Vector

The halfway vector `H` is the direction exactly between the light direction `L` and the view direction `V`:

```glsl
vec3 halfwayDir = normalize(lightDir + viewDir);
```

Visually:

```
  Phong:                           Blinn-Phong:
  
         N                                N
         │                                │     H
         │  R                             │   ╱
         │╱                               │ ╱
         │    ╲ V                          │╱    ╲ V
         │      ╲                          │      ╲
  ───────┴────────── surface        ───────┴────────── surface
                                          ╱
         angle = (V, R)                  L
                                   
                                   angle = (N, H)
```

The specular calculation becomes:

```glsl
vec3 halfwayDir = normalize(lightDir + viewDir);
float spec = pow(max(dot(normal, halfwayDir), 0.0), shininess);
```

### Why This Fixes the Problem

The halfway vector `H` always lies between `L` and `V`. The angle between `N` and `H` is always *smaller* than the angle between `V` and `R`. Crucially, the `N · H` dot product only reaches zero when the halfway vector is perpendicular to the surface — a far more extreme configuration than the 90° cutoff in Phong. In practice, the specular highlight tapers off smoothly without any harsh cutoff.

```
  Specular Intensity Comparison
  
  intensity
  1.0 │╲ ╲
      │ ╲  ╲
      │  ╲   ╲         ── Blinn-Phong: smooth falloff
      │   ╲    ╲        -- Phong: hard cutoff at 90°
  0.5 │    ╲     ╲
      │     ╲      ╲
      │      ╲       ╲
      │       ╲   ·····╲····> Phong drops to 0
  0.0 │────────╲─────────────
      └────────────────────── angle
      0°      90°      180°
```

### Shininess Difference

The Blinn-Phong highlight is generally wider than the equivalent Phong highlight at the same shininess exponent, because the angle between `N` and `H` is smaller than between `V` and `R`. To get a visually similar specular spot, you typically need to multiply the shininess exponent by **2 to 4** when switching from Phong to Blinn-Phong:

| Model       | Shininess for equivalent look |
|-------------|-------------------------------|
| Phong       | 8                             |
| Blinn-Phong | 16–32                         |

## Blinn-Phong Shaders

Let's implement both models so we can toggle between them and compare.

### Vertex Shader (`blinn_phong.vert`)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
```

### Fragment Shader (`blinn_phong.frag`)

```glsl
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform bool blinn;

uniform sampler2D floorTexture;

void main()
{
    vec3 color = texture(floorTexture, TexCoords).rgb;

    // Ambient
    vec3 ambient = 0.05 * color;

    // Diffuse
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 normal = normalize(Normal);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * color;

    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    float spec = 0.0;
    if (blinn)
    {
        vec3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0);
    }
    else
    {
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);
    }
    vec3 specular = vec3(0.3) * spec;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
```

Notice how the Phong path uses shininess `8.0` while the Blinn-Phong path uses `32.0` — roughly 4× higher — to produce a similar highlight size.

## Complete C++ Source Code

This program renders a textured floor plane with a single light source. Press **B** to toggle between Phong and Blinn-Phong specular models.

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
unsigned int loadTexture(const char* path);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool blinn = false;
bool blinnKeyPressed = false;

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
                                          "LearnOpenGL - Advanced Lighting",
                                          NULL, NULL);
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader shader("shaders/blinn_phong.vert", "shaders/blinn_phong.frag");

    // Floor plane vertices: positions, normals, texture coords
    float planeVertices[] = {
        // positions            // normals         // texcoords
         10.0f, -0.5f,  10.0f,  0.0f, 1.0f, 0.0f,  10.0f,  0.0f,
        -10.0f, -0.5f,  10.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
        -10.0f, -0.5f, -10.0f,  0.0f, 1.0f, 0.0f,   0.0f, 10.0f,

         10.0f, -0.5f,  10.0f,  0.0f, 1.0f, 0.0f,  10.0f,  0.0f,
        -10.0f, -0.5f, -10.0f,  0.0f, 1.0f, 0.0f,   0.0f, 10.0f,
         10.0f, -0.5f, -10.0f,  0.0f, 1.0f, 0.0f,  10.0f, 10.0f
    };

    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(6 * sizeof(float)));
    glBindVertexArray(0);

    unsigned int floorTexture = loadTexture("textures/wood.png");

    shader.use();
    shader.setInt("floorTexture", 0);

    glm::vec3 lightPos(0.0f, 0.0f, 0.0f);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        shader.setMat4("projection", projection);
        shader.setMat4("view", view);

        shader.setVec3("viewPos", camera.Position);
        shader.setVec3("lightPos", lightPos);
        shader.setBool("blinn", blinn);

        glBindVertexArray(planeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, floorTexture);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        std::cout << (blinn ? "Blinn-Phong" : "Phong") << "\r" << std::flush;

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);

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

    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS && !blinnKeyPressed)
    {
        blinn = !blinn;
        blinnKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_B) == GLFW_RELEASE)
    {
        blinnKeyPressed = false;
    }
}

unsigned int loadTexture(char const* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
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

## What You Should See

A large wooden floor plane lit by a single light source near the center. The specular highlight is clearly visible on the surface.

```
  ┌──────────────────────────────────────────┐
  │ LearnOpenGL - Advanced Lighting    [—][×]│
  ├──────────────────────────────────────────┤
  │                                          │
  │     ·  ·  ·                              │
  │   ·  SPECULAR  ·                         │
  │     ·  SPOT  ·      ← bright highlight   │
  │       · · ·          on the floor         │
  │░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  └──────────────────────────────────────────┘
     Phong (press B to switch)
```

- **With Phong**: the specular highlight has a visible hard edge where it cuts off.
- **With Blinn-Phong**: the highlight fades smoothly in all directions.
- Press **B** to toggle between the two models and observe the difference.

## Why Blinn-Phong Is Preferred

Blinn-Phong has become the standard for several reasons:

1. **No harsh cutoff**: the specular contribution fades naturally at all angles.
2. **More physically plausible**: the halfway vector model better approximates real-world micro-facet reflections.
3. **Historical default**: OpenGL's old fixed-function pipeline (pre-shader era) used Blinn-Phong as its default lighting model.
4. **Slightly faster**: computing the halfway vector (`normalize(L + V)`) avoids the `reflect()` function, though in practice the difference is negligible.
5. **Better for wide highlights**: materials with low shininess (rough surfaces) look more natural.

## When to Use Phong vs Blinn-Phong

| Scenario | Recommended Model |
|----------|-------------------|
| General-purpose rendering | Blinn-Phong |
| Matching older software that used Phong | Phong |
| Educational purposes (understanding reflection) | Phong first, then Blinn-Phong |
| Physically-based rendering (PBR) | Neither — use Cook-Torrance or similar |
| Fixed-function OpenGL emulation | Blinn-Phong (that's what it used) |

In practice, always default to Blinn-Phong unless you have a specific reason to use Phong.

---

## Key Takeaways

- Phong's specular model breaks down when the angle between the view and reflect directions exceeds 90°, causing an abrupt cutoff.
- Blinn-Phong replaces the reflect direction with the **halfway vector** `H = normalize(L + V)` and measures `dot(N, H)` instead of `dot(V, R)`.
- The halfway vector naturally avoids the >90° problem, producing smooth specular falloff at all angles.
- Blinn-Phong requires a **higher shininess exponent** (typically 2–4×) to match the visual width of a Phong highlight.
- Blinn-Phong is the industry standard for non-PBR rendering and was the default in OpenGL's fixed-function pipeline.

## Exercises

1. Adjust the shininess values in the fragment shader. Try Phong with shininess 2 and Blinn-Phong with shininess 8. Can you find pairs that produce visually similar highlights?
2. Add a second light source to the scene. How do the specular highlights from both models compare with multiple lights?
3. Render a sphere instead of a floor plane. The specular cutoff artifact in Phong is even more obvious on curved surfaces — observe the difference.
4. Instead of a boolean toggle, pass the shininess as a uniform so you can adjust it at runtime (e.g., with the up/down arrow keys). Find the Blinn-Phong shininess that most closely matches Phong at shininess 32.
5. Implement both models in the same fragment shader without the `if` branch — use two separate shader programs instead. Profile whether there's any measurable performance difference.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Model Loading Review](../03_model_loading/03_model.md) | [05. Advanced Lighting](.) | [Next: Gamma Correction →](02_gamma_correction.md) |
