# Model

We've built the foundation: we understand how Assimp organizes data (Chapter 1), and we have a Mesh class that can take vertex data and render it (Chapter 2). Now it's time to bring everything together with the **Model** class — the component that uses Assimp to load a file, walks the node tree, extracts meshes, loads textures, and produces a collection of Mesh objects ready to draw.

By the end of this chapter, loading and rendering a complex 3D model will be as simple as:

```cpp
Model backpack("resources/backpack/backpack.obj");

// in the render loop:
backpack.Draw(shader);
```

Two lines. That's the power of good abstraction.

---

## Model Class Overview

Our Model class needs to do the following:

1. **Load a model file** using Assimp.
2. **Walk the node tree** recursively, processing each node's meshes.
3. **Extract vertex data** (positions, normals, texture coordinates) and indices from each Assimp mesh.
4. **Load textures** referenced by each mesh's material, with caching to avoid loading the same image twice.
5. **Store everything** as a vector of our Mesh objects.
6. **Draw all meshes** when asked.

Here's the class interface:

```cpp
class Model {
public:
    Model(const std::string &path);
    void Draw(Shader &shader);

private:
    std::vector<Mesh>    meshes;
    std::string          directory;
    std::vector<Texture> textures_loaded;

    void loadModel(const std::string &path);
    void processNode(aiNode *node, const aiScene *scene);
    Mesh processMesh(aiMesh *mesh, const aiScene *scene);
    std::vector<Texture> loadMaterialTextures(aiMaterial *mat,
        aiTextureType type, std::string typeName);
};
```

Let's understand each member:

- **`meshes`** — All the Mesh objects that make up this model. Drawing the model means drawing each one.
- **`directory`** — The directory containing the model file. Texture paths in model files are often relative, so we need the directory to construct full paths.
- **`textures_loaded`** — A cache of all textures we've already loaded. If two meshes share the same texture file, we load it once and reuse it.

And each method:

- **`loadModel()`** — Entry point. Calls Assimp, validates the result, kicks off node processing.
- **`processNode()`** — Recursive. Processes the current node's meshes, then recurses into children.
- **`processMesh()`** — Converts an `aiMesh` into our `Mesh` class.
- **`loadMaterialTextures()`** — Loads textures of a given type from a material, using the cache.

---

## The Constructor and `Draw()`

The simplest parts first:

```cpp
Model::Model(const std::string &path)
{
    loadModel(path);
}

void Model::Draw(Shader &shader)
{
    for (unsigned int i = 0; i < meshes.size(); i++)
        meshes[i].Draw(shader);
}
```

The constructor delegates to `loadModel()`, and `Draw()` simply iterates over all meshes and draws each one. The Mesh class handles all the OpenGL details internally.

---

## Loading the Model: `loadModel()`

This function is where Assimp does its work:

```cpp
void Model::loadModel(const std::string &path)
{
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString() << std::endl;
        return;
    }

    directory = path.substr(0, path.find_last_of('/'));

    processNode(scene->mRootNode, scene);
}
```

Let's trace through:

1. **Create an `Assimp::Importer`** — This object manages the lifetime of the loaded data.

2. **Call `ReadFile`** with our standard post-processing flags:
   - `aiProcess_Triangulate` — Convert all faces to triangles.
   - `aiProcess_FlipUVs` — Flip texture coordinates for OpenGL's coordinate system.
   - `aiProcess_GenSmoothNormals` — Generate smooth normals if the file doesn't have them.
   - `aiProcess_CalcTangentSpace` — Calculate tangent/bitangent vectors (useful for normal mapping later).

3. **Validate the result** — Check for null scene, incomplete data, or missing root node.

4. **Extract the directory** — If the model path is `"resources/backpack/backpack.obj"`, we extract `"resources/backpack"`. We'll prepend this to texture filenames later.

5. **Start recursive processing** from the root node.

> **Important note on `Assimp::Importer` lifetime**: The importer is a local variable here. When `loadModel()` returns, the importer (and therefore the `aiScene`) is destroyed. This is fine because by that point we've already extracted all the data we need into our own Mesh objects. We don't keep any pointers to Assimp data.

---

## Walking the Node Tree: `processNode()`

This is a recursive function that walks the tree of nodes, processing each node's meshes:

```cpp
void Model::processNode(aiNode *node, const aiScene *scene)
{
    // process all meshes in this node
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }

    // then recursively process child nodes
    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}
```

