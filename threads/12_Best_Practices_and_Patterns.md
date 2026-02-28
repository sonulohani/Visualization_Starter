# Chapter 12: Best Practices and Patterns

## Golden Rules of Thread Safety

### Rule 1: Minimize Shared Mutable State

The fewer things threads share, the fewer bugs you'll have.

```cpp
// ❌ BAD: Many shared variables
int sharedA, sharedB, sharedC;
std::mutex mtx;

// ✅ GOOD: Encapsulate shared state
class SharedState {
    int a, b, c;
    mutable std::mutex mtx;
public:
    void update(int newA, int newB, int newC) {
        std::lock_guard<std::mutex> lock(mtx);
        a = newA; b = newB; c = newC;
    }
    std::tuple<int,int,int> get() const {
        std::lock_guard<std::mutex> lock(mtx);
        return {a, b, c};
    }
};
```

### Rule 2: Prefer `const` and Immutable Data

```cpp
// If data never changes after creation, no synchronization needed
const std::vector<int> lookupTable = generateTable();
// All threads can read this freely — zero contention
```

### Rule 3: Use `thread_local` When Possible

```cpp
// Each thread gets its own instance — no sharing
thread_local std::mt19937 rng(std::random_device{}());
```

### Rule 4: Keep Critical Sections Short

```cpp
// ❌ BAD: Lock held during slow I/O
{
    std::lock_guard<std::mutex> lock(mtx);
    data = processInput();         // Fast
    writeToFile(data);             // SLOW — blocks other threads!
}

// ✅ GOOD: Only lock the shared data access
auto localData = processInput();   // No lock needed
{
    std::lock_guard<std::mutex> lock(mtx);
    sharedData = localData;        // Quick copy under lock
}
writeToFile(localData);            // Slow I/O outside the lock
```

### Rule 5: Never Hold a Lock While Calling Unknown Code

```cpp
// ❌ DANGEROUS: callback might try to lock the same mutex → deadlock
{
    std::lock_guard<std::mutex> lock(mtx);
    userCallback();  // Who knows what this does?
}

// ✅ SAFER: Copy data out, call callback outside the lock
DataCopy copy;
{
    std::lock_guard<std::mutex> lock(mtx);
    copy = sharedData;
}
userCallback(copy);
```

---

## Design Patterns for Concurrency

### Pattern 1: Producer-Consumer

The most common threading pattern. One or more threads produce data, others consume it.

```cpp
template<typename T>
class Channel {
    std::queue<T> buffer;
    std::mutex mtx;
    std::condition_variable cvProducer, cvConsumer;
    const size_t capacity;
    bool closed = false;

public:
    explicit Channel(size_t capacity) : capacity(capacity) {}

    bool send(const T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        cvProducer.wait(lock, [this]{ return buffer.size() < capacity || closed; });
        if (closed) return false;
        buffer.push(value);
        cvConsumer.notify_one();
        return true;
    }

    bool receive(T& value) {
        std::unique_lock<std::mutex> lock(mtx);
        cvConsumer.wait(lock, [this]{ return !buffer.empty() || closed; });
        if (buffer.empty()) return false;  // Closed and empty
        value = buffer.front();
        buffer.pop();
        cvProducer.notify_one();
        return true;
    }

    void close() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            closed = true;
        }
        cvProducer.notify_all();
        cvConsumer.notify_all();
    }
};
```

### Pattern 2: Active Object

Encapsulate a thread inside an object. All method calls are queued and executed on the internal thread.

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

class ActiveObject {
    std::thread worker;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;

public:
    ActiveObject() : worker([this]{ run(); }) {}

    ~ActiveObject() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            done = true;
        }
        cv.notify_one();
        worker.join();
    }

    void send(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            tasks.push(std::move(task));
        }
        cv.notify_one();
    }

private:
    void run() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]{ return done || !tasks.empty(); });
                if (done && tasks.empty()) return;
                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }
};

int main() {
    ActiveObject obj;

    obj.send([]{ std::cout << "Task 1" << std::endl; });
    obj.send([]{ std::cout << "Task 2" << std::endl; });
    obj.send([]{ std::cout << "Task 3" << std::endl; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return 0;
}
```

### Pattern 3: Monitor Object

A class that internally handles all synchronization:

```cpp
class BankAccount {
    double balance;
    mutable std::mutex mtx;

public:
    explicit BankAccount(double initial) : balance(initial) {}

    void deposit(double amount) {
        std::lock_guard<std::mutex> lock(mtx);
        balance += amount;
    }

    bool withdraw(double amount) {
        std::lock_guard<std::mutex> lock(mtx);
        if (balance >= amount) {
            balance -= amount;
            return true;
        }
        return false;
    }

    double getBalance() const {
        std::lock_guard<std::mutex> lock(mtx);
        return balance;
    }

    // Atomic transfer between accounts
    static void transfer(BankAccount& from, BankAccount& to, double amount) {
        // Lock both accounts (deadlock-free)
        std::scoped_lock lock(from.mtx, to.mtx);
        if (from.balance >= amount) {
            from.balance -= amount;
            to.balance += amount;
        }
    }
};
```

### Pattern 4: Pipeline

Chain stages together, each running on its own thread:

```cpp
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

template<typename T>
class Stage {
    std::queue<T> input;
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;

public:
    void push(const T& item) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            input.push(item);
        }
        cv.notify_one();
    }

    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]{ return !input.empty() || done; });
        if (input.empty()) return false;
        item = input.front();
        input.pop();
        return true;
    }

    void finish() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            done = true;
        }
        cv.notify_all();
    }
};

