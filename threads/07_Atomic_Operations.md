# Chapter 7: Atomic Operations

## What are Atomic Operations?

An **atomic operation** is an operation that completes in a **single, indivisible step**. No other thread can see the operation "half done."

Think of it like a light switch — it's either ON or OFF, never stuck halfway.

```
Regular increment (3 steps — can be interrupted):
  Thread: READ → ADD → WRITE
                  ↑
          Another thread can sneak in here!

Atomic increment (1 step — cannot be interrupted):
  Thread: [READ+ADD+WRITE]  ← All at once, no interruption possible
```

Atomics are **faster than mutexes** because they use special CPU instructions (like `lock cmpxchg` on x86) instead of OS-level locks.

---

## `std::atomic<T>` — The Basics

```cpp
#include <iostream>
#include <thread>
#include <atomic>

std::atomic<int> counter(0);  // Atomic counter

void increment() {
    for (int i = 0; i < 100000; i++) {
        counter++;  // Atomic increment — thread-safe without mutex!
    }
}

int main() {
    std::thread t1(increment);
    std::thread t2(increment);

    t1.join();
    t2.join();

    std::cout << "Counter: " << counter << std::endl;  // Always 200000 ✅
    return 0;
}
```

No mutex, no locks — just works!

---

## Supported Types

`std::atomic<T>` works with:

| Type | Examples |
|------|----------|
| Integral types | `atomic<int>`, `atomic<long>`, `atomic<unsigned>` |
| Booleans | `atomic<bool>` (also `std::atomic_flag`) |
| Pointers | `atomic<int*>`, `atomic<MyClass*>` |
| Floating point (C++20) | `atomic<float>`, `atomic<double>` |
| User-defined types | Only if trivially copyable and `sizeof(T)` fits CPU word |

### Type Aliases

```cpp
// These are the same:
std::atomic<int>      counter1;
std::atomic_int       counter2;  // Convenient alias

std::atomic<bool>     flag1;
std::atomic_bool      flag2;

std::atomic<long>     val1;
std::atomic_long      val2;
```

---

## Atomic Operations

### Load and Store

```cpp
#include <atomic>

std::atomic<int> value(42);

void reader() {
    int v = value.load();  // Atomic read
    // Same as: int v = value;  (implicit load)
}

void writer() {
    value.store(100);  // Atomic write
    // Same as: value = 100;  (implicit store)
}
```

### Exchange

Atomically replaces the value and returns the **old** value:

```cpp
std::atomic<int> value(10);

int old = value.exchange(20);
// old = 10, value = 20
```

### Compare and Swap (CAS)

The **most important** atomic operation. It's the building block for lock-free algorithms.

```cpp
std::atomic<int> value(5);

int expected = 5;
bool success = value.compare_exchange_strong(expected, 10);
// If value == expected (5):
//   → Sets value = 10, returns true
// If value != expected:
//   → Sets expected = current value, returns false
```

#### Practical CAS Example: Lock-free counter

```cpp
#include <iostream>
#include <thread>
#include <atomic>

std::atomic<int> counter(0);

void safeAdd(int amount) {
    int expected = counter.load();
    while (!counter.compare_exchange_weak(expected, expected + amount)) {
        // CAS failed (another thread changed counter)
        // expected now has the new current value
        // Loop retries automatically
    }
}

int main() {
    std::thread t1(safeAdd, 100);
    std::thread t2(safeAdd, 200);

    t1.join();
    t2.join();

    std::cout << "Counter: " << counter << std::endl;  // 300
    return 0;
}
```

### `compare_exchange_weak` vs `compare_exchange_strong`

| Method | May fail spuriously? | Use case |
|--------|---------------------|----------|
| `compare_exchange_weak` | Yes (on some CPUs) | Inside a loop (most common) |
| `compare_exchange_strong` | No | When you need a single attempt |

> `weak` can be slightly faster in loops because it maps more directly to hardware on ARM/RISC platforms.

---

## Fetch Operations

These atomically read-modify-write and return the **old** value:

```cpp
std::atomic<int> value(10);

int old;

old = value.fetch_add(5);    // old = 10, value = 15
old = value.fetch_sub(3);    // old = 15, value = 12
old = value.fetch_and(0xFF); // Bitwise AND
old = value.fetch_or(0x0F);  // Bitwise OR
old = value.fetch_xor(0xFF); // Bitwise XOR

// Shorthand operators (don't return old value):
value += 5;
value -= 3;
value++;
value--;
```

---

## `std::atomic_flag` — The Simplest Atomic

`atomic_flag` is guaranteed to be **lock-free** on all platforms. It only has two states: set and clear.

```cpp
#include <iostream>
#include <thread>
#include <atomic>
#include <vector>

std::atomic_flag spinlock = ATOMIC_FLAG_INIT;  // Cleared state

void criticalSection(int id) {
    // Spin until we acquire the lock
    while (spinlock.test_and_set(std::memory_order_acquire)) {
        // Spinning... (busy wait)
    }

    // Critical section
    std::cout << "Thread " << id << " in critical section" << std::endl;

    // Release the lock
    spinlock.clear(std::memory_order_release);
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; i++) {
        threads.emplace_back(criticalSection, i);
    }
    for (auto& t : threads) {
        t.join();
    }
    return 0;
}
```

> This is a **spinlock** — a lock built entirely from atomic operations. Use with caution: it burns CPU while waiting.

---

## Memory Ordering

This is the **advanced** part of atomics. Memory ordering controls how operations on different atomic (and non-atomic) variables can be reordered by the compiler and CPU.

### Why Does Reordering Happen?

