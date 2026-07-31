## std::bind困惑点

```cpp
#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include <atomic>
#include <condition_variable>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

class ThreadPool  {
public:
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool&        operator=(const ThreadPool&) = delete;

    static ThreadPool& instance() {
        static ThreadPool ins;
        return ins;
    }

    using Task = std::packaged_task<void()>;


    ~ThreadPool() {
        stop();
    }

    template <class F, class... Args>
    auto commit(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using RetType = decltype(f(args...));
        if (stop_.load())
            return std::future<RetType>{};

        auto task = std::make_shared<std::packaged_task<RetType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<RetType> ret = task->get_future();
        {
            std::lock_guard<std::mutex> cv_mt(cv_mt_);
            tasks_.emplace([task] { (*task)(); });
        }
        cv_lock_.notify_one();
        return ret;
    }

    int idleThreadCount() {
        return thread_num_;
    }

private:
    ThreadPool(unsigned int num = 5)
        : stop_(false) {
            {
                if (num < 1)
                    thread_num_ = 1;
                else
                    thread_num_ = num;
            }
            start();
    }
    void start() {
        for (int i = 0; i < thread_num_; ++i) {
            pool_.emplace_back([this]() {
                while (!this->stop_.load()) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> cv_mt(cv_mt_);
                        this->cv_lock_.wait(cv_mt, [this] {
                            return this->stop_.load() || !this->tasks_.empty();
                        });
                        if (this->tasks_.empty())
                            return;

                        task = std::move(this->tasks_.front());
                        this->tasks_.pop();
                    }
                    this->thread_num_--;
                    task();
                    this->thread_num_++;
                }
            });
        }
    }
    void stop() {
        stop_.store(true);
        cv_lock_.notify_all();
        for (auto& td : pool_) {
            if (td.joinable()) {
                std::cout << "join thread " << td.get_id() << std::endl;
                td.join();
            }
        }
    }

private:
    std::mutex               cv_mt_;
    std::condition_variable  cv_lock_;
    std::atomic_bool         stop_;
    std::atomic_int          thread_num_;
    std::queue<Task>         tasks_;
    std::vector<std::thread> pool_;
};

#endif  // !__THREAD_POOL_H__


int m = 0;
ThreadPool::instance().commit([](int& m) {
	m = 1024;
	std::cout << "inner set m is " << m << std::endl;
	}, m);

std::this_thread::sleep_for(std::chrono::seconds(3));
std::cout << "m is " << m << std::endl;

std::this_thread::sleep_for(std::chrono::seconds(3));
ThreadPool::instance().commit([](int& m) {
	m = 1024;
	std::cout << "inner set m is " << m << std::endl;
	}, std::ref(m));
std::this_thread::sleep_for(std::chrono::seconds(3));
std::cout << "m is " << m << std::endl;

为什么直接传递m不会报错：对于入参要求为左值引用，传递了右值
```



这是一个非常经典且深入的 C++ 问题！

你之所以会感到疑惑，是因为你预期 `std::bind` 会将参数原封不动地传递给 lambda，如果 lambda 需要左值引用 `int&`，而你传了一个“值”，编译器应该因为无法将右值绑定到左值引用而报错。

但实际上**代码并没有报错，且外部的 `m` 并没有被修改（输出仍然是 0）**。这背后的核心原因在于 **`std::bind` 的参数传递机制（值拷贝与调用时的左值转换）**。

下面为你详细剖析原因：

### 1. 为什么直接传递 `m` 不会报错？

在你的 `commit` 函数中，有这样一行代码：
```cpp
auto task = std::make_shared<std::packaged_task<RetType()>>(
    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
);
```

当外部调用 `commit(lambda, m)` 时（注意这里的 `m` 是一个左值），发生了以下步骤：

