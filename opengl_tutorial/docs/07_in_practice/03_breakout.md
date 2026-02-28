# 2D Game: Breakout

It's time to put everything together. Throughout this tutorial series we've learned shaders, textures, transformations, lighting, framebuffers, blending, text rendering, and much more. In this final chapter, we'll combine many of these concepts into a single, complete project: a **Breakout** clone.

Breakout is a classic arcade game with simple rules: a paddle at the bottom of the screen, a ball that bounces around, and a wall of bricks at the top. The player moves the paddle left and right to keep the ball in play while it destroys bricks on contact. The game ends when all destroyable bricks are gone (you win) or the ball falls off the bottom (you lose a life).

```
┌────────────────────────────────────────┐
│  ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██  │  ← bricks
│  ██ ██    ██    ██ ██    ██    ██ ██  │
│  ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██ ██  │
│                                        │
│                                        │
│              ●                         │  ← ball
│           ↗                            │
│                                        │
│                                        │
│          ████████████                  │  ← paddle
└────────────────────────────────────────┘
```

This is a condensed but complete implementation. We won't build a production-quality game engine, but we will cover every essential component with working code that you can compile and play.

---

## Project Structure

```
breakout/
├── main.cpp                    — Window setup, game loop
├── game.h / game.cpp           — Game class: state machine, init, update, render
├── shader.h                    — Shader class (reuse from earlier chapters)
├── texture.h / texture.cpp     — Texture2D wrapper class
├── resource_manager.h / .cpp   — Singleton: loads and caches shaders/textures
├── sprite_renderer.h / .cpp    — Draws textured 2D quads
├── game_level.h / .cpp         — Loads brick layout from text files
├── game_object.h / .cpp        — Base object: position, size, velocity, color
├── ball_object.h / .cpp        — Ball: movement, radius, stuck-to-paddle state
├── particle_generator.h / .cpp — Particle trail behind the ball
├── post_processor.h / .cpp     — Framebuffer-based screen effects
├── text_renderer.h / .cpp      — FreeType text rendering (previous chapter)
└── power_up.h / .cpp           — Power-up items that drop from bricks
```

We'll provide complete source code for the core components and key snippets for the rest. Let's build it piece by piece.

---

## 1. Texture2D Class

A thin wrapper around OpenGL texture objects that stores format and dimension metadata:

```cpp
// texture.h
#ifndef TEXTURE_H
#define TEXTURE_H

#include <glad/glad.h>

class Texture2D
{
public:
    unsigned int ID;
    unsigned int Width, Height;
    unsigned int Internal_Format;
    unsigned int Image_Format;
    unsigned int Wrap_S;
    unsigned int Wrap_T;
    unsigned int Filter_Min;
    unsigned int Filter_Max;

    Texture2D()
        : Width(0), Height(0),
          Internal_Format(GL_RGB), Image_Format(GL_RGB),
          Wrap_S(GL_REPEAT), Wrap_T(GL_REPEAT),
          Filter_Min(GL_LINEAR), Filter_Max(GL_LINEAR)
    {
        glGenTextures(1, &this->ID);
    }

    void Generate(unsigned int width, unsigned int height, unsigned char *data)
    {
        this->Width  = width;
        this->Height = height;
        glBindTexture(GL_TEXTURE_2D, this->ID);
        glTexImage2D(GL_TEXTURE_2D, 0, this->Internal_Format,
                     width, height, 0, this->Image_Format,
                     GL_UNSIGNED_BYTE, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, this->Wrap_S);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, this->Wrap_T);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, this->Filter_Min);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, this->Filter_Max);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Bind() const
    {
        glBindTexture(GL_TEXTURE_2D, this->ID);
    }
};

#endif
```

---

## 2. Resource Manager

A singleton that loads shaders and textures by name, caching them to avoid duplicate loads:

```cpp
// resource_manager.h
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <map>
#include <string>
#include <glad/glad.h>

#include "texture.h"
#include "shader.h"

class ResourceManager
{
public:
    static std::map<std::string, Shader>    Shaders;
    static std::map<std::string, Texture2D> Textures;

    static Shader LoadShader(const char *vShaderFile, const char *fShaderFile,
                             const char *gShaderFile, std::string name);
    static Shader& GetShader(std::string name);

    static Texture2D LoadTexture(const char *file, bool alpha, std::string name);
    static Texture2D& GetTexture(std::string name);

    static void Clear();

private:
    ResourceManager() { }
    static Shader    loadShaderFromFile(const char *vFile, const char *fFile,
                                        const char *gFile = nullptr);
    static Texture2D loadTextureFromFile(const char *file, bool alpha);
};

#endif
```

```cpp
// resource_manager.cpp
#include "resource_manager.h"

#include <iostream>
#include <sstream>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

std::map<std::string, Texture2D> ResourceManager::Textures;
std::map<std::string, Shader>    ResourceManager::Shaders;

Shader ResourceManager::LoadShader(const char *vShaderFile,
                                    const char *fShaderFile,
                                    const char *gShaderFile,
                                    std::string name)
{
    Shaders[name] = loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
    return Shaders[name];
}

Shader& ResourceManager::GetShader(std::string name)
{
    return Shaders[name];
}

Texture2D ResourceManager::LoadTexture(const char *file, bool alpha,
                                        std::string name)
{
    Textures[name] = loadTextureFromFile(file, alpha);
    return Textures[name];
}

Texture2D& ResourceManager::GetTexture(std::string name)
{
    return Textures[name];
}

void ResourceManager::Clear()
{
    for (auto iter : Shaders)
        glDeleteProgram(iter.second.ID);
    for (auto iter : Textures)
        glDeleteTextures(1, &iter.second.ID);
}

Shader ResourceManager::loadShaderFromFile(const char *vFile,
                                            const char *fFile,
                                            const char *gFile)
{
    std::string vertexCode, fragmentCode, geometryCode;
    try
    {
        std::ifstream vertexFile(vFile);
        std::ifstream fragmentFile(fFile);
        std::stringstream vStream, fStream;
        vStream << vertexFile.rdbuf();
        fStream << fragmentFile.rdbuf();
        vertexCode   = vStream.str();
        fragmentCode = fStream.str();
        if (gFile != nullptr)
        {
            std::ifstream geometryFile(gFile);
            std::stringstream gStream;
            gStream << geometryFile.rdbuf();
            geometryCode = gStream.str();
        }
    }
    catch (std::exception e)
    {
        std::cout << "ERROR::SHADER: Failed to read shader files" << std::endl;
    }

    Shader shader;
    shader.Compile(vertexCode.c_str(), fragmentCode.c_str(),
                   gFile != nullptr ? geometryCode.c_str() : nullptr);
    return shader;
}

Texture2D ResourceManager::loadTextureFromFile(const char *file, bool alpha)
{
    Texture2D texture;
    if (alpha)
    {
        texture.Internal_Format = GL_RGBA;
        texture.Image_Format    = GL_RGBA;
    }
    int width, height, nrChannels;
    unsigned char *data = stbi_load(file, &width, &height, &nrChannels, 0);
    texture.Generate(width, height, data);
    stbi_image_free(data);
    return texture;
}
```

