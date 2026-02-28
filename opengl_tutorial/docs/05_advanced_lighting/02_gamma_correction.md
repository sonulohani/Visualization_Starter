# Gamma Correction

In the previous chapter we upgraded our specular model from Phong to Blinn-Phong. The lighting looked better, but there's a hidden problem lurking in every scene we've rendered so far: **we've been calculating lighting in the wrong color space**. Our colors have been silently distorted by gamma, and in this chapter we'll learn what gamma is, why it matters, and how to fix it.

---

## What Is Gamma?

The story begins with CRT monitors. A CRT doesn't have a linear relationship between the voltage it receives and the brightness it produces. If you send a voltage that *should* produce 50% brightness, the CRT actually displays something closer to 18% brightness. The relationship follows a power curve:

```
displayed_brightness = input_voltage ^ 2.2
```

This exponent (~2.2) is called the **gamma** of the display. Although modern LCD and OLED monitors aren't CRTs, they all emulate this same power curve for backward compatibility.

```
  Display Response Curve (gamma = 2.2)

  brightness
  1.0 │                           ╱
      │                         ╱
      │                       ╱
  0.8 │                     ╱
      │                   ╱
  0.6 │                 ╱
      │              ╱
  0.4 │           ╱
      │        ╱              ── linear (what you'd expect)
  0.2 │     ╱                 ── actual display (x^2.2)
      │  ╱
  0.0 │╱
      └──────────────────────── input value
      0.0                    1.0

  A linear value of 0.5 displays as 0.5^2.2 ≈ 0.22
  — much darker than you'd expect!
```

### Human Perception

Interestingly, human vision is non-linear too — we're far more sensitive to differences in dark tones than bright ones. The gamma curve of monitors *accidentally* aligns with this: it allocates more of the numerical range to dark values where our eyes are most sensitive. This is actually desirable for storing images efficiently, but it wreaks havoc on lighting calculations.

---

## The sRGB Color Space

To deal with monitor gamma, the industry standardized the **sRGB** color space. When artists create textures or when cameras capture photos, the colors are stored in sRGB — they've been *gamma-encoded* (roughly: `srgb = linear ^ (1/2.2)`) so that when the monitor applies its gamma curve (`displayed = srgb ^ 2.2`), the two cancel out and the viewer sees the intended brightness.

```
  The sRGB Workflow (for images):

  Real world    →   Camera/Artist   →   Image file   →   Monitor        →   Your eyes
  (linear)          applies 1/2.2       (sRGB encoded)    applies 2.2        (correct!)
                    "gamma encode"                         "gamma decode"

  linear_color ^ (1/2.2) = srgb_value
  srgb_value ^ 2.2 = linear_color  ← back to the original!
```

This works perfectly for displaying photographs and artwork. The problem arises when we try to do **math** on these colors.

---

## The Problem: Double Gamma

Here's what happens in a typical OpenGL program without gamma correction:

```
  The Problem (no gamma correction):

  Texture (sRGB)  →  Lighting math  →  Framebuffer  →  Monitor (^2.2)  →  Eyes
  (already encoded)   (assumes linear)   (sRGB values)   (applies gamma)    (TOO DARK!)
                      ↑
                      This math is WRONG because
                      the inputs aren't linear!

  Step by step with a value that should be 50% gray:
  ┌────────────────────────────────────────────────────────────┐
  │ 1. Texture stores 0.73  (sRGB encoding of 0.5 linear)     │
  │ 2. Lighting uses 0.73 as if it were linear (it's not!)    │
  │ 3. Result goes to framebuffer: still in sRGB-ish space    │
  │ 4. Monitor applies ^2.2: 0.73^2.2 ≈ 0.50                 │
  │    Lucky! ...but only for this one value.                  │
  │                                                            │
  │ For lighting calculations (addition, multiplication):      │
  │ 0.73 * 0.73 = 0.53  (what the shader computes)            │
  │ 0.50 * 0.50 = 0.25  (what it SHOULD be in linear)         │
  │ The results are completely different!                       │
  └────────────────────────────────────────────────────────────┘
```

The effects of working in the wrong color space include:

- **Lighting looks too dark** in mid-tones
- **Additive blending** produces incorrect results (colors appear over-saturated then suddenly bright)
- **Attenuation** (light falloff with distance) looks wrong
- **Diffuse shading** gradients are non-linear — the transition from lit to shadow is distorted

---

## The Solution: Linear Workflow

The fix is called the **linear workflow** or **gamma-correct rendering**:

1. **Convert sRGB inputs to linear space** before any math (remove gamma)
2. **Do all lighting calculations in linear space** (where the math is physically correct)
3. **Convert back to sRGB at the end** (re-apply gamma for the monitor)

