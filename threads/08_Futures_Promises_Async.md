# Chapter 8: Futures, Promises, and Async

## The Problem: Getting Results from Threads

`std::thread` doesn't have a built-in way to return a value. How do you get a result back?

```cpp
// ❌ std::thread can't return values directly
int result = ???;
std::thread t([]() { return 42; });  // Where does 42 go?
```

C++ provides **futures** and **promises** to solve this. Think of it as a postal system:
- **Promise**: A mailbox where a thread drops off a result
- **Future**: A receipt you use to pick up the result later

```
Thread A (caller):              Thread B (worker):
   ┌──────────┐
   │  future   │◄──────────────── promise.set_value(42)
   │  .get()   │    "Here's the result!"
   │  = 42     │
   └──────────┘
```

---

## `std::async` — The Easy Way

`std::async` is the simplest way to run a task asynchronously and get a result.

```cpp
#include <iostream>
#include <future>

int compute(int x, int y) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    return x + y;
}

int main() {
    // Launch async task
    std::future<int> result = std::async(std::launch::async, compute, 10, 20);

    std::cout << "Doing other work while waiting..." << std::endl;

    // Get the result (blocks if not ready)
    int value = result.get();
    std::cout << "Result: " << value << std::endl;  // Result: 30

    return 0;
}
```

### Launch Policies

| Policy | Behavior |
|--------|----------|
| `std::launch::async` | Runs in a **new thread** immediately |
| `std::launch::deferred` | Runs **lazily** when `.get()` is called (same thread) |
| `std::launch::async | std::launch::deferred` | Implementation chooses (default) |

```cpp
// Always runs in a new thread
auto f1 = std::async(std::launch::async, task);

// Only runs when you call .get() — same thread, no parallelism
auto f2 = std::async(std::launch::deferred, task);

// Let the implementation decide (default behavior)
auto f3 = std::async(task);
```

### Example: Parallel Computation

```cpp
#include <iostream>
#include <future>
#include <vector>
#include <numeric>

long long sumRange(const std::vector<int>& vec, size_t start, size_t end) {
    long long sum = 0;
    for (size_t i = start; i < end; i++) {
        sum += vec[i];
    }
    return sum;
}

int main() {
    // Create a large vector
    std::vector<int> numbers(10000000, 1);  // 10 million 1s

    size_t mid = numbers.size() / 2;

    // Split work into two async tasks
    auto future1 = std::async(std::launch::async, sumRange,
                              std::cref(numbers), 0, mid);
    auto future2 = std::async(std::launch::async, sumRange,
                              std::cref(numbers), mid, numbers.size());

    // Combine results
    long long total = future1.get() + future2.get();
    std::cout << "Total: " << total << std::endl;  // 10000000

    return 0;
}
```

---

## `std::future<T>` — The Result Holder

A `future` represents a value that will be available **sometime in the future**.

### Key Methods

```cpp
std::future<int> fut = std::async(std::launch::async, []{ return 42; });

// Check if the result is ready (non-blocking)
if (fut.valid()) {
    std::cout << "Future is valid" << std::endl;
}

// Wait with timeout
auto status = fut.wait_for(std::chrono::milliseconds(100));
if (status == std::future_status::ready) {
    std::cout << "Ready!" << std::endl;
} else if (status == std::future_status::timeout) {
    std::cout << "Not ready yet..." << std::endl;
} else if (status == std::future_status::deferred) {
    std::cout << "Task is deferred" << std::endl;
}

// Get the result (blocks if not ready, can only call ONCE)
int result = fut.get();

// fut.get();  // ❌ CRASH! Can only call get() once on a future
```

### Exception Propagation

If the async task throws, the exception is stored and re-thrown when you call `.get()`:

```cpp
#include <iostream>
#include <future>
#include <stdexcept>

int riskyTask() {
    throw std::runtime_error("Something went wrong!");
    return 42;
}

int main() {
    auto fut = std::async(std::launch::async, riskyTask);

    try {
        int result = fut.get();  // Re-throws the exception!
    } catch (const std::exception& e) {
        std::cout << "Caught: " << e.what() << std::endl;
    }

    return 0;
}
```

**Output:**
```
Caught: Something went wrong!
```

---

## `std::promise<T>` — Manually Set a Value

A `promise` gives you direct control over when and how a value is set.

```cpp
#include <iostream>
#include <thread>
#include <future>

void worker(std::promise<int> prom) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    prom.set_value(42);  // Fulfill the promise
}

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();  // Get the associated future

    std::thread t(worker, std::move(prom));  // Must move promise (not copyable)

    std::cout << "Waiting for result..." << std::endl;
    int result = fut.get();  // Blocks until promise is fulfilled
    std::cout << "Got: " << result << std::endl;

    t.join();
    return 0;
}
```

### Setting an Exception via Promise

```cpp
void failingWorker(std::promise<int> prom) {
    try {
        // Simulate failure
        throw std::runtime_error("Worker failed!");
    } catch (...) {
        prom.set_exception(std::current_exception());
    }
}

int main() {
    std::promise<int> prom;
    std::future<int> fut = prom.get_future();

    std::thread t(failingWorker, std::move(prom));

    try {
        int result = fut.get();  // Throws the stored exception
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    t.join();
    return 0;
}
```

---

## `std::shared_future<T>` — Multiple Consumers

