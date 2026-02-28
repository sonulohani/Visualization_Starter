# Chapter 2: Creating and Managing Threads

## Ways to Create Threads in C++

There are **5 main ways** to create a thread in C++:

1. Function pointer
2. Lambda expression
3. Function object (functor)
4. Member function
5. `std::async`

Let's explore each one.

---

## 1. Thread from a Function Pointer

The simplest way — pass a regular function to `std::thread`.

```cpp
#include <iostream>
#include <thread>

void printNumbers() {
    for (int i = 1; i <= 5; i++) {
        std::cout << "Number: " << i << std::endl;
    }
}

int main() {
    std::thread t(printNumbers);  // Create thread
    t.join();                      // Wait for it to finish
    return 0;
}
```

### Passing Arguments to Threads

```cpp
#include <iostream>
#include <thread>

void greet(const std::string& name, int times) {
    for (int i = 0; i < times; i++) {
        std::cout << "Hello, " << name << "!" << std::endl;
    }
}

int main() {
    // Arguments are passed AFTER the function
    std::thread t(greet, "Alice", 3);
    t.join();
    return 0;
}
```

**Output:**
```
Hello, Alice!
Hello, Alice!
Hello, Alice!
```

> **Important:** Arguments are **copied** into the thread by default. Even if you pass a reference, the thread stores a copy. Use `std::ref()` to pass actual references.

### Passing References with `std::ref`

```cpp
#include <iostream>
#include <thread>

void increment(int& value) {
    value += 10;
}

int main() {
    int num = 5;

    // ❌ WRONG: This copies num, changes won't be visible
    // std::thread t(increment, num);

    // ✅ CORRECT: Use std::ref to pass by reference
    std::thread t(increment, std::ref(num));
    t.join();

    std::cout << "num = " << num << std::endl;  // Output: num = 15
    return 0;
}
```

### Moving Arguments into Threads

For move-only types like `std::unique_ptr`:

```cpp
#include <iostream>
#include <thread>
#include <memory>

void process(std::unique_ptr<int> ptr) {
    std::cout << "Value: " << *ptr << std::endl;
}

int main() {
    auto ptr = std::make_unique<int>(42);

    // Must use std::move — unique_ptr can't be copied
    std::thread t(process, std::move(ptr));
    t.join();
    // ptr is now nullptr (ownership moved to the thread)
    return 0;
}
```

---

## 2. Thread from a Lambda Expression

Lambdas are the most common way to create threads in modern C++.

```cpp
#include <iostream>
#include <thread>

int main() {
    // Simple lambda
    std::thread t([]() {
        std::cout << "Hello from a lambda thread!" << std::endl;
    });
    t.join();

    // Lambda with captures
    int x = 10;
    std::thread t2([x]() {
        std::cout << "Captured value: " << x << std::endl;
    });
    t2.join();

    // Lambda with reference capture
    int result = 0;
    std::thread t3([&result]() {
        result = 42;
    });
    t3.join();
    std::cout << "Result: " << result << std::endl;  // 42

    return 0;
}
```

### Lambda with Parameters

```cpp
#include <iostream>
#include <thread>

int main() {
    auto worker = [](int id, const std::string& task) {
        std::cout << "Worker " << id << " doing: " << task << std::endl;
    };

    std::thread t1(worker, 1, "Download");
    std::thread t2(worker, 2, "Upload");

    t1.join();
    t2.join();

    return 0;
}
```

---

## 3. Thread from a Function Object (Functor)

A functor is a class/struct that overloads `operator()`.

```cpp
#include <iostream>
#include <thread>

class Counter {
    int limit;
public:
    Counter(int limit) : limit(limit) {}

    // This makes Counter a callable object
    void operator()() const {
        for (int i = 0; i < limit; i++) {
            std::cout << "Count: " << i << std::endl;
        }
    }
};

int main() {
    Counter counter(5);
    std::thread t(counter);  // Passes a COPY of counter
    t.join();

    // ⚠️ Watch out for "most vexing parse":
    // std::thread t(Counter(5));  // This is a function declaration, NOT a thread!
    // Fix with extra parentheses or braces:
    std::thread t2((Counter(5)));  // Extra parentheses
    std::thread t3{Counter(5)};    // Brace initialization (preferred)
    t2.join();
    t3.join();

    return 0;
}
```

---

## 4. Thread from a Member Function

You can run a member function of an object in a separate thread.

