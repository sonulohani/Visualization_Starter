# Assimp

Up to this point, every vertex we've rendered has been typed out by hand. We manually defined position arrays, normal arrays, and texture coordinates — first for a simple triangle, then for a cube. That approach worked fine while we were learning the fundamentals of shaders and lighting, but it falls apart the moment you want to render anything interesting. A character model can have tens of thousands of vertices. A detailed building can have hundreds of thousands. Nobody is going to type those out by hand.

In the real world, 3D artists create models in tools like **Blender**, **Maya**, **3ds Max**, or **ZBrush**. These tools let artists sculpt, texture, and animate complex objects, then export them in various file formats. Our job as graphics programmers is to *load* those files and get the data into OpenGL.

That's where **Assimp** comes in.

---

## The Problem: Too Many File Formats

3D model files come in a dizzying variety of formats:

| Format | Extension | Notes |
|--------|-----------|-------|
| Wavefront OBJ | `.obj` | Simple, text-based, widely supported. No animations. |
| FBX | `.fbx` | Autodesk's proprietary format. Supports animations, materials. |
| glTF | `.gltf`/`.glb` | The "JPEG of 3D." Modern, efficient, open standard. |
| COLLADA | `.dae` | XML-based, open standard. Verbose but feature-rich. |
| STL | `.stl` | Common in 3D printing. Geometry only. |
| 3DS | `.3ds` | Legacy 3ds Max format. |
| Blender | `.blend` | Blender's native format. |

Each format stores data differently. OBJ uses plain text with lines starting with `v` (vertex), `vn` (normal), and `f` (face). FBX uses a complex binary or ASCII tree structure. glTF uses JSON paired with binary buffers. Writing a parser for even *one* of these formats is a substantial project. Writing parsers for all of them would be an entire career.

We need a library that can read all these formats and hand us the data in one consistent structure.

---

## What Is Assimp?

**Assimp** (Open **Ass**et **Imp**ort Library) is an open-source library that loads 3D model files from over **40 different formats** and converts them into a single, unified in-memory data structure. It handles all the parsing, format differences, and quirks for us.

Think of Assimp as a universal translator: no matter what language (format) the model speaks, Assimp converts it into one common language that our code can understand.

Assimp's role in our pipeline looks like this:

```
┌─────────────────┐         ┌─────────────┐         ┌──────────────────┐
│  Model File     │         │             │         │  Our OpenGL Code │
│  (.obj/.fbx/    │───────▶│   Assimp    │───────▶│  (Mesh/Model     │
│   .gltf/etc.)   │         │             │         │   classes)       │
└─────────────────┘         └─────────────┘         └──────────────────┘
```

Assimp only *imports* models (despite having some export functionality). It gives us raw geometry data — vertices, normals, texture coordinates, faces — that we then upload to OpenGL ourselves.

---

## Assimp's Data Structure

This is the most important part of this chapter. Before we write any code, you need to understand how Assimp organizes the data it loads. Everything in Assimp stems from a single root object: the **Scene**.

### The Scene

When Assimp loads a model file, it produces an `aiScene` object. This is the top-level container that holds *everything*:

- **`mMeshes[]`** — An array of all the meshes in the scene. Each mesh contains the actual geometry: vertex positions, normals, texture coordinates, and face indices.
- **`mMaterials[]`** — An array of all the materials. Each material stores textures, colors, shininess values, and other surface properties.
- **`mRootNode`** — A pointer to the root of a tree of nodes. This tree defines *how* the meshes are arranged in space.

### Nodes

Nodes form a **tree hierarchy** (like a family tree, or a file system). The root node can have child nodes, which can have their own children, and so on.

Each node contains:
- **`mMeshes[]`** — An array of *indices* (not actual mesh data). These indices point into the scene's `mMeshes[]` array.
- **`mChildren[]`** — An array of child nodes.
- **`mTransformation`** — A 4×4 transformation matrix that positions this node relative to its parent.

Why a tree structure? Consider a car model. The car body is the root. Each wheel is a child node. If you move the car, the wheels move with it — that's the parent-child relationship in action.

### Meshes

A **Mesh** is where the actual vertex data lives. Each mesh contains:
- **Vertex positions** — The 3D coordinates of each vertex.
- **Normals** — The direction each vertex faces (for lighting).
- **Texture coordinates** — UV coordinates for mapping textures.
- **Faces** — Each face defines a polygon by listing which vertices it connects (by index).
- **Material index** — An index into the scene's `mMaterials[]` array, telling us which material this mesh uses.

