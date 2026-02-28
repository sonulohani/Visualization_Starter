# Chapter 10: Deadlocks and Common Problems

## The Five Horsemen of Concurrent Programming

| Problem | What Happens |
|---------|-------------|
| **Deadlock** | Threads stuck forever, waiting for each other |
| **Livelock** | Threads keep running but make no progress |
| **Starvation** | A thread never gets to run |
| **Race Condition** | Result depends on unpredictable timing |
| **Priority Inversion** | High-priority thread blocked by low-priority one |

---

## 1. Deadlock

A **deadlock** occurs when two or more threads are each waiting for a resource held by the other. Nobody can proceed.

```
Thread 1: Has Lock A, waiting for Lock B
Thread 2: Has Lock B, waiting for Lock A

Both are stuck FOREVER.
```

### Classic Deadlock Example

```cpp
#include <iostream>
#include <thread>
#include <mutex>

std::mutex mutexA, mutexB;

void thread1() {
    std::lock_guard<std::mutex> lockA(mutexA);  // Lock A first
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::lock_guard<std::mutex> lockB(mutexB);  // Then try to lock B
    // ❌ DEADLOCK if thread2 holds B and waits for A!
    std::cout << "Thread 1 done" << std::endl;
}

void thread2() {
    std::lock_guard<std::mutex> lockB(mutexB);  // Lock B first
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::lock_guard<std::mutex> lockA(mutexA);  // Then try to lock A
    // ❌ DEADLOCK — each thread holds what the other needs!
    std::cout << "Thread 2 done" << std::endl;
}

int main() {
    std::thread t1(thread1);
    std::thread t2(thread2);
    t1.join();
    t2.join();
    // This program HANGS — never prints "done"
    return 0;
}
```

### Four Conditions for Deadlock (Coffman Conditions)

ALL four must be true for deadlock to occur:

1. **Mutual Exclusion** — At least one resource is held in a non-sharable mode
2. **Hold and Wait** — A thread holding resources waits for additional ones
3. **No Preemption** — Resources can only be released voluntarily
4. **Circular Wait** — A circular chain of threads, each waiting for the next

Break **any one** condition and deadlock is impossible.

---

### Fix 1: Consistent Lock Ordering

Always acquire locks in the **same order**:

```cpp
void thread1() {
    std::lock_guard<std::mutex> lockA(mutexA);  // Always A first
    std::lock_guard<std::mutex> lockB(mutexB);  // Then B
    std::cout << "Thread 1 done" << std::endl;
}

void thread2() {
    std::lock_guard<std::mutex> lockA(mutexA);  // Always A first ✅
    std::lock_guard<std::mutex> lockB(mutexB);  // Then B
    std::cout << "Thread 2 done" << std::endl;
}
```

### Fix 2: `std::scoped_lock` (C++17)

Locks multiple mutexes **atomically** using a deadlock-avoidance algorithm:

```cpp
void thread1() {
    std::scoped_lock lock(mutexA, mutexB);  // Deadlock-free!
    std::cout << "Thread 1 done" << std::endl;
}

void thread2() {
    std::scoped_lock lock(mutexB, mutexA);  // Order doesn't matter!
    std::cout << "Thread 2 done" << std::endl;
}
```

### Fix 3: `std::lock()` with `std::unique_lock`

Pre-C++17 approach:

```cpp
void thread1() {
    std::unique_lock<std::mutex> lockA(mutexA, std::defer_lock);
    std::unique_lock<std::mutex> lockB(mutexB, std::defer_lock);

    std::lock(lockA, lockB);  // Lock both atomically

    std::cout << "Thread 1 done" << std::endl;
}
```

### Fix 4: Try-Lock with Backoff

```cpp
void thread1() {
    while (true) {
        std::unique_lock<std::mutex> lockA(mutexA);

        if (mutexB.try_lock()) {
            std::lock_guard<std::mutex> lockB(mutexB, std::adopt_lock);
            // Got both locks!
            std::cout << "Thread 1 done" << std::endl;
            return;
        }

        // Couldn't get B, release A and try again
        lockA.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
```

---

## 2. Livelock

Threads are active but keep undoing each other's work, like two people in a hallway who keep stepping aside in the same direction.

```cpp
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

std::atomic<bool> flagA(false), flagB(false);

void worker1() {
    int attempts = 0;
    while (attempts < 100) {
        flagA = true;
        // "Politely" wait for the other
        while (flagB) {
            flagA = false;                              // Back off
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            flagA = true;                               // Try again
        }
        // Critical section
        std::cout << "Worker 1 entered critical section" << std::endl;
        flagA = false;
        attempts++;
    }
}

// Worker 2 does the symmetric thing — both keep backing off!
```

### Fix: Random Backoff

```cpp
#include <random>

void workerWithRandomBackoff() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1, 10);

    while (true) {
        flagA = true;
        if (flagB) {
            flagA = false;
            // Random delay breaks the symmetry
            std::this_thread::sleep_for(std::chrono::milliseconds(dist(gen)));
        } else {
            // Got the resource!
            break;
        }
    }
}
```

---

## 3. Starvation

A thread **never** gets CPU time or access to a resource because other threads keep taking it.

```cpp
//  Example: Thread 3 never gets the lock because 1 and 2 keep trading it

std::mutex mtx;

void highPriorityWork() {
    while (true) {
        std::lock_guard<std::mutex> lock(mtx);
        // Does work...
    }
}

void lowPriorityWork() {
    // This thread might NEVER acquire the lock!
    std::lock_guard<std::mutex> lock(mtx);
    std::cout << "Finally got it!" << std::endl;
}
```

