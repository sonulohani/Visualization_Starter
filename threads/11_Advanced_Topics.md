# Chapter 11: Advanced Topics

## C++20 Synchronization Primitives

C++20 introduced several new synchronization tools that make concurrent programming easier.

---

## 1. `std::latch` — One-Time Barrier

A `latch` is a single-use countdown barrier. Threads call `count_down()` and one or more threads can `wait()` until the count reaches zero.

Think of it as: "Wait until N things are done."

```cpp
#include <iostream>
#include <thread>
#include <latch>
#include <vector>

int main() {
    const int NUM_WORKERS = 4;
    std::latch startSignal(1);          // Workers wait for this
    std::latch workDone(NUM_WORKERS);   // Main waits for this

    auto worker = [&](int id) {
        // Wait for the start signal
        startSignal.wait();

        std::cout << "Worker " << id << " started" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100 * id));
        std::cout << "Worker " << id << " finished" << std::endl;

        workDone.count_down();  // Signal one unit of work is done
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_WORKERS; i++) {
        threads.emplace_back(worker, i);
    }

    std::cout << "Preparing... GO!" << std::endl;
    startSignal.count_down();   // Release all workers at once

    workDone.wait();            // Wait for all workers to finish
    std::cout << "All workers done!" << std::endl;

    for (auto& t : threads) t.join();
    return 0;
}
```

**Output:**
```
Preparing... GO!
Worker 0 started
Worker 1 started
Worker 2 started
Worker 3 started
Worker 0 finished
Worker 1 finished
Worker 2 finished
Worker 3 finished
All workers done!
```

### Key Points
- `count_down(n)` decrements the counter by `n` (default 1)
- `wait()` blocks until the counter reaches 0
- `arrive_and_wait()` = `count_down()` + `wait()` in one call
- **Single-use** — cannot be reset

---

## 2. `std::barrier` — Reusable Barrier

A `barrier` is like a `latch` but can be **reused**. All threads synchronize at the barrier, then proceed together. Useful for iterative algorithms where threads must sync at each step.

```cpp
#include <iostream>
#include <thread>
#include <barrier>
#include <vector>

int main() {
    const int NUM_THREADS = 3;
    const int NUM_PHASES = 3;

    // Completion function runs when all threads arrive
    auto onComplete = []() noexcept {
        std::cout << "--- All threads synced! Moving to next phase ---" << std::endl;
    };

    std::barrier syncPoint(NUM_THREADS, onComplete);

    auto worker = [&](int id) {
        for (int phase = 0; phase < NUM_PHASES; phase++) {
            std::cout << "Thread " << id << " working on phase " << phase << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50 * (id + 1)));

            syncPoint.arrive_and_wait();  // Wait for all threads
            // All threads proceed together to next phase
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) t.join();
    return 0;
}
```

**Output:**
```
Thread 0 working on phase 0
Thread 1 working on phase 0
Thread 2 working on phase 0
--- All threads synced! Moving to next phase ---
Thread 0 working on phase 1
Thread 1 working on phase 1
Thread 2 working on phase 1
--- All threads synced! Moving to next phase ---
Thread 0 working on phase 2
Thread 1 working on phase 2
Thread 2 working on phase 2
--- All threads synced! Moving to next phase ---
```

---

## 3. `std::counting_semaphore` and `std::binary_semaphore`

A **semaphore** controls access to a limited number of resources. Unlike a mutex (which allows 1), a semaphore can allow N concurrent accesses.

### `std::counting_semaphore`

```cpp
#include <iostream>
#include <thread>
#include <semaphore>
#include <vector>

// Allow max 3 concurrent accesses
std::counting_semaphore<3> semaphore(3);

void accessResource(int id) {
    semaphore.acquire();  // Decrement count (blocks if 0)

    std::cout << "Thread " << id << " accessing resource" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Thread " << id << " releasing resource" << std::endl;

    semaphore.release();  // Increment count
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; i++) {
        threads.emplace_back(accessResource, i);
    }
    for (auto& t : threads) t.join();
    return 0;
}
```

