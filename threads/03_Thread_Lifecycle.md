# Chapter 3: Thread Lifecycle — join, detach, and RAII

## The Thread Lifecycle

When you create a `std::thread`, you **must** do one of two things before the thread object is destroyed:

1. **`join()`** — Wait for the thread to finish
2. **`detach()`** — Let the thread run independently

If you do neither, the program **terminates** (calls `std::terminate()`).

```
Thread Created
      │
      ├──── join()   → Main thread WAITS until worker finishes → Thread object is "joined"
      │
      └──── detach() → Thread runs independently in background → Thread object is "detached"

If NEITHER is called before destruction → std::terminate() → Program CRASHES
```

---

## `join()` — Wait for Completion

`join()` blocks the calling thread until the thread finishes execution.

```cpp
#include <iostream>
#include <thread>
#include <chrono>

void longTask() {
    std::cout << "Task started..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Task finished!" << std::endl;
}

int main() {
    std::thread t(longTask);

    std::cout << "Waiting for task..." << std::endl;
    t.join();  // Main thread blocks here for 2 seconds
    std::cout << "Task is done, continuing main." << std::endl;

    return 0;
}
```

**Output:**
```
Waiting for task...
Task started...
Task finished!
Task is done, continuing main.
```

### Rules for `join()`
- Can only be called **once** per thread.
- After `join()`, the thread is no longer "joinable".
- Calling `join()` on a non-joinable thread throws `std::system_error`.

```cpp
std::thread t(someFunction);
t.join();

// t.join();  // ❌ CRASH! Already joined.

if (t.joinable()) {
    t.join();  // ✅ Safe: check first
}
```

---

## `detach()` — Fire and Forget

`detach()` separates the thread from the `std::thread` object. The thread continues running independently in the background.

```cpp
#include <iostream>
#include <thread>
#include <chrono>

void backgroundTask() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Background task done!" << std::endl;
}

int main() {
    std::thread t(backgroundTask);
    t.detach();  // Thread runs in background

    std::cout << "Main thread continues immediately." << std::endl;

    // ⚠️ If main() exits before backgroundTask finishes,
    // the output from backgroundTask may never appear!
    std::this_thread::sleep_for(std::chrono::seconds(2));

    return 0;
}
```

### When to Use `detach()`
- Background logging
- Daemon-like tasks
- When you truly don't care when or if it finishes

### Dangers of `detach()`
```cpp
void dangerous() {
    int localVar = 42;

    std::thread t([&localVar]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        // ❌ UNDEFINED BEHAVIOR: localVar is destroyed when dangerous() returns!
        std::cout << localVar << std::endl;
    });

    t.detach();
    // dangerous() returns → localVar is destroyed
    // The detached thread is now accessing destroyed memory!
}
```

> **Rule:** Never let a detached thread reference local variables of the creating function. Copy data into the thread instead.

---

## `joinable()` — Check Thread State

```cpp
#include <iostream>
#include <thread>

int main() {
    std::thread t([]() {
        std::cout << "Running" << std::endl;
    });

    std::cout << "Before join - joinable: " << t.joinable() << std::endl;  // 1 (true)

    t.join();

    std::cout << "After join - joinable: " << t.joinable() << std::endl;   // 0 (false)

    // Default-constructed thread
    std::thread t2;
    std::cout << "Empty thread - joinable: " << t2.joinable() << std::endl; // 0 (false)

    return 0;
}
```

A thread is **joinable** if:
- It was constructed with a callable (not default-constructed)
- It has not been `join()`ed or `detach()`ed yet
- It has not been moved from

---

## The Problem: Exceptions and Thread Safety

What if an exception happens between thread creation and `join()`?

```cpp
void riskyFunction() {
    std::thread t([]() {
        // do work
    });

    doSomethingThatMightThrow();  // 💥 If this throws...

    t.join();  // ❌ This line is NEVER reached!
    // Thread destructor fires → std::terminate()!
}
```

### Fix 1: Try-Catch

```cpp
void fixWithTryCatch() {
    std::thread t([]() { /* work */ });

    try {
        doSomethingThatMightThrow();
    } catch (...) {
        t.join();  // Join even if exception occurs
        throw;     // Re-throw the exception
    }

    t.join();
}
```

This works but is **ugly and error-prone**. There's a better way.

---

## RAII Thread Guard

**RAII** (Resource Acquisition Is Initialization) is a C++ pattern where resources are acquired in the constructor and released in the destructor. We can use it to guarantee `join()` is called.

