# Hello Window

In the previous chapter we created a bare window that just sat there doing nothing. Now we'll bring it to life: clear the screen to a color, handle keyboard input, and properly respond to window resizing. By the end of this chapter, you'll have a solid understanding of the render loop — the beating heart of every real-time graphics application.

---

## The Render Loop

Every interactive graphical application follows the same fundamental pattern:

```
  ┌──────────────────────┐
  │  Process Input       │◀──┐
  ├──────────────────────┤   │
  │  Update State        │   │  Runs 60+ times
  ├──────────────────────┤   │  per second
  │  Render              │   │
  ├──────────────────────┤   │
  │  Swap Buffers        │───┘
  └──────────────────────┘
        │ (window closed)
        ▼
      Exit
```

In our GLFW application, this looks like:

```cpp
while (!glfwWindowShouldClose(window))
{
    // 1. Process input
    processInput(window);

    // 2. Rendering commands
    glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 3. Swap buffers and poll events
    glfwSwapBuffers(window);
    glfwPollEvents();
}
```

`glfwWindowShouldClose` returns `true` when the user clicks the close button or we call `glfwSetWindowShouldClose(window, true)` programmatically.

## Double Buffering

Why do we "swap buffers"? The answer is **double buffering**.

If we rendered directly to the screen, the user would see pixels being drawn one by one — leading to flicker and tearing. Instead, we use two buffers:

```
  ┌─────────────┐     ┌─────────────┐
  │ BACK BUFFER │     │FRONT BUFFER │
  │             │     │             │
  │  (we draw   │     │ (displayed  │
  │   here)     │     │  on screen) │
  │             │     │             │
  └─────────────┘     └─────────────┘
         │                    ▲
         │   glfwSwapBuffers  │
         └────────────────────┘
              (pointers swap)
```

1. **Back buffer**: We do all our rendering into this invisible buffer.
2. **Front buffer**: This is what's currently being displayed on screen.
3. **Swap**: When rendering is complete, `glfwSwapBuffers` swaps the two — the back buffer becomes the front buffer (displayed) and vice versa.

This swap is nearly instantaneous (it just swaps pointers), so the user always sees a complete, finished frame.

## Input Handling

GLFW provides two ways to handle input:

1. **Polling** — check the state of a key/button right now: `glfwGetKey`
2. **Callbacks** — register a function to be called when an event occurs: `glfwSetKeyCallback`

For game-like applications, polling is more common because you want to know the key state *every frame*. Let's create a simple input processing function:

```cpp
void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}
```

`glfwGetKey` returns:
- `GLFW_PRESS` — the key is currently held down
- `GLFW_RELEASE` — the key is not pressed

When the user presses Escape, we tell GLFW to close the window, which will cause the render loop's condition to become `true` and the loop to end.

We call `processInput` at the *beginning* of each frame:

```cpp
while (!glfwWindowShouldClose(window))
{
    processInput(window);

    // ... rendering ...

    glfwSwapBuffers(window);
    glfwPollEvents();
}
```

## Clearing the Screen

Right now our window shows whatever garbage was in video memory. Let's clear it to a nice teal color.

OpenGL's clear operation involves two steps:

### Step 1: Set the Clear Color

```cpp
glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
```

This is a **state-setting** function. It sets the color that OpenGL will use the *next* time you call `glClear`. The parameters are RGBA values, each ranging from 0.0 to 1.0:

| Parameter | Value | Meaning                     |
|-----------|-------|-----------------------------|
| Red       | 0.2   | 20% red                     |
| Green     | 0.3   | 30% green                   |
| Blue      | 0.3   | 30% blue                    |
| Alpha     | 1.0   | Fully opaque                |

This produces a dark teal/blue-green color.

### Step 2: Clear the Buffer

```cpp
glClear(GL_COLOR_BUFFER_BIT);
```

This is a **state-using** function. It fills the entire color buffer with the color set by `glClearColor`.

The parameter is a bitmask — you can clear multiple buffers at once:

| Buffer Bit              | What it clears                    |
|-------------------------|-----------------------------------|
| `GL_COLOR_BUFFER_BIT`   | The color buffer (pixel colors)   |
| `GL_DEPTH_BUFFER_BIT`   | The depth buffer (z-values)       |
| `GL_STENCIL_BUFFER_BIT` | The stencil buffer                |