---

## 3. Sprite Renderer

The sprite renderer is the workhorse of our 2D engine. It draws textured quads using a model matrix for position, size, and rotation — the same transformation approach we've used throughout this tutorial, but in 2D with an orthographic projection.

```cpp
// sprite_renderer.h
#ifndef SPRITE_RENDERER_H
#define SPRITE_RENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "texture.h"
#include "shader.h"

class SpriteRenderer
{
public:
    SpriteRenderer(Shader &shader);
    ~SpriteRenderer();

    void DrawSprite(Texture2D &texture, glm::vec2 position,
                    glm::vec2 size = glm::vec2(10.0f, 10.0f),
                    float rotate = 0.0f,
                    glm::vec3 color = glm::vec3(1.0f));
private:
    Shader       shader;
    unsigned int quadVAO;

    void initRenderData();
};

#endif
```

```cpp
// sprite_renderer.cpp
#include "sprite_renderer.h"

SpriteRenderer::SpriteRenderer(Shader &shader)
{
    this->shader = shader;
    this->initRenderData();
}

SpriteRenderer::~SpriteRenderer()
{
    glDeleteVertexArrays(1, &this->quadVAO);
}

void SpriteRenderer::DrawSprite(Texture2D &texture, glm::vec2 position,
                                 glm::vec2 size, float rotate, glm::vec3 color)
{
    this->shader.Use();

    // Build model matrix: translate → rotate around center → scale
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(position, 0.0f));

    model = glm::translate(model, glm::vec3(0.5f * size.x, 0.5f * size.y, 0.0f));
    model = glm::rotate(model, glm::radians(rotate), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::translate(model, glm::vec3(-0.5f * size.x, -0.5f * size.y, 0.0f));

    model = glm::scale(model, glm::vec3(size, 1.0f));

    this->shader.SetMatrix4("model", model);
    this->shader.SetVector3f("spriteColor", color);

    glActiveTexture(GL_TEXTURE0);
    texture.Bind();

    glBindVertexArray(this->quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void SpriteRenderer::initRenderData()
{
    unsigned int VBO;
    float vertices[] = {
        // pos      // tex
        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,

        0.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 0.0f
    };

    glGenVertexArrays(1, &this->quadVAO);
    glGenBuffers(1, &VBO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindVertexArray(this->quadVAO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}
```

The sprite shader is straightforward:

```glsl
// sprite.vs
#version 330 core
layout (location = 0) in vec4 vertex; // <vec2 position, vec2 texCoords>

out vec2 TexCoords;

uniform mat4 model;
uniform mat4 projection;

void main()
{
    TexCoords = vertex.zw;
    gl_Position = projection * model * vec4(vertex.xy, 0.0, 1.0);
}
```

```glsl
// sprite.fs
#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D image;
uniform vec3 spriteColor;

void main()
{
    color = vec4(spriteColor, 1.0) * texture(image, TexCoords);
}
```

The model matrix construction deserves attention. For rotation around the sprite's center (rather than its corner), we:

1. Translate to the sprite's position
2. Translate to the sprite's center (`+0.5 * size`)
3. Rotate
4. Translate back (`-0.5 * size`)
5. Scale to the desired size

```
Model Matrix Construction for 2D Sprites
─────────────────────────────────────────
Unit quad [0,1] → Scale to size → Rotate around center → Position on screen

  [0,1]──[1,1]      Scale         Rotate          Translate
    │      │      ┌────────┐    ┌────────┐      ┌────────┐
    │ unit │  →   │ sized  │ →  │ rotated│  →   │ final  │
    │ quad │      │  quad  │    │  quad  │      │position│
    │      │      └────────┘    └────────┘      └────────┘
  [0,0]──[1,0]
```

---

## 4. Game Object

A base class for all drawable game entities:

```cpp
// game_object.h
#ifndef GAME_OBJECT_H
#define GAME_OBJECT_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "texture.h"
#include "sprite_renderer.h"

class GameObject
{
public:
    glm::vec2   Position, Size, Velocity;
    glm::vec3   Color;
    float       Rotation;
    bool        IsSolid;
    bool        Destroyed;
    Texture2D   Sprite;

    GameObject()
        : Position(0.0f, 0.0f), Size(1.0f, 1.0f), Velocity(0.0f),
          Color(1.0f), Rotation(0.0f), IsSolid(false), Destroyed(false) { }

    GameObject(glm::vec2 pos, glm::vec2 size, Texture2D sprite,
               glm::vec3 color = glm::vec3(1.0f),
               glm::vec2 velocity = glm::vec2(0.0f, 0.0f))
        : Position(pos), Size(size), Velocity(velocity), Color(color),
          Rotation(0.0f), IsSolid(false), Destroyed(false), Sprite(sprite) { }

    virtual void Draw(SpriteRenderer &renderer)
    {
        renderer.DrawSprite(this->Sprite, this->Position, this->Size,
                            this->Rotation, this->Color);
    }
};

#endif
```

---

## 5. Ball Object

