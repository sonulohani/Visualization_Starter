# Face Culling

[← Previous: Blending](03_blending.md) | [Next: Framebuffers →](05_framebuffers.md)

---

Think about a cube. At any given moment, you can see at most three of its six faces. The other three are facing *away* from you — there's no point in rendering them. **Face culling** is the GPU optimization that skips these invisible faces entirely, potentially cutting the number of fragments processed in half.

This is one of those "free performance" techniques: a single line of code, no visual change (for closed meshes), and roughly 50% fewer triangles hitting the fragment shader.

## Front Faces and Back Faces

Every triangle in OpenGL has two sides: a **front face** and a **back face**. How does the GPU tell them apart? By looking at the order the vertices appear on screen.

### Winding Order

When you define a triangle's three vertices, the order you list them matters. OpenGL uses the **winding order** — the direction you'd trace around the triangle's vertices — to determine which side is the front:

```
  Counter-clockwise (CCW) = FRONT face (default)

            v1
           / \
          /   \
         /  →  \        Vertices trace CCW when
        /   F   \       viewed from the FRONT
       /         \
      v3─────────v2


  Clockwise (CW) = BACK face

            v1
           / \
          /   \
         /  ←  \        Same vertices trace CW when
        /   B   \       viewed from the BACK
       /         \
      v2─────────v3
```

Here's the key insight: a triangle that appears counter-clockwise from one side appears clockwise from the other side. The GPU checks this *after* projection to screen coordinates.

### How It Works in Practice

Consider defining a quad facing the camera (positive Z direction):

```cpp
// Front face — vertices wind counter-clockwise as seen from the front
float vertices[] = {
    // triangle 1
    -0.5f, -0.5f, 0.0f,  // bottom-left
     0.5f, -0.5f, 0.0f,  // bottom-right
     0.5f,  0.5f, 0.0f,  // top-right
    // triangle 2
     0.5f,  0.5f, 0.0f,  // top-right
    -0.5f,  0.5f, 0.0f,  // top-left
    -0.5f, -0.5f, 0.0f   // bottom-left
};
```

Looking at this quad from the front:

```
  (-0.5, 0.5)──────(0.5, 0.5)
       │ ╲              │
       │   ╲     T2     │        Both triangles wind CCW
       │     ╲          │        = front face visible
       │  T1   ╲        │
       │         ╲      │
  (-0.5,-0.5)──────(0.5,-0.5)

  T1: bottom-left → bottom-right → top-right  (CCW ✓)
  T2: top-right → top-left → bottom-left      (CCW ✓)
```

If you walk around to the *back* of this quad, the same vertices would appear to wind clockwise. That's a back face.

### A Cube Example

A properly wound cube has all its front faces pointing outward. From any viewpoint, the faces you can see are front faces (CCW), and the hidden faces are back faces (CW):

```
  Looking at a cube:

  ┌────────────────┐
  │    BACK FACE   │ ← facing away (CW from camera) — CULLED
  │  ┌──────────┐  │
  │  │          │  │
  │  │  FRONT   │  │ ← facing toward camera (CCW) — DRAWN
  │  │  FACE    │  │
  │  │          │  │
  │  └──────────┘  │
  └────────────────┘

  For a closed convex mesh, back faces are ALWAYS
  hidden behind front faces — safe to cull.
```

## Enabling Face Culling

```cpp
glEnable(GL_CULL_FACE);
```

That's it. OpenGL will now skip all back-facing triangles. With the default settings:

- **Front face** = counter-clockwise winding → **drawn**
- **Back face** = clockwise winding → **culled** (discarded before fragment shader)

## Configuration

### glCullFace — What to Cull

```cpp
glCullFace(GL_BACK);           // cull back faces (default)
glCullFace(GL_FRONT);          // cull front faces
glCullFace(GL_FRONT_AND_BACK); // cull everything (only points/lines render)
```

Culling front faces can be useful for rendering the inside of a room or creating certain shadow volume effects.

### glFrontFace — What Counts as "Front"

```cpp
glFrontFace(GL_CCW); // counter-clockwise is front (default)
glFrontFace(GL_CW);  // clockwise is front
```

You might need `GL_CW` if you're loading models from a format that uses clockwise winding by default, or if a negative scale has flipped the winding order:

```cpp
// A negative scale on one axis flips winding order!
model = glm::scale(model, glm::vec3(-1.0f, 1.0f, 1.0f));
// What was CCW is now CW — front/back faces are swapped
// Either change glFrontFace or reverse the scale
```

## Performance

Face culling happens very early in the pipeline — before the fragment shader. For a closed mesh, roughly half the triangles are back-facing at any time:

```
  Pipeline WITHOUT face culling:

  36 triangles (cube) → vertex shader (36) → rasterize → fragment shader (~36 faces worth)

  Pipeline WITH face culling:

  36 triangles (cube) → vertex shader (36) → CULL BACK FACES → fragment shader (~18 faces worth)
                                              └── 50% fewer fragments!
```

The vertex shader still processes all vertices (culling happens after vertex processing), but the expensive rasterization and fragment shader stages handle significantly fewer triangles. For complex scenes with many meshes, this adds up fast.

## When NOT to Cull

Face culling assumes that back faces are always hidden. This is true for **closed meshes** (meshes with no holes — like cubes, spheres, and character models), but not always:

**1. Open meshes / single-sided planes.** A ground plane has only one face — if you cull the back, it's invisible from below:

```
  Camera above:        Camera below:

  ┌──────────────┐     ┌──────────────┐
  │  ▓▓▓▓▓▓▓▓▓▓  │     │              │
  │  ▓▓ plane ▓▓  │     │   (nothing!) │
  │  ▓▓▓▓▓▓▓▓▓▓  │     │              │
  └──────────────┘     └──────────────┘
  (front face visible) (back face culled!)
```

**2. Transparent / see-through objects.** A tinted glass pane needs both sides visible — you can see through the front face to the back face.

**3. Two-sided materials.** Leaves, cloth, and paper look different on each side but both sides must render.

**4. Non-convex meshes with negative scales.** If you mirror a model with a negative scale, the winding order flips. Either account for this by changing `glFrontFace` or disable culling temporarily.

In practice: **enable culling globally, disable it temporarily for specific objects that need it:**

```cpp
glEnable(GL_CULL_FACE);

// Draw most objects (cubes, characters, buildings)...

glDisable(GL_CULL_FACE);
// Draw transparent objects, planes, foliage...
glEnable(GL_CULL_FACE);
```

## Complete Source Code

Here's a complete program demonstrating face culling. Press `C` to toggle culling on and off, and `F` to cycle which faces are culled (`GL_BACK`, `GL_FRONT`, `GL_FRONT_AND_BACK`).

### Vertex Shader (face_culling.vs)

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

### Fragment Shader (face_culling.fs)

```glsl
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture1;

void main()
{
    FragColor = texture(texture1, TexCoords);
}
```

### Main Program (main.cpp)

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>

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

bool cullingEnabled = true;
int cullMode = 0; // 0 = GL_BACK, 1 = GL_FRONT, 2 = GL_FRONT_AND_BACK
bool cKeyPressed = false;
bool fKeyPressed = false;

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
                                          "Face Culling", nullptr, nullptr);
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
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    Shader shader("face_culling.vs", "face_culling.fs");

    float cubeVertices[] = {
        // back face (CCW when viewed from outside → CW from camera = back)
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        // front face
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        // left face
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        // right face
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        // bottom face
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
        // top face
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f
    };

    float planeVertices[] = {
         5.0f, -0.5f,  5.0f,  2.0f, 0.0f,
        -5.0f, -0.5f, -5.0f,  0.0f, 2.0f,
        -5.0f, -0.5f,  5.0f,  0.0f, 0.0f,

         5.0f, -0.5f,  5.0f,  2.0f, 0.0f,
         5.0f, -0.5f, -5.0f,  2.0f, 2.0f,
        -5.0f, -0.5f, -5.0f,  0.0f, 2.0f
    };

    // Cube VAO
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    // Floor VAO
    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), &planeVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    unsigned int cubeTexture = loadTexture("resources/textures/marble.jpg");
    unsigned int floorTexture = loadTexture("resources/textures/metal.png");

    shader.use();
    shader.setInt("texture1", 0);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);

        // Apply current culling state
        if (cullingEnabled)
        {
            glEnable(GL_CULL_FACE);
            GLenum modes[] = { GL_BACK, GL_FRONT, GL_FRONT_AND_BACK };
            glCullFace(modes[cullMode]);
        }
        else
        {
            glDisable(GL_CULL_FACE);
        }

        // Draw cubes
        glBindVertexArray(cubeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, cubeTexture);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.0f, 0.0f, -1.0f));
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(2.0f, 0.0f, 0.0f));
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Draw floor (disable culling — it's a single plane)
        glDisable(GL_CULL_FACE);
        glBindVertexArray(planeVAO);
        glBindTexture(GL_TEXTURE_2D, floorTexture);
        model = glm::mat4(1.0f);
        shader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &cubeVBO);
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

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !cKeyPressed)
    {
        cullingEnabled = !cullingEnabled;
        cKeyPressed = true;
        std::cout << "Face culling: "
                  << (cullingEnabled ? "ON" : "OFF") << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE)
        cKeyPressed = false;

    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !fKeyPressed)
    {
        cullMode = (cullMode + 1) % 3;
        const char* names[] = { "GL_BACK", "GL_FRONT", "GL_FRONT_AND_BACK" };
        std::cout << "Cull face: " << names[cullMode] << std::endl;
        fKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE)
        fKeyPressed = false;
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