### Custom Thread Guard

```cpp
#include <iostream>
#include <thread>

class ThreadGuard {
    std::thread& t;
public:
    explicit ThreadGuard(std::thread& t) : t(t) {}

    ~ThreadGuard() {
        if (t.joinable()) {
            t.join();  // Always join on destruction
        }
    }

    // Non-copyable
    ThreadGuard(const ThreadGuard&) = delete;
    ThreadGuard& operator=(const ThreadGuard&) = delete;
};

void safeFunction() {
    std::thread t([]() {
        std::cout << "Working..." << std::endl;
    });

    ThreadGuard guard(t);  // Will auto-join when guard goes out of scope

    doSomethingThatMightThrow();  // Even if this throws, t gets joined!

    // guard destructor calls t.join() here
}
```

### Owning Thread Guard (Scoped Thread)

A version that **owns** the thread instead of just guarding it:

```cpp
class ScopedThread {
    std::thread t;
public:
    explicit ScopedThread(std::thread t) : t(std::move(t)) {
        if (!this->t.joinable()) {
            throw std::logic_error("No thread to guard");
        }
    }

    ~ScopedThread() {
        t.join();
    }

    // Non-copyable, non-movable
    ScopedThread(const ScopedThread&) = delete;
    ScopedThread& operator=(const ScopedThread&) = delete;
};

int main() {
    ScopedThread st(std::thread([]() {
        std::cout << "Safely managed thread" << std::endl;
    }));
    // Thread automatically joined when st is destroyed
    return 0;
}
```

---

## C++20: `std::jthread` — The Safe Thread

C++20 introduced `std::jthread` which:
1. **Automatically joins** in its destructor (RAII built-in)
2. Supports **cooperative cancellation** via `std::stop_token`

```cpp
#include <iostream>
#include <thread>

int main() {
    {
        std::jthread t([]() {
            std::cout << "Working in jthread..." << std::endl;
        });

        // No need to call join()!
        // When t goes out of scope, it automatically joins.
    }
    // Thread is already joined here.

    std::cout << "Done!" << std::endl;
    return 0;
}
```

### `std::jthread` with Stop Token

```cpp
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::jthread worker([](std::stop_token stopToken) {
        int counter = 0;
        while (!stopToken.stop_requested()) {
            std::cout << "Working... " << counter++ << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        std::cout << "Stop requested! Cleaning up..." << std::endl;
    });

    // Let it run for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Request the thread to stop
    worker.request_stop();

    // jthread destructor automatically joins
    return 0;
}
```

**Output:**
```
Working... 0
Working... 1
Working... 2
Working... 3
Stop requested! Cleaning up...
```

### Stop Callback

You can register callbacks that fire when a stop is requested:

```cpp
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    std::jthread worker([](std::stop_token stopToken) {
        // Register a callback
        std::stop_callback callback(stopToken, []() {
            std::cout << "Stop callback triggered!" << std::endl;
        });

        while (!stopToken.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));
    worker.request_stop();  // Triggers the callback

    return 0;
}
```

---

## Thread Sleep and Yield

### Sleeping

```cpp
#include <thread>
#include <chrono>

void example() {
    // Sleep for a duration
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::this_thread::sleep_for(std::chrono::microseconds(100));

    // Sleep until a specific time point
    auto wakeTime = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    std::this_thread::sleep_until(wakeTime);
}
```

### Yielding

`yield()` tells the OS "I don't need the CPU right now, give it to someone else."

```cpp
void busyWait(bool& ready) {
    while (!ready) {
        std::this_thread::yield();  // Don't hog the CPU
    }
}
```

> Use `yield()` in spin-loops to reduce CPU waste, but prefer condition variables or futures for real synchronization.

---

## Summary Table

| Feature | `std::thread` | `std::jthread` (C++20) |
|---------|--------------|----------------------|
| Auto-join | ❌ No | ✅ Yes |
| Cancellation | ❌ Manual | ✅ `stop_token` |
| Must call join/detach | ✅ Yes | ❌ No |
| Move-only | ✅ Yes | ✅ Yes |
| Exception safety | Manual RAII needed | Built-in |

---

**Previous:** [Chapter 2 — Creating and Managing Threads](02_Creating_and_Managing_Threads.md)
**Next:** [Chapter 4 — Sharing Data Between Threads](04_Sharing_Data_Between_Threads.md)
