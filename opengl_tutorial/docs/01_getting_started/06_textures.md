# Textures

[Previous: Shaders](05_shaders.md) | [Next: Transformations](07_transformations.md)

---

Up to this point, we've been coloring our geometry by passing color data through vertex attributes. But to add real visual detail to objects — wood grain, brick walls, metal surfaces — we'd need an absurd number of vertices, each with its own color. Instead, graphics programmers use **textures**: images mapped onto geometry to give the illusion of detail.

## What Is a Texture?

A texture is simply a 2D image (or even 1D or 3D) that we "wrap" onto the surface of our geometry. Think of it like gift wrapping: you have a flat piece of paper (the image) and you fold it around a 3D box (your geometry). Each pixel of the texture is called a **texel** (texture element), analogous to a pixel on screen.

Textures can store more than just colors — they can hold height data, normals, and other per-pixel information — but for now we'll focus on color textures.

## Texture Coordinates (UV Coordinates)

To tell OpenGL *which part* of the texture maps to *which vertex*, we use **texture coordinates**, often called **UV coordinates** (U for the horizontal axis, V for the vertical). Texture coordinates range from `0.0` to `1.0`:

- `(0.0, 0.0)` is the **bottom-left** corner of the texture
- `(1.0, 1.0)` is the **top-right** corner
- `(0.5, 0.5)` is the center

```
Texture Image
(0,1)         (1,1)
  +-----------+
  |           |
  |   IMAGE   |
  |           |
  +-----------+
(0,0)         (1,0)

Mapped onto a triangle:

      (0.5, 1.0)
         /\
        /  \
       /    \
      / tex  \
     /________\
(0.0,0.0)  (1.0,0.0)
```

We specify texture coordinates per vertex, and OpenGL **interpolates** those coordinates across the surface of each triangle (just like it interpolates colors). The fragment shader then uses the interpolated coordinate to look up the color from the texture image — this lookup is called **sampling**.

Here's how texture coordinates map to a rectangle (two triangles):

```
Vertex 0: position (-0.5, -0.5)  →  texCoord (0.0, 0.0)  bottom-left
Vertex 1: position ( 0.5, -0.5)  →  texCoord (1.0, 0.0)  bottom-right
Vertex 2: position ( 0.5,  0.5)  →  texCoord (1.0, 1.0)  top-right
Vertex 3: position (-0.5,  0.5)  →  texCoord (0.0, 1.0)  top-left
```

## Texture Wrapping

What happens if we specify texture coordinates outside the `[0.0, 1.0]` range? OpenGL provides several **wrapping modes** controlled via `glTexParameteri`:

| Mode | Behavior |
|------|----------|
| `GL_REPEAT` | The texture repeats (tiles). This is the default. |
| `GL_MIRRORED_REPEAT` | The texture repeats but mirrors on each repetition. |
| `GL_CLAMP_TO_EDGE` | Coordinates are clamped to `[0.0, 1.0]`; the edge texels stretch. |
| `GL_CLAMP_TO_BORDER` | Coordinates outside the range produce a user-defined border color. |

```
GL_REPEAT:
+---+---+---+
| A | A | A |
+---+---+---+
| A | A | A |
+---+---+---+

GL_MIRRORED_REPEAT:
+---+---+---+
| A | Ↄ | A |     (Ↄ = mirrored A)
+---+---+---+
| Ɐ | ? | Ɐ |     (Ɐ = vertically mirrored)
+---+---+---+

GL_CLAMP_TO_EDGE:
+---+===+===+
| A | → | → |     (edge pixels stretch outward)
+---+===+===+
| ↓ | ↘ | ↘ |
+---+===+===+

GL_CLAMP_TO_BORDER:
+---+---+---+
| B | B | B |     (B = border color)
+---+---+---+
| B | A | B |     (A = actual texture)
+---+---+---+
```

Texture coordinates have **S** and **T** axes (equivalent to X and Y). You set wrapping independently for each axis:

```cpp
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
```

For `GL_CLAMP_TO_BORDER`, you also set the border color:

```cpp
float borderColor[] = { 1.0f, 1.0f, 0.0f, 1.0f }; // yellow
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
```

## Texture Filtering

Texture coordinates rarely align exactly with texels. When a texture is stretched larger or squeezed smaller than its original resolution, OpenGL must decide which texel color to use for each fragment. This is **texture filtering**.

### GL_NEAREST (Nearest-Neighbor)

OpenGL selects the texel whose center is closest to the texture coordinate. This produces a sharp, **pixelated** look:

```
Zoomed in with GL_NEAREST:
+----+----+----+----+
|####|####|    |    |
|####|####|    |    |
+----+----+----+----+
|####|####|    |    |
|####|####|    |    |
+----+----+----+----+
|    |    |####|####|
|    |    |####|####|
+----+----+----+----+
Sharp, blocky — good for pixel art
```

### GL_LINEAR (Bilinear)

OpenGL takes the 4 nearest texels and interpolates between them based on distance. This produces a **smoother**, blurrier look:

```
Zoomed in with GL_LINEAR:
+----+----+----+----+
|████|▓▓▓▓|░░░░|    |
|████|▓▓▓▓|░░░░|    |
+----+----+----+----+
|▓▓▓▓|▒▒▒▒|░░░░|    |
|▓▓▓▓|▒▒▒▒|░░░░|    |
+----+----+----+----+
Smooth gradients between texels
```

Set filtering for **magnification** (texture stretched) and **minification** (texture shrunk):

```cpp
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

## Mipmaps

When an object is far away, its texture is rendered at a much smaller size than the original image. Sampling a high-resolution texture for a handful of screen pixels is wasteful and creates visual artifacts (shimmering, moiré patterns).

**Mipmaps** are a sequence of pre-computed, progressively smaller versions of the texture, each exactly half the size of the previous:

```
Level 0: 256×256  (original)
Level 1: 128×128
Level 2:  64×64
Level 3:  32×32
Level 4:  16×16
Level 5:   8×8
Level 6:   4×4
Level 7:   2×2
Level 8:   1×1
```

OpenGL automatically picks the mipmap level that best matches the size the texture appears on screen. You generate mipmaps with a single call after creating the texture:

```cpp
glGenerateMipmap(GL_TEXTURE_2D);
```

### Mipmap Filtering Modes

Mipmap filtering only applies to the **minification** filter (`GL_TEXTURE_MIN_FILTER`):

| Filter | Description |
|--------|-------------|
| `GL_NEAREST_MIPMAP_NEAREST` | Nearest mipmap level, nearest-neighbor sampling within it |
| `GL_LINEAR_MIPMAP_NEAREST` | Nearest mipmap level, linear sampling within it |
| `GL_NEAREST_MIPMAP_LINEAR` | Interpolate between the two closest mipmap levels, nearest-neighbor in each |
| `GL_LINEAR_MIPMAP_LINEAR` | Interpolate between two mipmap levels, linear sampling in each (**trilinear filtering**, highest quality) |

A common setup:

```cpp
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
```

> **Important:** Never set a mipmap filter on `GL_TEXTURE_MAG_FILTER`. Mipmaps are only used when textures are scaled *down*, not up. Setting a mipmap filter for magnification will result in an `GL_INVALID_ENUM` error.

## Loading Images with stb_image

We need a way to load image files (PNG, JPG, etc.) into raw pixel data. The [stb_image](https://github.com/nothings/stb) library is a popular single-header solution.

Download `stb_image.h` and add it to your project. In **exactly one** `.cpp` file, include it with the implementation macro:

```cpp
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
```

In all other files, include it without the macro:

```cpp
#include "stb_image.h"
```

Loading an image:

```cpp
int width, height, nrChannels;
unsigned char* data = stbi_load("container.jpg", &width, &height, &nrChannels, 0);
if (data) {
    // success — use the data
} else {
    std::cout << "Failed to load texture" << std::endl;
}
// After uploading to OpenGL:
stbi_image_free(data);
```

- `width`, `height`: dimensions of the loaded image
- `nrChannels`: number of color channels (3 for RGB, 4 for RGBA)
- The last parameter (`0`) means "give me however many channels the image has"

## Creating a Texture in OpenGL

Here's the complete process for creating a texture object and uploading image data:

```cpp
// 1. Generate a texture object
unsigned int texture;
glGenTextures(1, &texture);

// 2. Bind it so subsequent texture commands affect this texture
glBindTexture(GL_TEXTURE_2D, texture);

// 3. Set wrapping parameters
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

// 4. Set filtering parameters
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