The ball extends `GameObject` with a radius, a "stuck to paddle" flag, and movement logic:

```cpp
// ball_object.h
#ifndef BALL_OBJECT_H
#define BALL_OBJECT_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "game_object.h"

class BallObject : public GameObject
{
public:
    float Radius;
    bool  Stuck;

    BallObject()
        : GameObject(), Radius(12.5f), Stuck(true) { }

    BallObject(glm::vec2 pos, float radius, glm::vec2 velocity, Texture2D sprite)
        : GameObject(pos, glm::vec2(radius * 2.0f, radius * 2.0f), sprite,
                     glm::vec3(1.0f), velocity),
          Radius(radius), Stuck(true) { }

    glm::vec2 Move(float dt, unsigned int windowWidth)
    {
        if (!this->Stuck)
        {
            this->Position += this->Velocity * dt;

            // Bounce off left/right walls
            if (this->Position.x <= 0.0f)
            {
                this->Velocity.x = -this->Velocity.x;
                this->Position.x = 0.0f;
            }
            else if (this->Position.x + this->Size.x >= windowWidth)
            {
                this->Velocity.x = -this->Velocity.x;
                this->Position.x = windowWidth - this->Size.x;
            }

            // Bounce off top wall
            if (this->Position.y <= 0.0f)
            {
                this->Velocity.y = -this->Velocity.y;
                this->Position.y = 0.0f;
            }
        }
        return this->Position;
    }

    void Reset(glm::vec2 position, glm::vec2 velocity)
    {
        this->Position = position;
        this->Velocity = velocity;
        this->Stuck = true;
    }
};

#endif
```

---

## 6. Game Levels

Levels are defined in simple text files where each number represents a brick type:

```
1 1 1 1 1 1 1 1 1 1 1 1 1 1 1
1 1 1 1 1 1 1 1 1 1 1 1 1 1 1
2 2 2 2 2 0 0 0 0 0 2 2 2 2 2
3 3 3 3 3 3 3 3 3 3 3 3 3 3 3
4 4 4 4 4 0 0 0 0 0 4 4 4 4 4
5 5 5 5 5 5 5 5 5 5 5 5 5 5 5
```

- **0** = empty (no brick)
- **1** = solid / indestructible (gray)
- **2+** = destroyable bricks (different colors based on value)

```cpp
// game_level.h
#ifndef GAME_LEVEL_H
#define GAME_LEVEL_H

#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include "game_object.h"
#include "sprite_renderer.h"
#include "resource_manager.h"

class GameLevel
{
public:
    std::vector<GameObject> Bricks;

    GameLevel() { }

    void Load(const char *file, unsigned int levelWidth, unsigned int levelHeight)
    {
        this->Bricks.clear();
        unsigned int tileCode;
        std::string line;
        std::ifstream fstream(file);
        std::vector<std::vector<unsigned int>> tileData;

        if (fstream)
        {
            while (std::getline(fstream, line))
            {
                std::istringstream sstream(line);
                std::vector<unsigned int> row;
                while (sstream >> tileCode)
                    row.push_back(tileCode);
                tileData.push_back(row);
            }
        }
        if (tileData.size() > 0)
            this->init(tileData, levelWidth, levelHeight);
    }

    void Draw(SpriteRenderer &renderer)
    {
        for (auto &tile : this->Bricks)
            if (!tile.Destroyed)
                tile.Draw(renderer);
    }

    bool IsCompleted()
    {
        for (auto &tile : this->Bricks)
            if (!tile.IsSolid && !tile.Destroyed)
                return false;
        return true;
    }

private:
    void init(std::vector<std::vector<unsigned int>> tileData,
              unsigned int levelWidth, unsigned int levelHeight)
    {
        unsigned int height = tileData.size();
        unsigned int width  = tileData[0].size();
        float unit_width    = levelWidth / static_cast<float>(width);
        float unit_height   = levelHeight / static_cast<float>(height);

        for (unsigned int y = 0; y < height; ++y)
        {
            for (unsigned int x = 0; x < width; ++x)
            {
                if (tileData[y][x] == 0)
                    continue;

                glm::vec2 pos(unit_width * x, unit_height * y);
                glm::vec2 size(unit_width, unit_height);
                glm::vec3 color(1.0f);

                if (tileData[y][x] == 1)
                {
                    color = glm::vec3(0.8f, 0.8f, 0.7f);
                    GameObject obj(pos, size,
                                   ResourceManager::GetTexture("block_solid"),
                                   color);
                    obj.IsSolid = true;
                    this->Bricks.push_back(obj);
                }
                else
                {
                    // Color based on brick value
                    if (tileData[y][x] == 2)
                        color = glm::vec3(0.2f, 0.6f, 1.0f);
                    else if (tileData[y][x] == 3)
                        color = glm::vec3(0.0f, 0.7f, 0.0f);
                    else if (tileData[y][x] == 4)
                        color = glm::vec3(0.8f, 0.8f, 0.4f);
                    else if (tileData[y][x] == 5)
                        color = glm::vec3(1.0f, 0.5f, 0.0f);

                    GameObject obj(pos, size,
                                   ResourceManager::GetTexture("block"),
                                   color);
                    this->Bricks.push_back(obj);
                }
            }
        }
    }
};

#endif
```

---

## 7. Collision Detection

This is the most important piece of game logic. We need two types of collision:

- **AABB vs AABB** — for simple rectangular overlap tests
- **Circle vs AABB** — for the ball (circle) against bricks and the paddle (rectangles)

### AABB vs Circle Collision

The algorithm finds the point on the AABB closest to the circle's center, then checks if the distance to that point is less than the radius:

```
┌──────────────────────────────────────────────────────────────────┐
│              Circle vs AABB Collision                             │
│                                                                  │
│    ┌────────────────────┐                                        │
│    │                    │  AABB (brick)                           │
│    │        center──────│─────── difference = ball - aabb_center  │
│    │      (aabb)        │                                        │
│    │            closest─│──●    ← closest point on AABB to ball  │
│    │                    │   ╲                                     │
│    └────────────────────┘    ╲                                    │
│                               ○ ball center                      │
│                              ╱                                    │
│                        radius                                    │
│                                                                  │
│    Collision if: length(closest - ball_center) < ball.radius     │
└──────────────────────────────────────────────────────────────────┘
```

