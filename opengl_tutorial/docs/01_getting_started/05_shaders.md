# Shaders

In the previous chapter, we hardcoded shader source code as C strings and used a fixed orange color. That was fine for a first triangle, but real applications need flexible, dynamic shaders. In this chapter, we'll dive deep into GLSL — the language shaders are written in — learn how to pass data between shaders and from C++, make a rainbow triangle, and build a reusable Shader class that will serve us for the rest of this tutorial.

---

## What is GLSL?

**GLSL** (OpenGL Shading Language) is a C-like language designed specifically for writing GPU programs. Each shader is a complete GLSL program with its own `main()` function.

A shader always starts with a version declaration:

```glsl
#version 330 core
```

This tells the compiler to use GLSL version 3.30 in core profile — matching OpenGL 3.3.

The general structure of a shader:

```glsl
#version 330 core

// Input variables
in type inVariableName;

// Output variables
out type outVariableName;

// Uniform variables (set from C++)
uniform type uniformName;

void main()
{
    // Process inputs, compute outputs
    outVariableName = someValue;
}
```

## GLSL Data Types

GLSL has a rich set of built-in types optimized for graphics math.

### Scalar Types

| Type    | Description               | Example            |
|---------|---------------------------|--------------------|
| `bool`  | Boolean                   | `bool flag = true;`|
| `int`   | 32-bit signed integer     | `int count = 5;`   |
| `uint`  | 32-bit unsigned integer   | `uint id = 3u;`    |
| `float` | 32-bit floating point     | `float x = 1.0;`   |
| `double`| 64-bit floating point     | `double y = 1.0;`  |

### Vector Types

Vectors are the workhorses of GLSL. They come in 2, 3, and 4 component varieties:

| Type   | Components | Common Use                     |
|--------|------------|--------------------------------|
| `vec2` | 2 floats   | Texture coordinates (u, v)     |
| `vec3` | 3 floats   | Position (x, y, z), RGB color  |
| `vec4` | 4 floats   | Position + w, RGBA color       |
| `ivec3`| 3 ints     | Integer coordinates             |
| `bvec2`| 2 bools    | Conditional masks               |

You can create vectors in several ways:

```glsl
vec3 color = vec3(1.0, 0.5, 0.2);
vec4 position = vec4(color, 1.0);       // extend vec3 to vec4
vec2 uv = vec2(0.5, 0.5);
vec4 full = vec4(uv, 0.0, 1.0);        // extend vec2 to vec4
```

### Swizzling

One of GLSL's most powerful features is **swizzling** — accessing vector components by name in any order or combination:

```glsl
vec4 v = vec4(1.0, 2.0, 3.0, 4.0);

vec3 a = v.xyz;   // (1.0, 2.0, 3.0)
vec3 b = v.rgb;   // same thing — rgba aliases for xyzw
vec2 c = v.xx;    // (1.0, 1.0) — repeat components
vec4 d = v.zyxw;  // (3.0, 2.0, 1.0, 4.0) — rearrange
float e = v.w;    // 4.0 — single component
```