A single model file can contain multiple meshes. For example, a character model might have separate meshes for the head, body, arms, legs, and accessories.

### Materials

A **Material** describes the surface appearance of a mesh. It can contain:
- Diffuse textures (the "color" texture)
- Specular textures (where the surface is shiny)
- Normal maps, height maps, and more
- Ambient, diffuse, and specular colors
- Shininess, opacity, and other scalar properties

### Putting It All Together

Here is how all these pieces relate:

```
Scene
│
├── mMeshes[]
│   ├── Mesh 0 ─── vertices, normals, texCoords, faces, materialIndex
│   ├── Mesh 1 ─── vertices, normals, texCoords, faces, materialIndex
│   └── Mesh 2 ─── vertices, normals, texCoords, faces, materialIndex
│
├── mMaterials[]
│   ├── Material 0 ─── diffuse texture, specular texture, colors
│   ├── Material 1 ─── diffuse texture, specular texture, colors
│   └── Material 2 ─── diffuse texture, colors
│
└── mRootNode
    ├── mMeshes[] = {0}          ◄── index into Scene.mMeshes[]
    ├── mTransformation
    └── mChildren[]
        ├── Child Node A
        │   ├── mMeshes[] = {1}
        │   ├── mTransformation
        │   └── mChildren[] ...
        └── Child Node B
            ├── mMeshes[] = {2}
            ├── mTransformation
            └── mChildren[] ...
```

The key insight: **nodes don't contain geometry — they reference it**. The actual mesh data lives in `Scene.mMeshes[]`. Nodes just say "I use mesh #0 and mesh #2" by storing indices. This means the same mesh data can be referenced by multiple nodes (instancing).

Our loading strategy will be:
1. Start at `mRootNode`.
2. For each mesh index in the node, grab the actual mesh from `Scene.mMeshes[]`.
3. Extract vertices, normals, texture coordinates, and indices from the mesh.
4. Use the material index to look up textures from `Scene.mMaterials[]`.
5. Recurse into child nodes and repeat.

---

## Installing Assimp

### Linux (Debian/Ubuntu)

The easiest option is your package manager:

```bash
sudo apt install libassimp-dev
```

This installs the headers and shared library system-wide.

### Building from Source

If you need a newer version than what your package manager provides:

```bash
git clone https://github.com/assimp/assimp.git
cd assimp
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
```

### macOS

```bash
brew install assimp
```

### Windows (vcpkg)

```bash
vcpkg install assimp
```

### CMake Integration

Regardless of how you installed Assimp, you can link it in your `CMakeLists.txt` like this:

```cmake
find_package(assimp REQUIRED)

add_executable(model_loading main.cpp)
target_link_libraries(model_loading assimp::assimp)
```

If you're using a manual build or a non-standard install location, you may need to hint CMake:

```cmake
set(assimp_DIR "/path/to/assimp/lib/cmake/assimp")
find_package(assimp REQUIRED)
```

---

## Basic Usage Preview

Let's see how Assimp loads a model at the highest level. We'll flesh out the details in the next two chapters; this is just to give you a taste.

```cpp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

void loadModel(const std::string& path)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        std::cerr << "Assimp error: " << importer.GetErrorString() << std::endl;
        return;
    }

    // scene is now populated — start processing nodes and meshes
    // (we'll do this in the Model class)
}
```

Let's break this down:

1. **`Assimp::Importer importer`** — Create an importer object. It manages the loaded data's lifetime; when the importer is destroyed, the scene data is freed.

2. **`importer.ReadFile(path, flags)`** — Load the file at `path`. The second argument is a bitmask of post-processing flags that tell Assimp to transform the data as it loads.

3. **Error checking** — We check three things:
   - `scene` is not null (file loaded at all).
   - The `AI_SCENE_FLAGS_INCOMPLETE` flag is not set (data is complete).
   - `mRootNode` exists (the node tree is valid).

4. **Process the scene** — If everything checks out, we walk the node tree and extract mesh data. We'll implement this in the Model class.

---

## Post-Processing Flags

The second argument to `ReadFile` is where Assimp's power really shines. These flags tell Assimp to transform and fix up the data during import. Here are the most commonly used ones:

### `aiProcess_Triangulate`

**Always use this.** Many model formats can contain polygons with more than 3 vertices (quads, n-gons). OpenGL works best with triangles. This flag tells Assimp to split all faces into triangles before giving us the data.