```cpp
// Collision direction enum
enum Direction {
    UP,
    RIGHT,
    DOWN,
    LEFT
};

// Collision result: did it collide, from which direction, how far to resolve
typedef std::tuple<bool, Direction, glm::vec2> Collision;

Direction VectorDirection(glm::vec2 target)
{
    glm::vec2 compass[] = {
        glm::vec2( 0.0f,  1.0f),  // up
        glm::vec2( 1.0f,  0.0f),  // right
        glm::vec2( 0.0f, -1.0f),  // down
        glm::vec2(-1.0f,  0.0f)   // left
    };
    float max = 0.0f;
    unsigned int best_match = -1;
    for (unsigned int i = 0; i < 4; i++)
    {
        float dot_product = glm::dot(glm::normalize(target), compass[i]);
        if (dot_product > max)
        {
            max = dot_product;
            best_match = i;
        }
    }
    return (Direction)best_match;
}

Collision CheckCollision(BallObject &ball, GameObject &brick)
{
    // Ball center
    glm::vec2 center(ball.Position + ball.Radius);

    // AABB info
    glm::vec2 aabb_half_extents(brick.Size.x / 2.0f, brick.Size.y / 2.0f);
    glm::vec2 aabb_center(brick.Position + aabb_half_extents);

    // Vector from AABB center to ball center
    glm::vec2 difference = center - aabb_center;
    glm::vec2 clamped = glm::clamp(difference, -aabb_half_extents, aabb_half_extents);

    // Closest point on AABB to circle
    glm::vec2 closest = aabb_center + clamped;

    difference = closest - center;

    if (glm::length(difference) < ball.Radius)
        return std::make_tuple(true, VectorDirection(difference), difference);
    else
        return std::make_tuple(false, UP, glm::vec2(0.0f));
}
```

### Why Direction Matters

When the ball hits a brick, we need to know **which side** was hit to determine the bounce direction:

```
             Hit TOP → reverse Y velocity
                 ↓
          ┌──────────────┐
Hit LEFT  │              │  Hit RIGHT
→ reverse │    Brick     │  → reverse
  X vel   │              │    X vel
          └──────────────┘
                 ↑
           Hit BOTTOM → reverse Y velocity
```

The `VectorDirection` function determines which compass direction the collision vector most closely aligns with using dot products against the four cardinal directions.

---

## 8. The Game Class

The `Game` class ties everything together. It manages game state, processes input, updates physics, resolves collisions, and renders every frame:

```cpp
// game.h
#ifndef GAME_H
#define GAME_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <vector>

#include "game_level.h"
#include "ball_object.h"

enum GameState {
    GAME_ACTIVE,
    GAME_MENU,
    GAME_WIN
};

const glm::vec2 PLAYER_SIZE(100.0f, 20.0f);
const float PLAYER_VELOCITY(500.0f);

const glm::vec2 INITIAL_BALL_VELOCITY(100.0f, -350.0f);
const float BALL_RADIUS = 12.5f;

class Game
{
public:
    GameState              State;
    bool                   Keys[1024];
    unsigned int           Width, Height;
    std::vector<GameLevel> Levels;
    unsigned int           Level;
    unsigned int           Lives;

    Game(unsigned int width, unsigned int height);
    ~Game();

    void Init();
    void ProcessInput(float dt);
    void Update(float dt);
    void Render();
    void DoCollisions();

    void ResetLevel();
    void ResetPlayer();
};

#endif
```