Component name sets (you can use any set, but don't mix them):

| Set      | Components     | Typical use        |
|----------|----------------|--------------------|
| `xyzw`   | x, y, z, w     | Position/direction |
| `rgba`   | r, g, b, a     | Colors             |
| `stpq`   | s, t, p, q     | Texture coordinates|

### Matrix Types

| Type   | Size    | Common Use                 |
|--------|---------|----------------------------|
| `mat2` | 2×2     | 2D transformations         |
| `mat3` | 3×3     | Normal matrices            |
| `mat4` | 4×4     | Model/View/Projection      |

### Sampler Types

| Type          | Description                          |
|---------------|--------------------------------------|
| `sampler2D`   | 2D texture                           |
| `samplerCube` | Cubemap texture                      |
| `sampler3D`   | 3D texture                           |

We'll use these when we get to the Textures chapter.

## Ins and Outs — How Data Flows Between Shaders

In the graphics pipeline, the output of one stage becomes the input of the next. In GLSL, this is expressed with `in` and `out` keywords.

**Rule**: An `out` variable in one shader stage must have a matching `in` variable (same name and type) in the next stage.

```
  ┌──────────────────────┐        ┌──────────────────────┐
  │    VERTEX SHADER     │        │   FRAGMENT SHADER    │
  │                      │        │                      │
  │ in vec3 aPos;        │        │ in vec4 vertexColor; │ ◀── matches!
  │ out vec4 vertexColor;│───────▶│                      │
  │                      │        │ out vec4 FragColor;  │
  │ void main() {       │        │                      │
  │   gl_Position = ...; │        │ void main() {       │
  │   vertexColor = ...; │        │   FragColor =        │
  │ }                    │        │     vertexColor;     │
  │                      │        │ }                    │
  └──────────────────────┘        └──────────────────────┘
```

Let's make the vertex shader output a dark-red color and have the fragment shader use it:

**Vertex Shader:**

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;

out vec4 vertexColor;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    vertexColor = vec4(0.5, 0.0, 0.0, 1.0);
}
```

**Fragment Shader:**

```glsl
#version 330 core
in vec4 vertexColor;

out vec4 FragColor;

void main()
{
    FragColor = vertexColor;
}
```

The vertex shader writes to `vertexColor`; the fragment shader reads from `vertexColor`. The name and type must match exactly.

## Uniforms

What if we want to change the color from our C++ code — perhaps to animate it over time? We can't change `in`/`out` variables from C++; those flow between shader stages. Instead, we use **uniforms**.

A **uniform** is a global variable that:
- Is set from C++ code
- Is **constant** for all vertices/fragments during a single draw call
- Is accessible from any shader stage

### Setting a Uniform from C++

**Fragment Shader:**

```glsl
#version 330 core
out vec4 FragColor;

uniform vec4 ourColor;

void main()
{
    FragColor = ourColor;
}
```

**C++ code to update the uniform every frame:**

```cpp
while (!glfwWindowShouldClose(window))
{
    processInput(window);

    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);

    // Calculate a color that changes over time
    float timeValue = glfwGetTime();
    float greenValue = (sin(timeValue) / 2.0f) + 0.5f;

    // Find the uniform's location and set it
    int vertexColorLocation = glGetUniformLocation(shaderProgram, "ourColor");
    glUniform4f(vertexColorLocation, 0.0f, greenValue, 0.0f, 1.0f);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glfwSwapBuffers(window);
    glfwPollEvents();
}
```

Key details:

1. **`glGetUniformLocation`** looks up the location of the uniform by name. Returns `-1` if the name doesn't match anything (typos are a common bug).

2. **`glUniform4f`** sets a `vec4` uniform. The `4f` suffix means "4 floats." There's a whole family of these functions:

| Function | GLSL Type | Parameters |
|----------|-----------|------------|
| `glUniform1f` | `float` | 1 float |
| `glUniform2f` | `vec2` | 2 floats |
| `glUniform3f` | `vec3` | 3 floats |
| `glUniform4f` | `vec4` | 4 floats |
| `glUniform1i` | `int` / `sampler2D` | 1 int |
| `glUniform1ui` | `uint` | 1 unsigned int |
| `glUniformMatrix4fv` | `mat4` | 1 int (count), 1 bool (transpose), float* |

3. **You must call `glUseProgram` before setting uniforms** — uniforms are set on the *currently active* shader program.

> **Gotcha**: If a uniform is declared but never used in the shader, the GLSL compiler will optimize it out. `glGetUniformLocation` will then return `-1`, and `glUniform*` calls with `-1` are silently ignored.

### Complete Animated Color Example

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos, 1.0);\n"
    "}\0";

const char* fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "uniform vec4 ourColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = ourColor;\n"
    "}\0";

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
                                          "LearnOpenGL", nullptr, nullptr);
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

    // --- Compile shaders ---
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                  << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // --- Vertex data ---
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // --- Render loop ---
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        // Animate the color using time
        float timeValue = glfwGetTime();
        float greenValue = (sin(timeValue) / 2.0f) + 0.5f;
        int vertexColorLocation = glGetUniformLocation(shaderProgram, "ourColor");
        glUniform4f(vertexColorLocation, 0.0f, greenValue, 0.0f, 1.0f);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

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

You should see a triangle that smoothly pulses between black and green.

## More Vertex Attributes — Per-Vertex Color

Uniforms apply to the entire draw call. But what if we want each *vertex* to have its own color? We add color as an additional **vertex attribute**.

### Interleaved Vertex Data

We store both position and color data in the same buffer, interleaved:

```cpp
float vertices[] = {
    // positions         // colors
     0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // bottom-right: red
    -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // bottom-left:  green
     0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // top:          blue
};
```

The memory layout now looks like this:

```
  Byte offset: 0    4    8   12   16   20   24   28   32   36   40   44
               ┌────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┬────┐
               │ x0 │ y0 │ z0 │ r0 │ g0 │ b0 │ x1 │ y1 │ z1 │ r1 │ g1 │ b1 │ ...
               └────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┴────┘
               │◄─── Vertex 0 ──────────────▶│◄─── Vertex 1 ──────────────▶│
               │                              │
               │◄──── stride: 24 bytes ──────▶│
               │                              │
  Position:    offset = 0                     │
  Color:       offset = 12 ──────────────────-┘
