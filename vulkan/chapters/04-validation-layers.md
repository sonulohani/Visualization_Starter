# Chapter 4: Validation Layers

[<< Previous: Instance](03-instance.md) | [Next: Physical Devices >>](05-physical-devices.md)

---

## The Problem: Vulkan Has No Safety Net

By default, Vulkan does **zero error checking**. If you pass invalid parameters, the behavior is undefined:

```cpp
// OpenGL: Tells you what went wrong
glBindTexture(GL_TEXTURE_2D, 999999);  // → GL_INVALID_VALUE error

// Vulkan: Undefined behavior — could crash, render garbage, or appear to work
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, VK_NULL_HANDLE);
// → Maybe crash? Maybe works? Who knows!
```

### Why No Built-in Error Checking?

**Performance**. In a shipped game running at 144 FPS, the GPU driver processes tens of thousands of calls per frame. Checking every single one wastes precious CPU time.

But during development, you absolutely need error checking. That's where **validation layers** come in.

---

## What Are Validation Layers?

Validation layers are **optional middleware** that sits between your app and the Vulkan driver:

```
Without Layers:
  Your App ──────────────────────────── GPU Driver
                                         (no checking)

With Layers:
  Your App ─── Validation Layer ──── GPU Driver
                    │
                    ├── "Warning: You forgot to bind a descriptor set"
                    ├── "Error: Image layout transition is invalid"
                    └── "Performance: Consider using a staging buffer"
```

### Analogy

Validation layers are like a **spell checker** in a word processor:
- In the final printed book (release build), you don't need spell checking.
- While writing (development), you want every typo highlighted immediately.

---

## Enabling Validation Layers

### The One Layer You Need

There used to be many individual layers (`VK_LAYER_LUNARG_standard_validation`, etc.), but they've been consolidated into one:

```cpp
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
```

This single layer includes:
- **Parameter validation** — checks all function arguments
- **Object lifetime tracking** — detects use-after-free, double-free
- **Thread safety checks** — detects race conditions
- **API usage rules** — ensures correct call ordering
- **Performance warnings** — suggests optimizations
- **Shader validation** — checks SPIR-V against pipeline state

### Debug vs Release Toggle

```cpp
#ifdef NDEBUG
    const bool enableValidationLayers = false;  // Release: no overhead
#else
    const bool enableValidationLayers = true;   // Debug: full checking
#endif
```

`NDEBUG` is defined by CMake when you build in Release mode (`-DCMAKE_BUILD_TYPE=Release`). In Debug mode, it's not defined, so validation layers are on.

---

## Checking Layer Availability

Before enabling a layer, check that it's installed:

```cpp
bool checkValidationLayerSupport() {
    // Step 1: Get count
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    // Step 2: Get data
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // Step 3: Print all available layers (helpful for debugging)
    std::cout << "Available validation layers:\n";
    for (const auto& layer : availableLayers) {
        std::cout << "  - " << layer.layerName
                  << " (" << layer.description << ")\n";
    }

    // Step 4: Check each required layer
    for (const char* layerName : validationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            std::cerr << "Validation layer NOT found: " << layerName << "\n";
            return false;
        }
    }

    return true;
}
```

---

## Setting Up the Debug Messenger

Enabling the layer isn't enough — you need a **messenger** to receive the messages:

### Step 1: The Callback Function

This function is called by the validation layer whenever it has something to report:

```cpp
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    // Color-code by severity
    const char* prefix = "";
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            prefix = "[VERBOSE]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            prefix = "[INFO]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            prefix = "[WARNING]";
            break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            prefix = "[ERROR]";
            break;
        default: break;
    }

    std::cerr << prefix << " Validation: " << pCallbackData->pMessage << "\n\n";

    // Return VK_FALSE to NOT abort the Vulkan call that triggered this
    // Return VK_TRUE to abort it (useful for testing)
    return VK_FALSE;
}
```

### Understanding the Callback Parameters

| Parameter | What It Tells You |
|-----------|------------------|
| `messageSeverity` | How bad is it? (verbose → info → warning → error) |
| `messageType` | What category? (general, validation, performance) |
| `pCallbackData` | The actual message, plus object names and labels |
| `pUserData` | Your own data pointer (set during creation) |

