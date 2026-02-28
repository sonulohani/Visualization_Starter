# Chapter 6: Condition Variables

## What is a Condition Variable?

A **condition variable** lets one thread **wait** until another thread **signals** that some condition is met. It's how threads communicate: "Hey, the data is ready!" or "Hey, there's space in the buffer!"

Without condition variables, you'd have to use **busy waiting** (spinning in a loop), which wastes CPU:

```cpp
// ❌ BAD: Busy waiting — burns CPU cycles
while (!dataReady) {
    // Spin, spin, spin... doing nothing useful
}
```

With a condition variable, the waiting thread **sleeps** and is woken up only when needed:

```cpp
// ✅ GOOD: Thread sleeps until signaled
std::unique_lock<std::mutex> lock(mtx);
cv.wait(lock, []{ return dataReady; });  // Sleeps efficiently
```

---

## How Condition Variables Work

```
Producer Thread                    Consumer Thread
───────────────                    ───────────────
                                   lock mutex
                                   cv.wait(lock, predicate)
                                     → predicate false?
                                     → UNLOCK mutex & SLEEP 💤

produce data
lock mutex
set flag = true
unlock mutex
cv.notify_one()  ──────────────→   WAKE UP! 🔔
                                     → RE-LOCK mutex
                                     → check predicate → true!
                                     → continue execution
                                   consume data
                                   unlock mutex
```

---

## Basic Example: Producer-Consumer

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <string>

std::mutex mtx;
std::condition_variable cv;
std::string data;
bool dataReady = false;

void producer() {
    // Simulate work
    std::this_thread::sleep_for(std::chrono::seconds(1));

    {
        std::lock_guard<std::mutex> lock(mtx);
        data = "Hello from producer!";
        dataReady = true;
        std::cout << "Producer: data is ready" << std::endl;
    }   // Mutex unlocked BEFORE notify (good practice)

    cv.notify_one();  // Wake up one waiting thread
}

void consumer() {
    std::cout << "Consumer: waiting for data..." << std::endl;

    std::unique_lock<std::mutex> lock(mtx);

    // Wait until dataReady becomes true
    // The lambda is the PREDICATE — checked to prevent spurious wakeups
    cv.wait(lock, []{ return dataReady; });

    std::cout << "Consumer: got data = " << data << std::endl;
}

int main() {
    std::thread t1(consumer);
    std::thread t2(producer);

    t1.join();
    t2.join();

    return 0;
}
```

**Output:**
```
Consumer: waiting for data...
Producer: data is ready
Consumer: got data = Hello from producer!
```

---

## Why the Predicate is Important: Spurious Wakeups

The OS can **randomly wake up** a waiting thread even if no one called `notify()`. This is called a **spurious wakeup**. The predicate protects against this.

```cpp
// ❌ WRONG: No predicate — vulnerable to spurious wakeups
cv.wait(lock);

// ✅ CORRECT: With predicate — re-checks condition after each wakeup
cv.wait(lock, []{ return someCondition; });
```

`cv.wait(lock, predicate)` is equivalent to:

```cpp
while (!predicate()) {
    cv.wait(lock);
}
```

---

## `notify_one()` vs `notify_all()`

| Method | Behavior |
|--------|----------|
| `notify_one()` | Wakes up **one** waiting thread |
| `notify_all()` | Wakes up **all** waiting threads |

### When to use which?

- **`notify_one()`**: When only one thread can make progress (e.g., one item added to a queue).
- **`notify_all()`**: When multiple threads might be able to proceed, or when the condition changes in a way that affects all waiters.

```cpp
// One item produced → wake one consumer
cv.notify_one();

// Shutdown signal → wake ALL threads so they can exit
cv.notify_all();
```

---

## Complete Producer-Consumer Queue

A thread-safe queue is one of the most common patterns using condition variables:

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

template<typename T>
class ThreadSafeQueue {
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cvNotEmpty;

public:
    void push(const T& value) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            queue.push(value);
        }
        cvNotEmpty.notify_one();  // Wake up one waiting consumer
    }

    T pop() {
        std::unique_lock<std::mutex> lock(mtx);
        cvNotEmpty.wait(lock, [this]{ return !queue.empty(); });

        T value = queue.front();
        queue.pop();
        return value;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mtx);
        return queue.empty();
    }

private:
    // Mutable so const methods can lock
    mutable std::mutex mtx;  // Note: redeclared as mutable for const correctness
};

int main() {
    ThreadSafeQueue<int> queue;

    // Producer thread
    std::thread producer([&queue]() {
        for (int i = 0; i < 10; i++) {
            std::cout << "Producing: " << i << std::endl;
            queue.push(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Consumer thread
    std::thread consumer([&queue]() {
        for (int i = 0; i < 10; i++) {
            int value = queue.pop();  // Blocks if queue is empty
            std::cout << "Consumed: " << value << std::endl;
        }
    });

    producer.join();
    consumer.join();

    return 0;
}
```

---

## Bounded Buffer (Fixed-Size Queue)

