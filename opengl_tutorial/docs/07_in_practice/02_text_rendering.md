# Text Rendering

OpenGL knows how to draw triangles, points, and lines — but it has absolutely no built-in concept of text. There's no `glDrawText("Hello")`. If you want to show a score, a menu, debug information, or any readable string on screen, you need to build the entire text rendering pipeline yourself.

This turns out to be a surprisingly deep problem. Fonts are complex: they have varying widths, heights, baselines, kerning, ligatures, and hinting. In this chapter we'll implement a clean, practical text renderer using the **FreeType** library, which handles the hard parts of font rasterization so we can focus on the OpenGL side.

---

## Approaches to Text Rendering

There are several strategies for rendering text in OpenGL, each with trade-offs:

```
┌─────────────────────────────────────────────────────────────────────┐
│                  Text Rendering Approaches                          │
│                                                                     │
│  1. Bitmap Fonts (Texture Atlas)                                    │
│     ├── Pre-render all characters into a single texture             │
│     ├── Fast and simple to render                                   │
│     ├── Fixed size — scaling looks blurry or pixelated              │
│     └── Limited to the character set in the atlas                   │
│                                                                     │
│  2. FreeType + Per-Glyph Textures  ◄── our approach                │
│     ├── Load any TTF/OTF font at runtime                            │
│     ├── Render at any size with high quality                        │
│     ├── Slightly more complex setup                                 │
│     └── Each glyph is a separate texture (or packed into an atlas)  │
│                                                                     │
│  3. Signed Distance Fields (SDF)                                    │
│     ├── Store distance-to-edge instead of pixel coverage            │
│     ├── Scales beautifully at any size                              │
│     ├── Sharp edges even when zoomed in                             │
│     └── More complex generation and shader                          │
└─────────────────────────────────────────────────────────────────────┘
```

We'll use approach **2** — FreeType gives us high-quality glyph bitmaps from any TrueType or OpenType font file, and we convert those bitmaps into OpenGL textures. This is the standard approach for most OpenGL applications.

> **Note:** For production game engines, SDF-based text is increasingly popular because a single SDF texture can render sharp text at any scale. We focus on FreeType here because it's more straightforward and educational. Once you understand the basics, upgrading to SDF is a natural next step.

---

## The FreeType Library

**FreeType** is an open-source font engine that reads font files (`.ttf`, `.otf`, etc.) and rasterizes individual glyphs (characters) into grayscale bitmaps. It handles all the complexity of font formats, hinting, and anti-aliasing.

### Installation

**Linux (Debian/Ubuntu):**

```bash
sudo apt install libfreetype-dev
```

**macOS (Homebrew):**

```bash
brew install freetype
```