Later, when we do 3D rendering, we'll clear both color and depth:

```cpp
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
```

> **Note**: `glClearColor` only needs to be called once (or when you want to change the color), since it just sets state. `glClear` must be called every frame.

## Handling Window Resize

When the user resizes the window, the OpenGL viewport doesn't automatically adjust. We need to tell OpenGL the new size via a **framebuffer size callback**:

```cpp
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}
```

We register this callback after creating the window:

```cpp
glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
```

Now, whenever the window is resized, GLFW calls our function, and we update the viewport to match the new dimensions.

**Why "framebuffer size" instead of "window size"?** On high-DPI displays (like Retina screens), the framebuffer size (in pixels) is larger than the window size (in screen coordinates). For rendering, we care about actual pixels, so we use the framebuffer size callback.

```
  Window size:      800 × 600 (screen coordinates)
  Framebuffer size: 1600 × 1200 (on 2x Retina display)
```

## Complete Example

Here is the full program — a window that clears to teal and closes when you press Escape:

```cpp
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

int main()
{
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cout << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create window
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT,
                                          "LearnOpenGL", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Register the framebuffer size callback
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Load OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // Render loop
    while (!glfwWindowShouldClose(window))
    {
        // Input
        processInput(window);

        // Rendering: clear the screen
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Swap buffers and poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Clean up all GLFW resources
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

### What You Should See

A window filled with a dark teal color:

```
  ┌──────────────────────────────────────┐
  │ LearnOpenGL                    [—][×]│
  ├──────────────────────────────────────┤
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓ (dark teal) ▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  │▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓│
  └──────────────────────────────────────┘
```

- Pressing **Escape** closes the window cleanly.
- **Resizing** the window keeps the viewport correct (no black bars or stretching).

## The Flow of a Frame

Let's trace exactly what happens during one iteration of the render loop:

```
  Frame N begins
  │
  ├─▶ processInput()
  │     └─ Check if ESC is pressed
  │
  ├─▶ glClearColor(0.2, 0.3, 0.3, 1.0)
  │     └─ Set state: "clear color is teal"
  │
  ├─▶ glClear(GL_COLOR_BUFFER_BIT)
  │     └─ Fill the BACK buffer with teal
  │
  ├─▶ glfwSwapBuffers(window)
  │     └─ Swap: BACK (teal) → displayed, old FRONT → new BACK
  │
  └─▶ glfwPollEvents()
        └─ Process OS events, trigger callbacks (resize, etc.)

  Frame N+1 begins...
```

## A Note on Frame Rate

Without any extra configuration, the render loop runs as fast as possible — potentially thousands of frames per second. This wastes GPU power and generates heat. In practice, GLFW synchronizes with the monitor's refresh rate via **VSync**:

```cpp
glfwSwapInterval(1);  // Enable VSync: wait for 1 screen refresh between swaps
```

Add this after `glfwMakeContextCurrent` to limit the frame rate to your monitor's refresh rate (typically 60 Hz). A value of `0` disables VSync.

---

## Key Takeaways

- The **render loop** (process input → render → swap → poll) runs every frame and is the core of any real-time application.
- **Double buffering** prevents flicker by rendering to an off-screen buffer and swapping it to the display when complete.
- `glClearColor` sets a state (the clear color); `glClear` uses that state to fill the buffer.
- Use **`glfwSetFramebufferSizeCallback`** to handle window resizing — this keeps the viewport in sync with the actual pixel dimensions.
- `glfwGetKey` lets you poll the keyboard state every frame.

## Exercises

1. Try changing the clear color to pure red `(1.0, 0.0, 0.0, 1.0)`, then green, then blue. Get a feel for how RGBA maps to colors.
2. Make the background color cycle smoothly over time. Hint: use `glfwGetTime()` to get the elapsed time in seconds, and `sin()` to oscillate a value.
3. Add input handling for additional keys: make the arrow keys change the clear color. For example, pressing `R` makes the background red, `G` makes it green, `B` makes it blue.
4. Move `glClearColor` outside the render loop (before it). Does the program still work? Why? (Think about the state machine concept.)
5. What happens if you remove the `glClear` call entirely? Why?

---

| | Navigation | |
|:---|:---:|---:|
| [← Previous: Creating a Window](03_creating_a_window.md) | | [Next: Hello Triangle →](05_hello_triangle.md) |