A more realistic version with a maximum capacity:

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

template<typename T>
class BoundedBuffer {
    std::queue<T> buffer;
    const size_t maxSize;
    std::mutex mtx;
    std::condition_variable cvNotFull;
    std::condition_variable cvNotEmpty;

public:
    explicit BoundedBuffer(size_t maxSize) : maxSize(maxSize) {}

    void produce(const T& item) {
        std::unique_lock<std::mutex> lock(mtx);
        cvNotFull.wait(lock, [this]{ return buffer.size() < maxSize; });

        buffer.push(item);
        std::cout << "Produced: " << item
                  << " [size=" << buffer.size() << "]" << std::endl;

        cvNotEmpty.notify_one();
    }

    T consume() {
        std::unique_lock<std::mutex> lock(mtx);
        cvNotEmpty.wait(lock, [this]{ return !buffer.empty(); });

        T item = buffer.front();
        buffer.pop();
        std::cout << "Consumed: " << item
                  << " [size=" << buffer.size() << "]" << std::endl;

        cvNotFull.notify_one();
        return item;
    }
};

int main() {
    BoundedBuffer<int> buffer(3);  // Max 3 items

    std::thread producer([&buffer]() {
        for (int i = 0; i < 10; i++) {
            buffer.produce(i);
        }
    });

    std::thread consumer([&buffer]() {
        for (int i = 0; i < 10; i++) {
            buffer.consume();
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    });

    producer.join();
    consumer.join();

    return 0;
}
```

---

## `wait_for()` and `wait_until()` — Timed Waiting

Don't wait forever — set a timeout:

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

std::mutex mtx;
std::condition_variable cv;
bool ready = false;

void waitWithTimeout() {
    std::unique_lock<std::mutex> lock(mtx);

    // Wait for up to 2 seconds
    if (cv.wait_for(lock, std::chrono::seconds(2), []{ return ready; })) {
        std::cout << "Condition met!" << std::endl;
    } else {
        std::cout << "Timed out!" << std::endl;
    }
}

void signaler(bool shouldSignal) {
    if (shouldSignal) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        {
            std::lock_guard<std::mutex> lock(mtx);
            ready = true;
        }
        cv.notify_one();
    }
    // If shouldSignal is false, the waiter will time out
}

int main() {
    // Test 1: Signal arrives in time
    ready = false;
    std::thread t1(waitWithTimeout);
    std::thread t2(signaler, true);
    t1.join();
    t2.join();

    // Test 2: No signal → timeout
    ready = false;
    std::thread t3(waitWithTimeout);
    std::thread t4(signaler, false);
    t3.join();
    t4.join();

    return 0;
}
```

**Output:**
```
Condition met!
Timed out!
```

You can also use `wait_until()` with an absolute time point:

```cpp
auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
cv.wait_until(lock, deadline, []{ return ready; });
```

---

## `std::condition_variable_any`

`std::condition_variable` only works with `std::unique_lock<std::mutex>`.
`std::condition_variable_any` works with **any** lockable type:

```cpp
#include <shared_mutex>
#include <condition_variable>

std::shared_mutex sharedMtx;
std::condition_variable_any cvAny;

void example() {
    std::shared_lock<std::shared_mutex> lock(sharedMtx);
    cvAny.wait(lock, []{ return someCondition; });
}
```

> Use `std::condition_variable` by default (better performance). Only use `condition_variable_any` when you need non-standard lock types.

---

## Common Pattern: Notification Flag

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

class Event {
    std::mutex mtx;
    std::condition_variable cv;
    bool signaled = false;

public:
    void signal() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            signaled = true;
        }
        cv.notify_all();  // Wake ALL waiters
    }

    void wait() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]{ return signaled; });
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mtx);
        signaled = false;
    }
};

int main() {
    Event startEvent;

    auto worker = [&startEvent](int id) {
        std::cout << "Worker " << id << " waiting for start signal..." << std::endl;
        startEvent.wait();
        std::cout << "Worker " << id << " started!" << std::endl;
    };

    std::vector<std::thread> workers;
    for (int i = 0; i < 4; i++) {
        workers.emplace_back(worker, i);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "\n--- GO! ---\n" << std::endl;
    startEvent.signal();

    for (auto& w : workers) {
        w.join();
    }

    return 0;
}
```

---

## Summary

| Feature | Description |
|---------|-------------|
| `cv.wait(lock, pred)` | Sleep until predicate is true |
| `cv.wait_for(lock, duration, pred)` | Wait with timeout (relative) |
| `cv.wait_until(lock, timepoint, pred)` | Wait with timeout (absolute) |
| `cv.notify_one()` | Wake one waiting thread |
| `cv.notify_all()` | Wake all waiting threads |
| Always use a predicate | Prevents spurious wakeup bugs |
| Unlock mutex before notify | Better performance (avoids wake-then-block) |

---

**Previous:** [Chapter 5 — Mutexes and Locks](05_Mutexes_and_Locks.md)
**Next:** [Chapter 7 — Atomic Operations](07_Atomic_Operations.md)
