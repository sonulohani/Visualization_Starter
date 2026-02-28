# Cubemaps

[← Previous: Framebuffers](05_framebuffers.md) | [Next: Advanced Data →](07_advanced_data.md)

---

Imagine surrounding your entire scene with a giant photograph of the sky, mountains, and horizon. That's a **skybox**, and it's built on a texture type we haven't used yet: the **cubemap**. A cubemap is a single texture made up of six square images arranged as the faces of a cube. Instead of sampling with 2D texture coordinates, you sample with a 3D direction vector — and the GPU figures out which face to read from.

Cubemaps don't just make pretty skies. They also enable **environment mapping** — reflective and refractive surfaces that mirror or distort the world around them. Chrome spheres, glass objects, and water surfaces all use cubemaps.

## What Is a Cubemap?

A cubemap consists of six 2D textures, each representing one face of a cube:

```
             ┌──────────┐
             │  +Y (top) │
  ┌──────────┼──────────┼──────────┬──────────┐
  │ -X (left)│ +Z (front)│ +X (right)│ -Z (back) │
  └──────────┼──────────┼──────────┴──────────┘
             │ -Y (bottom)│
             └──────────┘
```

To sample a cubemap, you provide a **3D direction vector**. OpenGL determines which face the vector points to and where on that face it lands:

```
                    direction = (0.5, 0.8, 0.2)
                         ╱
                        ╱
                       ╱
             ┌────────╱───┐
             │       ╱    │
             │      ●     │   ← hits the +Y face
             │     +Y     │
             └────────────┘
```

The direction vector doesn't need to be normalized — only its direction matters, not its magnitude.

## Creating a Cubemap Texture

### Loading the Six Faces

```cpp
unsigned int loadCubemap(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height,
                                        &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, GL_RGB, width, height, 0, GL_RGB,
                         GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: "
                      << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,
                    GL_CLAMP_TO_EDGE);

    return textureID;
}
```

The key detail: `GL_TEXTURE_CUBE_MAP_POSITIVE_X + i` cycles through the six face targets:

| i | Target | Face |
|---|---|---|
| 0 | `GL_TEXTURE_CUBE_MAP_POSITIVE_X` | Right |
| 1 | `GL_TEXTURE_CUBE_MAP_NEGATIVE_X` | Left |
| 2 | `GL_TEXTURE_CUBE_MAP_POSITIVE_Y` | Top |
| 3 | `GL_TEXTURE_CUBE_MAP_NEGATIVE_Y` | Bottom |
| 4 | `GL_TEXTURE_CUBE_MAP_POSITIVE_Z` | Front |
| 5 | `GL_TEXTURE_CUBE_MAP_NEGATIVE_Z` | Back |

So the `faces` vector must be ordered: right, left, top, bottom, front, back.

```cpp
std::vector<std::string> faces {
    "resources/textures/skybox/right.jpg",
    "resources/textures/skybox/left.jpg",
    "resources/textures/skybox/top.jpg",
    "resources/textures/skybox/bottom.jpg",
    "resources/textures/skybox/front.jpg",
    "resources/textures/skybox/back.jpg"
};
unsigned int cubemapTexture = loadCubemap(faces);
```

### Why GL_CLAMP_TO_EDGE?

Cubemap edges are where two faces meet. Using `GL_CLAMP_TO_EDGE` prevents visible seams by clamping texture coordinates to the edge rather than wrapping or repeating. Note we set it on all three axes (S, T, and **R** — the third texture coordinate axis).

## Skybox

A skybox is a large cube centered on the camera with the cubemap textured on its inside faces. Because it always surrounds the camera, it creates the illusion of a distant environment.

### Skybox Vertex Data

We only need positions — no texture coordinates. We'll use the position itself as the direction vector to sample the cubemap:

```cpp
float skyboxVertices[] = {
    // positions
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};
```

### Skybox Shaders

