// Multi-Context Demo — loads vertex data on a background thread using a shared
// OpenGL context, then renders it on the main thread.
//
// Demonstrates:
//   - Creating two GLFW windows/contexts with resource sharing
//   - The one-thread-one-context rule
//   - Shared VBOs + per-context VAOs
//   - Synchronization with glFinish / std::atomic

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <atomic>
#include <iostream>
#include <thread>

// ── Vertex data ─────────────────────────────────────────────────────────────
// Triangle vertices: position (x, y) + color (r, g, b)
float vertices[] = {
    // positions      // colors
    -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, // bottom-left  (red)
    0.5f, -0.5f, 0.0f, 1.0f, 0.0f,  // bottom-right (green)
    0.0f, 0.5f, 0.0f, 0.0f, 1.0f    // top-center   (blue)
};

// ── Shaders ─────────────────────────────────────────────────────────────────
const char *vertex_shader_src = R"(
    #version 330 core
    layout (location = 0) in vec2 aPos;
    layout (location = 1) in vec3 aColor;
    out vec3 ourColor;
    void main() {
        gl_Position = vec4(aPos, 0.0, 1.0);
        ourColor = aColor;
    }
)";

const char *fragment_shader_src = R"(
    #version 330 core
    in vec3 ourColor;
    out vec4 FragColor;
    void main() {
        FragColor = vec4(ourColor, 1.0);
    }
)";

// ── Inter-thread communication ──────────────────────────────────────────────
std::atomic<unsigned int> shared_vbo{0};
std::atomic<bool> data_ready{false};

// ── Helper: compile a single shader ─────────────────────────────────────────
unsigned int CompileShader(unsigned int type, const char *source)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error:\n"
                  << log << std::endl;
    }
    return shader;
}

// ── Helper: link vertex + fragment shaders into a program ───────────────────
unsigned int BuildShaderProgram(const char *vs_src, const char *fs_src)
{
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, vs_src);
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, fs_src);

    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Program link error:\n"
                  << log << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// ── Loader thread: uploads vertex data using its own context ────────────────
void LoaderThread(GLFWwindow *loader_ctx)
{
    // Make the shared context current on THIS thread
    glfwMakeContextCurrent(loader_ctx);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // Create a VBO and upload vertex data (VBOs are shared between contexts)
    unsigned int vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // CRITICAL: glFinish blocks until all GPU commands have completed.
    // Without this, the main thread might read the VBO before the upload
    // is actually done on the GPU side.
    glFinish();

    // Signal the main thread
    shared_vbo.store(vbo);
    data_ready.store(true);

    std::cout << "[Loader] VBO " << vbo << " uploaded and ready.\n";

    // Release the context (good practice when the thread is done)
    glfwMakeContextCurrent(nullptr);
}

// ── Callbacks ───────────────────────────────────────────────────────────────
void FramebufferSizeCallback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// ── Main ────────────────────────────────────────────────────────────────────
int main()
{
    // 1. Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // 2. Create the MAIN window + context
    GLFWwindow *main_window = glfwCreateWindow(
        800, 600, "Multi-Context Demo", nullptr, nullptr);
    if (!main_window)
    {
        std::cerr << "Failed to create main window\n";
        glfwTerminate();
        return -1;
    }

    // 3. Create the LOADER context (invisible, shares resources with main)
    //    The last parameter is the key — it tells GLFW to share OpenGL
    //    objects (textures, buffers, shaders) between the two contexts.
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *loader_ctx = glfwCreateWindow(
        1, 1, "Loader", nullptr, main_window);
    if (!loader_ctx)
    {
        std::cerr << "Failed to create loader context\n";
        glfwDestroyWindow(main_window);
        glfwTerminate();
        return -1;
    }

    // 4. Launch the loader thread before making main context current
    //    (each thread makes its own context current)
    std::thread loader(LoaderThread, loader_ctx);

    // 5. Set up the main context on this (main) thread
    glfwMakeContextCurrent(main_window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSetFramebufferSizeCallback(main_window, FramebufferSizeCallback);

    // 6. Build shaders (shader programs are shared objects)
    unsigned int shader_program = BuildShaderProgram(
        vertex_shader_src, fragment_shader_src);

    // 7. VAO setup — VAOs are NOT shared, so we create one on this thread
    unsigned int vao = 0;
    bool vao_configured = false;

    // ── Render loop ─────────────────────────────────────────────────
    while (!glfwWindowShouldClose(main_window))
    {
        // Input
        if (glfwGetKey(main_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(main_window, true);

        // Clear
        glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Once the loader signals data is ready, create our local VAO
        // and hook up the shared VBO to it
        if (data_ready.load() && !vao_configured)
        {
            unsigned int vbo = shared_vbo.load();

            glGenVertexArrays(1, &vao);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);

            // position attribute (location = 0): 2 floats
            glVertexAttribPointer(
                0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
            glEnableVertexAttribArray(0);

            // color attribute (location = 1): 3 floats
            glVertexAttribPointer(
                1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                (void *)(2 * sizeof(float)));
            glEnableVertexAttribArray(1);

            glBindVertexArray(0);
            vao_configured = true;

            std::cout << "[Main] VAO configured with shared VBO.\n";
        }

        // Draw when data is available
        if (vao_configured)
        {
            glUseProgram(shader_program);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        glfwSwapBuffers(main_window);
        glfwPollEvents();
    }

    // ── Cleanup ─────────────────────────────────────────────────────
    loader.join();

    if (vao_configured)
    {
        glDeleteVertexArrays(1, &vao);
        unsigned int vbo = shared_vbo.load();
        glDeleteBuffers(1, &vbo);
    }
    glDeleteProgram(shader_program);

    glfwDestroyWindow(loader_ctx);
    glfwDestroyWindow(main_window);
    glfwTerminate();
    return 0;
}
