# Getting Started — Review

[Previous: Camera](10_camera.md) | [Next: Colors (Section 2)](../../docs/02_lighting/01_colors.md)

---

Congratulations! You've completed the entire "Getting Started" section of this OpenGL tutorial. Let's consolidate everything you've learned before moving on to lighting and more advanced topics.

## Summary of What You've Learned

Over the course of this section, you've gone from opening a blank window to flying through a 3D scene with a camera. Here's the journey:

1. **OpenGL Context & Window** — You set up GLFW, created a window with an OpenGL 3.3 core profile context, and loaded function pointers with GLAD.

2. **The Rendering Pipeline** — You learned how vertices flow through the pipeline: vertex shader → primitive assembly → rasterization → fragment shader → testing/blending → framebuffer.

3. **Hello Triangle** — You created your first geometry using VAOs, VBOs, and EBOs. You wrote minimal vertex and fragment shaders in GLSL.

4. **Shaders** — You dove deeper into GLSL: uniforms, vertex attributes, in/out variables, and how data flows from vertex shader to fragment shader via interpolation.

5. **Textures** — You loaded images with stb_image, created texture objects, and mapped images onto geometry using texture coordinates. You blended multiple textures with `mix()`.

6. **Transformations** — You learned vector and matrix math, then used GLM to scale, rotate, and translate objects with 4×4 transformation matrices.

7. **Coordinate Systems** — You understood the five coordinate spaces (local → world → view → clip → screen), set up model/view/projection matrices, and enabled depth testing for proper 3D rendering.

8. **Camera** — You built a first-person camera with WASD movement, mouse look (Euler angles), and scroll zoom, all encapsulated in a reusable Camera class.

## Key Concepts Checklist

Use this as a self-assessment. You should be able to explain each concept:

- [ ] **GLFW & GLAD** — Window creation, OpenGL context, function loading
- [ ] **VAO / VBO / EBO** — Vertex Array Object, Vertex Buffer Object, Element Buffer Object
- [ ] **Vertex Attributes** — `glVertexAttribPointer`, stride, offset, `glEnableVertexAttribArray`
- [ ] **Shaders (GLSL)** — Vertex shader, fragment shader, `in`/`out` variables, `uniform`
- [ ] **Shader Compilation** — `glCreateShader`, `glCompileShader`, `glLinkProgram`, error checking
- [ ] **Textures** — `glGenTextures`, `glBindTexture`, `glTexImage2D`, texture coordinates
- [ ] **Texture Parameters** — Wrapping modes, filtering modes, mipmaps
- [ ] **Texture Units** — `glActiveTexture`, `sampler2D`, multiple textures
- [ ] **Transformation Matrices** — Scale, rotation, translation; order matters
- [ ] **GLM** — `glm::translate`, `glm::rotate`, `glm::scale`, `glm::value_ptr`
- [ ] **Coordinate Systems** — Local, world, view, clip, screen space
- [ ] **Model/View/Projection** — What each matrix does, MVP multiplication order
- [ ] **Projection** — Orthographic vs. perspective, FOV, aspect ratio, near/far planes
- [ ] **Depth Testing** — `glEnable(GL_DEPTH_TEST)`, `GL_DEPTH_BUFFER_BIT`
- [ ] **Camera** — LookAt matrix, Euler angles (yaw/pitch), delta time
- [ ] **Input Handling** — Keyboard polling, mouse callbacks, scroll callbacks

## Common Mistakes and Troubleshooting

### Black Screen

| Possible Cause | Fix |
|----------------|-----|
| Shaders didn't compile | Check `glGetShaderiv(GL_COMPILE_STATUS)` and print the info log |
| Shader program didn't link | Check `glGetProgramiv(GL_LINK_STATUS)` and print the info log |
| Forgot to bind VAO before drawing | Call `glBindVertexArray(VAO)` before `glDrawArrays`/`glDrawElements` |
| Forgot to use shader program | Call `glUseProgram(shaderProgram)` before drawing |
| Vertices are behind the camera | Check your model/view/projection matrices; ensure objects are in front of the near plane |
| Clear color matches geometry color | Change `glClearColor` to something different |

### Weird / Wrong Colors

| Possible Cause | Fix |
|----------------|-----|
| Texture didn't load | Check `stbi_load` return value; verify file path |
| PNG loaded as GL_RGB instead of GL_RGBA | Use `GL_RGBA` for images with alpha channels |
| Texture is upside-down | Call `stbi_set_flip_vertically_on_load(true)` before loading |
| Sampler uniform not set | Set each `sampler2D` uniform to its texture unit index with `glUniform1i` |
| Wrong texture unit active | Verify `glActiveTexture(GL_TEXTUREn)` matches the sampler uniform value |