Remember from Chapter 1: a node's `mMeshes` array contains *indices* into the scene's `mMeshes` array. So `node->mMeshes[i]` gives us an index, and `scene->mMeshes[that_index]` gives us the actual `aiMesh*`.

The recursion pattern is a standard tree traversal:

```
processNode(root)
├── process root's meshes
├── processNode(child A)
│   ├── process child A's meshes
│   ├── processNode(grandchild A1)
│   │   └── process grandchild A1's meshes
│   └── processNode(grandchild A2)
│       └── process grandchild A2's meshes
└── processNode(child B)
    └── process child B's meshes
```

> **Note**: We're currently ignoring the transformation matrices in nodes (`node->mTransformation`). For simple model loading, this works fine. For scenes with complex hierarchies or animations, you'd want to accumulate the transformations down the tree and pass them to each mesh. We'll revisit this if we cover skeletal animation.

---

## Extracting Mesh Data: `processMesh()`

This is the largest function, but it's straightforward — it's just pulling data from Assimp's structures into ours:

```cpp
Mesh Model::processMesh(aiMesh *mesh, const aiScene *scene)
{
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture>      textures;

    // ---- vertices ----
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;

        // positions
        vertex.Position = glm::vec3(
            mesh->mVertices[i].x,
            mesh->mVertices[i].y,
            mesh->mVertices[i].z
        );

        // normals
        if (mesh->HasNormals())
        {
            vertex.Normal = glm::vec3(
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            );
        }
        else
        {
            vertex.Normal = glm::vec3(0.0f, 0.0f, 0.0f);
        }

        // texture coordinates
        if (mesh->mTextureCoords[0])
        {
            vertex.TexCoords = glm::vec2(
                mesh->mTextureCoords[0][i].x,
                mesh->mTextureCoords[0][i].y
            );
        }
        else
        {
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    // ---- indices ----
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    // ---- materials / textures ----
    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

        std::vector<Texture> diffuseMaps = loadMaterialTextures(
            material, aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());

        std::vector<Texture> specularMaps = loadMaterialTextures(
            material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
    }

    return Mesh(std::move(vertices), std::move(indices), std::move(textures));
}
```

Let's go through each section.

### Extracting Vertices

For each of the mesh's `mNumVertices` vertices, we create a `Vertex` and fill in:

- **Position** — Always available. Assimp stores positions in `mVertices[]`.
- **Normal** — We check `HasNormals()` first. If the model file didn't have normals and we didn't use `aiProcess_GenNormals`, this could be missing.
- **Texture coordinates** — Assimp supports up to 8 sets of texture coordinates per vertex (`mTextureCoords[0]` through `mTextureCoords[7]`). We only use the first set (`[0]`). We check if it's non-null before accessing it.

### Extracting Indices

Each face in `mesh->mFaces[]` contains an array of vertex indices that form a polygon. Since we used `aiProcess_Triangulate`, every face has exactly 3 indices. We flatten all faces into a single index array.

### Extracting Textures

Each mesh has a `mMaterialIndex` that tells us which material in the scene it uses. We look up that material and call `loadMaterialTextures()` for each texture type we care about. Right now that's just diffuse and specular maps, but you could easily add normal maps, height maps, and others.

---

## Loading Textures: `loadMaterialTextures()`

This function queries an Assimp material for all textures of a given type and loads them:

```cpp
std::vector<Texture> Model::loadMaterialTextures(aiMaterial *mat,
    aiTextureType type, std::string typeName)
{
    std::vector<Texture> textures;

    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);

        bool skip = false;
        for (unsigned int j = 0; j < textures_loaded.size(); j++)
        {
            if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
            {
                textures.push_back(textures_loaded[j]);
                skip = true;
                break;
            }
        }

        if (!skip)
        {
            Texture texture;
            texture.id   = TextureFromFile(str.C_Str(), directory);
            texture.type = typeName;
            texture.path = str.C_Str();
            textures.push_back(texture);
            textures_loaded.push_back(texture);
        }
    }

    return textures;
}
```

For each texture in the material:

1. **Get the texture path** from Assimp (`mat->GetTexture()`).
2. **Check the cache** — scan `textures_loaded` to see if we've already loaded a texture with this path. If so, reuse it (push the existing `Texture` struct) and skip loading.
3. **If not cached**, load the texture image from disk, create an OpenGL texture, and store it in both the return vector and the cache.

### Why Caching Matters