A regular `future` can only be `.get()`'d once. A `shared_future` can be read by **multiple threads**.

```cpp
#include <iostream>
#include <thread>
#include <future>
#include <vector>

int main() {
    std::promise<int> prom;
    std::shared_future<int> sharedFut = prom.get_future().share();

    // Multiple threads can all wait for the same result
    auto consumer = [](std::shared_future<int> fut, int id) {
        int result = fut.get();  // All threads can call get()!
        std::cout << "Consumer " << id << " got: " << result << std::endl;
    };

    std::vector<std::thread> consumers;
    for (int i = 0; i < 5; i++) {
        consumers.emplace_back(consumer, sharedFut, i);
    }

    // Producer
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    prom.set_value(100);

    for (auto& t : consumers) {
        t.join();
    }

    return 0;
}
```

**Output:**
```
Consumer 0 got: 100
Consumer 1 got: 100
Consumer 2 got: 100
Consumer 3 got: 100
Consumer 4 got: 100
```

---

## `std::packaged_task<T>` — Wrap a Callable

`packaged_task` wraps a callable so that its return value is captured in a `future`. Useful for task queues and thread pools.

```cpp
#include <iostream>
#include <thread>
#include <future>

int add(int a, int b) {
    return a + b;
}

int main() {
    // Wrap the function
    std::packaged_task<int(int, int)> task(add);

    // Get the future BEFORE executing
    std::future<int> result = task.get_future();

    // Run the task in a thread
    std::thread t(std::move(task), 3, 4);

    std::cout << "Result: " << result.get() << std::endl;  // 7

    t.join();
    return 0;
}
```

### Task Queue Example

```cpp
#include <iostream>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <functional>

std::queue<std::packaged_task<int()>> taskQueue;
std::mutex queueMutex;

void workerLoop() {
    while (true) {
        std::packaged_task<int()> task;

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (taskQueue.empty()) break;
            task = std::move(taskQueue.front());
            taskQueue.pop();
        }

        task();  // Execute the task
    }
}

int main() {
    std::vector<std::future<int>> results;

    // Submit tasks
    for (int i = 0; i < 5; i++) {
        std::packaged_task<int()> task([i]() {
            return i * i;
        });
        results.push_back(task.get_future());

        std::lock_guard<std::mutex> lock(queueMutex);
        taskQueue.push(std::move(task));
    }

    // Process all tasks
    std::thread worker(workerLoop);
    worker.join();

    // Collect results
    for (auto& fut : results) {
        std::cout << fut.get() << " ";
    }
    std::cout << std::endl;  // 0 1 4 9 16

    return 0;
}
```

---

## Comparison: async vs promise vs packaged_task

| Feature | `std::async` | `std::promise` | `std::packaged_task` |
|---------|-------------|----------------|---------------------|
| Creates thread | Yes (with async policy) | No | No |
| Returns future | Automatically | Via `get_future()` | Via `get_future()` |
| Sets value | Automatically (return value) | Manually (`set_value`) | Automatically (return value) |
| Flexibility | Low (fire-and-forget) | High (full control) | Medium (deferred execution) |
| Best for | Quick async operations | Custom thread management | Task queues/schedulers |

---

## Practical Example: Parallel File Processing

```cpp
#include <iostream>
#include <future>
#include <vector>
#include <string>
#include <fstream>

// Simulate processing a file
int processFile(const std::string& filename) {
    std::cout << "Processing " << filename << "..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return filename.length();  // Return file name length as dummy result
}

int main() {
    std::vector<std::string> files = {
        "report.txt", "data.csv", "config.json",
        "readme.md", "log.txt"
    };

    // Launch all file processing in parallel
    std::vector<std::future<int>> futures;
    for (const auto& file : files) {
        futures.push_back(
            std::async(std::launch::async, processFile, file)
        );
    }

    // Collect all results
    int totalSize = 0;
    for (size_t i = 0; i < futures.size(); i++) {
        int result = futures[i].get();
        std::cout << files[i] << " → " << result << std::endl;
        totalSize += result;
    }

    std::cout << "Total: " << totalSize << std::endl;

    return 0;
}
```

---

## `void` Futures — Synchronization Without Data

Use `std::future<void>` when you need to wait for completion but don't need a return value:

```cpp
#include <iostream>
#include <future>

int main() {
    std::promise<void> prom;
    std::future<void> fut = prom.get_future();

    std::thread t([&prom]() {
        std::cout << "Doing setup..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        prom.set_value();  // Signal completion (no value)
    });

    fut.get();  // Wait for completion
    std::cout << "Setup complete!" << std::endl;

    t.join();
    return 0;
}
```

---

## Summary

```
             ┌─────────────┐
             │  std::async  │  ← Simplest: launch & get result
             └──────┬───────┘
                    │ returns
                    ▼
             ┌─────────────┐
             │ std::future  │  ← Holds the result
             └──────────────┘
                    ▲
                    │ get_future()
             ┌──────┴───────┐
             │              │
      ┌──────────┐  ┌──────────────┐
      │ promise   │  │packaged_task │
      │set_value()│  │  (callable)  │
      └──────────┘  └──────────────┘
       Manual          Wraps a function
       control         for deferred execution
```

---

**Previous:** [Chapter 7 — Atomic Operations](07_Atomic_Operations.md)
**Next:** [Chapter 9 — Thread Pools](09_Thread_Pools.md)
