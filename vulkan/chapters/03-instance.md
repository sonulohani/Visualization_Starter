# Chapter 3: Your First Vulkan Instance

[<< Previous: Setup](02-setup.md) | [Next: Validation Layers >>](04-validation-layers.md)

---

## What is a Vulkan Instance?

The `VkInstance` is the **very first Vulkan object** you create. It's the root of everything.

### Analogy

Think of the Vulkan Instance as **opening a phone line to the GPU driver**:

```
Your App ───── VkInstance ───── Vulkan Loader ───── GPU Driver ───── GPU
                   │
                   └── "Hello, I'm VulkanTutorial v1.0,
                        I need Vulkan 1.3 with these extensions..."
```

Without an instance, you can't do anything with Vulkan. It's the "handshake" between your application and the system's Vulkan implementation.

### What the Instance Does

1. **Identifies your application** to the driver (name, version, engine).
2. **Specifies the Vulkan API version** you want to use.
3. **Enables extensions** (extra features beyond core Vulkan).
4. **Enables layers** (middleware like validation — see Chapter 4).
5. **Provides the root** for discovering GPUs (`VkPhysicalDevice`).

---

## The Application Class

We'll use a class structure that we'll build upon throughout the tutorial. This is a common pattern in Vulkan applications:

```cpp
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>

class HelloTriangleApp {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    // ─── Window ───
    GLFWwindow* window = nullptr;
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    // ─── Vulkan Objects ───
    VkInstance instance = VK_NULL_HANDLE;

    void initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Tutorial", nullptr, nullptr);
    }

    void initVulkan() {
        createInstance();
    }

    void createInstance() {
        // We'll fill this in below!
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        vkDestroyInstance(instance, nullptr);
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main() {
    HelloTriangleApp app;
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
```

### Why This Structure?

| Method | Purpose |
|--------|---------|
| `initWindow()` | Creates the OS window via GLFW |
| `initVulkan()` | Creates all Vulkan objects |
| `mainLoop()` | Runs until the window is closed |
| `cleanup()` | Destroys everything in **reverse order** |

The **reverse order cleanup** is important — in Vulkan, you must destroy child objects before their parents. Think of it like building blocks: you built from bottom to top, you destroy from top to bottom.

---

## Creating the Instance — Step by Step

### Step 1: Application Info

The `VkApplicationInfo` struct tells the driver about your application. This is **optional** but strongly recommended:

```cpp
VkApplicationInfo appInfo{};
appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pNext              = nullptr;                    // Extension chain (unused)
appInfo.pApplicationName   = "Hello Triangle";
appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);  // Your app's version
appInfo.pEngineName        = "No Engine";
appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
appInfo.apiVersion         = VK_API_VERSION_1_3;         // Vulkan version
```

#### Understanding Each Field

| Field | What It Does | Example |
|-------|-------------|---------|
| `sType` | Identifies the struct type (required on ALL Vulkan structs) | Always set this first |
| `pNext` | Pointer to extension structures (linked list) | `nullptr` for now |
| `pApplicationName` | Your app's name (driver can use for optimization profiles) | `"Hello Triangle"` |
| `applicationVersion` | Your app's version number | `VK_MAKE_VERSION(1, 0, 0)` |
| `pEngineName` | Engine name (some drivers optimize for known engines) | `"Unreal Engine"` |
| `engineVersion` | Engine version | `VK_MAKE_VERSION(5, 3, 0)` |
| `apiVersion` | **Highest Vulkan version you'll use** | `VK_API_VERSION_1_3` |

#### Why Does `apiVersion` Matter?

```
VK_API_VERSION_1_0  →  Basic Vulkan (2016)
VK_API_VERSION_1_1  →  + Subgroup operations, multiview
VK_API_VERSION_1_2  →  + Timeline semaphores, buffer device address
VK_API_VERSION_1_3  →  + Dynamic rendering, synchronization2
```

If you specify `VK_API_VERSION_1_3` but your GPU only supports 1.1, instance creation will **fail**. Always check support first (or use 1.0 for maximum compatibility).

#### The `sType` Pattern — Deep Dive

You'll see `sType` on almost every Vulkan struct. Here's why it exists:

```cpp
// Without sType, the driver has no way to know what struct it received
void* mysteryPointer = getStruct();  // Could be anything!

// With sType, the driver can check:
VkApplicationInfo* appInfo = (VkApplicationInfo*)mysteryPointer;
if (appInfo->sType == VK_STRUCTURE_TYPE_APPLICATION_INFO) {
    // Safe to use as VkApplicationInfo
}
```

This enables **forward compatibility** — newer Vulkan versions can add new struct types without breaking old drivers.