### Message Severity Levels

```
VERBOSE  →  Diagnostic information (very chatty)
  │
INFO     →  Informational messages (resource creation, etc.)
  │
WARNING  →  Likely a bug but won't crash immediately
  │
ERROR    →  Invalid usage that WILL cause problems
```

**Tip**: During development, filter at WARNING level or above to reduce noise:

```cpp
if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    std::cerr << "Validation: " << pCallbackData->pMessage << "\n";
}
```

### Message Types

| Type | Description |
|------|-------------|
| `GENERAL` | Something unrelated to the spec or performance |
| `VALIDATION` | You violated the Vulkan specification |
| `PERFORMANCE` | You're doing something non-optimal |

### Step 2: Create the Messenger

The debug messenger is an extension function, so we need to load it manually:

```cpp
VkDebugUtilsMessengerEXT debugMessenger;

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}
```

### Why Load Functions Manually?

`vkCreateDebugUtilsMessengerEXT` is an **extension function** — it's not part of core Vulkan. The Vulkan loader doesn't export it directly; you must ask for it by name using `vkGetInstanceProcAddr`.

### Step 3: Fill in the Create Info

```cpp
void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    // Which severities to receive
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    // Which message types to receive
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT    |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    // Our callback function
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;  // Optional: pass your own data to the callback
}

void setupDebugMessenger() {
    if (!enableValidationLayers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("Failed to set up debug messenger!");
    }

    std::cout << "Debug messenger created!\n";
}
```

---

## Catching Instance Creation/Destruction Errors

There's a chicken-and-egg problem: the debug messenger requires an instance, but we also want to catch errors during instance creation. The solution is to pass a temporary debug create info through the `pNext` chain:

```cpp
void createInstance() {
    if (enableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("Validation layers requested but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    // Get required extensions
    auto extensions = getRequiredExtensions();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Validation layers and debug messenger for instance creation
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();

        // This catches errors during vkCreateInstance and vkDestroyInstance
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create instance!");
    }
}

std::vector<const char*> getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}
```

### What is the `pNext` Chain?

The `pNext` field in every Vulkan struct forms a **linked list of extension structures**:

```
VkInstanceCreateInfo
  ├── sType: INSTANCE_CREATE_INFO
  ├── ... (main fields) ...
  └── pNext ──→ VkDebugUtilsMessengerCreateInfoEXT
                  ├── sType: DEBUG_UTILS_MESSENGER_CREATE_INFO
                  ├── ... (debug fields) ...
                  └── pNext ──→ nullptr (end of chain)
```

This is how Vulkan adds new features without changing existing structures.

---

## Updated Initialization and Cleanup Order

```cpp
void initVulkan() {
    createInstance();       // 1. Instance first
    setupDebugMessenger();  // 2. Debug messenger right after
    // ... (more objects in future chapters)
}

void cleanup() {
    // ... (destroy other objects first, in reverse order)

    if (enableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}
```

---

## Real Validation Messages You'll See

### Example 1: Forgot to Bind a Descriptor Set

```
[ERROR] Validation: VkDescriptorSet 0x0 is not bound for set 0 but pipeline
layout requires it. The Vulkan spec states: Each element of pDescriptorSets
must have been allocated with a VkDescriptorSetLayout that matches...
```

**Translation**: "You told the shader to expect data at binding 0, but you forgot to provide it."

### Example 2: Wrong Image Layout

```
[ERROR] Validation: Image layout transition from UNDEFINED to
SHADER_READ_ONLY_OPTIMAL is not allowed. Use TRANSFER_DST_OPTIMAL
as intermediate layout.
```

**Translation**: "You can't go directly from UNDEFINED to SHADER_READ_ONLY. You need to transition through TRANSFER_DST first."

### Example 3: Performance Warning

```
[WARNING] Validation: vkCreateBuffer: pCreateInfo->usage includes
VK_BUFFER_USAGE_VERTEX_BUFFER_BIT. Performance warning: use
DEVICE_LOCAL_BIT for best GPU performance.
```