Consider a model with 20 meshes that all use the same diffuse texture. Without caching, we'd load the same image 20 times, creating 20 separate OpenGL textures — a massive waste of memory and time. With our cache, we load it once and share the same OpenGL texture handle across all 20 meshes.

---

## The Texture Loading Helper: `TextureFromFile()`

This standalone function loads an image and creates an OpenGL texture. We keep it outside the class since it's a general utility:

```cpp
unsigned int TextureFromFile(const char *path, const std::string &directory)
{
    std::string filename = std::string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height,
                                    &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                     format, GL_UNSIGNED_BYTE, data);
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
        std::cerr << "Texture failed to load at path: " << filename << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
```

Key details:

- We **prepend the directory** to the texture path. Model files typically store relative paths like `"diffuse.png"`, not full paths.
- We **detect the format** from the number of channels. Grayscale images get `GL_RED`, RGB gets `GL_RGB`, and RGBA gets `GL_RGBA`.
- We **generate mipmaps** for better quality at distance.
- We use **stb_image** (`stbi_load`) for the actual image loading, just as in earlier chapters.

---

## Complete Model Header: `model.h`

Here's the full, self-contained Model class:

```cpp
#ifndef MODEL_H
#define MODEL_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <stb_image.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <string>
#include <vector>
#include <iostream>
#include <cstring>

#include "shader.h"
#include "mesh.h"

unsigned int TextureFromFile(const char *path, const std::string &directory);

class Model {
public:
    std::vector<Texture> textures_loaded;
    std::vector<Mesh>    meshes;
    std::string          directory;

    Model(const std::string &path)
    {
        loadModel(path);
    }

    void Draw(Shader &shader)
    {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

private:
    void loadModel(const std::string &path)
    {
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(path,
            aiProcess_Triangulate | aiProcess_FlipUVs |
            aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
            !scene->mRootNode)
        {
            std::cerr << "ERROR::ASSIMP::" << importer.GetErrorString()
                      << std::endl;
            return;
        }

        directory = path.substr(0, path.find_last_of('/'));
        processNode(scene->mRootNode, scene);
    }

    void processNode(aiNode *node, const aiScene *scene)
    {
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }

        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh processMesh(aiMesh *mesh, const aiScene *scene)
    {
        std::vector<Vertex>       vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture>      textures;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;

            vertex.Position = glm::vec3(
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z
            );

            if (mesh->HasNormals())
            {
                vertex.Normal = glm::vec3(
                    mesh->mNormals[i].x,
                    mesh->mNormals[i].y,
                    mesh->mNormals[i].z
                );
            }
            else
            {
                vertex.Normal = glm::vec3(0.0f);
            }

            if (mesh->mTextureCoords[0])
            {
                vertex.TexCoords = glm::vec2(
                    mesh->mTextureCoords[0][i].x,
                    mesh->mTextureCoords[0][i].y
                );
            }
            else
            {
                vertex.TexCoords = glm::vec2(0.0f);
            }

            vertices.push_back(vertex);
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }

        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];

        std::vector<Texture> diffuseMaps = loadMaterialTextures(
            material, aiTextureType_DIFFUSE, "texture_diffuse");
        textures.insert(textures.end(), diffuseMaps.begin(),
                        diffuseMaps.end());

        std::vector<Texture> specularMaps = loadMaterialTextures(
            material, aiTextureType_SPECULAR, "texture_specular");
        textures.insert(textures.end(), specularMaps.begin(),
                        specularMaps.end());

        return Mesh(std::move(vertices), std::move(indices),
                    std::move(textures));
    }

    std::vector<Texture> loadMaterialTextures(aiMaterial *mat,
        aiTextureType type, std::string typeName)
    {
        std::vector<Texture> textures;

        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);

            bool skip = false;
            for (unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if (std::strcmp(textures_loaded[j].path.data(),
                                str.C_Str()) == 0)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true;
                    break;
                }
            }

            if (!skip)
            {
                Texture texture;
                texture.id   = TextureFromFile(str.C_Str(), directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                textures_loaded.push_back(texture);
            }
        }

        return textures;
    }
};

unsigned int TextureFromFile(const char *path, const std::string &directory)
{
    std::string filename = std::string(path);
    filename = directory + '/' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(filename.c_str(), &width, &height,
                                    &nrComponents, 0);
    if (data)
    {
        GLenum format = GL_RGB;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                     format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    else
    {
        std::cerr << "Texture failed to load at path: " << filename
                  << std::endl;
    }

    stbi_image_free(data);
    return textureID;
}

#endif
```

