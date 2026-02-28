# Chapter 9: Thread Pools

## Why Thread Pools?

Creating and destroying threads is **expensive**. If you need to run many short tasks, creating a new thread for each one is wasteful:

```
Without Thread Pool:
  Task 1: [create thread]──[run]──[destroy thread]
  Task 2:      [create thread]──[run]──[destroy thread]
  Task 3:           [create thread]──[run]──[destroy thread]
  ...hundreds more tasks, each creating and destroying threads

With Thread Pool:
  Thread 1: [run task 1][run task 4][run task 7]...  (reused!)
  Thread 2: [run task 2][run task 5][run task 8]...  (reused!)
  Thread 3: [run task 3][run task 6][run task 9]...  (reused!)
  ...threads stay alive and pick up tasks from a queue
```

A **thread pool** pre-creates a fixed set of threads and reuses them. Tasks are submitted to a queue, and worker threads pick them up.

---

## Architecture of a Thread Pool

```
                    ┌──────────────────────┐
  submit(task) ───► │     Task Queue       │
                    │ [task][task][task]... │
                    └─────────┬────────────┘
                              │
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
        ┌──────────┐   ┌──────────┐   ┌──────────┐
        │ Worker 1 │   │ Worker 2 │   │ Worker 3 │
        │ (thread) │   │ (thread) │   │ (thread) │
        └──────────┘   └──────────┘   └──────────┘
```

Components:
1. **Task Queue** — Stores pending work
2. **Worker Threads** — Pick up and execute tasks
3. **Synchronization** — Mutex + condition variable to coordinate

---

## Simple Thread Pool Implementation

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>
#include <future>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; i++) {
            workers.emplace_back([this]() {
                workerLoop();
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();

        for (auto& worker : workers) {
            worker.join();
        }
    }

    // Submit a task and get a future for the result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using ReturnType = decltype(f(args...));

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tasks.emplace([task]() { (*task)(); });
        }

        condition.notify_one();
        return result;
    }

private:
    void workerLoop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queueMutex);
                condition.wait(lock, [this]() {
                    return stop || !tasks.empty();
                });

                if (stop && tasks.empty()) return;

                task = std::move(tasks.front());
                tasks.pop();
            }

            task();  // Execute outside the lock
        }
    }

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};
```

### Using the Thread Pool

```cpp
int main() {
    ThreadPool pool(4);  // 4 worker threads

    // Submit various tasks
    auto future1 = pool.submit([](int a, int b) {
        return a + b;
    }, 10, 20);

    auto future2 = pool.submit([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return std::string("Hello from pool!");
    });

    auto future3 = pool.submit([](int x) {
        return x * x;
    }, 7);

    // Get results
    std::cout << "Sum: " << future1.get() << std::endl;         // 30
    std::cout << "Message: " << future2.get() << std::endl;     // Hello from pool!
    std::cout << "Square: " << future3.get() << std::endl;      // 49

    return 0;
    // Pool destructor joins all threads
}
```

---

## Simpler Thread Pool (Without Return Values)

If you don't need return values, the implementation is simpler:

```cpp
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <vector>

class SimpleThreadPool {
public:
    explicit SimpleThreadPool(size_t numThreads) : stop(false) {
        for (size_t i = 0; i < numThreads; i++) {
            workers.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        cv.wait(lock, [this]{ return stop || !tasks.empty(); });

                        if (stop && tasks.empty()) return;

                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    task();
                }
            });
        }
    }

    void submit(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            tasks.push(std::move(task));
        }
        cv.notify_one();
    }

    ~SimpleThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        for (auto& w : workers) {
            w.join();
        }
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop;
};

int main() {
    SimpleThreadPool pool(3);

    for (int i = 0; i < 10; i++) {
        pool.submit([i]() {
            std::cout << "Task " << i << " running on thread "
                      << std::this_thread::get_id() << std::endl;
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    return 0;
}
```

---

## Practical Example: Parallel Image Processing

```cpp
#include <iostream>
#include <vector>
#include <future>

// Simulate processing an image tile
std::string processTile(int row, int col) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return "Tile(" + std::to_string(row) + "," + std::to_string(col) + ") done";
}

int main() {
    ThreadPool pool(std::thread::hardware_concurrency());

    const int ROWS = 4, COLS = 4;
    std::vector<std::future<std::string>> results;

    // Submit all tile processing tasks
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            results.push_back(pool.submit(processTile, r, c));
        }
    }

    // Collect results
    for (auto& fut : results) {
        std::cout << fut.get() << std::endl;
    }

    return 0;
}
```

---

## Practical Example: Parallel Web Requests (Simulated)

```cpp
#include <iostream>
#include <vector>
#include <future>
#include <string>

std::string fetchUrl(const std::string& url) {
    // Simulating network request
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return "Response from " + url;
}

int main() {
    ThreadPool pool(4);

    std::vector<std::string> urls = {
        "https://api.example.com/users",
        "https://api.example.com/posts",
        "https://api.example.com/comments",
        "https://api.example.com/albums",
        "https://api.example.com/photos",
    };

    std::vector<std::future<std::string>> futures;
    for (const auto& url : urls) {
        futures.push_back(pool.submit(fetchUrl, url));
    }

    // All requests run in parallel!
    for (auto& fut : futures) {
        std::cout << fut.get() << std::endl;
    }

    return 0;
}
```

---

## Thread Pool Sizing

How many threads should a pool have?

| Workload Type | Recommended Pool Size |
|---------------|----------------------|
| CPU-bound (computation) | `std::thread::hardware_concurrency()` |
| I/O-bound (network, disk) | `2 × hardware_concurrency()` or more |
| Mixed | Somewhere in between |

```cpp
// CPU-bound: one thread per core
size_t poolSize = std::thread::hardware_concurrency();

// I/O-bound: more threads (they'll be waiting on I/O anyway)
size_t poolSize = std::thread::hardware_concurrency() * 2;

// Fallback if hardware_concurrency() returns 0
size_t poolSize = std::max(1u, std::thread::hardware_concurrency());
```

---

## Thread Pool vs `std::async`

| Feature | Thread Pool | `std::async` |
|---------|-------------|-------------|
| Thread reuse | ✅ Yes | ❌ May create new thread each time |
| Control over thread count | ✅ You decide | ❌ Implementation decides |
| Task queue | ✅ Built-in | ❌ No queue |
| Complexity | More code | One line |
| Best for | Many short tasks | Few long tasks |

---

## Summary

A thread pool:
1. **Pre-creates** a fixed number of threads
2. **Queues** tasks for execution
3. **Reuses** threads across many tasks
4. **Minimizes** thread creation/destruction overhead
5. **Limits** the number of concurrent threads (prevents oversubscription)

---

**Previous:** [Chapter 8 — Futures, Promises, and Async](08_Futures_Promises_Async.md)
**Next:** [Chapter 10 — Deadlocks and Common Problems](10_Deadlocks_and_Common_Problems.md)
