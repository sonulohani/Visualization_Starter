# Light Casters

So far we've been using a single point-like light source. But the real world has many different kinds of lights — the sun illuminates everything from far away with parallel rays, a light bulb radiates in all directions and fades with distance, and a flashlight casts a focused cone of light. In this chapter we'll implement all three types: **directional lights**, **point lights**, and **spotlights**.

---

## 1. Directional Light

A directional light simulates a light source that is infinitely far away — like the sun. Because the source is so distant, all light rays are effectively **parallel**. This means the light's direction is the same for every object in the scene, regardless of position.

```
  Directional Light (like the Sun)
  ─────────────────────────────────
         ☀  (infinitely far away)
        ╱│╲
       ╱ │ ╲
      ↓  ↓  ↓  ↓  ↓  ↓  ↓
      │  │  │  │  │  │  │   All rays are parallel
      │  │  │  │  │  │  │
  ────┼──┼──┼──┼──┼──┼──┼────
      ■     ■        ■
     obj1  obj2     obj3

  Every object uses the SAME light direction.
  No position needed — only direction matters.
```

### Light Struct for Directional Light

We replace `position` with `direction`:

```glsl
struct Light {
    // vec3 position;  // no longer needed
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
```

### Updated Calculation

Instead of computing the light direction from position, we use the light's direction directly:

```glsl
// For a directional light, the direction is constant
vec3 lightDir = normalize(-light.direction);
```

We negate `light.direction` because we typically define the direction as "where the light is shining toward" (e.g., downward), but in our calculations we need the direction *from* the fragment *toward* the light.

### Complete Directional Light Fragment Shader

```glsl
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct Light {
    vec3 direction;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform vec3 viewPos;
uniform Material material;
uniform Light light;

void main()
{
    vec3 diffuseMap  = vec3(texture(material.diffuse, TexCoords));
    vec3 specularMap = vec3(texture(material.specular, TexCoords));

    // Ambient
    vec3 ambient = light.ambient * diffuseMap;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(-light.direction);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * diffuseMap;

    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * specularMap;

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
```

### Setting the Direction from C++

```cpp
objectShader.setVec3("light.direction", -0.2f, -1.0f, -0.3f);
objectShader.setVec3("light.ambient",    0.05f, 0.05f, 0.05f);
objectShader.setVec3("light.diffuse",    0.4f,  0.4f,  0.4f);
objectShader.setVec3("light.specular",   0.5f,  0.5f,  0.5f);
```

### Scene: Multiple Cubes with a Directional Light

To see the effect better, let's render multiple cubes scattered around the scene:

```cpp
glm::vec3 cubePositions[] = {
    glm::vec3( 0.0f,  0.0f,  0.0f),
    glm::vec3( 2.0f,  5.0f, -15.0f),
    glm::vec3(-1.5f, -2.2f, -2.5f),
    glm::vec3(-3.8f, -2.0f, -12.3f),
    glm::vec3( 2.4f, -0.4f, -3.5f),
    glm::vec3(-1.7f,  3.0f, -7.5f),
    glm::vec3( 1.3f, -2.0f, -2.5f),
    glm::vec3( 1.5f,  2.0f, -2.5f),
    glm::vec3( 1.5f,  0.2f, -1.5f),
    glm::vec3(-1.3f,  1.0f, -1.5f)
};

for (unsigned int i = 0; i < 10; i++)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, cubePositions[i]);
    float angle = 20.0f * i;
    model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
    objectShader.setMat4("model", model);

    glDrawArrays(GL_TRIANGLES, 0, 36);
}
```

All cubes are lit the same way from the same direction — just like sunlight hitting objects in a field.

---

## 2. Point Light

A point light has a specific position in the world and radiates light in **all directions**. Unlike a directional light, the light intensity diminishes (or **attenuates**) with distance — objects farther from the light receive less illumination.

```
  Point Light (like a Light Bulb)
  ────────────────────────────────
              ╱ | ╲
            ╱   |   ╲
          ╱     |     ╲
        ←── ────☀──── ──→   Light radiates in all directions
          ╲     |     ╱
            ╲   |   ╱
              ╲ | ╱

  Close objects: bright  ████████
  Far objects:   dim     ░░░░
```