```
  Gamma-Correct Pipeline:

  Texture (sRGB) → Remove gamma → Lighting math → Apply gamma → Monitor (^2.2) → Eyes
                   (^2.2)         (LINEAR space)   (^1/2.2)      (^2.2)          (CORRECT!)
                                  ↑
                                  Now the math works
                                  on real intensities

  The final two gamma operations cancel out:
  linear ^ (1/2.2) ^ 2.2 = linear  ✓
```

### Step 1: Convert sRGB Textures to Linear

When you sample a color texture in the fragment shader, the values are in sRGB. To convert to linear:

```glsl
vec3 texColor = texture(diffuseMap, TexCoords).rgb;
vec3 linearColor = pow(texColor, vec3(2.2));
```

### Step 2: Do All Lighting in Linear Space

All your ambient, diffuse, specular, and attenuation calculations should use linear-space colors. This is already what our shaders do — the only change is ensuring the *inputs* are linear.

### Step 3: Apply Gamma Correction to Output

After all lighting is computed, convert the final color back to sRGB:

```glsl
vec3 result = ambient + diffuse + specular;

// Gamma correction
float gamma = 2.2;
FragColor = vec4(pow(result, vec3(1.0 / gamma)), 1.0);
```

---

## Two Approaches in OpenGL

### Approach 1: Manual Gamma Correction in the Shader

Apply `pow(color, vec3(1.0/2.2))` as the very last operation in the fragment shader:

```glsl
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D diffuseMap;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    // Sample texture and convert sRGB → linear
    vec3 color = pow(texture(diffuseMap, TexCoords).rgb, vec3(2.2));

    // Ambient
    vec3 ambient = 0.1 * color;

    // Diffuse
    vec3 lightDir = normalize(lightPos - FragPos);
    vec3 normal = normalize(Normal);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * color;

    // Specular (Blinn-Phong)
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = vec3(0.3) * spec;

    vec3 result = ambient + diffuse + specular;

    // Apply gamma correction (linear → sRGB)
    result = pow(result, vec3(1.0 / 2.2));

    FragColor = vec4(result, 1.0);
}
```

This approach gives you complete control but requires you to remember the conversion in every shader.

### Approach 2: Let OpenGL Handle It (`GL_FRAMEBUFFER_SRGB`)

OpenGL can automatically convert linear values to sRGB when writing to the framebuffer:

```cpp
glEnable(GL_FRAMEBUFFER_SRGB);
```

With this enabled, OpenGL applies the `^(1/2.2)` conversion on every framebuffer write. You still need to ensure your inputs are in linear space (either manually or using sRGB texture formats), but you no longer need the final `pow()` in the shader.

```
  Comparison of approaches:

  ┌─────────────────────────────────────────────────────────────┐
  │ Manual:                                                     │
  │   Shader: pow(texColor, 2.2) → lighting → pow(result, 1/2.2)│
  │   Pro: Full control, works everywhere                       │
  │   Con: Must remember in every shader                        │
  │                                                             │
  │ GL_FRAMEBUFFER_SRGB:                                        │
  │   Shader: texColor (handled by format) → lighting           │
  │   OpenGL: automatically applies ^(1/2.2) on write           │
  │   Pro: Can't forget, handles blending correctly too         │
  │   Con: Only converts the final write, not intermediate FBOs │
  └─────────────────────────────────────────────────────────────┘
```

> **Note:** When `GL_FRAMEBUFFER_SRGB` is enabled, OpenGL also performs correct gamma-aware blending. With manual shader correction, blending operations still happen in sRGB space (incorrectly). This is another reason to prefer the OpenGL-managed approach when possible.

---

## sRGB Textures

Manually calling `pow(texture(...), vec3(2.2))` on every texture sample is tedious and error-prone. OpenGL provides a better way: **sRGB texture formats**.

When you load a color texture, specify the internal format as `GL_SRGB` (or `GL_SRGB_ALPHA` for textures with transparency) instead of `GL_RGB`:

```cpp
// Before (no gamma correction — treats sRGB data as linear):
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
             GL_UNSIGNED_BYTE, data);

// After (tells OpenGL the data is sRGB — it converts to linear on sampling):
glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB, width, height, 0, GL_RGB,
             GL_UNSIGNED_BYTE, data);

// For textures with alpha:
glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA, width, height, 0, GL_RGBA,
             GL_UNSIGNED_BYTE, data);
```

With `GL_SRGB`, every time you call `texture()` in the shader, OpenGL **automatically converts from sRGB to linear** before returning the color. You don't need the manual `pow()` on sampling.

### Which Textures Should Be sRGB?

Not all textures contain color data! Only textures that represent *colors as seen by humans* should use the sRGB format:

| Texture Type | Internal Format | Why |
|---|---|---|
| Diffuse / Albedo maps | `GL_SRGB` or `GL_SRGB_ALPHA` | Colors created by artists in sRGB space |
| Specular maps (color) | `GL_SRGB` | If they represent visible colors |
| Normal maps | `GL_RGB` | Stores direction vectors, not colors — already linear data |
| Height maps | `GL_RED` / `GL_RGB` | Stores height values — linear data |
| Specular maps (grayscale intensity) | `GL_RED` / `GL_RGB` | Stores intensity values — often linear |
| Metallic / Roughness maps | `GL_RED` / `GL_RGB` | Linear data for PBR calculations |

> **Critical rule:** If a texture stores *data* rather than *visible color*, keep it as `GL_RGB`. Applying sRGB→linear conversion to a normal map, for example, would distort the normals and produce incorrect lighting.

---

## Updating the Texture Loader

Let's update our `loadTexture()` function to support gamma correction:

```cpp
unsigned int loadTexture(char const* path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum dataFormat = GL_RGB;
        GLenum internalFormat = GL_RGB;

        if (nrComponents == 1)
        {
            dataFormat = GL_RED;
            internalFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            dataFormat = GL_RGB;
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
        }
        else if (nrComponents == 4)
        {
            dataFormat = GL_RGBA;
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                     dataFormat, GL_UNSIGNED_BYTE, data);
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

Usage:

```cpp
unsigned int diffuseMap = loadTexture("textures/wood.png", true);   // color → sRGB
unsigned int normalMap  = loadTexture("textures/normal.png", false); // data → linear
```

---

## Attenuation in Gamma Space

Before we added gamma correction, our light attenuation formula appeared to work acceptably:

```glsl
float distance    = length(lightPos - FragPos);
float attenuation = 1.0 / (distance * distance);
```

But it was actually wrong — the double-gamma effect (sRGB input, sRGB display) accidentally made the overly-fast quadratic falloff look more gradual. Once we add proper gamma correction, the physically-correct quadratic attenuation looks right:

```
  Attenuation: Before vs After Gamma Correction

  brightness
  1.0 │ ╲ ╲
      │  ╲  ╲
      │   ╲   ╲
  0.8 │    ╲    ╲
      │     ╲     ╲
  0.6 │      ╲      ╲
      │       ╲       ╲
  0.4 │        ╲        ╲        ── Without gamma correction
      │         ╲         ╲        (double gamma makes falloff
  0.2 │          ╲          ╲       look more gradual)
      │           ╲           ╲
  0.0 │────────────╲───────────── ── With gamma correction
      └──────────────────────────   (correct quadratic falloff)
      0           distance

  With gamma correction, 1/(d²) falloff
  is physically accurate — exactly what
  real light does in a vacuum.
```

If you were using hand-tuned attenuation constants (`constant`, `linear`, `quadratic`) that looked good *without* gamma correction, you may need to re-tune them after enabling it.

---

## Visual Comparison

The difference is most visible in mid-tones and in the transition from light to shadow:

```
  Without Gamma Correction:           With Gamma Correction:

  ┌────────────────────────┐          ┌────────────────────────┐
  │                        │          │                        │
  │    ████████            │          │    ██████████          │
  │  ██████████░░          │          │  ████████████░░░       │
  │  ██████████░░          │          │  ████████████░░░       │
  │  ████████░░░░          │          │  ██████████░░░░░       │
  │    ░░░░░░░░            │          │    ░░░░░░░░░░░         │
  │                        │          │                        │
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│          │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  └────────────────────────┘          └────────────────────────┘
  - Mid-tones too dark                - Smooth, natural gradient
  - Harsh light-to-dark transition    - Light falloff looks correct
  - Colors slightly washed out        - Colors vibrant and accurate