```cpp
// game.cpp
#include "game.h"
#include "resource_manager.h"
#include "sprite_renderer.h"
#include "ball_object.h"
#include "particle_generator.h"
#include "post_processor.h"
#include "text_renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <sstream>
#include <iostream>

SpriteRenderer    *Renderer;
GameObject        *Player;
BallObject        *Ball;
ParticleGenerator *Particles;
PostProcessor     *Effects;
TextRenderer      *Text;

float ShakeTime = 0.0f;

Game::Game(unsigned int width, unsigned int height)
    : State(GAME_MENU), Keys(), Width(width), Height(height), Level(0), Lives(3)
{
}

Game::~Game()
{
    delete Renderer;
    delete Player;
    delete Ball;
    delete Particles;
    delete Effects;
    delete Text;
    ResourceManager::Clear();
}

void Game::Init()
{
    // ── Load shaders ────────────────────────────────────────────────────
    ResourceManager::LoadShader("shaders/sprite.vs", "shaders/sprite.fs",
                                nullptr, "sprite");
    ResourceManager::LoadShader("shaders/particle.vs", "shaders/particle.fs",
                                nullptr, "particle");
    ResourceManager::LoadShader("shaders/post_processing.vs",
                                "shaders/post_processing.fs",
                                nullptr, "postprocessing");

    // ── Configure projection ────────────────────────────────────────────
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(this->Width),
                                       static_cast<float>(this->Height), 0.0f,
                                       -1.0f, 1.0f);

    ResourceManager::GetShader("sprite").Use().SetInteger("image", 0);
    ResourceManager::GetShader("sprite").SetMatrix4("projection", projection);
    ResourceManager::GetShader("particle").Use().SetInteger("sprite", 0);
    ResourceManager::GetShader("particle").SetMatrix4("projection", projection);

    // ── Load textures ───────────────────────────────────────────────────
    ResourceManager::LoadTexture("textures/background.jpg", false, "background");
    ResourceManager::LoadTexture("textures/ball.png", true, "ball");
    ResourceManager::LoadTexture("textures/block.png", false, "block");
    ResourceManager::LoadTexture("textures/block_solid.png", false, "block_solid");
    ResourceManager::LoadTexture("textures/paddle.png", true, "paddle");
    ResourceManager::LoadTexture("textures/particle.png", true, "particle");

    // ── Set up renderers ────────────────────────────────────────────────
    Renderer  = new SpriteRenderer(ResourceManager::GetShader("sprite"));
    Particles = new ParticleGenerator(ResourceManager::GetShader("particle"),
                                       ResourceManager::GetTexture("particle"),
                                       500);
    Effects   = new PostProcessor(ResourceManager::GetShader("postprocessing"),
                                   this->Width, this->Height);
    Text      = new TextRenderer(this->Width, this->Height);
    Text->Load("fonts/arial.ttf", 24);

    // ── Load levels ─────────────────────────────────────────────────────
    GameLevel one;   one.Load("levels/one.lvl",   this->Width, this->Height / 2);
    GameLevel two;   two.Load("levels/two.lvl",   this->Width, this->Height / 2);
    GameLevel three; three.Load("levels/three.lvl", this->Width, this->Height / 2);
    this->Levels.push_back(one);
    this->Levels.push_back(two);
    this->Levels.push_back(three);
    this->Level = 0;

    // ── Configure player paddle ─────────────────────────────────────────
    glm::vec2 playerPos = glm::vec2(
        this->Width / 2.0f - PLAYER_SIZE.x / 2.0f,
        this->Height - PLAYER_SIZE.y
    );
    Player = new GameObject(playerPos, PLAYER_SIZE,
                            ResourceManager::GetTexture("paddle"));

    // ── Configure ball ──────────────────────────────────────────────────
    glm::vec2 ballPos = playerPos + glm::vec2(PLAYER_SIZE.x / 2.0f - BALL_RADIUS,
                                               -BALL_RADIUS * 2.0f);
    Ball = new BallObject(ballPos, BALL_RADIUS, INITIAL_BALL_VELOCITY,
                          ResourceManager::GetTexture("ball"));
}

void Game::ProcessInput(float dt)
{
    if (this->State == GAME_MENU)
    {
        if (this->Keys[GLFW_KEY_ENTER])
            this->State = GAME_ACTIVE;
    }
    if (this->State == GAME_ACTIVE)
    {
        float velocity = PLAYER_VELOCITY * dt;

        if (this->Keys[GLFW_KEY_A] || this->Keys[GLFW_KEY_LEFT])
        {
            if (Player->Position.x >= 0.0f)
            {
                Player->Position.x -= velocity;
                if (Ball->Stuck)
                    Ball->Position.x -= velocity;
            }
        }
        if (this->Keys[GLFW_KEY_D] || this->Keys[GLFW_KEY_RIGHT])
        {
            if (Player->Position.x <= this->Width - Player->Size.x)
            {
                Player->Position.x += velocity;
                if (Ball->Stuck)
                    Ball->Position.x += velocity;
            }
        }
        if (this->Keys[GLFW_KEY_SPACE])
            Ball->Stuck = false;
    }
    if (this->State == GAME_WIN)
    {
        if (this->Keys[GLFW_KEY_ENTER])
        {
            Effects->Chaos = false;
            this->State = GAME_MENU;
        }
    }
}

void Game::Update(float dt)
{
    Ball->Move(dt, this->Width);
    this->DoCollisions();

    Particles->Update(dt, *Ball, 2, glm::vec2(Ball->Radius / 2.0f));

    if (ShakeTime > 0.0f)
    {
        ShakeTime -= dt;
        if (ShakeTime <= 0.0f)
            Effects->Shake = false;
    }

    // Ball fell off bottom edge
    if (Ball->Position.y >= this->Height)
    {
        --this->Lives;
        if (this->Lives == 0)
        {
            this->ResetLevel();
            this->State = GAME_MENU;
            this->Lives = 3;
        }
        this->ResetPlayer();
    }

    // Win condition
    if (this->State == GAME_ACTIVE && this->Levels[this->Level].IsCompleted())
    {
        this->ResetLevel();
        this->ResetPlayer();
        Effects->Chaos = true;
        this->State = GAME_WIN;
    }
}

void Game::Render()
{
    if (this->State == GAME_ACTIVE || this->State == GAME_MENU
        || this->State == GAME_WIN)
    {
        Effects->BeginRender();

            // Background
            Renderer->DrawSprite(ResourceManager::GetTexture("background"),
                                 glm::vec2(0.0f, 0.0f),
                                 glm::vec2(this->Width, this->Height), 0.0f);
            // Level bricks
            this->Levels[this->Level].Draw(*Renderer);
            // Player paddle
            Player->Draw(*Renderer);
            // Particles
            Particles->Draw();
            // Ball
            Ball->Draw(*Renderer);

        Effects->EndRender();
        Effects->Render(glfwGetTime());

        // HUD text
        std::stringstream ss;
        ss << this->Lives;
        Text->RenderText("Lives: " + ss.str(), 5.0f, 5.0f, 1.0f);
    }

    if (this->State == GAME_MENU)
    {
        Text->RenderText("Press ENTER to start", 250.0f, this->Height / 2.0f,
                         1.0f);
        Text->RenderText("Press A or D to select level", 225.0f,
                         this->Height / 2.0f + 30.0f, 0.75f);
    }

    if (this->State == GAME_WIN)
    {
        Text->RenderText("YOU WON!", 320.0f, this->Height / 2.0f - 20.0f,
                         1.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        Text->RenderText("Press ENTER for menu or ESC to quit", 180.0f,
                         this->Height / 2.0f, 1.0f);
    }
}

void Game::DoCollisions()
{
    // Ball vs bricks
    for (auto &brick : this->Levels[this->Level].Bricks)
    {
        if (brick.Destroyed)
            continue;

        Collision collision = CheckCollision(*Ball, brick);
        if (std::get<0>(collision))
        {
            if (!brick.IsSolid)
            {
                brick.Destroyed = true;
            }
            else
            {
                ShakeTime = 0.05f;
                Effects->Shake = true;
            }

            // Resolve collision
            Direction dir = std::get<1>(collision);
            glm::vec2 diff_vector = std::get<2>(collision);

            if (dir == LEFT || dir == RIGHT)
            {
                Ball->Velocity.x = -Ball->Velocity.x;
                float penetration = Ball->Radius - std::abs(diff_vector.x);
                if (dir == LEFT)
                    Ball->Position.x += penetration;
                else
                    Ball->Position.x -= penetration;
            }
            else // UP or DOWN
            {
                Ball->Velocity.y = -Ball->Velocity.y;
                float penetration = Ball->Radius - std::abs(diff_vector.y);
                if (dir == UP)
                    Ball->Position.y -= penetration;
                else
                    Ball->Position.y += penetration;
            }
        }
    }

    // Ball vs paddle
    Collision result = CheckCollision(*Ball, *Player);
    if (!Ball->Stuck && std::get<0>(result))
    {
        // Where on the paddle did the ball hit?
        float centerBoard = Player->Position.x + Player->Size.x / 2.0f;
        float distance = (Ball->Position.x + Ball->Radius) - centerBoard;
        float percentage = distance / (Player->Size.x / 2.0f);

        float strength = 2.0f;
        glm::vec2 oldVelocity = Ball->Velocity;
        Ball->Velocity.x = INITIAL_BALL_VELOCITY.x * percentage * strength;
        Ball->Velocity.y = -1.0f * std::abs(Ball->Velocity.y);
        Ball->Velocity = glm::normalize(Ball->Velocity)
                         * glm::length(oldVelocity);
    }
}

void Game::ResetLevel()
{
    if (this->Level == 0)
        this->Levels[0].Load("levels/one.lvl", this->Width, this->Height / 2);
    else if (this->Level == 1)
        this->Levels[1].Load("levels/two.lvl", this->Width, this->Height / 2);
    else if (this->Level == 2)
        this->Levels[2].Load("levels/three.lvl", this->Width, this->Height / 2);
}

void Game::ResetPlayer()
{
    Player->Size = PLAYER_SIZE;
    Player->Position = glm::vec2(this->Width / 2.0f - PLAYER_SIZE.x / 2.0f,
                                  this->Height - PLAYER_SIZE.y);
    Ball->Reset(Player->Position + glm::vec2(PLAYER_SIZE.x / 2.0f - BALL_RADIUS,
                                              -(BALL_RADIUS * 2.0f)),
                INITIAL_BALL_VELOCITY);
}
```

