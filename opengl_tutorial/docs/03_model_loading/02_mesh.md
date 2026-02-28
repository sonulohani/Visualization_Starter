# Mesh

In the previous chapter we learned how Assimp loads a 3D model file and organizes the data into scenes, nodes, and meshes. Now it's time to build the first of our two classes: the **Mesh** class.

A Mesh represents the smallest renderable unit: a chunk of geometry with its associated textures. A single model (like a character) may contain dozens of meshes — one for the head, one for the body, one for each piece of armor, and so on. Our Mesh class will take the raw vertex data from Assimp and set up everything OpenGL needs to draw it.

If you've followed the Getting Started and Lighting sections, you already know how to create VAOs, VBOs, EBOs, and draw with shaders. The Mesh class is essentially a clean encapsulation of all that work.

---

## What Data Does a Mesh Need?

Let's think about what a single mesh needs to be rendered:

1. **Vertices** — Each vertex has at minimum a position, a normal (for lighting), and texture coordinates (for texturing). More advanced meshes might also have tangent vectors, bone weights, and vertex colors, but we'll start with the essentials.

2. **Indices** — We use indexed drawing (`glDrawElements`) to avoid duplicating vertex data. The indices tell OpenGL which vertices form each triangle.

3. **Textures** — Each mesh may reference one or more textures: a diffuse map, a specular map, and potentially others. We need to know the OpenGL texture ID, the type of texture, and its file path (for caching).

---

## Defining Our Data Structures

Before writing the Mesh class itself, let's define the structs that represent our data.

### The Vertex Struct

```cpp
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};
```

Each `Vertex` packs all per-vertex attributes into a single struct. This is important because of how we'll feed data to OpenGL — more on that shortly.

### The Texture Struct

```cpp
struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};
```

- **`id`** — The OpenGL texture handle (created with `glGenTextures`).
- **`type`** — A string identifying the texture's role: `"texture_diffuse"` or `"texture_specular"`. We'll use this to set the correct shader uniform.
- **`path`** — The file path of the texture image. We store this so the Model class can check if a texture has already been loaded (avoiding duplicates).

---

## The Mesh Class Interface

Here's what our Mesh class looks like:

```cpp
class Mesh {
public:
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture>      textures;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices,
         std::vector<Texture> textures);
    void Draw(Shader &shader);

private:
    unsigned int VAO, VBO, EBO;
    void setupMesh();
};
```

We make `vertices`, `indices`, and `textures` public because there's no reason to hide data that the user might want to inspect or modify. The OpenGL objects (`VAO`, `VBO`, `EBO`) are private because they're an implementation detail.

The flow is simple:
1. Construct a Mesh with vertex/index/texture data.
2. The constructor calls `setupMesh()` to create the OpenGL buffers.
3. Each frame, call `Draw()` with a shader to render the mesh.

---

## The Constructor

```cpp
Mesh::Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices,
           std::vector<Texture> textures)
    : vertices(std::move(vertices)),
      indices(std::move(indices)),
      textures(std::move(textures))
{
    setupMesh();
}
```

We use `std::move` to avoid copying the potentially large vectors. After construction, the original vectors passed by the caller are left in a moved-from state (empty), and our member variables own the data.

---

## Setting Up the Mesh: `setupMesh()`

This is the core of the class — it creates and configures the VAO, VBO, and EBO. If you've set up vertex buffers before, this will look familiar, but there's an elegant trick with structs that makes it cleaner.

```cpp
void Mesh::setupMesh()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
                 indices.data(), GL_STATIC_DRAW);

    // vertex positions
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)0);

    // vertex normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, Normal));

    // vertex texture coords
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          (void*)offsetof(Vertex, TexCoords));

    glBindVertexArray(0);
}
```

Let's walk through the important parts.

### Uploading Vertex Data

```cpp
glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
             vertices.data(), GL_STATIC_DRAW);
```

We upload the entire `vertices` vector in one call. This works because `std::vector` stores its elements contiguously in memory, and our `Vertex` struct is a plain-old-data (POD) type. The GPU receives a flat block of memory like this:

```
Memory layout of vertices vector:
┌──────────────────────────────────────────────────────────────────┐
│ Vertex 0                         │ Vertex 1                     │ ...
│ Pos.x Pos.y Pos.z Norm.x Norm.y │ Norm.z TexU TexV Pos.x ...   │
│       Norm.z TexU  TexV          │                               │
└──────────────────────────────────────────────────────────────────┘
```

Actually, let's be more precise about the layout of a single Vertex:

```
One Vertex (32 bytes total):
┌────────────────┬────────────────┬──────────┐
│  Position      │  Normal        │ TexCoords│
│  vec3 (12B)    │  vec3 (12B)    │ vec2 (8B)│
│  x   y   z    │  x   y   z    │  u   v   │
└────────────────┴────────────────┴──────────┘
  offset: 0        offset: 12       offset: 24
```

### The Stride

The **stride** tells OpenGL how many bytes to skip to get from one vertex attribute to the same attribute in the next vertex. Since all our attributes are packed sequentially in a `Vertex` struct, the stride is simply `sizeof(Vertex)`:

```cpp
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
```

`sizeof(Vertex)` equals `sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec2)` = 12 + 12 + 8 = **32 bytes**. The C++ standard guarantees that struct members are laid out in declaration order, and since all our members are `float`-based vectors, there's no padding between them.

### The `offsetof` Macro

Here's the clever part. To tell OpenGL where each attribute starts within a `Vertex`, we use the C/C++ `offsetof` macro:

```cpp
// Normal starts 12 bytes into the struct (after Position)
glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                      (void*)offsetof(Vertex, Normal));

// TexCoords starts 24 bytes in (after Position + Normal)
glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                      (void*)offsetof(Vertex, TexCoords));
```

`offsetof(Vertex, Normal)` evaluates to the byte offset of the `Normal` member within the `Vertex` struct — which is 12 (3 floats × 4 bytes each). Similarly, `offsetof(Vertex, TexCoords)` evaluates to 24.

This is much more robust than hardcoding magic numbers like `(void*)12`. If we ever add a new field to `Vertex` (like tangent vectors), the offsets update automatically.

### Setting Up Attributes

We set up three vertex attribute pointers, one for each piece of data in our `Vertex` struct:

| Attribute | Location | Components | Offset |
|-----------|----------|------------|--------|
| Position | 0 | 3 (vec3) | 0 |
| Normal | 1 | 3 (vec3) | offsetof(Vertex, Normal) = 12 |
| TexCoords | 2 | 2 (vec2) | offsetof(Vertex, TexCoords) = 24 |

These match the vertex shader's `layout (location = ...)` declarations:

```glsl
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
```

---

## Drawing the Mesh: `Draw()`

The `Draw()` function activates the correct textures, sets the shader uniforms, and issues the draw call. The tricky part is the **texture naming convention**.

### Texture Naming Convention

Our shader expects texture uniforms to follow this pattern:

```
material.texture_diffuse1
material.texture_diffuse2
material.texture_specular1
material.texture_specular2
...
```

The format is `material.texture_{type}{number}`, where `type` comes from our `Texture` struct's `type` field, and `number` is a sequential counter starting at 1. This lets a mesh have multiple textures of the same type (e.g., two diffuse textures blended together).

### The Implementation

```cpp
void Mesh::Draw(Shader &shader)
{
    unsigned int diffuseNr  = 1;
    unsigned int specularNr = 1;

    for (unsigned int i = 0; i < textures.size(); i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);

        std::string number;
        std::string name = textures[i].type;
        if (name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if (name == "texture_specular")
            number = std::to_string(specularNr++);

        shader.setInt("material." + name + number, i);
        glBindTexture(GL_TEXTURE_2D, textures[i].id);
    }

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()),
                   GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
}
```

Let's trace through this step by step:

1. **Initialize counters** for each texture type. We track diffuse and specular counts separately.

2. **For each texture**:
   - Activate the texture unit `GL_TEXTURE0 + i`. OpenGL guarantees at least 16 texture units (0–15).
   - Determine the uniform name by combining the type and the count. For example, the first diffuse texture becomes `"material.texture_diffuse1"`, the second becomes `"material.texture_diffuse2"`.
   - Set the sampler uniform in the shader to point to texture unit `i`.
   - Bind the actual texture.

3. **Draw the mesh** using indexed rendering. `glDrawElements` reads the indices from the EBO and draws the corresponding triangles.