**Windows:**
Download from [freetype.org](https://freetype.org) or use vcpkg:

```bash
vcpkg install freetype
```

### CMake Integration

```cmake
find_package(Freetype REQUIRED)
target_include_directories(${PROJECT_NAME} PRIVATE ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${FREETYPE_LIBRARIES})
```

### Headers

```cpp
#include <ft2build.h>
#include FT_FREETYPE_H
```

The unusual `#include FT_FREETYPE_H` syntax is a FreeType convention — `FT_FREETYPE_H` is a macro that expands to the actual header path.

---

## Loading a Font

Initialize FreeType and load a font face:

```cpp
FT_Library ft;
if (FT_Init_FreeType(&ft))
{
    std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
    return -1;
}

FT_Face face;
if (FT_New_Face(ft, "fonts/arial.ttf", 0, &face))
{
    std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
    return -1;
}

FT_Set_Pixel_Sizes(face, 0, 48);
```

- **`FT_Init_FreeType`** initializes the library.
- **`FT_New_Face`** loads a font file. The `0` is the face index (some font files contain multiple faces).
- **`FT_Set_Pixel_Sizes(face, 0, 48)`** sets the desired glyph size. Width `0` means "calculate automatically from height." We're requesting 48-pixel tall glyphs.

---

## Glyph Metrics

Before we generate textures, it's important to understand how glyph metrics work. Each character has specific measurements that determine its positioning:

```
          ┌─────────────────────────────── bearingX ──┐
          │                                            │
          │    ┌──────────────┐                        │
          │    │              │ ─── bearingY            │
          │    │    ██████    │     (from baseline      │
          │    │   ██    ██   │      to glyph top)      │
          │    │   ██    ██   │                          │
          │    │   ████████   │ ─── height               │
          │    │   ██    ██   │                          │
──────────┼────│   ██    ██   │──────────── baseline ────│
          │    │              │                          │
          │    └──────────────┘                          │
          │                                              │
          │◄─────────── advance ──────────────────────►  │
          │    (distance to next character's origin)      │

  ─── width ───
  (bitmap width)

  Origin ─── The starting point for this glyph.
  Bearing ── Offset from origin to the top-left of the glyph bitmap.
  Advance ── How far to move the cursor for the next character.
```

These metrics are measured in 1/64th of a pixel by FreeType, so we'll divide by 64 (or right-shift by 6) when using them.

---

## Generating Glyph Textures

We'll pre-load textures for the first 128 ASCII characters and store them in a map:

```cpp
struct Character {
    unsigned int TextureID;  // glyph texture
    glm::ivec2   Size;       // glyph width and height
    glm::ivec2   Bearing;    // offset from baseline to top-left of glyph
    unsigned int Advance;    // horizontal offset to advance to next glyph
};

std::map<char, Character> Characters;
```

FreeType bitmaps are grayscale — one byte per pixel — which isn't 4-byte aligned like OpenGL expects by default. We must tell OpenGL to accept 1-byte alignment:

```cpp
glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
```

Now load each character:

```cpp
for (unsigned char c = 0; c < 128; c++)
{
    // Load the glyph: FT_LOAD_RENDER rasterizes it immediately
    if (FT_Load_Char(face, c, FT_LOAD_RENDER))
    {
        std::cout << "ERROR::FREETYPE: Failed to load Glyph '" << c << "'" << std::endl;
        continue;
    }

    // Generate an OpenGL texture from the glyph bitmap
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        face->glyph->bitmap.width,
        face->glyph->bitmap.rows,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        face->glyph->bitmap.buffer
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Character character = {
        texture,
        glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
        glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
        static_cast<unsigned int>(face->glyph->advance.x)
    };
    Characters.insert(std::pair<char, Character>(c, character));
}
```

After loading all glyphs, we can release FreeType resources:

```cpp
FT_Done_Face(face);
FT_Done_FreeType(ft);
```

Key points about the texture creation:

- We use `GL_RED` as both the internal format and the data format because each glyph bitmap is a single-channel grayscale image (one byte per pixel).
- `GL_CLAMP_TO_EDGE` prevents sampling artifacts at glyph edges.
- `GL_LINEAR` filtering provides smooth anti-aliased text.

---

## The Text Shader

Our text shader uses an **orthographic projection** so we can work in screen-space pixel coordinates. The vertex shader transforms a quad, and the fragment shader samples the glyph texture and applies color.

### Vertex Shader

```glsl
#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>

out vec2 TexCoords;

uniform mat4 projection;

void main()
{
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
```

We pack both position and texture coordinates into a single `vec4` attribute. This keeps the VBO simple: 4 floats per vertex.

### Fragment Shader

```glsl
#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D text;
uniform vec3 textColor;

void main()
{
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(textColor, 1.0) * sampled;
}
```

This is the key trick: the glyph texture is a single-channel (red) image where each pixel represents *coverage* — how much of that pixel is "inside" the glyph. We use this value as the **alpha channel** of a white color, then multiply by the desired text color. The result: the glyph shape is drawn in the text color, and the background is fully transparent.

### Projection Setup

For pixel-perfect text placement, we use an orthographic projection that maps directly to screen coordinates:

```cpp
glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH),
                                   0.0f, static_cast<float>(SCR_HEIGHT));
```

With this projection, `(0, 0)` is the bottom-left of the screen and `(800, 600)` is the top-right (for an 800×600 window).

### Blending

Since our text uses alpha transparency, we must enable blending:

```cpp
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
```

---

## Rendering Setup: VAO and VBO

We need a quad (two triangles, 6 vertices) for each character, but since every character has different dimensions and positions, we'll update the VBO dynamically using `glBufferSubData`:

```cpp
unsigned int VAO, VBO;
glGenVertexArrays(1, &VAO);
glGenBuffers(1, &VBO);

glBindVertexArray(VAO);
glBindBuffer(GL_ARRAY_BUFFER, VBO);

// Allocate enough space for a quad (6 vertices × 4 floats each)
// but don't fill it yet — we'll update it per character
glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

glEnableVertexAttribArray(0);
glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

glBindBuffer(GL_ARRAY_BUFFER, 0);
glBindVertexArray(0);
```

---

## The RenderText Function

This is the heart of our text renderer. For each character in the string, we calculate its screen-space quad, update the VBO, bind the glyph texture, and draw:

```cpp
void RenderText(Shader &shader, std::string text, float x, float y,
                float scale, glm::vec3 color)
{
    shader.use();
    shader.setVec3("textColor", color);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    for (auto c = text.begin(); c != text.end(); c++)
    {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        // Update VBO for each character
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Advance cursor (advance is in 1/64th pixels)
        x += (ch.Advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
```

Let's trace through what happens for each character:

```
Character Rendering Process (for the letter 'A')
─────────────────────────────────────────────────
  1. Look up 'A' in the Characters map
  2. Calculate quad position:
     xpos = cursor_x + bearing_x * scale
     ypos = cursor_y - (height - bearing_y) * scale
                        └── this drops glyphs that extend below the baseline
                            (like 'g', 'p', 'y')
  3. Calculate quad size:
     w = glyph_width * scale
     h = glyph_height * scale
  4. Upload 6 vertices (2 triangles) to the VBO:

     (xpos, ypos+h)────────(xpos+w, ypos+h)
          │  ╲                    │
          │    ╲    Triangle 2    │
          │      ╲                │
          │        ╲              │
          │  Tri 1   ╲           │
          │            ╲         │
     (xpos, ypos)──────(xpos+w, ypos)

  5. Bind the glyph's texture and draw
  6. Advance cursor_x by (advance >> 6) * scale
```

The `ypos` calculation deserves special attention. Characters like `g`, `p`, and `y` have **descenders** that drop below the baseline. The expression `ch.Size.y - ch.Bearing.y` gives us the distance from the baseline to the bottom of the glyph, and subtracting this from `y` pushes those characters down correctly.

---

## Complete Source Code

Here's a complete, compilable program that renders "Hello OpenGL!" on screen:

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <iostream>
#include <map>
#include <string>

// ─── Settings ───────────────────────────────────────────────────────────────

const unsigned int SCR_WIDTH  = 800;
const unsigned int SCR_HEIGHT = 600;

// ─── Shader sources ─────────────────────────────────────────────────────────

const char *textVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec4 vertex;
out vec2 TexCoords;
uniform mat4 projection;
void main()
{
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

const char *textFragmentShaderSource = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;
uniform sampler2D text;
uniform vec3 textColor;
void main()
{
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = vec4(textColor, 1.0) * sampled;
}
)";

// ─── Character data ─────────────────────────────────────────────────────────

struct Character {
    unsigned int TextureID;
    glm::ivec2   Size;
    glm::ivec2   Bearing;
    unsigned int Advance;
};

std::map<char, Character> Characters;
unsigned int textVAO, textVBO;

// ─── Shader helpers ─────────────────────────────────────────────────────────

unsigned int compileShader(GLenum type, const char *source)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    char infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        std::cout << "SHADER ERROR:\n" << infoLog << std::endl;
    }
    return shader;
}

