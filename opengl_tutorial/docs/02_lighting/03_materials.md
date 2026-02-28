# Materials

In the previous chapter we hardcoded the ambient, diffuse, and specular strengths directly in the fragment shader. But in the real world, different objects react to light very differently — a shiny metal ball doesn't look like a rubber duck, and neither looks like a wooden table. In this chapter we'll introduce **materials** so each object can define how it interacts with light.

## Why Materials Matter

Consider these real-world surfaces:

```
  Different Materials Under the Same Light
  ─────────────────────────────────────────

  Polished Steel          Rubber Ball           Wood Plank
  ┌──────────┐           ┌──────────┐          ┌──────────┐
  │ ░░██████ │           │ ░░████░░ │          │ ░░███░░░ │
  │ ░███●███ │           │ ░██████░ │          │ ░█████░░ │
  │ ░░██████ │           │ ░░████░░ │          │ ░░███░░░ │
  └──────────┘           └──────────┘          └──────────┘
  Bright specular         No specular          Faint specular
  Small highlight         Wide diffuse          Warm tones
```

Each material has different properties:
- **Ambient color**: how much ambient light it reflects
- **Diffuse color**: the main color under direct light
- **Specular color**: the color of the highlight
- **Shininess**: how focused the specular highlight is

## The Material Struct in GLSL

We define a `struct` in GLSL to group all material properties:

```glsl
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};

uniform Material material;
```