```

Each vertex now takes 6 floats = 24 bytes. The **stride** is 24 for both attributes. The **offset** differs:

| Attribute | Location | Components | Stride | Offset |
|-----------|----------|------------|--------|--------|
| Position  | 0        | 3 (xyz)    | 24     | 0      |
| Color     | 1        | 3 (rgb)    | 24     | 12     |

### Configuring Two Attributes

```cpp
// Position attribute (location = 0)
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
glEnableVertexAttribArray(0);

// Color attribute (location = 1)
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                      (void*)(3 * sizeof(float)));
glEnableVertexAttribArray(1);
```

The offset for the color attribute is `3 * sizeof(float)` = 12 bytes — it starts after the first 3 position floats.

### Updated Shaders

**Vertex Shader:**

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
}
```

**Fragment Shader:**

```glsl
#version 330 core
in vec3 ourColor;

out vec4 FragColor;

void main()
{
    FragColor = vec4(ourColor, 1.0);
}
```

### Fragment Interpolation — The Rainbow Triangle

When the rasterizer generates fragments between vertices, it **automatically interpolates** all `out`/`in` variables from the vertex shader. This is called **fragment interpolation**.

For our triangle:
- Bottom-right vertex outputs `(1, 0, 0)` — **red**
- Bottom-left vertex outputs `(0, 1, 0)` — **green**
- Top vertex outputs `(0, 0, 1)` — **blue**

Fragments between these vertices receive a smoothly blended mix of these colors:

```
                    (blue)
                      /\
                     /  \
                    / ···\
                   / ·····\        ··· = interpolated colors
                  / ·······\       The center is roughly
                 / ·········\      (0.33, 0.33, 0.33) — gray
                /············\
               /______________\
          (green)          (red)
```

This produces the classic **rainbow triangle** — one of the most iconic images in OpenGL tutorials. The smooth color gradient is entirely handled by the hardware at no extra cost to you.

### Complete Rainbow Triangle Code

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

const char* vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aColor;\n"
    "out vec3 ourColor;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos, 1.0);\n"
    "   ourColor = aColor;\n"
    "}\0";