### Attenuation

In the real world, light intensity decreases roughly with the inverse square of distance. In practice, we use a slightly modified formula that gives artists more control:

```
                        1.0
  attenuation = ─────────────────────────────
                Kc + Kl × d + Kq × d²

  Where:
    d  = distance from light to fragment
    Kc = constant  term (usually 1.0)
    Kl = linear    term (smooth falloff)
    Kq = quadratic term (slower at close range, fast at distance)
```

```
  Attenuation Curve
  ─────────────────
  Intensity
  1.0 │█
      │██
      │ ██
      │  ███
      │    ████
      │       ██████
      │            ██████████████████
  0.0 └──────────────────────────────→ Distance
```

### Attenuation Values Table

The following table provides good starting values for Kl (linear) and Kq (quadratic) for various effective ranges. Keep Kc = 1.0.

| Distance | Constant | Linear | Quadratic |
|----------|----------|--------|-----------|
| 7        | 1.0      | 0.7    | 1.8       |
| 13       | 1.0      | 0.35   | 0.44      |
| 20       | 1.0      | 0.22   | 0.20      |
| 32       | 1.0      | 0.14   | 0.07      |
| 50       | 1.0      | 0.09   | 0.032     |
| 65       | 1.0      | 0.07   | 0.017     |
| 100      | 1.0      | 0.045  | 0.0075    |
| 160      | 1.0      | 0.027  | 0.0028    |
| 200      | 1.0      | 0.022  | 0.0019    |
| 325      | 1.0      | 0.014  | 0.0007    |
| 600      | 1.0      | 0.007  | 0.0002    |
| 3250     | 1.0      | 0.0014 | 0.000007  |

> **Tip:** For a small room, use the values for distance 50. For outdoor scenes, try 200–600.

### Point Light Struct

```glsl
struct Light {
    vec3 position;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
```

### Point Light Fragment Shader

```glsl
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct Light {
    vec3 position;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform vec3 viewPos;
uniform Material material;
uniform Light light;

void main()
{
    vec3 diffuseMap  = vec3(texture(material.diffuse, TexCoords));
    vec3 specularMap = vec3(texture(material.specular, TexCoords));

    // Ambient
    vec3 ambient = light.ambient * diffuseMap;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * diffuseMap;

    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * specularMap;

    // Attenuation
    float distance    = length(light.position - FragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance
                              + light.quadratic * (distance * distance));

    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
```

### Setting Point Light Uniforms from C++

```cpp
objectShader.setVec3("light.position", lightPos);
objectShader.setFloat("light.constant",  1.0f);
objectShader.setFloat("light.linear",    0.09f);
objectShader.setFloat("light.quadratic", 0.032f);
objectShader.setVec3("light.ambient",  0.2f, 0.2f, 0.2f);
objectShader.setVec3("light.diffuse",  0.5f, 0.5f, 0.5f);
objectShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
```

Now cubes close to the light appear bright, and cubes far away gradually fade into darkness.

---

## 3. Spotlight

A spotlight is like a flashlight — it has a **position**, a **direction** it points at, and a **cutoff angle** that defines the cone of illumination. Fragments inside the cone are lit; fragments outside are dark.

```
  Spotlight (like a Flashlight)
  ─────────────────────────────
              position
                ●
               ╱│╲
              ╱ │ ╲
             ╱  │  ╲  cutoff angle
            ╱   │   ╲
           ╱    ↓    ╲
          ╱  direction ╲
         ╱              ╲
        ╱   lit area     ╲
       ╱                  ╲
      ────────────────────── surface
         ████████████
         bright center

      Outside the cone → dark
      Inside the cone  → illuminated
```

### How It Works

For each fragment, we calculate the angle between:
1. The vector from the light to the fragment (`lightDir`)
2. The spotlight's aim direction (`light.direction`)

If this angle is within the cutoff, the fragment is lit. We use the dot product of the two normalized vectors (which gives us `cos(θ)`) and compare it to `cos(cutoff)`:

```glsl
float theta = dot(lightDir, normalize(-light.direction));

if (theta > light.cutOff)  // using cosines, so > means smaller angle
{
    // inside the spotlight cone — do lighting
}
else
{
    // outside — ambient only
}
```

> **Important:** We compare cosines, not angles. Since `cos` decreases as the angle increases, a fragment is inside the cone when `theta > light.cutOff` (where `cutOff` is stored as `cos(angle)`).

### Hard Edge Problem

The simple if/else produces a **hard edge** at the cutoff boundary — the transition from bright to dark is abrupt and looks unnatural:

```
  Hard-Edge Spotlight           Soft-Edge Spotlight
  ─────────────────            ───────────────────
  ░░░░██████████░░░░           ░░░▒▓██████████▓▒░░░
  ░░░░██████████░░░░           ░░░▒▓██████████▓▒░░░
  ░░░░██████████░░░░           ░░░▒▓██████████▓▒░░░
       sharp edge               gradual falloff
```

### Soft Edges with Inner and Outer Cutoff

To create a smooth transition, we define **two** cutoff angles:

- **Inner cutoff** (`cutOff`): Full brightness inside this cone.
- **Outer cutoff** (`outerCutOff`): Zero brightness outside this cone. Between inner and outer, intensity fades smoothly.

```
  Soft-Edge Spotlight Cross Section
  ──────────────────────────────────
         ●  light position
        ╱│╲
       ╱ │ ╲
      ╱  │  ╲╲
     ╱   │   ╲ ╲  ← outer cutoff
    ╱    │    ╲  ╲
   ╱ inner│    ╲  ╲
  ╱  cutoff│    ╲   ╲
  ─────────────────────
  │████████│▓▓▓▓│░░░░│
   full     fade  dark
   bright
```

The intensity is calculated as:

```glsl
float theta     = dot(lightDir, normalize(-light.direction));
float epsilon   = light.cutOff - light.outerCutOff;
float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
```

This linearly interpolates between full brightness at the inner cutoff and zero at the outer cutoff.

### Spotlight Struct

```glsl
struct Light {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
```

### Flashlight: Using the Camera as a Spotlight

A fun application: make the spotlight follow the camera (like holding a flashlight):

```cpp
objectShader.setVec3("light.position",  camera.Position);
objectShader.setVec3("light.direction", camera.Front);
objectShader.setFloat("light.cutOff",      glm::cos(glm::radians(12.5f)));
objectShader.setFloat("light.outerCutOff", glm::cos(glm::radians(17.5f)));
```

We pass the **cosine** of the angles because the shader compares dot products (which are cosines) directly. This avoids an expensive `acos` call in the shader.

### Complete Spotlight Fragment Shader

```glsl
#version 330 core
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct Light {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;

    float constant;
    float linear;
    float quadratic;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform vec3 viewPos;
uniform Material material;
uniform Light light;

void main()
{
    vec3 diffuseMap  = vec3(texture(material.diffuse, TexCoords));
    vec3 specularMap = vec3(texture(material.specular, TexCoords));

    // Ambient (always applied)
    vec3 ambient = light.ambient * diffuseMap;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(light.position - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = light.diffuse * diff * diffuseMap;

    // Specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = light.specular * spec * specularMap;

    // Spotlight soft edge
    float theta     = dot(lightDir, normalize(-light.direction));
    float epsilon   = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    diffuse  *= intensity;
    specular *= intensity;

    // Attenuation
    float distance    = length(light.position - FragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance
                              + light.quadratic * (distance * distance));

    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
```

### Complete C++ Source Code (Spotlight / Flashlight)

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/shader.h>
#include <learnopengl/camera.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

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

