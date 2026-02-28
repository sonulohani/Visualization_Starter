# Chapter 5: Mutexes and Locks

## What is a Mutex?

A **mutex** (mutual exclusion) is a synchronization primitive that ensures only **one thread at a time** can access a critical section.

Think of it like a bathroom door lock:
- Only one person can be inside (locked).
- Others must wait until the person comes out (unlocked).

```
Thread 1: [LOCK]──[critical section]──[UNLOCK]
Thread 2:          waiting...          [LOCK]──[critical section]──[UNLOCK]
Thread 3:          waiting...                   waiting...          [LOCK]──[critical section]──[UNLOCK]
```

---

## `std::mutex` — Basic Mutex

```cpp
#include <iostream>
#include <thread>
#include <mutex>

std::mutex mtx;
int sharedCounter = 0;

void increment(int times) {
    for (int i = 0; i < times; i++) {
        mtx.lock();        // Acquire the lock
        sharedCounter++;   // Critical section
        mtx.unlock();      // Release the lock
    }
}

int main() {
    std::thread t1(increment, 100000);
    std::thread t2(increment, 100000);

    t1.join();
    t2.join();

    std::cout << "Counter: " << sharedCounter << std::endl;  // Always 200000
    return 0;
}
```

### Problems with Manual `lock()`/`unlock()`

```cpp
void dangerous() {
    mtx.lock();
    doSomething();       // 💥 If this throws an exception...
    mtx.unlock();        // ❌ This line is NEVER reached!
    // The mutex stays LOCKED FOREVER → other threads are stuck!
}
```

**Never use raw `lock()`/`unlock()`.** Always use RAII wrappers.

---

## `std::lock_guard` — Simple RAII Lock

`lock_guard` locks the mutex on construction and unlocks it on destruction. **You can't forget to unlock.**

```cpp
#include <iostream>
#include <thread>
#include <mutex>

std::mutex mtx;
int counter = 0;

void safeIncrement() {
    for (int i = 0; i < 100000; i++) {
        std::lock_guard<std::mutex> lock(mtx);  // Locked here
        counter++;
    }  // Automatically unlocked here (lock_guard destructor)
}

int main() {
    std::thread t1(safeIncrement);
    std::thread t2(safeIncrement);
    t1.join();
    t2.join();
    std::cout << "Counter: " << counter << std::endl;  // 200000 ✅
    return 0;
}
```

### Limiting the Lock Scope

Lock only what you need — keep critical sections **small**:

```cpp
void processData() {
    // Prepare data (no lock needed)
    auto data = generateData();

    {   // Start a new scope just for the lock
        std::lock_guard<std::mutex> lock(mtx);
        sharedContainer.push_back(data);  // Only this needs protection
    }   // Lock released here

    // Post-processing (no lock needed)
    logResult(data);
}
```

---

## `std::unique_lock` — Flexible Lock

`unique_lock` is more powerful than `lock_guard`. It supports:
- Deferred locking
- Timed locking
- Manual lock/unlock
- Moving ownership
- Use with condition variables

```cpp
#include <iostream>
#include <thread>
#include <mutex>

std::mutex mtx;

void example() {
    // 1. Normal locking (same as lock_guard)
    std::unique_lock<std::mutex> lock1(mtx);
    // ...
    lock1.unlock();  // Can manually unlock

    // 2. Deferred locking — don't lock yet
    std::unique_lock<std::mutex> lock2(mtx, std::defer_lock);
    // Do some non-critical work...
    lock2.lock();    // Lock when ready
    // ...
    lock2.unlock();

    // 3. Try to lock (non-blocking)
    std::unique_lock<std::mutex> lock3(mtx, std::try_to_lock);
    if (lock3.owns_lock()) {
        std::cout << "Got the lock!" << std::endl;
    } else {
        std::cout << "Lock was busy, doing something else." << std::endl;
    }
}
```

### Timed Locking with `std::timed_mutex`

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>

std::timed_mutex tmtx;

void worker(int id) {
    // Try to lock for up to 100ms
    std::unique_lock<std::timed_mutex> lock(tmtx, std::chrono::milliseconds(100));

    if (lock.owns_lock()) {
        std::cout << "Worker " << id << ": Got the lock!" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    } else {
        std::cout << "Worker " << id << ": Couldn't get lock, timed out." << std::endl;
    }
}

int main() {
    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    t1.join();
    t2.join();
    return 0;
}
```

---

## `std::scoped_lock` (C++17) — Lock Multiple Mutexes

When you need to lock **multiple mutexes** at once, `scoped_lock` prevents deadlocks by locking them all atomically.

```cpp
#include <iostream>
#include <thread>
#include <mutex>

std::mutex mtx1, mtx2;

void transfer(int& from, int& to, int amount) {
    // Locks BOTH mutexes at once — deadlock-free!
    std::scoped_lock lock(mtx1, mtx2);

    if (from >= amount) {
        from -= amount;
        to += amount;
    }
}

