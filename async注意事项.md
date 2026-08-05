# async注意事项

```cpp
future析构时，async会阻塞，参考如下代码：
void DeadLock() {
    std::mutex  mtx;
    std::cout << "DeadLock begin " << std::endl;
    std::lock_guard<std::mutex>  dklock(mtx);
    {
        std::future<void> futures = std::async(std::launch::async, [&mtx]() {
            std::cout << "std::async called " << std::endl;
            std::lock_guard<std::mutex>  dklock(mtx);
            std::cout << "async working...." << std::endl;
            });
    }

    std::cout << "DeadLock end " << std::endl;
}

这样会造成死锁

详细解析原因，并补充使用async的相关事项
```

这是一个非常经典的 C++ 并发陷阱。你所展示的代码正是 **`std::async` 导致死锁的最典型场景**。

要理解这个死锁，核心在于理解 **`std::future` 的析构特性**，特别是当它由 `std::async` 创建时的特殊行为。

下面为你详细解析死锁原因，并总结使用 `std::async` 的相关注意事项。

---

### 一、 死锁原因详细解析

#### 1. 执行时间线分析

*   **步骤 1 (主线程)**：主线程执行 `std::lock_guard<std::mutex> dklock(mtx);`，成功获取了互斥锁 `mtx`。此时主线程持有锁。
*   **步骤 2 (主线程)**：主线程调用 `std::async(std::launch::async, ...)`，创建了一个新线程（异步任务）。
*   **步骤 3 (主线程)**：局部作用域 `{}` 结束，局部变量 `futures` 即将析构。
*   **步骤 4 (主线程阻塞)**：**这是最关键的一步。** 由 `std::launch::async` 创建的 `std::future`，在其析构函数中会**阻塞等待**异步任务执行完毕。也就是说，主线程停在这里，等待子线程结束。
*   **步骤 5 (子线程执行)**：子线程开始执行 Lambda 函数，打印信息后，执行 `std::lock_guard<std::mutex> dklock(mtx);`，尝试获取互斥锁 `mtx`。
*   **步骤 6 (子线程阻塞)**：因为 `mtx` 正被主线程持有，子线程无法获取，于是子线程阻塞等待主线程释放锁。

#### 2. 死锁形成

*   **主线程**：持有锁 `mtx`，等待子线程执行完毕（`future` 析构阻塞）。
*   **子线程**：等待获取锁 `mtx`，才能继续执行完毕。

主线程等子线程，子线程等主线程的锁。两者互不相让，形成**循环等待**，导致死锁。

#### 3. 核心原理：为什么 `future` 析构会阻塞？

在 C++ 标准中，`std::future` 的析构函数通常只是释放共享状态，**不会阻塞**。但是有一个**特例**：

> 如果 `std::future` 是由 `std::async` 且启动策略为 `std::launch::async` 返回的，并且它是共享状态的最后一个所有者，那么它的析构函数会阻塞，直到异步任务执行完毕。

**为什么 C++ 标准要这样设计？**
为了防止“脱缰的线程”。如果 `future` 析构不等待，由于 `future` 持有任务的共享状态，任务可能还在执行时共享状态就被销毁了，或者任务抛出的异常会丢失。为了保证 `std::async` 语义的完整性（要么拿到结果，要么确保任务安全结束不带病退出），标准强制要求 `async` 返回的 `future` 在析构时等待任务完成。

*(注：如果是通过 `std::promise` 或 `std::packaged_task` 创建的 `future`，析构时**不会**阻塞，只会将状态置为失效。)*

---

### 二、 `std::async` 使用注意事项与最佳实践

基于上述特性，在使用 `std::async` 时需要格外小心，以下是重要的注意事项：

#### 1. 避免在持有锁时等待 `future` (解决死锁的根本)

正如你代码中的问题，永远不要在持有互斥锁的情况下，去析构一个由 `std::async` 返回的 `future`（或者调用 `future.get()` / `future.wait()`）。