### Nothing Drawn

| Possible Cause | Fix |
|----------------|-----|
| Wrong vertex count | Check the count parameter in `glDrawArrays`/`glDrawElements` |
| Wrong index type | If using `unsigned int` indices, pass `GL_UNSIGNED_INT` to `glDrawElements` |
| Vertex data not uploaded | Verify `glBufferData` was called with the correct size and data pointer |
| Attribute pointers misconfigured | Double-check stride and offset values in `glVertexAttribPointer` |
| Forgot `glEnableVertexAttribArray` | Each attribute needs to be explicitly enabled |

### Camera Not Working

| Possible Cause | Fix |
|----------------|-----|
| Delta time is zero or huge | Verify `deltaTime` is calculated each frame as `currentFrame - lastFrame` |
| Mouse callback not registered | Call `glfwSetCursorPosCallback` before the render loop |
| Large initial mouse jump | Use the `firstMouse` flag to skip the first delta |
| Yaw starts at 0 (looking along +X) | Initialize `yaw = -90.0f` to start looking along -Z |
| Camera moves too fast/slow | Adjust `MovementSpeed` or check that delta time is in seconds |

### Depth Issues

| Possible Cause | Fix |
|----------------|-----|
| Back faces drawn over front faces | Enable depth testing: `glEnable(GL_DEPTH_TEST)` |
| Depth buffer not cleared | Add `GL_DEPTH_BUFFER_BIT` to `glClear` |
| Z-fighting (flickering faces) | Increase the near plane distance; avoid extremely large far/near ratio |

## Architecture of a Basic OpenGL Application

Here's the high-level structure of every program we've built:

```
┌──────────────────────────────────────────────────────────┐
│                    INITIALIZATION                         │
│                                                          │
│  1. Initialize GLFW, set window hints                    │
│  2. Create window, make context current                  │
│  3. Load OpenGL functions (GLAD)                         │
│  4. Set callbacks (framebuffer, mouse, scroll)           │
│  5. Enable features (depth test, etc.)                   │
│                                                          │
├──────────────────────────────────────────────────────────┤
│                   SHADER SETUP                            │
│                                                          │
│  6. Compile vertex shader                                │
│  7. Compile fragment shader                              │
│  8. Link into shader program                             │
│  9. Delete individual shaders                            │
│                                                          │
├──────────────────────────────────────────────────────────┤
│                  GEOMETRY SETUP                            │
│                                                          │
│  10. Define vertex data (positions, colors, UVs, etc.)   │
│  11. Create VAO, VBO (and EBO if indexed)                │
│  12. Upload data with glBufferData                       │
│  13. Configure vertex attributes                         │
│                                                          │
├──────────────────────────────────────────────────────────┤
│                  TEXTURE SETUP                            │
│                                                          │
│  14. Load image files (stb_image)                        │
│  15. Create texture objects, upload pixel data            │
│  16. Set wrapping and filtering parameters               │
│  17. Assign texture units to sampler uniforms            │
│                                                          │
├──────────────────────────────────────────────────────────┤
│                   RENDER LOOP                             │
│                                                          │
│  while (!glfwWindowShouldClose(window)) {                │
│      a. Calculate delta time                             │
│      b. Process input (keyboard)                         │
│      c. Clear color and depth buffers                    │
│      d. Bind textures to texture units                   │
│      e. Activate shader program                          │
│      f. Set uniforms (MVP matrices, etc.)                │
│      g. Bind VAO                                         │
│      h. Draw call (glDrawArrays / glDrawElements)        │
│      i. Swap buffers, poll events                        │
│  }                                                       │
│                                                          │
├──────────────────────────────────────────────────────────┤
│                    CLEANUP                                │
│                                                          │
│  Delete VAO, VBO, EBO, shader program                    │
│  glfwTerminate()                                         │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

## Mini-Project: Textured Scene with Camera

Combine everything from this section into a single project. Here's a complete program that renders a scene of textured cubes you can fly through:

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "camera.h"

#include <iostream>

const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

void framebuffer_size_callback(GLFWwindow* window, int w, int h) {
    glViewport(0, 0, w, h);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    camera.ProcessMouseMovement(xpos - lastX, lastY - ypos);
    lastX = xpos;
    lastY = ypos;
}

void scroll_callback(GLFWwindow* window, double /*xoff*/, double yoff) {
    camera.ProcessMouseScroll(static_cast<float>(yoff));
}

void processInput(GLFWwindow* window) {
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

unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        std::cout << "Shader compilation error:\n" << infoLog << std::endl;
    }
    return shader;
}

unsigned int loadTexture(const char* path, GLenum format) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

int main() {
    // --- Init ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "OpenGL Review - 3D Scene",
                                          NULL, NULL);
    if (!window) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // --- Shaders ---
    const char* vertSrc = R"(
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

    const char* fragSrc = R"(
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

    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertSrc);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    // --- Geometry ---
    float vertices[] = {
        // positions          // tex coords
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

    // --- Textures ---
    stbi_set_flip_vertically_on_load(true);
    unsigned int tex1 = loadTexture("container.jpg", GL_RGB);
    unsigned int tex2 = loadTexture("awesomeface.png", GL_RGBA);

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "texture1"), 0);
    glUniform1i(glGetUniformLocation(program, "texture2"), 1);

    // --- Render Loop ---
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, tex2);

        glUseProgram(program);

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glUniformMatrix4fv(glGetUniformLocation(program, "projection"),
                           1, GL_FALSE, glm::value_ptr(projection));

        glm::mat4 view = camera.GetViewMatrix();
        glUniformMatrix4fv(glGetUniformLocation(program, "view"),
                           1, GL_FALSE, glm::value_ptr(view));

        glBindVertexArray(VAO);
        for (unsigned int i = 0; i < 10; i++) {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);

            float angle = 20.0f * i + currentFrame * 15.0f * (i + 1);
            model = glm::rotate(model, glm::radians(angle),
                                glm::vec3(1.0f, 0.3f, 0.5f));

            float scale = 0.8f + 0.2f * sin(currentFrame + i);
            model = glm::scale(model, glm::vec3(scale));

            glUniformMatrix4fv(glGetUniformLocation(program, "model"),
                               1, GL_FALSE, glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // --- Cleanup ---
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(program);
    glfwTerminate();
    return 0;
}
```