unsigned int createShaderProgram(const char *vertSrc, const char *fragSrc)
{
    unsigned int vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    unsigned int frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    int success;
    char infoLog[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        std::cout << "PROGRAM ERROR:\n" << infoLog << std::endl;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}

// ─── Text rendering ─────────────────────────────────────────────────────────

void RenderText(unsigned int shader, std::string text, float x, float y,
                float scale, glm::vec3 color)
{
    glUseProgram(shader);
    glUniform3f(glGetUniformLocation(shader, "textColor"),
                color.x, color.y, color.z);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(textVAO);

    for (auto c = text.begin(); c != text.end(); c++)
    {
        Character ch = Characters[*c];

        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        glBindBuffer(GL_ARRAY_BUFFER, textVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.Advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ─── Main ───────────────────────────────────────────────────────────────────

void framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

int main()
{
    // ── GLFW / OpenGL setup ─────────────────────────────────────────────

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "Text Rendering", NULL, NULL);
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // ── Compile text shader ─────────────────────────────────────────────

    unsigned int textShader = createShaderProgram(textVertexShaderSource,
                                                  textFragmentShaderSource);

    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(SCR_WIDTH),
                                       0.0f, static_cast<float>(SCR_HEIGHT));
    glUseProgram(textShader);
    glUniformMatrix4fv(glGetUniformLocation(textShader, "projection"),
                       1, GL_FALSE, glm::value_ptr(projection));

    // ── FreeType setup ──────────────────────────────────────────────────

    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        std::cout << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return -1;
    }

    FT_Face face;
    if (FT_New_Face(ft, "fonts/arial.ttf", 0, &face))
    {
        std::cout << "ERROR::FREETYPE: Failed to load font" << std::endl;
        return -1;
    }

    FT_Set_Pixel_Sizes(face, 0, 48);

    // ── Load glyph textures ─────────────────────────────────────────────

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned char c = 0; c < 128; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cout << "ERROR::FREETYPE: Failed to load Glyph '"
                      << c << "'" << std::endl;
            continue;
        }

        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0, GL_RED, GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        Characters.insert(std::pair<char, Character>(c, character));
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // ── Text VAO/VBO setup ──────────────────────────────────────────────

    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ── Render loop ─────────────────────────────────────────────────────

    while (!glfwWindowShouldClose(window))
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        RenderText(textShader, "Hello OpenGL!", 25.0f, 300.0f, 1.0f,
                   glm::vec3(0.95f, 0.90f, 0.75f));

        RenderText(textShader, "Text rendering is working!", 50.0f, 200.0f, 0.6f,
                   glm::vec3(0.3f, 0.7f, 0.9f));

        RenderText(textShader, "(press ESC to exit)", 250.0f, 50.0f, 0.4f,
                   glm::vec3(0.5f, 0.5f, 0.5f));

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // ── Cleanup ─────────────────────────────────────────────────────────

    glDeleteVertexArrays(1, &textVAO);
    glDeleteBuffers(1, &textVBO);
    glDeleteProgram(textShader);
    glfwTerminate();
    return 0;
}
```

When you run this program, you should see "Hello OpenGL!" rendered in large warm-toned text near the center of the screen, a smaller blue subtitle below it, and a faint instruction at the bottom.

---

## Rendering Text with a 3D Scene

In a real application you'll typically render your 3D scene first, then overlay text on top. The key is to manage OpenGL state carefully:

```cpp
// 1. Render the 3D scene normally
glEnable(GL_DEPTH_TEST);
renderScene(sceneShader, camera);