int main() {
    int account1 = 1000;
    int account2 = 500;

    std::thread t1(transfer, std::ref(account1), std::ref(account2), 200);
    std::thread t2(transfer, std::ref(account2), std::ref(account1), 100);

    t1.join();
    t2.join();

    std::cout << "Account 1: " << account1 << std::endl;
    std::cout << "Account 2: " << account2 << std::endl;

    return 0;
}
```

> `scoped_lock` uses a deadlock-avoidance algorithm internally. It's the recommended way to lock multiple mutexes.

---

## `std::recursive_mutex` — Re-entrant Locking

A regular `std::mutex` **cannot be locked twice** by the same thread (it would deadlock). A `recursive_mutex` allows the **same thread** to lock it multiple times.

```cpp
#include <iostream>
#include <thread>
#include <mutex>

std::recursive_mutex rmtx;

void recursiveFunction(int depth) {
    std::lock_guard<std::recursive_mutex> lock(rmtx);

    std::cout << "Depth: " << depth << std::endl;

    if (depth > 0) {
        recursiveFunction(depth - 1);  // Locks the same mutex again — OK!
    }
}

int main() {
    std::thread t(recursiveFunction, 3);
    t.join();
    return 0;
}
```

**Output:**
```
Depth: 3
Depth: 2
Depth: 1
Depth: 0
```

> **Caution:** If you need a recursive mutex, it often means your design could be improved. Try refactoring to avoid it.

---

## `std::shared_mutex` (C++17) — Reader-Writer Lock

When many threads **read** data but few **write**, a shared mutex is more efficient than a regular mutex.

- **Readers** can hold the lock simultaneously (shared access)
- **Writers** get exclusive access (no readers or other writers)

```cpp
#include <iostream>
#include <thread>
#include <shared_mutex>
#include <vector>

std::shared_mutex rwMutex;
std::vector<int> data;

void reader(int id) {
    std::shared_lock<std::shared_mutex> lock(rwMutex);  // Shared (read) lock
    std::cout << "Reader " << id << ": data has "
              << data.size() << " elements." << std::endl;
    // Multiple readers can be here simultaneously!
}

void writer(int value) {
    std::unique_lock<std::shared_mutex> lock(rwMutex);  // Exclusive (write) lock
    data.push_back(value);
    std::cout << "Writer added " << value << std::endl;
    // Only one writer at a time, no readers allowed
}

int main() {
    std::thread w1(writer, 10);
    w1.join();

    std::thread r1(reader, 1);
    std::thread r2(reader, 2);
    std::thread r3(reader, 3);

    r1.join();
    r2.join();
    r3.join();

    return 0;
}
```

```
Concurrency behavior:

Regular mutex:    Reader1 → Reader2 → Reader3   (sequential)
Shared mutex:     Reader1 ─┐
                  Reader2 ─┤ (all at once!)
                  Reader3 ─┘
```

---

## `std::call_once` and `std::once_flag` — Initialize Once

Ensures a function is called **exactly once**, even with multiple threads.

```cpp
#include <iostream>
#include <thread>
#include <mutex>

std::once_flag initFlag;
int* sharedResource = nullptr;

void initResource() {
    std::cout << "Initializing resource (called only once)..." << std::endl;
    sharedResource = new int(42);
}

void worker(int id) {
    // Only the first thread to reach this actually calls initResource
    std::call_once(initFlag, initResource);

    std::cout << "Worker " << id << ": resource = " << *sharedResource << std::endl;
}

int main() {
    std::thread t1(worker, 1);
    std::thread t2(worker, 2);
    std::thread t3(worker, 3);

    t1.join();
    t2.join();
    t3.join();

    delete sharedResource;
    return 0;
}
```

**Output:**
```
Initializing resource (called only once)...
Worker 1: resource = 42
Worker 2: resource = 42
Worker 3: resource = 42
```

> `std::call_once` is perfect for lazy initialization of singletons or global resources.

---

## Mutex Types Summary

| Mutex Type | Recursive? | Timed? | Shared? | Use Case |
|------------|-----------|--------|---------|----------|
| `std::mutex` | ❌ | ❌ | ❌ | Basic mutual exclusion |
| `std::timed_mutex` | ❌ | ✅ | ❌ | Lock with timeout |
| `std::recursive_mutex` | ✅ | ❌ | ❌ | Recursive function locking |
| `std::recursive_timed_mutex` | ✅ | ✅ | ❌ | Recursive + timeout |
| `std::shared_mutex` | ❌ | ❌ | ✅ | Reader-writer patterns |
| `std::shared_timed_mutex` | ❌ | ✅ | ✅ | Reader-writer + timeout |

## Lock Types Summary

| Lock Type | Flexibility | RAII? | Movable? | Use Case |
|-----------|-------------|-------|----------|----------|
| `std::lock_guard` | Low | ✅ | ❌ | Simple scope-based locking |
| `std::unique_lock` | High | ✅ | ✅ | Deferred/timed/condition vars |
| `std::scoped_lock` | Medium | ✅ | ❌ | Lock multiple mutexes safely |
| `std::shared_lock` | Medium | ✅ | ✅ | Shared (read) access |

---

**Previous:** [Chapter 4 — Sharing Data Between Threads](04_Sharing_Data_Between_Threads.md)
**Next:** [Chapter 6 — Condition Variables](06_Condition_Variables.md)