### Fixes for Starvation
- Use **fair locks** (FIFO ordering) — not directly available in C++ standard, but can be implemented
- Use **shared_mutex** to let readers proceed in parallel
- Ensure all threads get a chance (e.g., round-robin scheduling)

---

## 4. Race Conditions in Detail

### TOCTOU (Time of Check to Time of Use)

```cpp
std::map<int, std::string> cache;
std::mutex cacheMutex;

// ❌ BAD: Race between check and use
void badLookup(int key) {
    cacheMutex.lock();
    bool exists = cache.count(key) > 0;
    cacheMutex.unlock();

    // ⚠️ Another thread could modify cache HERE

    if (exists) {
        cacheMutex.lock();
        std::string value = cache[key];  // Key might be gone!
        cacheMutex.unlock();
    }
}

// ✅ GOOD: Check and use in the same critical section
void goodLookup(int key) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (cache.count(key) > 0) {
        std::string value = cache[key];  // Safe!
    }
}
```

### Double-Checked Locking (Broken and Fixed)

A common pattern for lazy initialization:

```cpp
// ❌ BROKEN: Classic double-checked locking
class Singleton {
    static Singleton* instance;
    static std::mutex mtx;

public:
    static Singleton* getInstance() {
        if (instance == nullptr) {          // First check (no lock)
            std::lock_guard<std::mutex> lock(mtx);
            if (instance == nullptr) {      // Second check (with lock)
                instance = new Singleton(); // ❌ Can be reordered!
            }
        }
        return instance;
    }
};

// ✅ FIXED: Use std::call_once
class Singleton {
    static std::unique_ptr<Singleton> instance;
    static std::once_flag initFlag;

public:
    static Singleton& getInstance() {
        std::call_once(initFlag, []() {
            instance = std::make_unique<Singleton>();
        });
        return *instance;
    }
};

// ✅ EVEN SIMPLER: Meyers' Singleton (C++11 guarantees thread safety)
class Singleton {
public:
    static Singleton& getInstance() {
        static Singleton instance;  // Thread-safe in C++11+
        return instance;
    }
};
```

---

## 5. Priority Inversion

A high-priority thread is blocked by a low-priority thread holding a lock, while a medium-priority thread keeps preempting the low-priority one.

```
Priority:  High > Medium > Low

1. Low-priority thread acquires Lock
2. High-priority thread tries to acquire Lock → BLOCKED
3. Medium-priority thread preempts Low → Low can't release Lock
4. High-priority thread is effectively lower priority than Medium!
```

### Fix: Priority Inheritance
The OS temporarily raises the low-priority thread's priority to match the high-priority waiter. This is typically an OS-level feature, not directly controllable in standard C++.

---

## 6. False Sharing

When two threads modify **different variables** that happen to be on the **same cache line**, the CPU keeps invalidating the cache, destroying performance.

```cpp
// ❌ BAD: counter1 and counter2 likely share a cache line (64 bytes)
struct BadCounters {
    std::atomic<int> counter1;  // These are probably adjacent
    std::atomic<int> counter2;  // in memory (same cache line)
};

// ✅ GOOD: Pad to different cache lines
struct GoodCounters {
    alignas(64) std::atomic<int> counter1;  // Own cache line
    alignas(64) std::atomic<int> counter2;  // Own cache line
};
```

```
Cache line (64 bytes):
┌─────────────────────────────────────────────┐
│ counter1 │ counter2 │       padding         │
└─────────────────────────────────────────────┘
   Core 1      Core 2
   writes      writes
   here        here
       ↕ INVALIDATION PING-PONG ↕

After fix — separate cache lines:
┌─────────────────────┐  ┌─────────────────────┐
│ counter1 │ padding  │  │ counter2 │ padding  │
└─────────────────────┘  └─────────────────────┘
   Core 1 only              Core 2 only
   No interference!
```

---

## 7. ABA Problem

Occurs with CAS (compare-and-swap) operations:

1. Thread reads value A
2. Another thread changes A → B → A
3. First thread's CAS succeeds (value is still A) but meaning has changed

```cpp
// Thread 1: reads head → node A
// Thread 2: removes A, removes B, re-adds A (same pointer!)
// Thread 1: CAS succeeds (head still points to A) — but the list structure changed!
```

### Fix: Use versioned pointers or hazard pointers (advanced lock-free techniques).

---

## Debugging Concurrency Bugs

### Tools

| Tool | Platform | What it catches |
|------|----------|----------------|
| **ThreadSanitizer (TSan)** | GCC/Clang | Data races |
| **Helgrind** | Valgrind | Races, deadlocks |
| **Intel Inspector** | Intel | Races, deadlocks, memory |
| **AddressSanitizer (ASan)** | GCC/Clang | Memory errors |

### Using ThreadSanitizer

```bash
# Compile with TSan
g++ -std=c++17 -fsanitize=thread -g program.cpp -o program

# Run — it will report any data races
./program
```

**Example TSan output:**
```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x... by thread T1:
    #0 increment() program.cpp:10
  Previous read of size 4 at 0x... by thread T2:
    #0 increment() program.cpp:10
```

---

## Summary: Prevention Strategies

| Problem | Prevention |
|---------|-----------|
| Deadlock | Consistent lock ordering, `scoped_lock`, lock hierarchy |
| Livelock | Random backoff |
| Starvation | Fair scheduling, time limits |
| Race condition | Proper synchronization (mutex, atomic) |
| False sharing | Cache line alignment (`alignas(64)`) |
| TOCTOU | Keep check+use in same critical section |
| ABA | Versioned pointers, hazard pointers |

---

**Previous:** [Chapter 9 — Thread Pools](09_Thread_Pools.md)
**Next:** [Chapter 11 — Advanced Topics](11_Advanced_Topics.md)