**skybox.vs:**

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
```

Two important tricks here:

1. **`TexCoords = aPos`** — the cube vertex position *is* the sampling direction. A vertex at `(-1, 1, -1)` samples the cubemap in the direction `(-1, 1, -1)`.

2. **`gl_Position = pos.xyww`** — by setting `z = w`, the resulting depth after perspective division is `w/w = 1.0`, which is the maximum depth value. This ensures the skybox is always behind everything else.

The view matrix has its **translation removed** so the skybox doesn't move with the camera — it appears infinitely far away:

```cpp
glm::mat4 view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
//                          ^^^^^^^^^
// mat3 strips translation, mat4 converts back (translation = 0)
```

**skybox.fs:**

```glsl
#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{
    FragColor = texture(skybox, TexCoords);
}
```

Notice the sampler type is `samplerCube` (not `sampler2D`) and we sample with a `vec3`.

### Drawing the Skybox

We draw the skybox **last**, with the depth function set to `GL_LEQUAL`:

```cpp
// Draw scene objects first...

// Then draw skybox
glDepthFunc(GL_LEQUAL);
skyboxShader.use();
// set view (without translation) and projection
glBindVertexArray(skyboxVAO);
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
glDrawArrays(GL_TRIANGLES, 0, 36);
glDepthFunc(GL_LESS); // restore default
```

Why `GL_LEQUAL`? The skybox vertices have depth 1.0 (because of the `xyww` trick). The default `GL_LESS` would fail because 1.0 is not less than 1.0 (the cleared depth). `GL_LEQUAL` allows the skybox to pass where nothing else has been drawn.

```
  Why draw the skybox LAST?

  ┌────────────────────────────────────┐
  │  Objects drawn first               │
  │  → depth buffer filled with < 1.0  │
  │                                    │
  │  Skybox drawn last (depth = 1.0)   │
  │  → fails depth test where objects  │
  │    already rendered                │
  │  → only fills remaining background │
  │                                    │
  │  Result: skybox behind everything, │
  │  GPU skips fragment shader for     │
  │  pixels already covered by objects │
  └────────────────────────────────────┘
```

This is also an optimization: the early depth test rejects skybox fragments that are behind scene objects, saving fragment shader work.

## Environment Mapping: Reflection

Reflective surfaces (chrome, mirrors, polished metal) show a distorted image of their surroundings. We can fake this by reflecting the camera-to-fragment direction vector around the surface normal, then using that reflected vector to sample the cubemap.

```
              Normal (N)
                 ↑
                 │
  Incoming (I)   │   Reflected (R)
     ╲           │          ╱
      ╲          │         ╱
       ╲         │        ╱
        ╲        │       ╱
         ╲       │      ╱
          ╲      │     ╱
           ●─────●────●  surface
```

GLSL provides a built-in `reflect` function:

```glsl
vec3 R = reflect(I, N);
```

### Reflection Shaders

**reflect.vs:**

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 Normal;
out vec3 Position;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    Normal = mat3(transpose(inverse(model))) * aNormal;
    Position = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

**reflect.fs:**

```glsl
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 Position;

uniform vec3 cameraPos;
uniform samplerCube skybox;

void main()
{
    vec3 I = normalize(Position - cameraPos);
    vec3 R = reflect(I, normalize(Normal));
    FragColor = vec4(texture(skybox, R).rgb, 1.0);
}
```

The fragment shader:
1. Computes the view direction `I` (from camera to fragment position in world space)
2. Reflects it around the surface normal `N`
3. Uses the reflected vector `R` to sample the cubemap

The result: the object appears to mirror the environment. A cube looks like polished chrome.

## Environment Mapping: Refraction

Refraction bends light as it passes through a material boundary (like air to glass). GLSL's `refract` function handles this:

```glsl
vec3 R = refract(I, N, ratio);
```

Where `ratio` is the ratio of refractive indices (the medium the light comes from divided by the medium it enters):

| Material | Refractive Index | Ratio (from air) |
|---|---|---|
| Air | 1.00 | — |
| Water | 1.33 | 1.00 / 1.33 = 0.75 |
| Glass | 1.52 | 1.00 / 1.52 = 0.66 |
| Diamond | 2.42 | 1.00 / 2.42 = 0.41 |

```
              Normal (N)
                 ↑
                 │
  Incoming (I)   │
     ╲           │
      ╲          │
       ╲         │
  ──────●────────●──────── surface
                 │╲
                 │  ╲
                 │    ╲   Refracted (R)
                 │      ╲
                 │        ↘
```

### Refraction Fragment Shader

```glsl
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 Position;

uniform vec3 cameraPos;
uniform samplerCube skybox;
uniform float refractiveIndex; // e.g. 1.52 for glass