**修复方案：**
将 `future` 保存到锁的作用域外部，确保在释放锁之后才析构 `future`。

```cpp
void NoDeadLock() {
    std::mutex mtx;
    std::future<void> futures; // 声明在外部

    std::cout << "NoDeadLock begin " << std::endl;
    {
        std::lock_guard<std::mutex> dklock(mtx); // 获取锁
        // 启动异步任务
        futures = std::async(std::launch::async, [&mtx]() {
            std::cout << "std::async called " << std::endl;
            std::lock_guard<std::mutex> dklock(mtx);
            std::cout << "async working...." << std::endl;
        });
        // 锁即将释放，futures 不会在这里析构
    } // <- 此处 dklock 析构，主线程释放 mtx

    // 子线程此时可以获取到 mtx 并执行
    // futures 在函数结束时析构，此时才阻塞等待子线程完成（子线程早已拿到锁并执行完毕）
    std::cout << "DeadLock end " << std::endl; 
}
```

#### 2. 显式指定启动策略，不要用默认值

`std::async` 的默认启动策略是 `std::launch::async | std::launch::deferred`。
这意味着编译器**有可能选择 `deferred`（延迟执行）**策略。在 `deferred` 策略下，任务根本不会创建新线程，而是等到你调用 `future.get()` 或 `future.wait()` 时，才在**当前线程**同步执行。

**隐患：**
如果你以为它必定异步执行，而在主线程后续代码中直接调用 `get()`，它实际上变成了同步阻塞调用，完全失去了并发的意义，甚至可能因为调用时机的不同导致逻辑错误。

**最佳实践：**
如果你确实需要并发，请**始终显式指定 `std::launch::async`**。
```cpp
auto f = std::async(std::launch::async, /* task */);
```

#### 3. 警惕“隐蔽的”析构阻塞

有时死锁并非像你的例子那样明显发生在同一个函数的局部作用域内，而是发生在临时对象上。

```cpp
std::async(std::launch::async, []{ /* long task */ }); 
// 没有接收返回的 future！
```
这行代码会创建一个临时 `future` 对象，在这条语句结束时，临时对象立即析构，导致**当前线程在此处直接阻塞**，直到任务执行完毕。这实际上退化成了同步调用！

**最佳实践：**
始终将 `std::async` 的返回值保存到一个变量中，直到你确实不需要它并发执行时，再让其析构或显式调用 `wait()`。

#### 4. 注意线程耗尽问题

`std::async` 每次调用（指定 `async` 策略时）底层通常会创建一个新线程（标准并未强制要求使用线程池，大多数实现是直接创建线程）。如果在循环中大量使用 `std::async`，可能会导致系统线程数爆炸，引发资源耗尽崩溃。

**最佳实践：**
对于高频、大量的微小异步任务，**不要使用 `std::async`**，应该使用成熟的**线程池**（Thread Pool）配合 `std::packaged_task` 来实现，以复用线程资源。

#### 5. 异常处理机制

这是 `std::async` 的一个优点。如果异步任务中抛出了未捕获的异常，异常会被存储在 `future` 的共享状态中。当主线程调用 `future.get()` 时，异常会被重新抛出。
**注意：** 如果你不调用 `get()`，仅仅是 `future` 析构，异常会被静默吞掉（即任务失败了你却不知道）。因此，建议总是调用 `get()` 获取结果或异常。

#### 6. 如果不想析构阻塞，怎么办？

如果异步任务是“发射后不管”的，你不希望主线程等待它，由于 `std::future` 析构会阻塞，你不能直接用 `std::async`。
你需要使用分离的 `std::thread`，或者在线程内部捕获任务并处理异常，而不依赖 `future` 机制。
不过 C++20 引入了 `std::jthread`，结合 `std::stop_token` 可以更好地管理这种生命周期，但依然不能直接返回结果。要实现真正“不阻塞析构且能获取结果”的异步，通常需要自己封装或使用第三方库（如 Boost.Asio 的 coroutine）。