# Chapter 1: Introduction to Threads

## What is a Thread?

A **thread** is the smallest unit of execution within a process. Think of it like this:

- A **process** is a running program (like opening Chrome).
- A **thread** is a single task happening inside that program (like loading one tab).

A process can have **multiple threads**, all sharing the same memory space but executing different tasks simultaneously.

```
┌─────────────────────────────────────────┐
│              PROCESS                     │
│                                         │
│  ┌──────────┐  ┌──────────┐  ┌────────┐│
│  │ Thread 1 │  │ Thread 2 │  │Thread 3││
│  │ (Main)   │  │ (Worker) │  │(Worker)││
│  └──────────┘  └──────────┘  └────────┘│
│                                         │
│         Shared Memory Space             │
└─────────────────────────────────────────┘
```

## Why Do We Need Threads?

### 1. Parallelism
Run multiple tasks at the **same time** on multi-core CPUs.

```
Without Threads:
  Task A ████████████████████████████████  → 8 seconds
  Task B                                  ████████████████████████████████  → 8 seconds
  Total: 16 seconds

With Threads:
  Task A ████████████████████████████████  → 8 seconds
  Task B ████████████████████████████████  → 8 seconds (runs simultaneously)
  Total: ~8 seconds
```

### 2. Responsiveness
Keep the UI responsive while background work happens (e.g., downloading a file while the user can still click buttons).

### 3. Resource Sharing
Threads share the same memory, making communication between them faster than between separate processes.

### 4. Efficient I/O
While one thread waits for disk/network I/O, other threads can keep working.

## Process vs Thread

| Feature | Process | Thread |
|---------|---------|--------|
| Memory | Separate memory space | Shared memory space |
| Creation | Heavyweight (slow) | Lightweight (fast) |
| Communication | IPC (pipes, sockets) | Shared variables |
| Crash Impact | One crash doesn't affect others | One crash can kill all threads |
| Overhead | High | Low |

## Concurrency vs Parallelism

These terms are often confused but mean different things:

### Concurrency
Multiple tasks **making progress** over time, possibly by switching between them rapidly.

```
Single Core - Concurrency (time-slicing):

CPU:  [Thread A][Thread B][Thread A][Thread B][Thread A]
       ├────────┼────────┼────────┼────────┼────────┤
       0ms     10ms     20ms     30ms     40ms     50ms

Both threads make progress, but only one runs at any instant.
```

### Parallelism
Multiple tasks **literally running at the same time** on different CPU cores.

```
Multi Core - Parallelism:

Core 1: [Thread A][Thread A][Thread A][Thread A][Thread A]
Core 2: [Thread B][Thread B][Thread B][Thread B][Thread B]
         ├────────┼────────┼────────┼────────┼────────┤
         0ms     10ms     20ms     30ms     40ms     50ms

Both threads truly run simultaneously.
```

## Threads in C++

C++ introduced native threading support in **C++11** with the `<thread>` header. Before that, developers had to use platform-specific APIs like:
- **POSIX Threads (pthreads)** on Linux/macOS
- **Windows Threads (Win32 API)** on Windows

### The C++ Threading Library Includes:

| Header | Purpose |
|--------|---------|
| `<thread>` | Creating and managing threads |
| `<mutex>` | Mutual exclusion (locks) |
| `<condition_variable>` | Thread synchronization |
| `<atomic>` | Lock-free atomic operations |
| `<future>` | Asynchronous results |
| `<shared_mutex>` | Reader-writer locks (C++17) |
| `<semaphore>` | Counting/binary semaphores (C++20) |
| `<latch>` | Single-use thread barrier (C++20) |
| `<barrier>` | Reusable thread barrier (C++20) |

## Your First Thread Program

```cpp
#include <iostream>
#include <thread>

// This function will run in a separate thread
void sayHello() {
    std::cout << "Hello from a thread!" << std::endl;
}

int main() {
    // Create a thread that runs sayHello()
    std::thread t(sayHello);

    // Wait for the thread to finish
    t.join();

    std::cout << "Back in main!" << std::endl;
    return 0;
}
```

**Output:**
```
Hello from a thread!
Back in main!
```

### What Just Happened?

1. `std::thread t(sayHello);` — Creates a new thread and starts executing `sayHello()` immediately.
2. `t.join();` — The main thread **waits** for thread `t` to finish before continuing.
3. After `join()` returns, we know `sayHello()` is done.

## Compiling Threaded Programs

```bash
# Linux/macOS with g++
g++ -std=c++17 -pthread program.cpp -o program

# Linux/macOS with clang++
clang++ -std=c++17 -pthread program.cpp -o program

# Windows with MSVC (threads are linked automatically)
cl /std:c++17 program.cpp
```

The `-pthread` flag links the POSIX thread library on Unix-like systems.

## Key Terminology

| Term | Meaning |
|------|---------|
| **Thread** | Smallest unit of execution in a process |
| **Main Thread** | The thread that starts when `main()` runs |
| **Spawn** | Creating a new thread |
| **Join** | Waiting for a thread to finish |
| **Detach** | Letting a thread run independently |
| **Race Condition** | Bug caused by unsynchronized shared data access |
| **Deadlock** | Two or more threads stuck waiting for each other forever |
| **Mutex** | Lock that ensures only one thread accesses a resource at a time |
| **Critical Section** | Code block that accesses shared data (needs protection) |
| **Thread-safe** | Code that works correctly when called from multiple threads |

---

**Next Chapter:** [Chapter 2 — Creating and Managing Threads](02_Creating_and_Managing_Threads.md)