```cpp
#include <iostream>
#include <thread>

class Printer {
public:
    void printMessage(const std::string& msg) {
        std::cout << "Message: " << msg << std::endl;
    }

    void printRepeat(const std::string& msg, int times) {
        for (int i = 0; i < times; i++) {
            std::cout << msg << std::endl;
        }
    }
};

int main() {
    Printer printer;

    // Syntax: std::thread(member_function_pointer, object_pointer, args...)
    std::thread t1(&Printer::printMessage, &printer, "Hello!");
    std::thread t2(&Printer::printRepeat, &printer, "Repeat", 3);

    t1.join();
    t2.join();

    return 0;
}
```

> **Note:** The second argument is a pointer to the object. You can also use `std::ref(printer)` or pass a `shared_ptr`.

---

## 5. Using `std::async`

`std::async` launches a function asynchronously and returns a `std::future` to retrieve the result.

```cpp
#include <iostream>
#include <future>

int computeSum(int a, int b) {
    return a + b;
}

int main() {
    // Launch asynchronously
    std::future<int> result = std::async(std::launch::async, computeSum, 10, 20);

    // Do other work here...

    // Get the result (blocks if not ready yet)
    std::cout << "Sum = " << result.get() << std::endl;  // Sum = 30

    return 0;
}
```

> We'll cover `std::async` and `std::future` in detail in Chapter 8.

---

## Multiple Threads

You can create and manage multiple threads using containers.

```cpp
#include <iostream>
#include <thread>
#include <vector>

void worker(int id) {
    std::cout << "Worker " << id << " started" << std::endl;
    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "Worker " << id << " finished" << std::endl;
}

int main() {
    const int NUM_THREADS = 5;
    std::vector<std::thread> threads;

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back(worker, i);
    }

    // Wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "All workers done!" << std::endl;
    return 0;
}
```

**Possible Output** (order may vary):
```
Worker 0 started
Worker 2 started
Worker 1 started
Worker 3 started
Worker 4 started
Worker 0 finished
Worker 1 finished
Worker 2 finished
Worker 3 finished
Worker 4 finished
All workers done!
```

---

## Thread ID

Every thread has a unique ID.

```cpp
#include <iostream>
#include <thread>

void showId() {
    std::cout << "Thread ID: " << std::this_thread::get_id() << std::endl;
}

int main() {
    std::cout << "Main Thread ID: " << std::this_thread::get_id() << std::endl;

    std::thread t1(showId);
    std::thread t2(showId);

    // Get ID from outside the thread
    std::cout << "t1's ID: " << t1.get_id() << std::endl;
    std::cout << "t2's ID: " << t2.get_id() << std::endl;

    t1.join();
    t2.join();

    return 0;
}
```

---

## Hardware Concurrency

Find out how many threads can truly run in parallel:

```cpp
#include <iostream>
#include <thread>

int main() {
    unsigned int cores = std::thread::hardware_concurrency();
    std::cout << "This machine supports " << cores
              << " concurrent threads." << std::endl;
    // Example output: "This machine supports 8 concurrent threads."
    return 0;
}
```

> This typically returns the number of CPU cores (including hyperthreading). Returns `0` if the value is not computable.

---

## Thread Ownership and `std::thread` Properties

`std::thread` is **move-only** — it cannot be copied.

```cpp
#include <iostream>
#include <thread>

void task() {
    std::cout << "Running task" << std::endl;
}

int main() {
    std::thread t1(task);

    // ❌ Cannot copy a thread
    // std::thread t2 = t1;  // Compile error!

    // ✅ Can move a thread (transfers ownership)
    std::thread t2 = std::move(t1);
    // t1 is now empty (not joinable)

    // ✅ Can also return threads from functions
    t2.join();

    return 0;
}
```

### Returning Threads from Functions

```cpp
#include <iostream>
#include <thread>

std::thread createWorker(int id) {
    return std::thread([id]() {
        std::cout << "Worker " << id << " running" << std::endl;
    });
}

int main() {
    std::thread worker = createWorker(42);
    worker.join();
    return 0;
}
```

---

## Summary

| Method | When to Use |
|--------|-------------|
| Function pointer | Simple standalone functions |
| Lambda | Most common; inline code, capturing variables |
| Functor | When you need stateful callable objects |
| Member function | Running methods on existing objects |
| `std::async` | When you need a return value from the thread |

---

**Previous:** [Chapter 1 — Introduction to Threads](01_Introduction_to_Threads.md)
**Next:** [Chapter 3 — Thread Lifecycle: join, detach, and RAII](03_Thread_Lifecycle.md)
