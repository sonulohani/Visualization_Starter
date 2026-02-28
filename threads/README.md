# C++ Threads — Complete Tutorial

A comprehensive, beginner-friendly tutorial on multithreading in C++ covering everything from basic concepts to advanced patterns.

## Chapters

### Fundamentals
| # | Chapter | Topics |
|---|---------|--------|
| 1 | [Introduction to Threads](01_Introduction_to_Threads.md) | What are threads, process vs thread, concurrency vs parallelism, C++ threading library overview |
| 2 | [Creating and Managing Threads](02_Creating_and_Managing_Threads.md) | Function pointers, lambdas, functors, member functions, `std::async`, passing arguments, `std::ref`, `std::move`, thread IDs, hardware concurrency |
| 3 | [Thread Lifecycle](03_Thread_Lifecycle.md) | `join()`, `detach()`, `joinable()`, RAII thread guards, `std::jthread` (C++20), stop tokens, sleep, yield |

### Synchronization
| # | Chapter | Topics |
|---|---------|--------|
| 4 | [Sharing Data Between Threads](04_Sharing_Data_Between_Threads.md) | Data races, race conditions, critical sections, `thread_local`, immutable data, stack vs heap safety |
| 5 | [Mutexes and Locks](05_Mutexes_and_Locks.md) | `mutex`, `lock_guard`, `unique_lock`, `scoped_lock`, `recursive_mutex`, `shared_mutex`, `timed_mutex`, `call_once` |
| 6 | [Condition Variables](06_Condition_Variables.md) | `condition_variable`, spurious wakeups, predicates, `notify_one`/`notify_all`, producer-consumer queue, bounded buffer, timed waiting |

### Lock-Free & Async
| # | Chapter | Topics |
|---|---------|--------|
| 7 | [Atomic Operations](07_Atomic_Operations.md) | `std::atomic`, load/store, exchange, CAS, fetch operations, `atomic_flag`, memory ordering (seq_cst, acquire/release, relaxed), spinlocks |
| 8 | [Futures, Promises, and Async](08_Futures_Promises_Async.md) | `std::async`, `std::future`, `std::promise`, `std::shared_future`, `std::packaged_task`, exception propagation, void futures |

### Patterns & Problems
| # | Chapter | Topics |
|---|---------|--------|
| 9 | [Thread Pools](09_Thread_Pools.md) | Why thread pools, architecture, full implementation, task queues, pool sizing |
| 10 | [Deadlocks and Common Problems](10_Deadlocks_and_Common_Problems.md) | Deadlock (4 conditions + fixes), livelock, starvation, TOCTOU, false sharing, ABA problem, priority inversion, debugging with TSan |

### Advanced & Reference
| # | Chapter | Topics |
|---|---------|--------|
| 11 | [Advanced Topics](11_Advanced_Topics.md) | C++20: `latch`, `barrier`, `semaphore`, `atomic_ref`, `atomic::wait/notify`; lock-free stack, RCU pattern, memory fences, coroutines |
| 12 | [Best Practices and Patterns](12_Best_Practices_and_Patterns.md) | Golden rules, producer-consumer, active object, monitor, pipeline patterns, common mistakes, thread-safety checklist, quick reference |

## Suggested Learning Path

```
Beginner:     Ch 1 → 2 → 3 → 4
Intermediate: Ch 5 → 6 → 7 → 8
Advanced:     Ch 9 → 10 → 11 → 12
```

## Prerequisites

- Basic C++ knowledge (functions, classes, templates)
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)
- C++20 compiler for advanced topics (GCC 10+, Clang 11+, MSVC 2019 16.8+)

## How to Compile

```bash
# C++17 (most examples)
g++ -std=c++17 -pthread example.cpp -o example

# C++20 (chapters 3, 11 advanced features)
g++ -std=c++20 -pthread example.cpp -o example

# With ThreadSanitizer (for debugging)
g++ -std=c++17 -fsanitize=thread -g example.cpp -o example
```
