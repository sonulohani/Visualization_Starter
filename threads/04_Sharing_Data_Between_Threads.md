# Chapter 4: Sharing Data Between Threads

## The Problem: Shared Mutable State

When multiple threads access the **same data** and at least one thread **writes** to it, you have a **data race** — one of the most dangerous bugs in concurrent programming.

```cpp
#include <iostream>
#include <thread>

int counter = 0;  // Shared data

void increment() {
    for (int i = 0; i < 100000; i++) {
        counter++;  // ❌ NOT thread-safe!
    }
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);

    t1.join();
    t2.join();

    // Expected: 200000
    // Actual: Some random number less than 200000!
    std::cout << "Counter: " << counter << std::endl;

    return 0;
}
```

**Sample outputs from multiple runs:**
```
Counter: 143267
Counter: 167892
Counter: 152341
```

### Why Does This Happen?

`counter++` is **not** a single operation. It's actually three steps:

```
1. READ  counter from memory → register  (register = 5)
2. ADD   1 to register                    (register = 6)
3. WRITE register back to memory          (counter = 6)
```

When two threads do this at the same time:

```
Thread 1                    Thread 2
────────                    ────────
READ counter (= 5)
                            READ counter (= 5)    ← Both read 5!
ADD 1 (register = 6)
                            ADD 1 (register = 6)
WRITE 6 to counter
                            WRITE 6 to counter     ← Should be 7, but writes 6!

Result: counter = 6 (should be 7) — one increment is LOST!
```

This is called a **race condition**: the result depends on the unpredictable timing of thread execution.

---

## What is a Critical Section?

A **critical section** is any block of code that accesses shared data. To avoid race conditions, only **one thread** should execute a critical section at a time.

```
Thread 1                    Thread 2
────────                    ────────
[Enter critical section]
  READ counter (= 5)
  ADD 1 (= 6)              [BLOCKED — waiting]
  WRITE 6                  [BLOCKED — waiting]
[Exit critical section]
                            [Enter critical section]
                              READ counter (= 6)
                              ADD 1 (= 7)
                              WRITE 7
                            [Exit critical section]

Result: counter = 7 ✅ CORRECT!
```

---

## Types of Shared Data Issues

### 1. Data Race
Two or more threads access the same memory location concurrently, and at least one writes to it, without synchronization.

### 2. Race Condition
A broader concept: the correctness of the program depends on the relative timing of events. Data races are a subset.

```cpp
// Race condition example: check-then-act
if (!queue.empty()) {        // Thread 1 checks
    // Another thread might remove the last element HERE
    auto item = queue.front(); // ❌ Queue might be empty now!
    queue.pop();
}
```

### 3. Torn Read/Write
Reading or writing a value that's larger than the CPU's word size can be interrupted halfway.

```cpp
// On a 32-bit system, a 64-bit write can be "torn"
uint64_t sharedValue = 0;  // 64-bit

// Thread 1: writes 0x0000000100000002
// Thread 2: reads — might see 0x0000000100000000 (half old, half new)
```

---

## Ways to Protect Shared Data

C++ provides several tools to protect shared data:

| Mechanism | Header | Use Case |
|-----------|--------|----------|
| `std::mutex` | `<mutex>` | Basic mutual exclusion |
| `std::lock_guard` | `<mutex>` | RAII mutex locking |
| `std::unique_lock` | `<mutex>` | Flexible mutex locking |
| `std::scoped_lock` | `<mutex>` | Lock multiple mutexes (C++17) |
| `std::shared_mutex` | `<shared_mutex>` | Reader-writer lock (C++17) |
| `std::atomic` | `<atomic>` | Lock-free atomic operations |
| `std::condition_variable` | `<condition_variable>` | Thread signaling |

We'll cover mutexes and locks in the next chapter. For now, here's a quick preview of the fix:

```cpp
#include <iostream>
#include <thread>
#include <mutex>

int counter = 0;
std::mutex mtx;  // Protects counter

void safeIncrement() {
    for (int i = 0; i < 100000; i++) {
        std::lock_guard<std::mutex> lock(mtx);  // Lock
        counter++;                                // Safe!
    }   // Automatically unlocked here
}

int main() {
    std::thread t1(safeIncrement);
    std::thread t2(safeIncrement);

    t1.join();
    t2.join();

    std::cout << "Counter: " << counter << std::endl;  // Always 200000 ✅
    return 0;
}
```

---

## Thread-Local Storage

Sometimes, each thread needs its **own copy** of a variable. Use `thread_local`:

```cpp
#include <iostream>
#include <thread>

thread_local int threadLocalCounter = 0;  // Each thread gets its own copy

void increment() {
    for (int i = 0; i < 5; i++) {
        threadLocalCounter++;
    }
    std::cout << "Thread " << std::this_thread::get_id()
              << " counter = " << threadLocalCounter << std::endl;
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);

    t1.join();
    t2.join();

    // Main thread's copy is still 0 (untouched)
    std::cout << "Main counter = " << threadLocalCounter << std::endl;

    return 0;
}
```

**Output:**
```
Thread 140234567 counter = 5
Thread 140234568 counter = 5
Main counter = 0
```

Each thread has its own independent copy — no sharing, no races!

### When to Use `thread_local`
- Per-thread caches
- Thread-specific error codes
- Random number generators (each thread gets its own seed)
- Avoiding synchronization when sharing isn't needed

---

## Immutable Shared Data

The simplest way to avoid data races: **don't modify shared data**.

```cpp
#include <iostream>
#include <thread>
#include <string>

void printConfig(const std::string& config) {
    // Only READING — no synchronization needed
    std::cout << "Config: " << config << std::endl;
}

int main() {
    const std::string config = "debug=true;timeout=30";  // Immutable

    std::thread t1(printConfig, std::cref(config));
    std::thread t2(printConfig, std::cref(config));

    t1.join();
    t2.join();

    return 0;
}
```

> **Rule of thumb:** If data is only read (never written) after thread creation, no synchronization is needed.

---

## Stack vs Heap Data

### Stack Data (Local Variables) — Usually Safe
Each thread has its own stack, so local variables are inherently thread-safe.

```cpp
void safeFunction() {
    int localVar = 42;  // Each thread gets its own copy
    localVar++;          // No race — it's on this thread's stack
}
```

### Heap/Global Data — Needs Protection
```cpp
int globalVar = 0;          // ❌ Shared — needs protection
static int staticVar = 0;   // ❌ Shared — needs protection

void allocateAndShare() {
    int* heapVar = new int(0);
    // If heapVar's pointer is shared with another thread → needs protection
}
```

---

## Summary: When Do You Need Synchronization?

| Scenario | Synchronization Needed? |
|----------|------------------------|
| Multiple threads reading shared data | ❌ No |
| One thread writes, others read | ✅ Yes |
| Multiple threads write shared data | ✅ Yes |
| Each thread uses its own data | ❌ No |
| Using `thread_local` variables | ❌ No |
| Using `const` shared data | ❌ No |
| Shared container (e.g., `std::vector`) | ✅ Yes (even for reads while another writes) |

---

**Previous:** [Chapter 3 — Thread Lifecycle](03_Thread_Lifecycle.md)
**Next:** [Chapter 5 — Mutexes and Locks](05_Mutexes_and_Locks.md)