This mini-project demonstrates every major concept from Section 1 in one cohesive program.

## Glossary

| Term | Definition |
|------|------------|
| **VAO** | Vertex Array Object — stores vertex attribute configuration |
| **VBO** | Vertex Buffer Object — GPU-side buffer holding vertex data |
| **EBO / IBO** | Element/Index Buffer Object — stores indices for indexed drawing |
| **GLSL** | OpenGL Shading Language — the C-like language for writing shaders |
| **Shader** | A small program that runs on the GPU (vertex, fragment, geometry, etc.) |
| **Uniform** | A global shader variable set from CPU code; constant for all vertices/fragments in a draw call |
| **Vertex Attribute** | Per-vertex data (position, color, texture coordinates, normals, etc.) |
| **Texture** | An image mapped onto geometry via texture coordinates |
| **Texel** | A single pixel of a texture image |
| **Sampler** | A GLSL type (`sampler2D`) that reads from a texture |
| **Texture Unit** | A slot where a texture is bound; allows multiple textures simultaneously |
| **Mipmap** | Pre-computed, progressively smaller versions of a texture |
| **MVP** | Model-View-Projection — the three matrices that transform vertices from local to clip space |
| **Model Matrix** | Transforms local → world space (position, rotation, scale of an object) |
| **View Matrix** | Transforms world → view space (the camera's inverse transform) |
| **Projection Matrix** | Transforms view → clip space (perspective or orthographic) |
| **NDC** | Normalized Device Coordinates — the [-1, 1] range after perspective division |
| **Depth Buffer** | Per-pixel buffer storing depth values for correct occlusion |
| **Euler Angles** | Yaw, pitch, roll — angles describing orientation |
| **Delta Time** | Time between current and previous frame; used for frame-rate-independent movement |
| **GLM** | OpenGL Mathematics — a header-only C++ math library for graphics |
| **stb_image** | A single-header C library for loading images |
| **Framebuffer** | The final target where rendered pixels are stored (default = the screen) |
| **Rasterization** | Converting primitives (triangles) into fragments (potential pixels) |
| **Fragment** | A candidate pixel produced by rasterization; may be discarded by depth test or other tests |
| **Homogeneous Coordinates** | 4D representation of 3D points (w=1) and directions (w=0) |

## What's Next

In the next section, **Lighting**, you'll learn:

- **Colors** in OpenGL and how light interacts with surfaces
- **Basic lighting** — the Phong lighting model (ambient, diffuse, specular)
- **Materials** — defining how surfaces respond to light
- **Lighting maps** — diffuse and specular maps for detailed surfaces
- **Light casters** — directional lights, point lights, spotlights
- **Multiple lights** — combining all light types in one scene

The foundation you've built in this section — shaders, textures, transformations, coordinate systems, and camera — is exactly what you need to start creating beautifully lit 3D scenes.

---

[Previous: Camera](10_camera.md) | [Next: Colors (Section 2)](../../docs/02_lighting/01_colors.md)