### Step 2: Gather Required Extensions

Extensions add features beyond core Vulkan. GLFW tells us which extensions are needed for window rendering:

```cpp
// Get extensions required by GLFW for window surfaces
uint32_t glfwExtensionCount = 0;
const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

// Convert to a vector we can modify
std::vector<const char*> requiredExtensions(glfwExtensions,
                                             glfwExtensions + glfwExtensionCount);

// Print what GLFW needs
std::cout << "Required extensions:\n";
for (const auto& ext : requiredExtensions) {
    std::cout << "  - " << ext << "\n";
}
```

Typical output:
```
Required extensions:
  - VK_KHR_surface        ← Generic surface support
  - VK_KHR_xcb_surface    ← Linux-specific (or VK_KHR_win32_surface on Windows)
```

#### Checking Extension Availability

It's good practice to verify all required extensions are available:

```cpp
void checkExtensionSupport(const std::vector<const char*>& required) {
    uint32_t availableCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, nullptr);

    std::vector<VkExtensionProperties> available(availableCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availableCount, available.data());

    std::cout << "\nChecking extension support:\n";
    for (const char* reqExt : required) {
        bool found = false;
        for (const auto& avail : available) {
            if (strcmp(reqExt, avail.extensionName) == 0) {
                found = true;
                break;
            }
        }
        std::cout << "  " << reqExt << ": "
                  << (found ? "SUPPORTED" : "NOT FOUND!") << "\n";
        if (!found) {
            throw std::runtime_error(std::string("Required extension not supported: ") + reqExt);
        }
    }
}
```

### Step 3: Create the Instance

Now we fill in the `VkInstanceCreateInfo` struct and call the creation function:

```cpp
VkInstanceCreateInfo createInfo{};
createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
createInfo.pApplicationInfo        = &appInfo;
createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size());
createInfo.ppEnabledExtensionNames = requiredExtensions.data();
createInfo.enabledLayerCount       = 0;       // No validation layers (Chapter 4)
createInfo.ppEnabledLayerNames     = nullptr;

VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to create Vulkan instance!");
}

std::cout << "Vulkan instance created successfully!\n";
```

---

## Understanding VkResult

Every Vulkan function that can fail returns a `VkResult`. Here are the common values:

| Value | Meaning |
|-------|---------|
| `VK_SUCCESS` | Everything worked! |
| `VK_NOT_READY` | Operation not yet complete |
| `VK_ERROR_OUT_OF_HOST_MEMORY` | Your system ran out of RAM |
| `VK_ERROR_OUT_OF_DEVICE_MEMORY` | GPU ran out of VRAM |
| `VK_ERROR_INITIALIZATION_FAILED` | Vulkan couldn't start up |
| `VK_ERROR_LAYER_NOT_PRESENT` | Requested validation layer doesn't exist |
| `VK_ERROR_EXTENSION_NOT_PRESENT` | Requested extension doesn't exist |
| `VK_ERROR_INCOMPATIBLE_DRIVER` | Driver doesn't support requested Vulkan version |

### Helper Function for Error Checking

A useful helper to use throughout the tutorial:

```cpp
#include <string>

const char* vkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS:                        return "VK_SUCCESS";
        case VK_NOT_READY:                      return "VK_NOT_READY";
        case VK_TIMEOUT:                        return "VK_TIMEOUT";
        case VK_ERROR_OUT_OF_HOST_MEMORY:       return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:     return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:    return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:              return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_LAYER_NOT_PRESENT:        return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:    return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:      return "VK_ERROR_INCOMPATIBLE_DRIVER";
        default:                                return "UNKNOWN_ERROR";
    }
}

#define VK_CHECK(call)                                                     \
    do {                                                                   \
        VkResult result_ = (call);                                         \
        if (result_ != VK_SUCCESS) {                                       \
            throw std::runtime_error(std::string("Vulkan error: ")         \
                + vkResultToString(result_) + "\n  in " + #call            \
                + "\n  at " + __FILE__ + ":" + std::to_string(__LINE__));  \
        }                                                                  \
    } while (0)

// Usage:
VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
// If it fails, throws: "Vulkan error: VK_ERROR_INCOMPATIBLE_DRIVER
//                        in vkCreateInstance(&createInfo, nullptr, &instance)
//                        at src/main.cpp:87"
```

---

## The Complete createInstance() Function

Putting it all together:

```cpp
void createInstance() {
    // ─── Application Info ───
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    // ─── Required Extensions ───
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions,
                                         glfwExtensions + glfwExtensionCount);

    // Print extensions for debugging
    std::cout << "Enabling " << extensions.size() << " instance extensions:\n";
    for (const auto& ext : extensions) {
        std::cout << "  - " << ext << "\n";
    }

    // ─── Create Info ───
    VkInstanceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo        = &appInfo;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount       = 0;

    // ─── Create the Instance ───
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan instance! Error: "
                                  + std::string(vkResultToString(result)));
    }

    std::cout << "Vulkan instance created!\n";
}
```

---

## Understanding the allocator Parameter

Every `vkCreate*` and `vkDestroy*` function has an **allocator** parameter:

```cpp
vkCreateInstance(&createInfo, nullptr, &instance);
//                              ↑
//                        allocator (nullptr = use default)

vkDestroyInstance(instance, nullptr);
//                            ↑
//                      same allocator
```

This lets you provide **custom memory allocation** (e.g., a tracking allocator for debugging memory leaks). For 99% of applications, `nullptr` is fine.

---

## Common Mistakes

### Mistake 1: Forgetting to set `sType`

```cpp
VkApplicationInfo appInfo{};
// WRONG: forgot sType
appInfo.pApplicationName = "My App";

// RIGHT: always set sType first
appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
appInfo.pApplicationName = "My App";
```

Using `{}` initialization zeroes everything, which often "works" but is technically wrong — the driver relies on `sType` to know what it's reading.

### Mistake 2: Using a higher API version than the driver supports

```cpp
appInfo.apiVersion = VK_API_VERSION_1_3;  // GPU only supports 1.1 → FAIL
```

**Solution**: Query the supported version first:
```cpp
uint32_t apiVersion = 0;
vkEnumerateInstanceVersion(&apiVersion);
uint32_t major = VK_API_VERSION_MAJOR(apiVersion);
uint32_t minor = VK_API_VERSION_MINOR(apiVersion);
std::cout << "Supported Vulkan: " << major << "." << minor << "\n";
```

### Mistake 3: Destroying the instance before other Vulkan objects

```cpp
// WRONG ORDER:
vkDestroyInstance(instance, nullptr);
vkDestroyDevice(device, nullptr);       // Device belongs to instance — already destroyed!

// RIGHT ORDER:
vkDestroyDevice(device, nullptr);       // Children first
vkDestroyInstance(instance, nullptr);   // Parent last
```

---

## The Vulkan Object Lifecycle

Every Vulkan object follows the same pattern:

```
┌─────────────────────────────────────────┐
│         Vulkan Object Lifecycle          │
│                                          │
│  1. Fill a CreateInfo struct             │
│       ↓                                  │
│  2. Call vkCreate*() or vkAllocate*()    │
│       ↓                                  │
│  3. Use the object                       │
│       ↓                                  │
│  4. Call vkDestroy*() or vkFree*()       │
│                                          │
│  Rule: Destroy in REVERSE creation order │
└─────────────────────────────────────────┘
```

We'll see this pattern for every object: devices, swap chains, pipelines, buffers, images, and more.

---

## Try It Yourself

1. **Build and run the code above**. Verify the instance is created successfully.

2. **Print the Vulkan version** your system supports:
   ```cpp
   uint32_t version;
   vkEnumerateInstanceVersion(&version);
   std::cout << "Vulkan " << VK_API_VERSION_MAJOR(version)
             << "." << VK_API_VERSION_MINOR(version)
             << "." << VK_API_VERSION_PATCH(version) << "\n";
   ```

3. **Intentionally cause an error**: Change `apiVersion` to `VK_MAKE_VERSION(99, 0, 0)` and see what error you get.

4. **Request a non-existent extension**: Add `"VK_FAKE_extension"` to the extensions list and see the error.

5. **Count how many extensions** your system supports. Compare with a friend's system if possible.

---

## Key Takeaways

- `VkInstance` is the **first Vulkan object** you create — everything depends on it.
- `VkApplicationInfo` tells the driver about your app (optional but recommended).
- **Extensions** add features beyond core Vulkan; GLFW requires specific ones.
- Every Vulkan struct needs `sType` set correctly.
- The **two-call enumeration pattern** (count → allocate → fill) is used everywhere.
- Always check `VkResult` for errors.
- **Destroy objects in reverse creation order**.

---

## What We've Built So Far

```
✅ Instance (VkInstance)
⬜ Validation Layers
⬜ Physical Device
⬜ Logical Device
⬜ Surface
⬜ Swap Chain
⬜ Pipeline
⬜ Drawing!
```

Next up: We'll add **validation layers** to catch our mistakes early.

---

[<< Previous: Setup](02-setup.md) | [Next: Validation Layers >>](04-validation-layers.md)