// 5. Load the image
int width, height, nrChannels;
unsigned char* data = stbi_load("container.jpg", &width, &height, &nrChannels, 0);
if (data) {
    // 6. Upload pixel data to the texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    // 7. Generate mipmaps
    glGenerateMipmap(GL_TEXTURE_2D);
} else {
    std::cout << "Failed to load texture" << std::endl;
}
// 8. Free the image data
stbi_image_free(data);
```

### glTexImage2D Parameters Explained

```cpp
glTexImage2D(
    GL_TEXTURE_2D,    // target: the bound 2D texture
    0,                // mipmap level (0 = base level)
    GL_RGB,           // internal format: how OpenGL stores the texture
    width,            // width of the image
    height,           // height of the image
    0,                // border: must always be 0 (legacy)
    GL_RGB,           // format of the source pixel data
    GL_UNSIGNED_BYTE, // data type of the source pixel data
    data              // pointer to the actual image data
);
```

> **Note:** If your image has an alpha channel (PNG with transparency), use `GL_RGBA` for both the internal format and the source format.

## Applying a Texture to Our Rectangle

Let's put it all together. We'll add texture coordinates to our vertex data and update our shaders.

### Updated Vertex Data

Each vertex now has: **position (3)** + **color (3)** + **texCoord (2)** = 8 floats:

```cpp
float vertices[] = {
    // positions          // colors           // texture coords
     0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f,   // top right
     0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // bottom right
    -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f,   // bottom left
    -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f    // top left
};

unsigned int indices[] = {
    0, 1, 3,   // first triangle
    1, 2, 3    // second triangle
};
```

### Updated Vertex Attribute Pointers

With 3 attributes and a stride of `8 * sizeof(float)`:

```cpp
// Position attribute (location 0)
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                      (void*)0);
glEnableVertexAttribArray(0);

// Color attribute (location 1)
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                      (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);

// Texture coordinate attribute (location 2)
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                      (void*)(6 * sizeof(float)));
glEnableVertexAttribArray(2);
```

```
Memory layout per vertex (8 floats, 32 bytes):
+------+------+------+------+------+------+------+------+
| posX | posY | posZ | colR | colG | colB | texU | texV |
+------+------+------+------+------+------+------+------+
 offset:  0      4      8     12     16     20     24     28
 attrib: |--- position ---|  |--- color -------|  |-tex-|
```

### Vertex Shader

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec3 ourColor;
out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
    TexCoord = aTexCoord;
}
```

### Fragment Shader

```glsl
#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

uniform sampler2D ourTexture;

void main() {
    FragColor = texture(ourTexture, TexCoord);
}
```

The `texture()` GLSL function takes a **sampler** and **texture coordinates** and returns the sampled color at that point. `sampler2D` is a special type that references a texture object.

To mix the texture color with vertex colors, multiply them:

```glsl
FragColor = texture(ourTexture, TexCoord) * vec4(ourColor, 1.0);
```

### Drawing

Before drawing, bind the texture:

```cpp
glBindTexture(GL_TEXTURE_2D, texture);
glBindVertexArray(VAO);
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
```

## Texture Units

A **texture unit** is a location where a texture can be bound. OpenGL guarantees at least **16** texture units: `GL_TEXTURE0` through `GL_TEXTURE15`. This lets you use multiple textures simultaneously in a single draw call.

By default, the active texture unit is `GL_TEXTURE0`, which is why our single texture example worked without explicitly setting one.

To activate a texture unit:

```cpp
glActiveTexture(GL_TEXTURE0);  // activate unit 0
glBindTexture(GL_TEXTURE_2D, texture1);

glActiveTexture(GL_TEXTURE1);  // activate unit 1
glBindTexture(GL_TEXTURE_2D, texture2);
```

In the fragment shader, each `sampler2D` uniform must be told which texture unit to read from. You do this by setting the uniform to the texture unit **index** (an integer):

```cpp
ourShader.use();
glUniform1i(glGetUniformLocation(ourShader.ID, "texture1"), 0); // unit 0
glUniform1i(glGetUniformLocation(ourShader.ID, "texture2"), 1); // unit 1
```

## Mixing Two Textures

Let's blend two textures together: a container and a smiley face.

### Loading Two Textures

```cpp
// --- Texture 1 ---
unsigned int texture1;
glGenTextures(1, &texture1);
glBindTexture(GL_TEXTURE_2D, texture1);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

int width, height, nrChannels;
unsigned char* data = stbi_load("container.jpg", &width, &height,
                                &nrChannels, 0);
if (data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
}
stbi_image_free(data);

// --- Texture 2 ---
unsigned int texture2;
glGenTextures(1, &texture2);
glBindTexture(GL_TEXTURE_2D, texture2);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

// stb_image flips PNGs vertically by default origin; tell it to flip
stbi_set_flip_vertically_on_load(true);

data = stbi_load("awesomeface.png", &width, &height, &nrChannels, 0);
if (data) {
    // Note: PNG has alpha channel → GL_RGBA
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
}
stbi_image_free(data);
```