const char* fragmentShaderSource = "#version 330 core\n"
    "in vec3 ourColor;\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(ourColor, 1.0);\n"
    "}\0";

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
                                          "LearnOpenGL", nullptr, nullptr);
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

    // --- Compile shaders ---
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
                  << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
                  << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // --- Vertex data: position + color ---
    float vertices[] = {
        // positions         // colors
         0.5f, -0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // bottom-right: red
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // bottom-left:  green
         0.0f,  0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // top:          blue
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Color attribute (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // --- Render loop ---
    while (!glfwWindowShouldClose(window))
    {
        processInput(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

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

---

## Building a Reusable Shader Class

Writing shader compilation code every time is tedious and error-prone. Let's encapsulate it in a reusable class that:

1. Reads shader source code from files on disk
2. Compiles and links them
3. Provides convenient functions for setting uniforms

### The Interface

```cpp
// shader.h
#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

class Shader
{
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath);

    void use() const;

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setVec4(const std::string& name, float x, float y, float z, float w) const;

private:
    void checkCompileErrors(unsigned int shader, std::string type);
};

#endif
```

### The Implementation

```cpp
// shader.h (continued — this is a header-only implementation)

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    // 1. Read shader source code from files
    std::string vertexCode;
    std::string fragmentCode;
    std::ifstream vShaderFile;
    std::ifstream fShaderFile;

    vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try
    {
        vShaderFile.open(vertexPath);
        fShaderFile.open(fragmentPath);

        std::stringstream vShaderStream, fShaderStream;
        vShaderStream << vShaderFile.rdbuf();
        fShaderStream << fShaderFile.rdbuf();

        vShaderFile.close();
        fShaderFile.close();

        vertexCode = vShaderStream.str();
        fragmentCode = fShaderStream.str();
    }
    catch (std::ifstream::failure& e)
    {
        std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: "
                  << e.what() << std::endl;
    }

    const char* vShaderCode = vertexCode.c_str();
    const char* fShaderCode = fragmentCode.c_str();

    // 2. Compile shaders
    unsigned int vertex, fragment;

    // Vertex shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    // Fragment shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    // 3. Link shader program
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    // 4. Delete individual shaders (now linked into program)
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void Shader::use() const
{
    glUseProgram(ID);
}

void Shader::setBool(const std::string& name, bool value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) const
{
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) const
{
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const
{
    glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
}

void Shader::setVec4(const std::string& name,
                     float x, float y, float z, float w) const
{
    glUniform4f(glGetUniformLocation(ID, name.c_str()), x, y, z, w);
}

void Shader::checkCompileErrors(unsigned int shader, std::string type)
{
    int success;
    char infoLog[1024];

    if (type != "PROGRAM")
    {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::SHADER_COMPILATION_ERROR of type: " << type
                      << "\n" << infoLog
                      << "\n -- --------------------------------------------------- -- "
                      << std::endl;
        }
    }
    else
    {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cout << "ERROR::PROGRAM_LINKING_ERROR of type: " << type
                      << "\n" << infoLog
                      << "\n -- --------------------------------------------------- -- "
                      << std::endl;
        }
    }
}
```

### Using the Shader Class

First, create shader files on disk:

**shaders/vertex.glsl:**

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

out vec3 ourColor;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    ourColor = aColor;
}
```

**shaders/fragment.glsl:**

```glsl
#version 330 core
in vec3 ourColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(ourColor, 1.0);
}
```

Then in your C++ code:

```cpp
#include "shader.h"

// ... (GLFW/GLAD setup as before) ...

// Replace all the shader compilation code with one line:
Shader ourShader("shaders/vertex.glsl", "shaders/fragment.glsl");

// In the render loop:
ourShader.use();
ourShader.setFloat("someUniform", 1.0f);  // if you had a uniform
glBindVertexArray(VAO);
glDrawArrays(GL_TRIANGLES, 0, 3);
```

Compare this to the 40+ lines of manual compilation code we had before. The Shader class encapsulates all the boilerplate and provides clear error messages when things go wrong.

### Updated Project Structure

```
opengl_tutorial/
├── CMakeLists.txt
├── src/
│   └── main.cpp
├── include/
│   └── shader.h
├── shaders/
│   ├── vertex.glsl
│   └── fragment.glsl
└── third_party/
    └── glad/
```

Update your `CMakeLists.txt` to include the new header directory:

```cmake
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
```

---

## Summary of Data Flow

Here is a complete picture of how data flows from your C++ application through the shaders:

```
  ┌───────────────────────────────────────────────────────────────────┐
  │                        C++ APPLICATION                            │
  │                                                                   │
  │  float vertices[] = { pos, color, pos, color, ... };             │
  │                          │                                        │
  │  glBufferData ──────────▶│                                        │
  │                          ▼                                        │
  │              ┌──── GPU MEMORY (VBO) ────┐                         │
  │              │ x y z r g b x y z r g b  │                         │
  │              └──────────────────────────-┘                        │
  │                          │                                        │
  │  glVertexAttribPointer ──┤  "location 0 = xyz at offset 0"       │
  │                          │  "location 1 = rgb at offset 12"      │
  │                          ▼                                        │
  │  ┌───────── VERTEX SHADER ─────────┐                              │
  │  │ in vec3 aPos;    (location 0)   │                              │
  │  │ in vec3 aColor;  (location 1)   │                              │
  │  │ out vec3 ourColor;              │                              │
  │  │                                 │                              │
  │  │ gl_Position = vec4(aPos, 1.0);  │                              │
  │  │ ourColor = aColor;              │───── per vertex              │
  │  └─────────────────────────────────┘                              │
  │                          │                                        │
  │              (rasterizer interpolates)                             │
  │                          │                                        │
  │                          ▼                                        │
  │  ┌──────── FRAGMENT SHADER ────────┐                              │
  │  │ in vec3 ourColor; (interpolated)│                              │
  │  │ out vec4 FragColor;             │                              │
  │  │                                 │                              │
  │  │ FragColor = vec4(ourColor, 1.0);│───── per pixel               │
  │  └─────────────────────────────────┘                              │
  │                          │                                        │
  │                          ▼                                        │
  │                    ┌──────────┐                                    │
  │                    │  Screen  │                                    │
  │                    └──────────┘                                    │
  │                                                                   │
  │  Uniforms: glUniform* ─────────▶ accessible in ANY shader stage   │
  │            (same value for all vertices/fragments in a draw call)  │
  └───────────────────────────────────────────────────────────────────┘
```

---

## Key Takeaways

- **GLSL** is a C-like language with built-in vector/matrix types and swizzling. It runs on the GPU.
- Data flows from vertex shader to fragment shader via **`out`/`in` variables** with matching names.
- **Uniforms** are global variables set from C++ via `glGetUniformLocation` + `glUniform*`. They are constant across a single draw call.
- You can send **multiple attributes per vertex** (position, color, normals, texture coords) by interleaving them in a VBO and configuring the stride and offset in `glVertexAttribPointer`.
- The rasterizer **automatically interpolates** vertex shader outputs across fragments — this is how the rainbow triangle works.
- A **Shader class** encapsulates the boilerplate of reading, compiling, linking, and using shaders. We'll use it throughout the rest of this tutorial.

## Exercises

1. **Flip the triangle upside down** by modifying the vertex shader (not the vertex data). Hint: negate the y component of `gl_Position`.

2. **Move the triangle to the right** using a uniform `float xOffset`. Set it from C++ and use it in the vertex shader: `gl_Position = vec4(aPos.x + xOffset, aPos.y, aPos.z, 1.0);`

3. **Output the vertex position as a color** in the vertex shader: `ourColor = aPos;` What do you see? Why does the bottom-left corner appear black? (Think about what happens when position values are negative — colors clamp to 0.)

4. **Combine uniforms and vertex attributes**: Use vertex colors but also add a `uniform float mixValue;` that blends between the vertex color and a solid white. Use GLSL's `mix()` function: `FragColor = vec4(mix(ourColor, vec3(1.0), mixValue), 1.0);`

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Hello Triangle](04_hello_triangle.md) | | Next: Textures → |