---

## Using the Model Class

With both `mesh.h` and `model.h` in place, rendering a 3D model becomes trivial. Here's a complete `main.cpp` that loads and renders a model with basic lighting:

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "shader.h"
#include "camera.h"
#include "model.h"

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_callback(GLFWwindow *window, double xpos, double ypos);
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);

const unsigned int SCR_WIDTH  = 1280;
const unsigned int SCR_HEIGHT = 720;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX     = SCR_WIDTH / 2.0f;
float lastY     = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

int main()
{
    // --- GLFW & OpenGL setup ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "Model Loading", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
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
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    // --- Shader ---
    Shader modelShader("shaders/model.vs", "shaders/model.fs");

    // --- Model ---
    stbi_set_flip_vertically_on_load(false);
    Model ourModel("resources/backpack/backpack.obj");

    // --- Render loop ---
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        modelShader.use();

        // view & projection
        glm::mat4 projection = glm::perspective(
            glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        modelShader.setMat4("projection", projection);
        modelShader.setMat4("view", view);

        // lighting uniforms
        modelShader.setVec3("viewPos", camera.Position);
        modelShader.setVec3("lightPos", glm::vec3(1.2f, 1.0f, 2.0f));
        modelShader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));

        // model matrix
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(1.0f));
        modelShader.setMat4("model", model);

        ourModel.Draw(modelShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window)
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

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow *window, double xposIn, double yposIn)
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

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}
```

> **Note about `stbi_set_flip_vertically_on_load(false)`**: We set this to `false` here because we already use `aiProcess_FlipUVs` in our model loader. If you're seeing upside-down textures, try toggling one or the other (but not both — they cancel each other out).

---

## Where to Get 3D Models

You need actual model files to test with! Here are some great free sources:

| Source | URL | Notes |
|--------|-----|-------|
| Sketchfab | [sketchfab.com](https://sketchfab.com) | Huge library, many free models. Download as glTF or OBJ. |
| Poly Haven | [polyhaven.com](https://polyhaven.com) | High-quality models, textures, and HDRIs. All CC0 (public domain). |
| TurboSquid | [turbosquid.com](https://www.turbosquid.com) | Many free models available, filter by price. |
| Open3DModel | [open3dmodel.com](https://open3dmodel.com) | Free models in various formats. |
| Morgan McGuire's Archive | [casual-effects.com](https://casual-effects.com/data/) | Classic test models (Sponza, Sibenik, etc.) |

A popular test model is the **backpack** model from Sketchfab, which we've used in our examples. When you download a model, make sure:

1. It comes with textures (usually in the same directory or a `textures/` subdirectory).
2. You place it so the relative texture paths in the model file resolve correctly.
3. You try OBJ or glTF first — they tend to have fewer compatibility issues.

---

## The Shader for Model Rendering

The shader we need is essentially the same as our lighting maps shader from the Lighting section. The only change is that the texture uniform names follow our `material.texture_diffuse1` convention.

**Vertex shader** (`shaders/model.vs`):

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

**Fragment shader** (`shaders/model.fs`):

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
    // sample textures
    vec3 diffuseTex  = texture(material.texture_diffuse1, TexCoords).rgb;
    vec3 specularTex = texture(material.texture_specular1, TexCoords).rgb;

    // ambient
    vec3 ambient = 0.15 * diffuseTex;

    // diffuse
    vec3 norm     = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff    = max(dot(norm, lightDir), 0.0);
    vec3 diffuse  = diff * diffuseTex * lightColor;

    // specular (Blinn-Phong)
    vec3 viewDir    = normalize(viewPos - FragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec      = pow(max(dot(norm, halfwayDir), 0.0), 32.0);
    vec3 specular   = spec * specularTex * lightColor;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
```

This shader uses Blinn-Phong lighting, which you should be familiar with from the Lighting section. The only "new" part is that the texture samples come from the `material` struct whose member names match what our Mesh's `Draw()` function sets.

---

## Common Issues and Troubleshooting

### Textures Not Showing / White Model

**Cause**: The texture files aren't being found. The paths stored in the model file don't match the actual file locations on disk.

**Fix**: Check where your model file expects textures to be. Print the texture paths Assimp gives you:

```cpp
aiString str;
mat->GetTexture(type, i, &str);
std::cout << "Texture path: " << str.C_Str() << std::endl;
std::cout << "Full path: " << directory + "/" + str.C_Str() << std::endl;
```