void main()
{
    float ratio = 1.0 / refractiveIndex;
    vec3 I = normalize(Position - cameraPos);
    vec3 R = refract(I, normalize(Normal), ratio);
    FragColor = vec4(texture(skybox, R).rgb, 1.0);
}
```

> **Note:** Real refraction is more complex — light should refract *twice* (entering and exiting the object), and different wavelengths refract by different amounts (chromatic dispersion). Our single-refraction approximation looks good enough for most real-time purposes.

## Dynamic Environment Maps

The skybox cubemap is static — it was loaded from image files and never changes. What if objects in your scene should reflect *each other*?

The concept is straightforward: render the scene six times from the reflective object's position, once for each cubemap face, into a framebuffer with a cubemap texture attachment. Then use this dynamically generated cubemap for the reflection.

```
  For each of the 6 faces:
  1. Set camera at object position, looking in face direction
  2. Render scene (excluding the reflective object itself)
  3. Store result in cubemap face

  Then: use the filled cubemap for the object's reflection shader
```

This is expensive — 6 extra render passes per reflective object per frame. Optimizations include:
- Only update the cubemap every few frames
- Use lower resolution for the cubemap faces
- Only update faces that are visible to the main camera
- Use screen-space reflections instead for planar surfaces

We won't implement dynamic cubemaps in this chapter's code, but understanding the concept is important as you'll encounter it in advanced rendering techniques.

## Complete Source Code

Here's a complete program with a skybox and two objects: one using reflection and one using refraction. Press `R` to toggle between reflection and refraction on the cubes.

### Skybox Shaders

**skybox.vs:**

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;
    vec4 pos = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
```

**skybox.fs:**

```glsl
#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{
    FragColor = texture(skybox, TexCoords);
}
```

### Environment Mapping Shaders

**envmap.vs:**

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;

out vec3 Normal;
out vec3 Position;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    Normal = mat3(transpose(inverse(model))) * aNormal;
    Position = vec3(model * vec4(aPos, 1.0));
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
```

**envmap.fs:**

```glsl
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 Position;

uniform vec3 cameraPos;
uniform samplerCube skybox;
uniform bool useRefraction;
uniform float refractiveIndex;

void main()
{
    vec3 I = normalize(Position - cameraPos);
    vec3 R;
    if (useRefraction)
    {
        float ratio = 1.0 / refractiveIndex;
        R = refract(I, normalize(Normal), ratio);
    }
    else
    {
        R = reflect(I, normalize(Normal));
    }
    FragColor = vec4(texture(skybox, R).rgb, 1.0);
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
#include <vector>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadCubemap(std::vector<std::string> faces);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool useRefraction = false;
bool rKeyPressed = false;

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
                                          "Cubemaps", nullptr, nullptr);
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

    Shader envmapShader("envmap.vs", "envmap.fs");
    Shader skyboxShader("skybox.vs", "skybox.fs");

    float cubeVertices[] = {
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

    float skyboxVertices[] = {
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    // Cube VAO (positions + normals)
    unsigned int cubeVAO, cubeVBO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)(3 * sizeof(float)));

    // Skybox VAO (positions only)
    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                          (void*)0);

    // Load cubemap
    std::vector<std::string> faces {
        "resources/textures/skybox/right.jpg",
        "resources/textures/skybox/left.jpg",
        "resources/textures/skybox/top.jpg",
        "resources/textures/skybox/bottom.jpg",
        "resources/textures/skybox/front.jpg",
        "resources/textures/skybox/back.jpg"
    };
    unsigned int cubemapTexture = loadCubemap(faces);

    envmapShader.use();
    envmapShader.setInt("skybox", 0);
    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        // --- Draw reflective/refractive cubes ---
        envmapShader.use();
        envmapShader.setMat4("view", view);
        envmapShader.setMat4("projection", projection);
        envmapShader.setVec3("cameraPos", camera.Position);
        envmapShader.setBool("useRefraction", useRefraction);
        envmapShader.setFloat("refractiveIndex", 1.52f); // glass

        glBindVertexArray(cubeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);

        // Cube 1 — reflective/refractive
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(-1.0f, 0.0f, -1.0f));
        envmapShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Cube 2 — reflective/refractive (different position)
        model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(1.5f, 0.0f, 0.5f));
        float angle = static_cast<float>(glfwGetTime()) * 25.0f;
        model = glm::rotate(model, glm::radians(angle),
                             glm::vec3(0.5f, 1.0f, 0.0f));
        envmapShader.setMat4("model", model);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // --- Draw skybox (last) ---
        glDepthFunc(GL_LEQUAL);
        skyboxShader.use();
        glm::mat4 skyView = glm::mat4(glm::mat3(camera.GetViewMatrix()));
        skyboxShader.setMat4("view", skyView);
        skyboxShader.setMat4("projection", projection);

        glBindVertexArray(skyboxVAO);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glDepthFunc(GL_LESS);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &skyboxVBO);

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

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !rKeyPressed)
    {
        useRefraction = !useRefraction;
        rKeyPressed = true;
        std::cout << "Mode: "
                  << (useRefraction ? "Refraction" : "Reflection")
                  << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE)
        rKeyPressed = false;
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

