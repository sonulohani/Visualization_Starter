# Colors

In the previous section we briefly touched on how colors work in OpenGL, but so far we've only scratched the surface. In this chapter we'll dive deeper into what colors are in computer graphics and set up the foundation for the lighting chapters that follow.

## Colors in the Real World

In the real world, colors can take any conceivable value — there is an infinite spectrum of wavelengths. What we see as "color" is actually the wavelength of light that an object *reflects* (rather than absorbs). White sunlight is a combination of all visible wavelengths. When this light hits a blue toy, it absorbs all wavelengths *except* those in the blue range, which are reflected back to our eyes. That reflected light is what we perceive as the object's color.

## Colors in Computer Graphics: The RGB Model

In computer graphics we approximate colors using the **Red, Green, Blue (RGB)** model. Every color is described as a combination of these three primary components. By mixing different intensities of red, green, and blue we can represent virtually any color:

```
Red   + Green       = Yellow
Red         + Blue  = Magenta
      Green + Blue  = Cyan
Red   + Green + Blue = White
(nothing)            = Black
```

In OpenGL, color components are represented as floating-point values in the range **0.0** to **1.0**, where `0.0` means "no intensity" and `1.0` means "full intensity." We store colors in GLSL using `vec3` or `vec4` vectors:

```glsl
vec3 coral = vec3(1.0, 0.5, 0.31);   // a coral/orange color
vec3 white = vec3(1.0, 1.0, 1.0);    // pure white
vec3 green = vec3(0.0, 1.0, 0.0);    // pure green
vec3 black = vec3(0.0, 0.0, 0.0);    // pure black
```

> **Note:** You may also see colors as integers `0–255` in other contexts (HTML, image editors). OpenGL normalizes these to the `0.0–1.0` range. To convert: divide each component by 255. For example, `(255, 128, 79)` becomes approximately `(1.0, 0.5, 0.31)`.

## How Light Color Interacts with Object Color

When light strikes an object, the resulting color we see is determined by **component-wise multiplication** of the light's color and the object's color:

```
result = lightColor * objectColor
```

Each RGB component of the light is multiplied independently with the corresponding component of the object color. This simulates how objects absorb certain wavelengths and reflect others.

### Example 1: White Light on a Coral Object

White light contains full intensity of all components:

```
lightColor  = (1.0, 1.0, 1.0)   // white
objectColor = (1.0, 0.5, 0.31)  // coral

result = (1.0 * 1.0,  1.0 * 0.5,  1.0 * 0.31)
       = (1.0, 0.5, 0.31)  // coral — all components reflected fully
```

The coral object appears its natural color because the white light provides full intensity for every channel.

### Example 2: Green Light on a Coral Object

Now let's see what happens when we shine a green light (only the green component is non-zero):

```
lightColor  = (0.0, 1.0, 0.0)   // green
objectColor = (1.0, 0.5, 0.31)  // coral

result = (0.0 * 1.0,  1.0 * 0.5,  0.0 * 0.31)
       = (0.0, 0.5, 0.0)   // dark green
```

The object absorbs the red and blue components (there is no red or blue light to reflect). Only the green component of the object (0.5) is visible, giving us a dark green color.

### Example 3: Olive Light on a Coral Object

One more example with a dark olive light:

```
lightColor  = (0.33, 0.42, 0.18)
objectColor = (1.0,  0.5,  0.31)

result = (0.33 * 1.0,  0.42 * 0.5,  0.18 * 0.31)
       = (0.33, 0.21, 0.06)   // brownish dark color
```

As you can see, the light color has a dramatic influence on what color an object appears to have. Creative use of light colors can produce very different visual effects.

## Setting Up a Lighting Scene

To start working with lighting we need a simple scene:

1. **An object** to illuminate — we'll use the textured cube from the previous section, but render it with a solid color for now.
2. **A light source** — represented by a smaller cube at the light's position.

Here's a conceptual diagram of our scene:

```
    Scene Layout (top-down view)
    ─────────────────────────────
              ┌───┐
              │ L │  ← Light source (small white cube)
              └───┘
                  \
                   \  light rays
                    \
                ┌───────┐
                │       │
                │   O   │  ← Object (coral-colored cube)
                │       │
                └───────┘
                    |
                  Camera
```