glm::vec3 cubePositions[] = {
    glm::vec3( 0.0f,  0.0f,  0.0f),
    glm::vec3( 2.0f,  5.0f, -15.0f),
    glm::vec3(-1.5f, -2.2f, -2.5f),
    glm::vec3(-3.8f, -2.0f, -12.3f),
    glm::vec3( 2.4f, -0.4f, -3.5f),
    glm::vec3(-1.7f,  3.0f, -7.5f),
    glm::vec3( 1.3f, -2.0f, -2.5f),
    glm::vec3( 1.5f,  2.0f, -2.5f),
    glm::vec3( 1.5f,  0.2f, -1.5f),
    glm::vec3(-1.3f,  1.0f, -1.5f)
};

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
        "LearnOpenGL - Light Casters (Spotlight)", NULL, NULL);
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

    Shader objectShader("shaders/spotlight.vert", "shaders/spotlight.frag");

    float vertices[] = {
        // positions          // normals           // texture coords
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
    };

    unsigned int VBO, cubeVAO;
    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(cubeVAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int diffuseMap  = loadTexture("textures/container2.png");
    unsigned int specularMap = loadTexture("textures/container2_specular.png");

    objectShader.use();
    objectShader.setInt("material.diffuse", 0);
    objectShader.setInt("material.specular", 1);

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
        objectShader.setFloat("material.shininess", 32.0f);

        // Flashlight: spotlight follows camera
        objectShader.setVec3("light.position",  camera.Position);
        objectShader.setVec3("light.direction", camera.Front);
        objectShader.setFloat("light.cutOff",      glm::cos(glm::radians(12.5f)));
        objectShader.setFloat("light.outerCutOff", glm::cos(glm::radians(17.5f)));

        objectShader.setFloat("light.constant",  1.0f);
        objectShader.setFloat("light.linear",    0.09f);
        objectShader.setFloat("light.quadratic", 0.032f);

        objectShader.setVec3("light.ambient",  0.1f, 0.1f, 0.1f);
        objectShader.setVec3("light.diffuse",  0.8f, 0.8f, 0.8f);
        objectShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);

        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT,
            0.1f, 100.0f
        );
        glm::mat4 view = camera.GetViewMatrix();
        objectShader.setMat4("projection", projection);
        objectShader.setMat4("view", view);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, diffuseMap);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, specularMap);

        glBindVertexArray(cubeVAO);
        for (unsigned int i = 0; i < 10; i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);
            float angle = 20.0f * i;
            model = glm::rotate(model, glm::radians(angle),
                                glm::vec3(1.0f, 0.3f, 0.5f));
            objectShader.setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &cubeVAO);
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

unsigned int loadTexture(const char* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if      (nrComponents == 1) format = GL_RED;
        else if (nrComponents == 3) format = GL_RGB;
        else if (nrComponents == 4) format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format,
                     GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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

---

## Key Takeaways

- **Directional lights** have no position — only a direction. All rays are parallel. Great for sunlight.
- **Point lights** have a position and radiate in all directions. They use **attenuation** to fade with distance using the formula: `1 / (Kc + Kl*d + Kq*d²)`.
- **Spotlights** have a position, direction, and cutoff angle. They illuminate a cone-shaped area.
- Use **inner and outer cutoff angles** for smooth spotlight edges: `intensity = clamp((θ - outer) / (inner - outer), 0, 1)`.
- Cutoff angles are stored as **cosines** to avoid expensive `acos` in the shader.
- A flashlight effect is achieved by setting the spotlight position/direction to the camera's position/front.

## Exercises

1. **Directional light:** Change the sun direction over time using `sin`/`cos` to simulate a day/night cycle. Adjust light colors (warm orange at "sunset", blue-white at "noon").
2. **Point light movement:** Orbit the point light around the scene. Experiment with different attenuation values and observe how the illumination radius changes.
3. **Flashlight toggle:** Add a key (e.g., `F`) to toggle the flashlight on and off. When off, set diffuse and specular to `(0,0,0)`.
4. **Spotlight color:** Change the flashlight to emit colored light (try red for a dark horror scene, or warm yellow for a cozy camping feel).
5. **Cone size:** Experiment with different cutoff angles. What happens with a very narrow cone (5°)? A very wide one (45°)?

---

| [← Lighting Maps](04_lighting_maps.md) | [Next: Multiple Lights →](06_multiple_lights.md) |
|:---|---:|