**Translation**: "Your vertex buffer is in CPU-visible memory. Move it to GPU memory for better performance."

### Example 4: Synchronization Issue

```
[ERROR] Validation: Submit-time validation: command buffer
VkCommandBuffer 0x55a8f8004f80 used in queue 0x55a8e4005bc0.
The semaphore VkSemaphore 0x25 was not signaled before being waited on.
```

**Translation**: "You told the GPU to wait for a semaphore that nobody is going to signal. Deadlock!"

---

## Debugging Tips

### Tip 1: Use Object Names

Name your Vulkan objects so validation messages are easier to read:

```cpp
void setDebugObjectName(VkDevice device, uint64_t object,
                        VkObjectType type, const char* name)
{
    if (!enableValidationLayers) return;

    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = type;
    nameInfo.objectHandle = object;
    nameInfo.pObjectName = name;

    auto func = (PFN_vkSetDebugUtilsObjectNameEXT)
        vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT");
    if (func) func(device, &nameInfo);
}

// Usage (after creating objects):
setDebugObjectName(device, (uint64_t)vertexBuffer,
                   VK_OBJECT_TYPE_BUFFER, "Player Vertex Buffer");
```

Now validation messages will say "Player Vertex Buffer" instead of "VkBuffer 0x55a8f8004f80".

### Tip 2: Break on Error

For critical debugging, make the callback trigger a breakpoint:

```cpp
if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    std::cerr << "[ERROR] " << pCallbackData->pMessage << "\n";
    #ifdef _MSC_VER
        __debugbreak();  // MSVC
    #else
        __builtin_trap(); // GCC/Clang
    #endif
}
```

---

## Complete Code for This Chapter

```cpp
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <cstdlib>

// ─── Configuration ───

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

// ─── Extension Function Loaders ───

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    return func ? func(instance, pCreateInfo, pAllocator, pDebugMessenger)
                : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func) func(instance, debugMessenger, pAllocator);
}

// ─── Debug Callback ───

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "Validation: " << pCallbackData->pMessage << "\n\n";
    }
    return VK_FALSE;
}

// ─── Application ───

class HelloTriangleApp {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Tutorial", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void createInstance() {
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("Validation layers requested but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        auto extensions = getRequiredExtensions();

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }

        std::cout << "Instance created with "
                  << (enableValidationLayers ? "validation layers ON" : "validation OFF")
                  << "\n";
    }

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
    }

    void setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("Failed to set up debug messenger!");
        }
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            bool found = false;
            for (const auto& props : availableLayers) {
                if (strcmp(layerName, props.layerName) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
        return true;
    }

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        return extensions;
    }
};

int main() {
    HelloTriangleApp app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

---

## Try It Yourself

1. **Run the complete code** and verify you see "validation layers ON".

2. **Trigger a validation error**: After creating the instance, intentionally destroy it twice:
   ```cpp
   vkDestroyInstance(instance, nullptr);
   vkDestroyInstance(instance, nullptr);  // Should trigger a validation error
   ```

3. **Filter severity levels**: Change the callback to only print `ERROR` messages (not `WARNING` or `VERBOSE`).

4. **Add `INFO` severity** to the messenger and observe the extra messages during instance creation.

---

## Key Takeaways

- Vulkan has **zero built-in error checking** — validation layers add it back.
- Use `VK_LAYER_KHRONOS_validation` — it's the one layer that does everything.
- **Always develop with validation layers ON**. Disable only for release builds.
- The debug messenger **callback function** receives all validation messages.
- Use `pNext` chains to extend Vulkan structs with additional data.
- Extension functions must be **loaded manually** via `vkGetInstanceProcAddr`.
- **Name your objects** for clearer validation messages.

---

## What We've Built So Far

```
✅ Instance (VkInstance)
✅ Validation Layers (VkDebugUtilsMessengerEXT)
⬜ Physical Device
⬜ Logical Device
⬜ Surface
⬜ Swap Chain
⬜ Pipeline
⬜ Drawing!
```

---

[<< Previous: Instance](03-instance.md) | [Next: Physical Devices >>](05-physical-devices.md)