4. **Reset the active texture unit** to `GL_TEXTURE0` as a good practice. This avoids accidentally modifying a higher texture unit in later code.

---

## Complete Mesh Header: `mesh.h`

Here's the full, self-contained Mesh class. You can drop this into your project as a header file:

```cpp
#ifndef MESH_H
#define MESH_H

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

#include "shader.h"

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

class Mesh {
public:
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture>      textures;

    Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices,
         std::vector<Texture> textures)
        : vertices(std::move(vertices)),
          indices(std::move(indices)),
          textures(std::move(textures))
    {
        setupMesh();
    }

    void Draw(Shader &shader)
    {
        unsigned int diffuseNr  = 1;
        unsigned int specularNr = 1;

        for (unsigned int i = 0; i < textures.size(); i++)
        {
            glActiveTexture(GL_TEXTURE0 + i);

            std::string number;
            std::string name = textures[i].type;
            if (name == "texture_diffuse")
                number = std::to_string(diffuseNr++);
            else if (name == "texture_specular")
                number = std::to_string(specularNr++);

            shader.setInt("material." + name + number, i);
            glBindTexture(GL_TEXTURE_2D, textures[i].id);
        }

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()),
                       GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glActiveTexture(GL_TEXTURE0);
    }

private:
    unsigned int VAO, VBO, EBO;

    void setupMesh()
    {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER,
                     vertices.size() * sizeof(Vertex),
                     vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(unsigned int),
                     indices.data(), GL_STATIC_DRAW);

        // position attribute
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)0);

        // normal attribute
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, Normal));

        // texture coordinate attribute
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                              (void*)offsetof(Vertex, TexCoords));

        glBindVertexArray(0);
    }
};

#endif
```

---

## How It All Connects to the Shader

The Mesh class assumes a vertex shader and fragment shader that match its attribute layout and texture naming convention.

**Vertex shader:**

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
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoords = aTexCoords;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
```

**Fragment shader:**

```glsl
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

struct Material {
    sampler2D texture_diffuse1;
    sampler2D texture_specular1;
};

uniform Material material;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;

void main()
{
    // ambient
    vec3 ambient = 0.1 * texture(material.texture_diffuse1, TexCoords).rgb;

    // diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * texture(material.texture_diffuse1, TexCoords).rgb;

    // specular
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * texture(material.texture_specular1, TexCoords).rgb;

    FragColor = vec4((ambient + diffuse + specular) * lightColor, 1.0);
}
```

Notice how the fragment shader's `Material` struct has members named `texture_diffuse1` and `texture_specular1` — exactly matching the naming convention our `Draw()` function uses.

---

## Key Takeaways

- A **Mesh** is the smallest renderable unit: geometry + textures, wrapped in OpenGL buffers.

- The **Vertex struct** packs position, normal, and texture coordinates into a contiguous block of memory that maps directly to OpenGL vertex attributes.

- The **`offsetof` macro** gives us the byte offset of each struct member, making vertex attribute setup robust and maintainable.

- Struct memory layout lets us upload an entire `std::vector<Vertex>` to the GPU in a single `glBufferData` call — no manual interleaving needed.

- The **texture naming convention** (`material.texture_diffuse1`, `material.texture_specular1`, etc.) creates a contract between the Mesh class and the shader, allowing automatic binding of multiple textures.

- The Mesh class **encapsulates** VAO/VBO/EBO creation and drawing, so the rest of our code never has to touch raw OpenGL buffer commands.

## Exercises

1. **Add tangent vectors** to the Vertex struct. Add `glm::vec3 Tangent` and `glm::vec3 Bitangent` members, and set up the corresponding vertex attributes in `setupMesh()`. You'll need these for normal mapping later.

2. **Support additional texture types**: modify `Draw()` to handle `"texture_normal"` and `"texture_height"` texture types with their own counters.

3. **Debug visualization**: write a `DrawWireframe()` method that calls `glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)` before drawing and resets it after. This is useful for debugging mesh geometry.

4. **Think about resource management**: our current Mesh class doesn't delete the VAO/VBO/EBO when destroyed. What happens if a Mesh is copied? What would a proper destructor look like? (Hint: consider the Rule of Five.)

---

| [< Previous: Assimp](01_assimp.md) | [Next: Model >](03_model.md) |
|:---|---:|