unsigned int loadTexture(char const* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
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
```

### What You Should See

Two textured cubes on a metal floor. With the default settings (culling back faces), the scene looks perfectly normal — identical to not having culling at all.

- Press **C** to toggle culling on/off. With culling off, the scene looks the same (back faces are hidden by depth testing anyway — culling just saves the GPU work).
- Press **F** to cycle cull modes:
  - `GL_BACK` — normal (you see the outside of the cubes)
  - `GL_FRONT` — you see the *inside* of the cubes (like looking into an open box)
  - `GL_FRONT_AND_BACK` — the cubes disappear entirely (only wireframe with `glPolygonMode` would show anything)

```
  GL_BACK (normal):       GL_FRONT (inside out):

  ┌──────────────────┐    ┌──────────────────┐
  │   ┌────────┐     │    │   ┌────────┐     │
  │   │ marble │     │    │   │ inside │     │
  │   │ (front)│     │    │   │  (back │     │
  │   └────────┘     │    │   │ faces) │     │
  │  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓  │    │   └────────┘     │
  │  ▓▓▓ floor ▓▓▓▓  │    │  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓  │
  └──────────────────┘    └──────────────────┘
```

---

## Key Takeaways

- **Face culling** skips rendering triangles that face away from the camera — a free performance boost for closed meshes.
- OpenGL determines front/back faces by **winding order**: counter-clockwise (CCW) = front by default.
- `glEnable(GL_CULL_FACE)` enables culling. `glCullFace` chooses which faces to cull. `glFrontFace` sets the winding convention.
- Culling saves ~50% of fragment processing for closed geometry.
- **Disable culling** for planes, transparent objects, foliage, and any geometry where both sides should be visible.
- Negative scales on an odd number of axes flip winding order — keep this in mind.

## Exercises

1. Enable face culling in one of your earlier projects (like the lighting scene). Verify the scene looks identical. If you can, measure the frame time before and after — on complex scenes, you should see a difference.
2. Set `glCullFace(GL_FRONT)` and look at a cube. You're seeing the inside. Think about how this could be used for a "room interior" effect.
3. Create a simple plane (two triangles) with a texture. Enable culling and rotate the camera around it. It disappears from one side. Now disable culling — it's visible from both sides. Discuss: which is more appropriate for a floor? For a wall in a closed room?
4. Apply a negative X scale (`glm::scale(model, glm::vec3(-1.0f, 1.0f, 1.0f))`) to a cube with culling enabled. What happens? Fix it by either changing `glFrontFace` or adjusting the scale.
5. Count the number of triangles your cube has (36 vertices ÷ 3 = 12 triangles). With back-face culling, roughly how many triangles does the GPU actually rasterize at any given time? Verify by using an OpenGL profiling tool if available.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Blending](03_blending.md) | [Table of Contents](../../README.md) | [Next: Framebuffers →](05_framebuffers.md) |