---

## 9. Particle System

Particles trail behind the ball, adding visual polish. The system uses an object pool — a fixed-size array of particles that get recycled:

```cpp
// particle_generator.h (key parts)
struct Particle {
    glm::vec2 Position, Velocity;
    glm::vec4 Color;
    float     Life;

    Particle() : Position(0.0f), Velocity(0.0f), Color(1.0f), Life(0.0f) { }
};

class ParticleGenerator
{
public:
    ParticleGenerator(Shader shader, Texture2D texture, unsigned int amount);

    void Update(float dt, GameObject &object, unsigned int newParticles,
                glm::vec2 offset = glm::vec2(0.0f));
    void Draw();

private:
    std::vector<Particle> particles;
    unsigned int amount;
    Shader shader;
    Texture2D texture;
    unsigned int VAO;

    void init();
    unsigned int firstUnusedParticle();
    void respawnParticle(Particle &particle, GameObject &object,
                         glm::vec2 offset = glm::vec2(0.0f));
};
```

```cpp
// particle_generator.cpp (core logic)
void ParticleGenerator::Update(float dt, GameObject &object,
                                unsigned int newParticles, glm::vec2 offset)
{
    // Spawn new particles
    for (unsigned int i = 0; i < newParticles; ++i)
    {
        int unusedParticle = this->firstUnusedParticle();
        this->respawnParticle(this->particles[unusedParticle], object, offset);
    }

    // Update all particles
    for (auto &p : this->particles)
    {
        p.Life -= dt;
        if (p.Life > 0.0f)
        {
            p.Position -= p.Velocity * dt;
            p.Color.a -= dt * 2.5f;
        }
    }
}

void ParticleGenerator::Draw()
{
    // Additive blending for a glowing effect
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    this->shader.Use();
    for (auto &particle : this->particles)
    {
        if (particle.Life > 0.0f)
        {
            this->shader.SetVector2f("offset", particle.Position);
            this->shader.SetVector4f("color", particle.Color);
            this->texture.Bind();
            glBindVertexArray(this->VAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }

    // Reset to default blending
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

unsigned int lastUsedParticle = 0;

unsigned int ParticleGenerator::firstUnusedParticle()
{
    // Search from last used particle (usually returns almost instantly)
    for (unsigned int i = lastUsedParticle; i < this->amount; ++i)
    {
        if (this->particles[i].Life <= 0.0f)
        {
            lastUsedParticle = i;
            return i;
        }
    }
    // Wrap around
    for (unsigned int i = 0; i < lastUsedParticle; ++i)
    {
        if (this->particles[i].Life <= 0.0f)
        {
            lastUsedParticle = i;
            return i;
        }
    }
    // All particles in use — overwrite the first one
    lastUsedParticle = 0;
    return 0;
}

void ParticleGenerator::respawnParticle(Particle &particle, GameObject &object,
                                         glm::vec2 offset)
{
    float random = ((rand() % 100) - 50) / 10.0f;
    float rColor = 0.5f + ((rand() % 100) / 100.0f);
    particle.Position = object.Position + random + offset;
    particle.Color = glm::vec4(rColor, rColor, rColor, 1.0f);
    particle.Life = 1.0f;
    particle.Velocity = object.Velocity * 0.1f;
}
```

The particle search algorithm is O(1) amortized — by starting from the last known dead particle instead of always from index 0, we skip over all the live particles we just spawned.

---

## 10. Post-Processing Effects

The entire game scene renders to a framebuffer object (FBO), and a full-screen shader applies effects before displaying to screen. This uses concepts from the framebuffers chapter:

```cpp
// post_processor.h (key parts)
class PostProcessor
{
public:
    Shader PostProcessingShader;
    Texture2D Texture;
    unsigned int Width, Height;
    bool Confuse, Chaos, Shake;

    PostProcessor(Shader shader, unsigned int width, unsigned int height);

    void BeginRender();
    void EndRender();
    void Render(float time);

private:
    unsigned int MSFBO, FBO;  // multisampled FBO, regular FBO
    unsigned int RBO;
    unsigned int VAO;

    void initRenderData();
};
```