int main() {
    Stage<int> stage1;
    Stage<int> stage2;

    // Pipeline: source → double → square → print
    std::thread doubler([&](){
        int val;
        while (stage1.pop(val)) {
            stage2.push(val * 2);
        }
        stage2.finish();
    });

    std::thread printer([&](){
        int val;
        while (stage2.pop(val)) {
            std::cout << val * val << " ";  // Square and print
        }
        std::cout << std::endl;
    });

    // Feed data
    for (int i = 1; i <= 5; i++) {
        stage1.push(i);
    }
    stage1.finish();

    doubler.join();
    printer.join();

    return 0;
}
```

---

## Common Mistakes and How to Avoid Them

### Mistake 1: Forgetting to Join or Detach

```cpp
// ❌ CRASH: thread destroyed without join/detach
void bad() {
    std::thread t([]{ /* work */ });
}   // Destructor calls std::terminate()!

// ✅ FIX: Use jthread (C++20) or always join
void good() {
    std::jthread t([]{ /* work */ });
}   // Auto-joins
```

### Mistake 2: Dangling References in Detached Threads

```cpp
// ❌ BAD
void bad() {
    int x = 42;
    std::thread t([&x]{ use(x); });  // x destroyed when bad() returns!
    t.detach();
}

// ✅ FIX: Capture by value
void good() {
    int x = 42;
    std::thread t([x]{ use(x); });   // Thread has its own copy
    t.detach();
}
```

### Mistake 3: Locking Granularity Too Large

```cpp
// ❌ BAD: Lock held during entire function
void processRequest(Request req) {
    std::lock_guard<std::mutex> lock(globalMtx);
    auto parsed = parse(req);          // Doesn't need lock
    auto validated = validate(parsed); // Doesn't need lock
    database.save(validated);          // Only THIS needs the lock
}

// ✅ GOOD: Lock only what needs protection
void processRequest(Request req) {
    auto parsed = parse(req);
    auto validated = validate(parsed);
    {
        std::lock_guard<std::mutex> lock(globalMtx);
        database.save(validated);
    }
}
```

### Mistake 4: Returning Locked Reference

```cpp
class Container {
    std::vector<int> data;
    std::mutex mtx;

public:
    // ❌ BAD: Caller gets reference to internal data without holding lock
    int& getElement(size_t index) {
        std::lock_guard<std::mutex> lock(mtx);
        return data[index];  // Lock released, but reference is still live!
    }

    // ✅ GOOD: Return by value
    int getElement(size_t index) {
        std::lock_guard<std::mutex> lock(mtx);
        return data[index];  // Returns a copy
    }
};
```

### Mistake 5: Using `std::async` Without Storing the Future

```cpp
// ❌ BAD: Future is immediately destroyed → blocks until async completes!
std::async(std::launch::async, longTask);  // NOT parallel!

// ✅ GOOD: Store the future
auto future = std::async(std::launch::async, longTask);
// Do other work...
future.get();
```

---

## Checklist: Is Your Code Thread-Safe?

- [ ] Is shared mutable data protected by a mutex or made atomic?
- [ ] Are critical sections as short as possible?
- [ ] Are locks acquired in a consistent order (or using `scoped_lock`)?
- [ ] Do all threads join/detach before the program exits?
- [ ] Are no dangling references passed to threads?
- [ ] Is `const` / `thread_local` used where possible?
- [ ] Are condition variable waits using predicates?
- [ ] Are you checking `joinable()` before calling `join()`?
- [ ] Are exceptions handled in threads (they don't propagate to parent)?
- [ ] Have you tested with ThreadSanitizer?

---

## Quick Reference: What to Use When

| I need to... | Use |
|--------------|-----|
| Run a task in parallel | `std::jthread` or `std::async` |
| Get a result from a thread | `std::async` + `std::future` |
| Protect shared data | `std::mutex` + `std::lock_guard` |
| Lock multiple mutexes | `std::scoped_lock` |
| Wait for a condition | `std::condition_variable` |
| Increment a counter atomically | `std::atomic<int>` |
| Read-heavy, write-rare data | `std::shared_mutex` |
| Initialize something once | `std::call_once` or Meyers' singleton |
| Limit concurrent access to N | `std::counting_semaphore` |
| Synchronize at a point | `std::latch` (one-time) or `std::barrier` (reusable) |
| Run many short tasks | Thread pool |
| Store per-thread data | `thread_local` |
| Cancel a thread cooperatively | `std::jthread` + `std::stop_token` |

---

## Recommended Learning Path

```
Beginner:
  Chapter 1 (Intro) → Chapter 2 (Creating) → Chapter 3 (Lifecycle) →
  Chapter 4 (Sharing Data)

Intermediate:
  Chapter 5 (Mutexes) → Chapter 6 (Condition Variables) →
  Chapter 7 (Atomics) → Chapter 8 (Futures)

Advanced:
  Chapter 9 (Thread Pools) → Chapter 10 (Deadlocks) →
  Chapter 11 (Advanced) → Chapter 12 (Best Practices)
```

---

**Previous:** [Chapter 11 — Advanced Topics](11_Advanced_Topics.md)
**Back to Index:** [README](README.md)