// 2. Render text overlay
glDisable(GL_DEPTH_TEST);
glEnable(GL_BLEND);
glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

RenderText(textShader, "Score: 42", 10.0f, 570.0f, 0.5f,
           glm::vec3(1.0f, 1.0f, 1.0f));
RenderText(textShader, "FPS: 60", 10.0f, 545.0f, 0.4f,
           glm::vec3(0.0f, 1.0f, 0.0f));

// 3. Restore state if needed
glEnable(GL_DEPTH_TEST);
glDisable(GL_BLEND);
```

We disable depth testing for text so it always renders on top of the scene, regardless of the 3D geometry's depth values. The orthographic projection ensures the text appears in screen space.

---

## Performance Considerations

Our implementation is clean and correct, but it has a per-character overhead that matters at scale:

```
┌──────────────────────────────────────────────────────────────┐
│           Performance: Current vs. Optimized                  │
│                                                              │
│  Current approach (per character):                            │
│    • 1 texture bind                                          │
│    • 1 VBO update (glBufferSubData)                          │
│    • 1 draw call (glDrawArrays)                              │
│    → 100 characters = 100 draw calls                         │
│                                                              │
│  Optimized: Texture Atlas + Batching                         │
│    • Pack ALL glyphs into ONE texture                        │
│    • Build ALL quads into ONE VBO                            │
│    • 1 draw call for all characters                          │
│    → 100 characters = 1 draw call                            │
│                                                              │
│  For a few lines of text (HUD, score, FPS), our approach     │
│  is perfectly fine. For chat windows, terminals, or editors,  │
│  you'll want atlas-based batching.                           │
└──────────────────────────────────────────────────────────────┘
```

**Texture Atlas Approach:** Instead of one texture per glyph, pack all glyphs into a single large texture (e.g., 512×512). When rendering, use different texture coordinates per character to sample the right glyph. This eliminates all texture bind calls and allows batch rendering with a single draw call.

Libraries like **stb_truetype** can generate packed font atlases efficiently.

---

## Key Takeaways

- OpenGL has **no built-in text rendering** — you must build it yourself or use a library.
- **FreeType** rasterizes font glyphs into grayscale bitmaps that we convert to OpenGL textures.
- Each glyph has **metrics** (bearing, advance, size) that determine its positioning relative to the baseline and the next character.
- Set `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` before loading glyph textures — FreeType bitmaps are single-byte aligned, not 4-byte aligned.
- The fragment shader uses the glyph texture's red channel as an **alpha mask**, multiplied by the desired text color.
- Use an **orthographic projection** for screen-space text placement.
- **Enable blending** (`GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`) so the glyph alpha makes the background transparent.
- Render text **last** (after the 3D scene) with depth testing disabled for a proper overlay.
- For production use, switch to a **texture atlas** to batch all characters into a single draw call.

## Exercises

1. **Colored words:** Modify the render loop to display each word in a different color. Render "Red Green Blue" with each word matching its name.
2. **Dynamic text:** Display the current FPS (frames per second) as text in the corner of the screen, updating every frame. You'll need a frame counter and a timer.
3. **Centered text:** Write a `GetTextWidth()` function that sums the advances for all characters in a string, then use it to center text horizontally on screen.
4. **Scaling animation:** Make the text scale up and down using `sin(glfwGetTime())`. This demonstrates how the scale parameter in `RenderText` works.
5. **Multiple fonts:** Load a second font (e.g., a monospace font) and render different lines of text with different fonts. You'll need a second `Characters` map.
6. **Texture atlas:** Instead of one texture per glyph, pack all 128 glyphs into a single large texture. Update the rendering code to use different texture coordinates per glyph. Measure the performance difference with a large block of text.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Debugging](01_debugging.md) | [07. In Practice](.) | [Next: Breakout →](03_breakout.md) |