```cpp
// post_processor.cpp (core flow)
void PostProcessor::BeginRender()
{
    glBindFramebuffer(GL_FRAMEBUFFER, this->MSFBO);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void PostProcessor::EndRender()
{
    // Resolve multisampled FBO to regular FBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, this->MSFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->FBO);
    glBlitFramebuffer(0, 0, this->Width, this->Height,
                      0, 0, this->Width, this->Height,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::Render(float time)
{
    this->PostProcessingShader.Use();
    this->PostProcessingShader.SetFloat("time", time);
    this->PostProcessingShader.SetInteger("confuse", this->Confuse);
    this->PostProcessingShader.SetInteger("chaos", this->Chaos);
    this->PostProcessingShader.SetInteger("shake", this->Shake);

    glActiveTexture(GL_TEXTURE0);
    this->Texture.Bind();
    glBindVertexArray(this->VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
```

The post-processing fragment shader:

```glsl
// post_processing.fs
#version 330 core
in vec2 TexCoords;
out vec4 color;

uniform sampler2D scene;
uniform float     time;
uniform bool      confuse;
uniform bool      chaos;
uniform bool      shake;

const float offset = 1.0 / 300.0;

void main()
{
    color = vec4(0.0);
    vec2 tc = TexCoords;

    if (chaos)
    {
        // Swirly distortion
        tc.x += sin(tc.y * 30.0 + time * 5.0) * 0.03;
        tc.y += cos(tc.x * 30.0 + time * 5.0) * 0.03;
    }
    if (confuse)
    {
        // Invert colors and flip UVs
        tc = 1.0 - tc;
    }

    if (shake)
    {
        // Screen shake: offset UV + edge detection kernel
        float strength = 0.01;
        tc.x += cos(time * 10.0) * strength;
        tc.y += cos(time * 15.0) * strength;

        // Edge detection via convolution kernel
        vec2 offsets[9] = vec2[](
            vec2(-offset,  offset), vec2(0.0,  offset), vec2(offset,  offset),
            vec2(-offset,  0.0),    vec2(0.0,  0.0),    vec2(offset,  0.0),
            vec2(-offset, -offset), vec2(0.0, -offset), vec2(offset, -offset)
        );
        float kernel[9] = float[](
            -1, -1, -1,
            -1,  8, -1,
            -1, -1, -1
        );
        vec3 sampleTex[9];
        for (int i = 0; i < 9; i++)
            sampleTex[i] = vec3(texture(scene, tc + offsets[i]));
        vec3 col = vec3(0.0);
        for (int i = 0; i < 9; i++)
            col += sampleTex[i] * kernel[i];
        color = vec4(col, 1.0);
    }
    else
    {
        color = texture(scene, tc);
    }

    if (confuse)
    {
        color = vec4(1.0 - color.rgb, 1.0);
    }
}
```

---

## 11. Power-Ups (Overview)

Power-ups add variety. When a brick is destroyed, there's a small chance a power-up spawns and falls toward the paddle:

| Power-Up | Effect | Duration |
|:---------|:-------|:---------|
| Speed | Increases ball velocity | Permanent until reset |
| Sticky | Ball sticks to paddle on contact, re-launch with Space | 20 seconds |
| Pass-Through | Ball destroys bricks without bouncing | 10 seconds |
| Pad-Size-Increase | Widens the paddle | Permanent until reset |
| Confuse | Activates confuse post-processing effect | 15 seconds |
| Chaos | Activates chaos post-processing effect | 15 seconds |

```cpp
// power_up.h
struct PowerUp : public GameObject
{
    std::string Type;
    float       Duration;
    bool        Activated;

    PowerUp(std::string type, glm::vec3 color, float duration,
            glm::vec2 position, Texture2D texture)
        : GameObject(position, glm::vec2(60.0f, 20.0f), texture, color,
                     glm::vec2(0.0f, 150.0f)),
          Type(type), Duration(duration), Activated(false) { }
};
```

Spawn logic (called when a brick is destroyed):

```cpp
bool ShouldSpawn(unsigned int chance)
{
    unsigned int random = rand() % chance;
    return random == 0;
}

void Game::SpawnPowerUps(GameObject &block)
{
    if (ShouldSpawn(75))
        this->PowerUps.push_back(PowerUp("speed", glm::vec3(0.5f, 0.5f, 1.0f),
                                          0.0f, block.Position,
                                          ResourceManager::GetTexture("powerup_speed")));
    if (ShouldSpawn(75))
        this->PowerUps.push_back(PowerUp("sticky", glm::vec3(1.0f, 0.5f, 1.0f),
                                          20.0f, block.Position,
                                          ResourceManager::GetTexture("powerup_sticky")));
    if (ShouldSpawn(75))
        this->PowerUps.push_back(PowerUp("pass-through", glm::vec3(0.5f, 1.0f, 0.5f),
                                          10.0f, block.Position,
                                          ResourceManager::GetTexture("powerup_passthrough")));
    if (ShouldSpawn(75))
        this->PowerUps.push_back(PowerUp("pad-size-increase", glm::vec3(1.0f, 0.6f, 0.4f),
                                          0.0f, block.Position,
                                          ResourceManager::GetTexture("powerup_increase")));
    if (ShouldSpawn(15))
        this->PowerUps.push_back(PowerUp("confuse", glm::vec3(1.0f, 0.3f, 0.3f),
                                          15.0f, block.Position,
                                          ResourceManager::GetTexture("powerup_confuse")));
    if (ShouldSpawn(15))
        this->PowerUps.push_back(PowerUp("chaos", glm::vec3(0.9f, 0.25f, 0.25f),
                                          15.0f, block.Position,
                                          ResourceManager::GetTexture("powerup_chaos")));
}
```

---

## 12. Audio (Brief Overview)

Sound effects dramatically improve game feel. The most common library for simple game audio is **irrKlang** (easy but proprietary) or **OpenAL** (open standard, more complex).

With irrKlang, adding audio is just a few lines:

```cpp
#include <irrKlang.h>
using namespace irrklang;

ISoundEngine *SoundEngine = createIrrKlangDevice();

// In Init():
SoundEngine->play2D("audio/breakout.mp3", true);  // background music, looped

// On brick hit:
SoundEngine->play2D("audio/bleep.mp3", false);

// On paddle hit:
SoundEngine->play2D("audio/bleep.wav", false);
```

For a lightweight open-source alternative, consider **miniaudio** (single-header, cross-platform) or **SoLoud** (C++ with a simple API).

---

## 13. Main Entry Point

The `main.cpp` creates the window, sets up callbacks, and runs the game loop:

```cpp
// main.cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "game.h"
#include "resource_manager.h"

#include <iostream>

const unsigned int SCREEN_WIDTH  = 800;
const unsigned int SCREEN_HEIGHT = 600;

Game Breakout(SCREEN_WIDTH, SCREEN_HEIGHT);

void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    if (key >= 0 && key < 1024)
    {
        if (action == GLFW_PRESS)
            Breakout.Keys[key] = true;
        else if (action == GLFW_RELEASE)
            Breakout.Keys[key] = false;
    }
}

void framebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, false);

    GLFWwindow *window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT,
                                          "Breakout", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glfwSetKeyCallback(window, keyCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Breakout.Init();

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        glfwPollEvents();

        Breakout.ProcessInput(deltaTime);
        Breakout.Update(deltaTime);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        Breakout.Render();

        glfwSwapBuffers(window);
    }

    ResourceManager::Clear();
    glfwTerminate();
    return 0;
}
```

---

## How It All Fits Together

Here's the data flow during a single frame of gameplay:

```
┌─────────────────────────────────────────────────────────────────────┐
│                    One Frame of Breakout                             │
│                                                                     │
│  1. Calculate deltaTime                                             │
│  2. glfwPollEvents()  →  keyCallback updates Game.Keys[]            │
│  3. ProcessInput(dt)                                                │
│     ├── Move paddle left/right based on Keys[]                      │
│     └── Release ball on Space                                       │
│  4. Update(dt)                                                      │
│     ├── Ball.Move(dt)  →  bounce off walls                          │
│     ├── DoCollisions()                                              │
│     │   ├── Ball vs each Brick  →  destroy brick, reverse velocity  │
│     │   └── Ball vs Paddle      →  redirect based on hit position   │
│     ├── Particles.Update(dt)  →  spawn/fade particles               │
│     ├── Check ball fell off  →  lose life or game over              │
│     └── Check level complete  →  win state                          │
│  5. Render()                                                        │
│     ├── Effects.BeginRender()    →  bind FBO                        │
│     ├── Draw background                                             │
│     ├── Draw bricks (skip destroyed)                                │
│     ├── Draw paddle                                                 │
│     ├── Draw particles (additive blend)                             │
│     ├── Draw ball                                                   │
│     ├── Effects.EndRender()      →  resolve to texture              │
│     ├── Effects.Render(time)     →  apply shake/chaos/confuse       │
│     └── Draw text (lives, menu, win)                                │
│  6. glfwSwapBuffers()                                               │
└─────────────────────────────────────────────────────────────────────┘
```

---

## OpenGL Concepts Used

This project draws on nearly every topic covered in the tutorial series:

| Concept | Where Used |
|:--------|:-----------|
| **Shaders** | Sprite rendering, particles, post-processing |
| **Textures** | Every visual element (sprites, bricks, ball, background) |
| **Blending** | Particles (additive), text (alpha), ball/paddle (transparency) |
| **Framebuffers** | Post-processing (render scene to FBO, apply effects) |
| **Vertex attributes** | Quad geometry for sprites, particles, screen quad |
| **Uniform variables** | Projection/model matrices, colors, time, effect toggles |
| **Orthographic projection** | Entire game (2D) + text overlay |
| **Transformations** | Sprite position/size/rotation via model matrix |
| **Dynamic buffer updates** | Text rendering (`glBufferSubData`), particles |
| **Multisampled FBOs** | Anti-aliased post-processing |

---

## Final Words

Congratulations — if you've followed this tutorial series from "Hello Triangle" to this Breakout game, you've built a solid foundation in modern OpenGL. You understand the GPU pipeline, you can write shaders, load models, implement lighting, use advanced techniques like shadow mapping and deferred rendering, and now you can combine it all into an interactive application.

### Where to Go from Here

- **Vulkan** — The successor to OpenGL. More verbose but gives you explicit control over the GPU. The mental model from OpenGL translates well.
- **Ray Tracing** — Both real-time (RTX/DXR) and offline. Understanding rasterization makes learning ray tracing much easier.
- **Game Engines** — Engines like Godot, Unreal, and Unity use the same concepts under the hood. Your OpenGL knowledge will help you understand what the engine is doing and why.
- **More OpenGL** — There's plenty we didn't cover: compute shaders, tessellation, bindless textures, indirect rendering, and more.
- **Graphics Research** — Topics like global illumination, physically-based rendering (PBR), volumetric effects, and neural rendering are active areas of research.

The most important thing is to keep building projects. Every new project will teach you something the tutorials can't.

---

## Key Takeaways

- A **2D game engine** built on OpenGL uses the same pipeline as 3D rendering: shaders, textures, vertex buffers, and framebuffers — just with orthographic projection.
- The **sprite renderer** pattern (one quad, many model matrices) is the 2D equivalent of the model/mesh rendering we did in 3D.
- **Collision detection** between circles and AABBs uses the closest-point-on-AABB algorithm with direction detection for proper bounce resolution.
- **Object pooling** (for particles) avoids per-frame allocations and keeps performance predictable.
- **Post-processing** via render-to-FBO lets you apply full-screen effects (shake, distortion, color inversion) without modifying individual draw calls.
- **Game architecture** separates concerns: resource management, rendering, physics/collision, input handling, and game state are all independent systems that compose together.
- Building a complete game — even a simple one — exercises every part of the OpenGL knowledge you've gained throughout this series.

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Text Rendering](02_text_rendering.md) | [07. In Practice](.) | |