Only 3 threads can be in the critical section at once. Others wait.

### `std::binary_semaphore`

A semaphore with max count of 1 — similar to a mutex but can be released from a **different thread**.

```cpp
#include <iostream>
#include <thread>
#include <semaphore>

std::binary_semaphore smphSignal(0);  // Starts empty (blocked)

void producer() {
    std::cout << "Producing data..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    smphSignal.release();  // Signal the consumer
}

void consumer() {
    smphSignal.acquire();  // Wait for signal
    std::cout << "Data received!" << std::endl;
}

int main() {
    std::thread t1(consumer);
    std::thread t2(producer);
    t1.join();
    t2.join();
    return 0;
}
```

### Semaphore vs Mutex

| Feature | Mutex | Semaphore |
|---------|-------|-----------|
| Max concurrent | 1 | N (configurable) |
| Must unlock from same thread | ✅ Yes | ❌ No |
| Ownership concept | ✅ Yes | ❌ No |
| Use case | Protect critical section | Limit resource access |

---

## 4. `std::atomic_ref` (C++20)

Apply atomic operations to **non-atomic** variables:

```cpp
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

int main() {
    int counter = 0;  // Regular (non-atomic) int

    auto worker = [&counter]() {
        std::atomic_ref<int> atomicCounter(counter);
        for (int i = 0; i < 100000; i++) {
            atomicCounter++;  // Atomic operation on non-atomic variable!
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) t.join();

    std::cout << "Counter: " << counter << std::endl;  // 400000 ✅
    return 0;
}
```

> Useful when you can't change a variable's type to atomic but need atomic access.

---

## 5. `std::atomic::wait()` and `notify()` (C++20)

Atomic variables can now act like lightweight condition variables:

```cpp
#include <iostream>
#include <thread>
#include <atomic>

std::atomic<int> value(0);

void producer() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    value.store(42);
    value.notify_one();  // Wake up the waiter
}

void consumer() {
    value.wait(0);  // Block while value == 0
    std::cout << "Got value: " << value.load() << std::endl;  // 42
}

int main() {
    std::thread t1(consumer);
    std::thread t2(producer);
    t1.join();
    t2.join();
    return 0;
}
```

---

## 6. Lock-Free Data Structures

Lock-free algorithms use atomic operations (especially CAS) instead of mutexes.

### Lock-Free Stack (Treiber Stack)

```cpp
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

template<typename T>
class LockFreeStack {
    struct Node {
        T data;
        Node* next;
        Node(const T& data) : data(data), next(nullptr) {}
    };

    std::atomic<Node*> head{nullptr};

public:
    void push(const T& value) {
        Node* newNode = new Node(value);
        newNode->next = head.load();
        // CAS loop: keep trying until we successfully update head
        while (!head.compare_exchange_weak(newNode->next, newNode)) {
            // newNode->next is updated to current head automatically
        }
    }

    bool pop(T& result) {
        Node* oldHead = head.load();
        while (oldHead &&
               !head.compare_exchange_weak(oldHead, oldHead->next)) {
            // oldHead is updated to current head automatically
        }
        if (oldHead) {
            result = oldHead->data;
            delete oldHead;
            return true;
        }
        return false;
    }

    ~LockFreeStack() {
        T dummy;
        while (pop(dummy)) {}
    }
};

int main() {
    LockFreeStack<int> stack;

    // Push from multiple threads
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&stack, i]() {
            for (int j = 0; j < 1000; j++) {
                stack.push(i * 1000 + j);
            }
        });
    }
    for (auto& t : threads) t.join();

    // Pop all elements
    int count = 0;
    int val;
    while (stack.pop(val)) count++;
    std::cout << "Popped " << count << " elements" << std::endl;  // 4000

    return 0;
}
```

---

## 7. Thread-Safe Singleton Patterns

### Meyers' Singleton (Recommended)