Make sure those paths point to real files.

### Model Is Way Too Big or Too Small

**Cause**: Different modeling tools use different unit scales. A model created in Blender with default settings might be 100x larger than one from Maya.

**Fix**: Scale the model matrix:

```cpp
glm::mat4 model = glm::mat4(1.0f);
model = glm::scale(model, glm::vec3(0.01f)); // scale down 100x
```

### Model Appears Upside Down

**Cause**: Conflicting Y-axis conventions. Some model formats use Y-up, others use Z-up.

**Fix**: Try rotating the model:

```cpp
model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
```

Or toggle the `aiProcess_FlipUVs` flag and `stbi_set_flip_vertically_on_load()`.

### Model Has Missing Parts or Looks Broken

**Cause**: The model may have n-gons that didn't triangulate properly, or it has multiple UV sets.

**Fix**: Ensure `aiProcess_Triangulate` is in your post-processing flags. Try loading the model in Blender first to verify it looks correct there.

### Slow Loading

**Cause**: Large models with many textures take time to load, especially if textures are high-resolution.

**Fix**: Load models asynchronously (on a separate thread), or pre-process textures to smaller sizes. You can also use `aiProcess_OptimizeMeshes` to reduce mesh count.

---

## The Full Picture

Let's step back and see how all the pieces fit together:

```
main.cpp
│
├── creates Shader
├── creates Model("path/to/model.obj")
│   │
│   └── loadModel()
│       ├── Assimp::Importer loads the file → aiScene
│       └── processNode(rootNode, scene)
│           ├── for each mesh index in node:
│           │   └── processMesh(aiMesh, scene)
│           │       ├── extract vertices (pos, normal, UV)
│           │       ├── extract indices (from faces)
│           │       ├── loadMaterialTextures(diffuse)
│           │       │   └── TextureFromFile() → stb_image → OpenGL texture
│           │       ├── loadMaterialTextures(specular)
│           │       │   └── TextureFromFile() → stb_image → OpenGL texture
│           │       └── return Mesh(vertices, indices, textures)
│           │           └── setupMesh() → VAO, VBO, EBO
│           └── recurse into child nodes
│
└── render loop
    └── ourModel.Draw(shader)
        └── for each mesh:
            └── mesh.Draw(shader)
                ├── bind textures to correct units
                ├── set material uniforms
                └── glDrawElements()
```

---

## Key Takeaways

- The **Model class** ties Assimp to our Mesh class. It loads a file, walks the node tree, extracts data, and produces a vector of renderable Meshes.

- **`processNode()`** is recursive — it handles the tree structure of nodes, visiting every mesh in the model regardless of how deeply nested it is.

- **`processMesh()`** translates Assimp's data structures into our own, pulling out positions, normals, texture coordinates, and face indices.

- **Texture caching** via `textures_loaded` prevents the same image from being loaded multiple times — critical for performance with complex models.

- **`TextureFromFile()`** uses stb_image for loading and handles different channel counts (grayscale, RGB, RGBA).

- The **shader** for model rendering is essentially the same as our lighting maps shader, with texture uniforms named to match the Mesh class's convention.

- Two lines of code — constructing a Model and calling Draw — can render arbitrarily complex 3D models.

## Exercises

1. **Load multiple models**: place two or three models in your scene at different positions using different model matrices. Render them all in the same frame.

2. **Add a light source visualization**: render a small cube or sphere at the light position so you can see where the light is. Use a separate unlit shader for this.

3. **Implement a model viewer**: add keyboard controls to rotate the model (not the camera). Let the user press `R` to reset the rotation. Display the model name in the window title.

4. **Try different file formats**: download the same model in OBJ, FBX, and glTF formats. Load each one and compare — do they look the same? Are there differences in texture handling?

5. **Add normal map support**: extend `processMesh()` to also load `aiTextureType_NORMALS` textures, update the Vertex struct with tangent/bitangent vectors (from `mesh->mTangents` and `mesh->mBitangents`), and write a shader that uses the normal map for per-pixel lighting.

6. **Profile your loader**: use `std::chrono` to measure how long `loadModel()` takes for different models. Which step takes the longest — file parsing, texture loading, or OpenGL buffer creation?

---

| [< Previous: Mesh](02_mesh.md) | [Next: Advanced OpenGL >](../04_advanced_opengl/01_depth_testing.md) |
|:---|---:|