#### 第一步：`std::bind` 按值拷贝了 `m`
`std::bind` 的设计哲学是**默认按值存储参数**（除非你显式使用 `std::ref` 或 `std::cref`）。
虽然 `Args&&... args` 通过完美转发将 `m` 作为左值引用传给了 `bind`，但 `std::bind` 内部会**将 `m` 拷贝一份**，存放在 `bind` 返回的可调用对象内部。此时，`bind` 对象内部拥有一个 `m` 的副本（我们暂且叫它 `m_copy`）。

#### 第二步：任务执行时，`std::bind` 将内部副本作为左值传递
当线程池的 worker 线程执行 `(*task)()` 时，`std::bind` 返回的对象被调用。
关键点来了：**`std::bind` 在调用被包装的函数时，总是将存储的参数作为左值传递，即使它内部是按值存储的。**
因为 `bind` 对象本身是一个左值（通过 `(*task)()` 调用），所以它内部存储的 `m_copy` 也会被当作左值来传递。

#### 第三步：完美匹配 lambda 的 `int&`
你的 lambda 要求参数 `int& m`（非 const 左值引用）。
由于 `std::bind` 传递的是它内部 `m_copy` 的左值引用，完美匹配了 lambda 的要求！因此，**编译器不会报错**。

### 2. 为什么没有报错，但外部的 `m` 没有被修改？

虽然没报错，但 lambda 修改的是 `std::bind` 内部拷贝出来的那个 `m_copy`，而**不是外部作用域的 `m`**。

整个流程如下：
1. 外部的 `m` (值为 0) 被传入 `commit`。
2. `std::bind` 拷贝出一个新变量 `m_copy` (值为 0)。
3. 线程池执行任务，把 `m_copy` 的引用传给 lambda。
4. lambda 把 `m_copy` 修改为 1024。
5. 任务结束，`m_copy` 随 `bind` 对象销毁。
6. 主线程打印外部的 `m`，依然是 0。

### 3. 为什么使用 `std::ref(m)` 就能成功修改？

当你使用 `std::ref(m)` 时，情况发生了变化：

```cpp
ThreadPool::instance().commit([](int& m) { ... }, std::ref(m));
```

`std::ref(m)` 会生成一个 `std::reference_wrapper<int>` 对象。
`std::bind` 识别到这是一个 `reference_wrapper`，它就**不再进行值拷贝**，而是直接存储了这个引用。
当任务执行时，`std::bind` 会将 `reference_wrapper` 解包成原来的 `int&` 传递给 lambda。
此时，lambda 拿到的就是外部真实 `m` 的左值引用，修改自然会生效。

### 总结与延伸

*   **`std::bind` 的“障眼法”**：`std::bind` 默认按值拷贝参数，但在调用时又总是将参数作为左值传出。这就导致了一个现象：**几乎任何参数传给 `std::bind`，在调用包装函数时都会变成左值**。这就是为什么即使 lambda 要求 `int&`，传值进去也不会报“无法将右值绑定到左值引用”的编译错误。
*   **C++14 后的最佳实践**：在现代 C++ 中，由于 `std::bind` 的这种行为容易让人迷惑（正如你遇到的这个问题），通常推荐使用 **Lambda 表达式**来替代 `std::bind`。

如果你把 `commit` 改造成支持 C++14 的 lambda 捕获方式（或者你的线程池支持直接传入无参闭包），代码会清晰得多：
```cpp
// 直接在外部捕获 m 的引用，语义非常明确
ThreadPool::instance().commit([&m]() {
    m = 1024;
    std::cout << "inner set m is " << m << std::endl;
});
```

**附：关于你的线程池代码的一个小建议**
在你的 `commit` 函数中：
```cpp
using RetType = decltype(f(args...));
```
这里如果 `f` 是重载函数或者有多个重载的 `operator()`，`decltype(f(args...))` 可能会推导失败。更严谨的写法是使用 `std::invoke_result`：
```cpp
using RetType = std::invoke_result_t<F, Args...>;
```