- **`ambient`**: The color reflected under ambient lighting (usually the same as the surface color).
- **`diffuse`**: The color reflected under diffuse lighting (the object's main color under direct light).
- **`specular`**: The color of the specular highlight (often gray or white for most materials).
- **`shininess`**: The scattering/radius of the specular highlight. Higher = more focused.

## The Light Struct

Just as objects have material properties, lights can have different intensities for each component. A bright white fluorescent light has different properties than a warm candle:

```glsl
struct Light {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Light light;
```

- **`light.ambient`**: Usually set low (e.g., `0.2, 0.2, 0.2`) so ambient light doesn't dominate.
- **`light.diffuse`**: The main color/intensity of the light (e.g., `0.5, 0.5, 0.5`).
- **`light.specular`**: Usually set to full intensity `(1.0, 1.0, 1.0)` for a crisp highlight.

## Updated Fragment Shader

Now we rewrite our Phong lighting to use these structs:

```glsl
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;

uniform vec3 viewPos;
uniform Material material;
uniform Light light;

void main()
{
    // Ambient
    vec3 ambient = light.ambient * material.ambient;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * (diff * material.diffuse);

    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * (spec * material.specular);

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
```

Notice that we no longer multiply by `objectColor` — the material's `ambient` and `diffuse` properties *are* the object's color. Each lighting component is modulated by both the light's intensity and the material's reflectance.

## Vertex Shader (Unchanged)

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

## Setting Material Uniforms from C++

GLSL struct members are accessed with dot notation when setting uniforms:

```cpp
objectShader.setVec3("material.ambient",   1.0f, 0.5f, 0.31f);
objectShader.setVec3("material.diffuse",   1.0f, 0.5f, 0.31f);
objectShader.setVec3("material.specular",  0.5f, 0.5f, 0.5f);
objectShader.setFloat("material.shininess", 32.0f);

objectShader.setVec3("light.position", lightPos);
objectShader.setVec3("light.ambient",  0.2f, 0.2f, 0.2f);
objectShader.setVec3("light.diffuse",  0.5f, 0.5f, 0.5f);
objectShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
```

Under the hood, OpenGL treats struct members as individual uniforms with the dotted name. For example, `"material.ambient"` is the uniform name you pass to `glGetUniformLocation`.

## Real-World Material Values

The table below lists material properties for various real-world materials. These values come from classic OpenGL material tables and produce reasonably realistic results:

| Material | Ambient | Diffuse | Specular | Shininess |
|----------|---------|---------|----------|-----------|
| Emerald | (0.0215, 0.1745, 0.0215) | (0.07568, 0.61424, 0.07568) | (0.633, 0.727811, 0.633) | 76.8 |
| Jade | (0.135, 0.2225, 0.1575) | (0.54, 0.89, 0.63) | (0.316228, 0.316228, 0.316228) | 12.8 |
| Obsidian | (0.05375, 0.05, 0.06625) | (0.18275, 0.17, 0.22525) | (0.332741, 0.328634, 0.346435) | 38.4 |
| Pearl | (0.25, 0.20725, 0.20725) | (1.0, 0.829, 0.829) | (0.296648, 0.296648, 0.296648) | 11.264 |
| Ruby | (0.1745, 0.01175, 0.01175) | (0.61424, 0.04136, 0.04136) | (0.727811, 0.626959, 0.626959) | 76.8 |
| Turquoise | (0.1, 0.18725, 0.1745) | (0.396, 0.74151, 0.69102) | (0.297254, 0.30829, 0.306678) | 12.8 |
| Brass | (0.329412, 0.223529, 0.027451) | (0.780392, 0.568627, 0.113725) | (0.992157, 0.941176, 0.807843) | 27.9 |
| Bronze | (0.2125, 0.1275, 0.054) | (0.714, 0.4284, 0.18144) | (0.393548, 0.271906, 0.166721) | 25.6 |
| Chrome | (0.25, 0.25, 0.25) | (0.4, 0.4, 0.4) | (0.774597, 0.774597, 0.774597) | 76.8 |
| Copper | (0.19125, 0.0735, 0.0225) | (0.7038, 0.27048, 0.0828) | (0.256777, 0.137622, 0.086014) | 12.8 |
| Gold | (0.24725, 0.1995, 0.0745) | (0.75164, 0.60648, 0.22648) | (0.628281, 0.555802, 0.366065) | 51.2 |
| Silver | (0.19225, 0.19225, 0.19225) | (0.50754, 0.50754, 0.50754) | (0.508273, 0.508273, 0.508273) | 51.2 |
| Black Plastic | (0.0, 0.0, 0.0) | (0.01, 0.01, 0.01) | (0.50, 0.50, 0.50) | 32.0 |
| Cyan Plastic | (0.0, 0.1, 0.06) | (0.0, 0.50980, 0.50980) | (0.50196, 0.50196, 0.50196) | 32.0 |
| Green Plastic | (0.0, 0.0, 0.0) | (0.1, 0.35, 0.1) | (0.45, 0.55, 0.45) | 32.0 |
| Red Plastic | (0.0, 0.0, 0.0) | (0.5, 0.0, 0.0) | (0.7, 0.6, 0.6) | 32.0 |
| White Plastic | (0.0, 0.0, 0.0) | (0.55, 0.55, 0.55) | (0.70, 0.70, 0.70) | 32.0 |
| Yellow Plastic | (0.0, 0.0, 0.0) | (0.5, 0.5, 0.0) | (0.60, 0.60, 0.50) | 32.0 |
| Black Rubber | (0.02, 0.02, 0.02) | (0.01, 0.01, 0.01) | (0.4, 0.4, 0.4) | 10.0 |
| Cyan Rubber | (0.0, 0.05, 0.05) | (0.4, 0.5, 0.5) | (0.04, 0.7, 0.7) | 10.0 |
| Green Rubber | (0.0, 0.05, 0.0) | (0.4, 0.5, 0.4) | (0.04, 0.7, 0.04) | 10.0 |
| Red Rubber | (0.05, 0.0, 0.0) | (0.5, 0.4, 0.4) | (0.7, 0.04, 0.04) | 10.0 |
| White Rubber | (0.05, 0.05, 0.05) | (0.5, 0.5, 0.5) | (0.7, 0.7, 0.7) | 10.0 |
| Yellow Rubber | (0.05, 0.05, 0.0) | (0.5, 0.5, 0.4) | (0.7, 0.7, 0.04) | 10.0 |

> **Source:** [devernay.free.fr/cours/opengl/materials.html](http://devernay.free.fr/cours/opengl/materials.html)

## Example: Cycling Through Materials

Here's how you might cycle through materials when a key is pressed. We define a few materials in an array and switch between them:

```cpp
struct MaterialValues {
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;
};

MaterialValues materials[] = {
    // Gold
    { glm::vec3(0.24725f, 0.1995f, 0.0745f),
      glm::vec3(0.75164f, 0.60648f, 0.22648f),
      glm::vec3(0.628281f, 0.555802f, 0.366065f),
      51.2f },
    // Silver
    { glm::vec3(0.19225f, 0.19225f, 0.19225f),
      glm::vec3(0.50754f, 0.50754f, 0.50754f),
      glm::vec3(0.508273f, 0.508273f, 0.508273f),
      51.2f },
    // Emerald
    { glm::vec3(0.0215f, 0.1745f, 0.0215f),
      glm::vec3(0.07568f, 0.61424f, 0.07568f),
      glm::vec3(0.633f, 0.727811f, 0.633f),
      76.8f },
    // Cyan Rubber
    { glm::vec3(0.0f, 0.05f, 0.05f),
      glm::vec3(0.4f, 0.5f, 0.5f),
      glm::vec3(0.04f, 0.7f, 0.7f),
      10.0f },
    // Red Plastic
    { glm::vec3(0.0f, 0.0f, 0.0f),
      glm::vec3(0.5f, 0.0f, 0.0f),
      glm::vec3(0.7f, 0.6f, 0.6f),
      32.0f }
};

int currentMaterial = 0;
const int numMaterials = 5;
```

In `processInput`, toggle material on key press (with debounce):

```cpp
bool mKeyPressed = false;

void processInput(GLFWwindow* window)
{
    // ... existing movement code ...

    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS && !mKeyPressed)
    {
        mKeyPressed = true;
        currentMaterial = (currentMaterial + 1) % numMaterials;
    }
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_RELEASE)
    {
        mKeyPressed = false;
    }
}
```

In the render loop, apply the current material:

```cpp
objectShader.setVec3("material.ambient",  materials[currentMaterial].ambient);
objectShader.setVec3("material.diffuse",  materials[currentMaterial].diffuse);
objectShader.setVec3("material.specular", materials[currentMaterial].specular);
objectShader.setFloat("material.shininess", materials[currentMaterial].shininess);
```

## Animating the Light Color

To really see the effect of materials, let's make the light color change over time. This will demonstrate how different materials respond to different light colors:

```cpp
glm::vec3 lightColor;
lightColor.x = static_cast<float>(sin(glfwGetTime() * 2.0));
lightColor.y = static_cast<float>(sin(glfwGetTime() * 0.7));
lightColor.z = static_cast<float>(sin(glfwGetTime() * 1.3));

glm::vec3 diffuseColor = lightColor   * glm::vec3(0.5f);
glm::vec3 ambientColor = diffuseColor * glm::vec3(0.2f);

objectShader.setVec3("light.ambient", ambientColor);
objectShader.setVec3("light.diffuse", diffuseColor);
objectShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
```

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
        "LearnOpenGL - Materials", NULL, NULL);
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

    Shader objectShader("shaders/materials.vert", "shaders/materials.frag");
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

    unsigned int lightCubeVAO;
    glGenVertexArrays(1, &lightCubeVAO);
    glBindVertexArray(lightCubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        objectShader.use();
        objectShader.setVec3("viewPos", camera.Position);

        // Animate light color
        glm::vec3 lightColor;
        lightColor.x = static_cast<float>(sin(glfwGetTime() * 2.0));
        lightColor.y = static_cast<float>(sin(glfwGetTime() * 0.7));
        lightColor.z = static_cast<float>(sin(glfwGetTime() * 1.3));
        glm::vec3 diffuseColor = lightColor   * glm::vec3(0.5f);
        glm::vec3 ambientColor = diffuseColor * glm::vec3(0.2f);

        objectShader.setVec3("light.position", lightPos);
        objectShader.setVec3("light.ambient",  ambientColor);
        objectShader.setVec3("light.diffuse",  diffuseColor);
        objectShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);

        // Material: gold
        objectShader.setVec3("material.ambient",   0.24725f, 0.1995f, 0.0745f);
        objectShader.setVec3("material.diffuse",   0.75164f, 0.60648f, 0.22648f);
        objectShader.setVec3("material.specular",  0.628281f, 0.555802f, 0.366065f);
        objectShader.setFloat("material.shininess", 51.2f);

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

## Key Takeaways

- **Materials** define how an object reacts to light (ambient, diffuse, specular reflectance, and shininess).
- **Light properties** define the intensity of each lighting component independently.
- Using structs in GLSL keeps related uniforms organized.
- Struct uniforms are set using dot notation: `"material.ambient"`, `"light.diffuse"`, etc.
- Real-world material tables let you simulate metals, plastics, rubber, and gemstones.
- Changing the light color dramatically affects how materials appear.

## Exercises

1. **Material gallery:** Render multiple cubes side by side, each with a different material from the table. This makes a great reference scene.
2. **Custom material:** Define your own material values for something like "rusty iron" or "frosted glass." Tweak until it looks right.
3. **Light properties:** Keep the material fixed (e.g., gold) and experiment with different light ambient/diffuse/specular values. What happens when `light.ambient` is `(1,1,1)`?
4. **Multiple materials:** Render a scene with three objects — a gold sphere, a green rubber cube, and a chrome teapot (or cubes for now). Each uses different material properties.

---

| [← Basic Lighting](02_basic_lighting.md) | [Next: Lighting Maps →](04_lighting_maps.md) |
|:---|---:|