### Two Shader Programs

A key detail: we'll use **two different shader programs**:

1. **Object shader** — applies lighting calculations, takes `objectColor` and `lightColor` as uniforms.
2. **Light shader** — simply outputs white (`vec4(1.0)`). We don't want lighting to affect the lamp itself.

Both cubes share the same vertex data (a cube), but they use different shaders and different model transforms (the lamp is smaller and positioned differently).

## The Shaders

### Object Vertex Shader (`object.vert`)

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

### Object Fragment Shader (`object.frag`)

```glsl
#version 330 core
out vec4 FragColor;

uniform vec3 objectColor;
uniform vec3 lightColor;

void main()
{
    FragColor = vec4(lightColor * objectColor, 1.0);
}
```

The fragment shader multiplies the light color by the object color — exactly the color interaction we discussed above.

### Light Source Vertex Shader (`light_cube.vert`)

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

### Light Source Fragment Shader (`light_cube.frag`)

```glsl
#version 330 core
out vec4 FragColor;

void main()
{
    FragColor = vec4(1.0); // always white, regardless of lighting
}
```

## Complete C++ Source Code

Below is the full application code. It creates a window with a coral-colored cube illuminated by a white light cube. We use the `Shader` and `Camera` classes from the Getting Started section.

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
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL - Colors", NULL, NULL);
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

    // Build and compile shaders
    Shader objectShader("shaders/object.vert", "shaders/object.frag");
    Shader lightCubeShader("shaders/light_cube.vert", "shaders/light_cube.frag");

    // Cube vertex data (positions only — no normals or tex coords needed yet)
    float vertices[] = {
        // positions
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,

        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,

        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f
    };

    // Object cube VAO/VBO
    unsigned int VBO, cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(cubeVAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Light cube VAO (same VBO, different VAO configuration)
    unsigned int lightCubeVAO;
    glGenVertexArrays(1, &lightCubeVAO);
    glBindVertexArray(lightCubeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
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

        // Draw the object cube
        objectShader.use();
        objectShader.setVec3("objectColor", 1.0f, 0.5f, 0.31f);
        objectShader.setVec3("lightColor",  1.0f, 1.0f, 1.0f);

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

        // Draw the light cube
        lightCubeShader.use();
        lightCubeShader.setMat4("projection", projection);
        lightCubeShader.setMat4("view", view);

        model = glm::mat4(1.0f);
        model = glm::translate(model, lightPos);
        model = glm::scale(model, glm::vec3(0.2f));  // smaller cube
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

## What You Should See

When you run this program, you should see a coral-colored cube at the center of the screen, with a small white cube off to the side representing the light source. The scene has a dark gray background (`0.1, 0.1, 0.1`).

Right now the lighting is very simple — just `lightColor * objectColor` — so the entire cube appears as a flat, uniform coral color with no shading, no depth perception from light, and no highlights. It looks rather flat and lifeless. We'll fix that in the next chapter by implementing a proper lighting model.

## Key Takeaways

- Colors in OpenGL are represented as `vec3` or `vec4` with values from **0.0 to 1.0**.
- The perceived color of an object is determined by **component-wise multiplication** of the light color and the object's color: `result = lightColor * objectColor`.
- Different light colors cause the same object to appear dramatically different.
- A lighting scene needs at least an **object** to illuminate and a **light source**.
- We use **separate shader programs** for the object (lighting calculations) and the light source (always white).
- Both the object and the lamp cube can share the same VBO, but use separate VAOs and shaders.

## Exercises

1. **Experiment with light colors:** Change `lightColor` to different values — try pure red `(1, 0, 0)`, pure blue `(0, 0, 1)`, and a warm yellow `(1, 0.9, 0.7)`. Observe how the coral cube changes appearance.
2. **Animate the light color:** Use `sin(glfwGetTime())` to vary the light color over time. Can you create a smoothly pulsing color effect?
3. **Try different object colors:** Change `objectColor` to various values and see how they interact with white and colored lights.

---

| [← Getting Started](../01_getting_started/) | [Next: Basic Lighting →](02_basic_lighting.md) |
|:---|---:|