```cpp
class Singleton {
public:
    static Singleton& getInstance() {
        static Singleton instance;  // Thread-safe since C++11
        return instance;
    }

    void doSomething() {
        std::cout << "Singleton working" << std::endl;
    }

    // Delete copy/move
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;

private:
    Singleton() {
        std::cout << "Singleton created" << std::endl;
    }
};
```

---

## 8. Read-Copy-Update (RCU) Pattern

A pattern for read-heavy workloads where writers create new versions of data and readers access the old version until they're done:

```cpp
#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>

struct Config {
    std::string server;
    int port;
    bool debug;
};

std::atomic<std::shared_ptr<const Config>> globalConfig;

void reader(int id) {
    for (int i = 0; i < 5; i++) {
        auto config = globalConfig.load();
        std::cout << "Reader " << id << ": server=" << config->server
                  << " port=" << config->port << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void writer() {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));

    // Create a new config (readers still see the old one)
    auto newConfig = std::make_shared<const Config>(
        Config{"new-server.com", 8080, true}
    );
    globalConfig.store(newConfig);  // Atomically swap
    std::cout << "Writer: config updated!" << std::endl;
}

int main() {
    globalConfig.store(std::make_shared<const Config>(
        Config{"old-server.com", 80, false}
    ));

    std::vector<std::thread> threads;
    threads.emplace_back(writer);
    for (int i = 0; i < 3; i++) {
        threads.emplace_back(reader, i);
    }
    for (auto& t : threads) t.join();

    return 0;
}
```

---

## 9. Coroutines and Threads (C++20)

C++20 coroutines can work with threads for cooperative multitasking. A simplified conceptual example:

```cpp
#include <iostream>
#include <coroutine>
#include <thread>

// Simplified awaitable that resumes on a new thread
struct ResumeOnNewThread {
    bool await_ready() { return false; }

    void await_suspend(std::coroutine_handle<> handle) {
        std::thread([handle]() {
            handle.resume();
        }).detach();
    }

    void await_resume() {}
};

// Note: Full coroutine support requires a task/promise type (simplified here)
// This shows the concept — real-world use requires a coroutine library
// like cppcoro or folly::coro
```

> Coroutines are a deep topic on their own. The key insight is that they can **suspend** and **resume** execution without blocking a thread.

---

## 10. Memory Fences

`std::atomic_thread_fence` provides ordering guarantees without an associated atomic variable:

```cpp
#include <atomic>
#include <thread>

int data = 0;
std::atomic<bool> ready(false);

void producer() {
    data = 42;
    std::atomic_thread_fence(std::memory_order_release);  // Fence
    ready.store(true, std::memory_order_relaxed);
}

void consumer() {
    while (!ready.load(std::memory_order_relaxed)) {}
    std::atomic_thread_fence(std::memory_order_acquire);  // Fence
    assert(data == 42);  // Guaranteed
}
```

> Fences are a low-level tool. Prefer atomic operations with proper memory ordering.

---

## Summary of C++20 Additions

| Feature | Purpose | Header |
|---------|---------|--------|
| `std::jthread` | Auto-joining thread with stop token | `<thread>` |
| `std::latch` | Single-use countdown barrier | `<latch>` |
| `std::barrier` | Reusable multi-phase barrier | `<barrier>` |
| `std::counting_semaphore` | Limit concurrent access to N | `<semaphore>` |
| `std::binary_semaphore` | Binary semaphore (like mutex, but cross-thread) | `<semaphore>` |
| `std::atomic_ref` | Atomic operations on non-atomic vars | `<atomic>` |
| `atomic::wait/notify` | Lightweight condition-variable on atomics | `<atomic>` |
| `std::stop_token` | Cooperative cancellation | `<stop_token>` |

---

**Previous:** [Chapter 10 — Deadlocks and Common Problems](10_Deadlocks_and_Common_Problems.md)
**Next:** [Chapter 12 — Best Practices and Patterns](12_Best_Practices_and_Patterns.md)