unsigned int loadCubemap(std::vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height,
                                        &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, GL_RGB, width, height, 0, GL_RGB,
                         GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: "
                      << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T,
                    GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,
                    GL_CLAMP_TO_EDGE);

    return textureID;
}
```

### What You Should See

A scenic skybox surrounding your scene, with two cubes that reflect the environment like polished chrome. One cube slowly rotates, and its reflections shift dynamically as its surface normals change relative to the camera. Press `R` to switch to refraction mode — the cubes become glass-like, showing a distorted see-through view of the skybox.

```
  Reflection mode:              Refraction mode:

  ┌──────────────────────┐      ┌──────────────────────┐
  │  sky  sky  sky  sky  │      │  sky  sky  sky  sky  │
  │                      │      │                      │
  │    ┌────┐   ┌────┐   │      │    ┌────┐   ┌────┐   │
  │    │ ≋≋ │   │ ≋≋ │   │      │    │ ⟋⟋ │   │ ⟋⟋ │   │
  │    │ ≋≋ │   │ ≋≋ │   │      │    │ ⟋⟋ │   │ ⟋⟋ │   │
  │    └────┘   └────┘   │      │    └────┘   └────┘   │
  │  (mirror-like)       │      │  (glass-like)        │
  │  sky  sky  sky  sky  │      │  sky  sky  sky  sky  │
  └──────────────────────┘      └──────────────────────┘
```

---

## Key Takeaways

- A **cubemap** is a texture sampled with a 3D direction vector, composed of 6 face images.
- **Skyboxes** use cubemaps to create the illusion of a surrounding environment. Strip the translation from the view matrix so the skybox doesn't move with the camera.
- Draw the skybox **last** with `GL_LEQUAL` and depth set to maximum (the `xyww` trick) for both correctness and performance.
- **Reflection** uses `reflect(I, N)` to bounce the view direction off the surface normal, then samples the cubemap with the reflected vector.
- **Refraction** uses `refract(I, N, ratio)` with the ratio of refractive indices to simulate light bending through transparent materials.
- **Dynamic environment maps** re-render the scene into a cubemap each frame — powerful but expensive.

## Exercises

1. Try different skybox textures. Many free skybox packs are available online (search for "free cubemap skybox"). Make sure the face order matches what your loader expects.
2. Mix reflection with a base material color: `FragColor = vec4(mix(objectColor, texture(skybox, R).rgb, 0.8), 1.0)`. A reflectivity factor of 0.8 means 80% reflection, 20% base color. Experiment with different values.
3. Implement a **Fresnel effect**: objects should be more reflective at glancing angles. Approximate Fresnel with: `float fresnel = pow(1.0 - max(dot(normalize(-I), N), 0.0), 3.0);` and use it to interpolate between reflection and a base color.
4. Try different refractive indices: water (1.33), ice (1.31), diamond (2.42). Which looks most convincingly transparent?
5. Implement **chromatic aberration**: sample the cubemap three times with slightly different refractive indices for R, G, and B channels (e.g., 1.50, 1.52, 1.54). This simulates how real glass separates white light into a rainbow at edges.
6. Instead of drawing the skybox last, try drawing it first with `glDepthMask(GL_FALSE)` (disable depth writing). Then draw the scene normally. Both approaches produce the same result — why? Which is more efficient and why?

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Framebuffers](05_framebuffers.md) | [Table of Contents](../../README.md) | [Next: Advanced Data →](07_advanced_data.md) |