```

---

## Complete C++ Source Code

This program renders a textured floor with gamma correction. Press **G** to toggle gamma correction on and off to see the difference. Press **B** to toggle Blinn-Phong as in the previous chapter.

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
unsigned int loadTexture(const char* path, bool gammaCorrection);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

bool gammaEnabled = false;
bool gammaKeyPressed = false;

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
                                          "LearnOpenGL - Gamma Correction",
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

    Shader shader("shaders/gamma_correction.vert",
                   "shaders/gamma_correction.frag");

    float planeVertices[] = {
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

    unsigned int floorTexture       = loadTexture("textures/wood.png", false);
    unsigned int floorTextureGamma  = loadTexture("textures/wood.png", true);

    shader.use();
    shader.setInt("floorTexture", 0);

    glm::vec3 lightPositions[] = {
        glm::vec3(-3.0f, 0.0f, 0.0f),
        glm::vec3(-1.0f, 0.0f, 0.0f),
        glm::vec3( 1.0f, 0.0f, 0.0f),
        glm::vec3( 3.0f, 0.0f, 0.0f)
    };
    glm::vec3 lightColors[] = {
        glm::vec3(0.25f),
        glm::vec3(0.50f),
        glm::vec3(0.75f),
        glm::vec3(1.00f)
    };

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
        shader.setBool("gamma", gammaEnabled);

        for (unsigned int i = 0; i < 4; i++)
        {
            shader.setVec3("lightPositions[" + std::to_string(i) + "]",
                           lightPositions[i]);
            shader.setVec3("lightColors[" + std::to_string(i) + "]",
                           lightColors[i]);
        }

        glBindVertexArray(planeVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,
                      gammaEnabled ? floorTextureGamma : floorTexture);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        std::cout << (gammaEnabled ? "Gamma correction: ON" :
                      "Gamma correction: OFF") << "\r" << std::flush;

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

    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS && !gammaKeyPressed)
    {
        gammaEnabled = !gammaEnabled;
        gammaKeyPressed = true;
    }
    if (glfwGetKey(window, GLFW_KEY_G) == GLFW_RELEASE)
    {
        gammaKeyPressed = false;
    }
}

unsigned int loadTexture(char const* path, bool gammaCorrection)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum dataFormat = GL_RGB;
        GLenum internalFormat = GL_RGB;

        if (nrComponents == 1)
        {
            dataFormat = GL_RED;
            internalFormat = GL_RED;
        }
        else if (nrComponents == 3)
        {
            dataFormat = GL_RGB;
            internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
        }
        else if (nrComponents == 4)
        {
            dataFormat = GL_RGBA;
            internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                     dataFormat, GL_UNSIGNED_BYTE, data);
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

### Vertex Shader (`gamma_correction.vert`)

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
    FragPos = aPos;
    Normal = aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(aPos, 1.0);
}
```

### Fragment Shader (`gamma_correction.frag`)

```glsl
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D floorTexture;
uniform vec3 lightPositions[4];
uniform vec3 lightColors[4];
uniform vec3 viewPos;
uniform bool gamma;

vec3 BlinnPhong(vec3 normal, vec3 fragPos, vec3 lightPos, vec3 lightColor)
{
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;

    float distance = length(lightPos - fragPos);
    float attenuation = 1.0 / (gamma ? distance * distance : distance);

    diffuse *= attenuation;
    specular *= attenuation;

    return diffuse + specular;
}

void main()
{
    vec3 color = texture(floorTexture, TexCoords).rgb;
    vec3 lighting = vec3(0.0);

    for (int i = 0; i < 4; i++)
        lighting += BlinnPhong(normalize(Normal), FragPos,
                               lightPositions[i], lightColors[i]);

    color *= lighting;

    if (gamma)
        color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
```

The shader uses `1/distance` attenuation when gamma is off (to compensate for the double-gamma making quadratic look too aggressive) and physically-correct `1/distance²` when gamma correction is enabled.

---

## Key Takeaways

- **Gamma** is the power-curve relationship between input values and displayed brightness (~2.2 for standard monitors).
- **sRGB** is the standard gamma-encoded color space used by image files, textures, and the web.
- Doing lighting math on sRGB values produces **incorrect results** — colors appear too dark, and gradients are distorted.
- The **linear workflow** converts inputs to linear space, performs all math there, and converts back to sRGB at the end.
- Use `GL_SRGB` / `GL_SRGB_ALPHA` internal formats for **color textures** so OpenGL auto-converts on sampling. Keep normal maps and data textures as `GL_RGB`.
- Use `glEnable(GL_FRAMEBUFFER_SRGB)` for automatic output conversion, or apply `pow(color, 1/2.2)` manually in the shader.
- After enabling gamma correction, **re-tune your attenuation constants** — the physically correct quadratic falloff now works as expected.

## Exercises

1. Run the program and toggle gamma correction with **G**. Observe the brightness difference, especially in mid-tones near the edges of the light pools.
2. Instead of loading two copies of the texture (sRGB and non-sRGB), use `glEnable(GL_FRAMEBUFFER_SRGB)` and only one `GL_SRGB` texture. Compare the results.
3. Render a gradient from black to white across a quad. With gamma correction off, the midpoint (0.5) should appear darker than expected. With gamma correction on, it should appear at perceived 50% brightness.
4. If you have a scene with normal maps from a previous chapter, load the normal map as `GL_SRGB` instead of `GL_RGB` and observe how the lighting breaks. This demonstrates why non-color data must stay in linear format.
5. Implement a configurable gamma value (pass it as a uniform controlled by arrow keys). While 2.2 is standard, some monitors deviate. Try 1.8 and 2.6 to see the effect.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Advanced Lighting](01_advanced_lighting.md) | [05. Advanced Lighting](.) | [Next: Shadow Mapping →](03_shadow_mapping.md) |