```
Before:                  After aiProcess_Triangulate:
┌───────────┐            ┌───────────┐
│           │            │         ╱ │
│   Quad    │    ───▶    │  Tri  ╱   │
│           │            │     ╱ Tri │
└───────────┘            └───╱──────┘
```

### `aiProcess_FlipUVs`

OpenGL expects texture coordinate `(0,0)` to be at the **bottom-left** of the image. However, many image formats (and some model formats) place `(0,0)` at the **top-left**. This flag flips the UV coordinates vertically so textures appear correctly without needing to flip the images themselves.

> **Note:** If you're using `stb_image` and calling `stbi_set_flip_vertically_on_load(true)`, you may or may not need this flag depending on the model format. If textures look upside-down, toggle this flag.

### `aiProcess_GenNormals`

If the model file doesn't contain normal data, this flag tells Assimp to generate normals for each face. Without normals, lighting calculations won't work.

### `aiProcess_GenSmoothNormals`

Like `aiProcess_GenNormals`, but generates *smooth* normals by averaging the normals of all faces that share a vertex. This produces smoother-looking lighting at the cost of sharp edges becoming rounded.

### `aiProcess_CalcTangentSpace`

Calculates tangent and bitangent vectors for each vertex. These are needed for **normal mapping** (which we'll cover in a later chapter). If you plan to use normal maps, include this flag.

### `aiProcess_OptimizeMeshes`

Merges small meshes into larger ones, reducing the number of draw calls. Useful for complex scenes with many tiny meshes.

### A Common Combination

For most of our work, this set of flags will serve us well:

```cpp
unsigned int flags = aiProcess_Triangulate
                   | aiProcess_FlipUVs
                   | aiProcess_GenSmoothNormals
                   | aiProcess_CalcTangentSpace;

const aiScene* scene = importer.ReadFile(path, flags);
```

You can always add more flags later if your specific model requires them. See the [Assimp documentation](https://assimp.sourceforge.net/lib_html/postprocess_8h.html) for the full list.

---

## What We'll Build

Now that we understand Assimp's data structure and how to load a file, we need to build two classes that convert Assimp's data into something OpenGL can render:

1. **Mesh class** (next chapter) — Represents a single mesh. Stores vertices, indices, and textures. Handles VAO/VBO/EBO setup and drawing.

2. **Model class** (chapter after next) — Represents a complete model (which may contain many meshes). Uses Assimp to load the file, walks the node tree, and creates Mesh objects.

The relationship looks like this:

```
Model
├── loadModel() ─── uses Assimp to parse the file
├── processNode() ─── walks the node tree recursively
├── processMesh() ─── extracts vertex data from each aiMesh
└── meshes[]
    ├── Mesh 0 ─── VAO, VBO, EBO, Draw()
    ├── Mesh 1 ─── VAO, VBO, EBO, Draw()
    └── Mesh 2 ─── VAO, VBO, EBO, Draw()
```

---

## Key Takeaways

- **3D models come in many formats** (OBJ, FBX, glTF, etc.), each with its own structure. Writing individual parsers for each is impractical.

- **Assimp** is a library that loads 40+ model formats into a single unified data structure, saving us enormous effort.

- **The Scene object** is the root of all loaded data. It contains arrays of meshes and materials, and a tree of nodes.

- **Nodes** form a hierarchy and reference meshes by index — they don't contain geometry directly.

- **Meshes** contain the actual vertex data: positions, normals, texture coordinates, and face indices.

- **Materials** define surface properties: textures, colors, and other parameters.

- **Post-processing flags** like `aiProcess_Triangulate` and `aiProcess_FlipUVs` let Assimp fix up the data as it loads.

- We'll build a **Mesh** class and a **Model** class in the next two chapters to turn this raw data into rendered output.

## Exercises

1. **Install Assimp** on your system and verify it works by adding it to your CMakeLists.txt and including the headers. Make sure it compiles without errors.

2. **Write a simple loader** that opens a `.obj` file with Assimp and prints the number of meshes and materials in the scene. You can find free `.obj` files online, or export a simple cube from Blender.

3. **Experiment with post-processing flags**: load a model without `aiProcess_Triangulate`, then with it. Print the number of vertices in each face (`mesh->mFaces[i].mNumIndices`) — what changes?

4. **Read the Assimp docs** for the `aiScene`, `aiNode`, and `aiMesh` structures. Identify which fields we'll need for our Mesh and Model classes.

---

| [< Previous: Lighting Maps](../02_lighting/06_lighting_maps.md) | [Next: Mesh >](02_mesh.md) |
|:---|---:|