> **Why `stbi_set_flip_vertically_on_load(true)`?** OpenGL expects the first row of pixels to be the **bottom** of the image, but most image formats store the top row first. This function flips the image on load so it matches OpenGL's convention.

### Fragment Shader for Mixing

```glsl
#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform float mixValue;

void main() {
    FragColor = mix(texture(texture1, TexCoord),
                    texture(texture2, TexCoord),
                    mixValue);
}
```

The GLSL `mix(a, b, t)` function performs **linear interpolation**: `result = a * (1 - t) + b * t`. When `mixValue` is `0.0`, you see only `texture1`; at `1.0`, only `texture2`; at `0.2`, mostly container with a faint smiley.

### Setting Uniforms

```cpp
ourShader.use();
glUniform1i(glGetUniformLocation(ourShader.ID, "texture1"), 0);
glUniform1i(glGetUniformLocation(ourShader.ID, "texture2"), 1);
glUniform1f(glGetUniformLocation(ourShader.ID, "mixValue"), 0.2f);
```

### Drawing with Both Textures

```cpp
glActiveTexture(GL_TEXTURE0);
glBindTexture(GL_TEXTURE_2D, texture1);
glActiveTexture(GL_TEXTURE1);
glBindTexture(GL_TEXTURE_2D, texture2);

glBindVertexArray(VAO);
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
```

## Complete Source Code

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>

// Vertex shader source
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

out vec3 ourColor;
out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
    TexCoord = aTexCoord;
}
)";

// Fragment shader source
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec3 ourColor;
in vec2 TexCoord;

uniform sampler2D texture1;
uniform sampler2D texture2;
uniform float mixValue;

void main() {
    FragColor = mix(texture(texture1, TexCoord),
                    texture(texture2, TexCoord),
                    mixValue);
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
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(800, 600, "Textures", NULL, NULL);
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

    // Build and compile shaders
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "Vertex shader compilation failed:\n" << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "Fragment shader compilation failed:\n" << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "Shader program linking failed:\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Vertex data with positions, colors, and texture coordinates
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

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)0);
    glEnableVertexAttribArray(0);
    // Color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Load and create textures
    unsigned int texture1, texture2;

    // Texture 1
    glGenTextures(1, &texture1);
    glBindTexture(GL_TEXTURE_2D, texture1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    unsigned char* data = stbi_load("container.jpg", &width, &height,
                                    &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load texture: container.jpg" << std::endl;
    }
    stbi_image_free(data);

    // Texture 2
    glGenTextures(1, &texture2);
    glBindTexture(GL_TEXTURE_2D, texture2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);
    data = stbi_load("awesomeface.png", &width, &height, &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load texture: awesomeface.png" << std::endl;
    }
    stbi_image_free(data);

    // Tell each sampler which texture unit it belongs to
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture2"), 1);
    glUniform1f(glGetUniformLocation(shaderProgram, "mixValue"), 0.2f);

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Bind textures to texture units
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

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
```

## Key Takeaways

- **Textures** map 2D images onto 3D geometry using **texture coordinates** (`0.0` to `1.0`).
- **Wrapping modes** control behavior outside the `[0.0, 1.0]` range.
- **Filtering** determines how texels are sampled: `GL_NEAREST` for pixelated, `GL_LINEAR` for smooth.
- **Mipmaps** are pre-scaled copies that improve performance and reduce aliasing at distance.
- **stb_image** is a simple single-header library for loading image files.
- **Texture units** allow multiple textures to be active simultaneously.
- The GLSL `mix()` function blends two textures with a configurable ratio.

## Exercises

1. **Flip the smiley:** Make the `awesomeface` texture render upside-down by modifying the fragment shader (hint: use `vec2(TexCoord.x, 1.0 - TexCoord.y)`).

2. **Experiment with wrapping:** Set texture coordinates to range from `0.0` to `2.0` and try each wrapping mode (`GL_REPEAT`, `GL_MIRRORED_REPEAT`, `GL_CLAMP_TO_EDGE`, `GL_CLAMP_TO_BORDER`) to see the visual difference.

3. **Pixel-art look:** Display only a single texel in the center of the rectangle by clamping texture coordinates to a tiny range. Then switch between `GL_NEAREST` and `GL_LINEAR` filtering to see the difference.

4. **Interactive mix:** Use the up/down arrow keys to increase/decrease the `mixValue` uniform, blending between the two textures in real time.

---

[Previous: Shaders](05_shaders.md) | [Next: Transformations](07_transformations.md)