Compilers and CPUs reorder instructions for performance. In single-threaded code, this is invisible. In multi-threaded code, it can cause bugs.

```cpp
// Thread 1
data = 42;         // (A)
ready.store(true); // (B)

// Thread 2
if (ready.load())  // (C)
    use(data);     // (D)

// Without memory ordering, the CPU might reorder (A) and (B),
// so Thread 2 could see ready=true but data is still 0!
```

### Memory Order Options

| Order | Guarantee | Performance |
|-------|-----------|-------------|
| `memory_order_relaxed` | Only atomicity, no ordering | Fastest |
| `memory_order_acquire` | Sees all writes before the matching release | Fast |
| `memory_order_release` | Makes all previous writes visible to acquire | Fast |
| `memory_order_acq_rel` | Both acquire and release | Medium |
| `memory_order_seq_cst` | Full sequential consistency (default) | Slowest |

### `memory_order_seq_cst` (Default)

The **safest** and **slowest**. All threads see operations in the same order.

```cpp
std::atomic<bool> x(false), y(false);
std::atomic<int> z(0);

void writeX() { x.store(true); }   // Uses seq_cst by default
void writeY() { y.store(true); }

void readXY() {
    while (!x.load()) {}
    if (y.load()) z++;
}

void readYX() {
    while (!y.load()) {}
    if (x.load()) z++;
}

// With seq_cst: z is guaranteed to be at least 1
// Both threads see x and y changes in the same global order
```

### `memory_order_acquire` / `memory_order_release`

Used in pairs. Release "publishes" data, acquire "subscribes" to it.

```cpp
#include <iostream>
#include <thread>
#include <atomic>

std::atomic<bool> ready(false);
int data = 0;

void producer() {
    data = 42;                                    // (1) Non-atomic write
    ready.store(true, std::memory_order_release);  // (2) Release: publishes (1)
}

void consumer() {
    while (!ready.load(std::memory_order_acquire)) {} // (3) Acquire: sees (1)
    std::cout << data << std::endl;                    // (4) Guaranteed to print 42
}

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join();
    return 0;
}
```

```
Producer:                      Consumer:
  data = 42  ─────────┐
  ready = true ───────┐│
    (release)         ││
                      ││
                      │└──→ ready == true  (acquire)
                      └───→ data == 42     ✅ guaranteed!
```

### `memory_order_relaxed`

No ordering guarantees. Only guarantees the operation itself is atomic.

```cpp
// Good use case: simple counter where order doesn't matter
std::atomic<int> counter(0);

void count() {
    for (int i = 0; i < 1000; i++) {
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}
```

---

## Practical Example: Spinlock with Proper Ordering

```cpp
class Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // Spin — optionally add a pause hint for better CPU usage
            #if defined(__x86_64__) || defined(_M_X64)
                __builtin_ia32_pause();  // CPU hint: "I'm spinning"
            #endif
        }
    }

    void unlock() {
        flag.clear(std::memory_order_release);
    }
};

// Usage:
Spinlock spin;
int sharedData = 0;

void worker() {
    spin.lock();
    sharedData++;  // Protected by spinlock
    spin.unlock();
}
```

---

## `std::atomic` with User-Defined Types

You can use `atomic<T>` with your own types if they are **trivially copyable**:

```cpp
#include <atomic>

struct Point {
    float x, y;
};

static_assert(std::is_trivially_copyable<Point>::value, "Point must be trivially copyable");

std::atomic<Point> position({0.0f, 0.0f});

void updatePosition() {
    Point newPos{1.0f, 2.0f};
    position.store(newPos);
}

void readPosition() {
    Point p = position.load();
    // p.x and p.y are consistent (both from the same store)
}
```

> **Note:** Large structs may use a hidden mutex internally. Check `atomic<T>::is_lock_free()` to verify.

---

## Checking Lock-Free Support

```cpp
#include <iostream>
#include <atomic>

int main() {
    std::atomic<int> ai;
    std::atomic<long long> all;
    std::atomic<double> ad;

    std::cout << "int is lock-free: " << ai.is_lock_free() << std::endl;
    std::cout << "long long is lock-free: " << all.is_lock_free() << std::endl;
    std::cout << "double is lock-free: " << ad.is_lock_free() << std::endl;

    // Compile-time check (C++17):
    std::cout << "atomic<int> always lock-free: "
              << std::atomic<int>::is_always_lock_free << std::endl;

    return 0;
}
```

---

## When to Use Atomics vs Mutexes

| Scenario | Use |
|----------|-----|
| Simple counter/flag | `std::atomic` |
| Complex data structure updates | `std::mutex` |
| Multiple variables updated together | `std::mutex` (atomics can't do this) |
| Maximum performance needed | `std::atomic` with relaxed ordering |
| Lock-free data structures | `std::atomic` + CAS |
| Simple read/write of single values | `std::atomic` |
| Protecting code blocks | `std::mutex` |

---

## Summary

| Operation | Description | Returns |
|-----------|-------------|---------|
| `load()` | Read atomically | Current value |
| `store(val)` | Write atomically | void |
| `exchange(val)` | Write and return old value | Old value |
| `compare_exchange_strong(exp, des)` | CAS (reliable) | bool |
| `compare_exchange_weak(exp, des)` | CAS (may fail spuriously) | bool |
| `fetch_add(val)` | Add and return old | Old value |
| `fetch_sub(val)` | Subtract and return old | Old value |
| `fetch_and/or/xor(val)` | Bitwise ops and return old | Old value |

---

**Previous:** [Chapter 6 — Condition Variables](06_Condition_Variables.md)
**Next:** [Chapter 8 — Futures, Promises, and Async](08_Futures_Promises_Async.md)
