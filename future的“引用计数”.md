# future的“引用计数”

在 C++ 中，提到 `std::future` 的“引用计数”，我们需要先澄清一个核心概念：**`std::future` 本身并不直接管理引用计数，真正拥有和管理引用计数的是它底层的“共享状态”。**

理解这一点，是掌握 `future`、`shared_future` 以及异步任务生命周期的关键。

下面为您详细拆解 `future` 与共享状态的引用计数机制。

---

### 一、 核心概念：什么是共享状态？

在 C++ 异步编程模型中，有三个核心角色：
1.  **Provider（生产者）**：`std::promise`、`std::packaged_task`、`std::async`。
2.  **Shared State（共享状态）**：一个在堆上动态分配的内部对象（通常由智能指针管理）。
3.  **Future（消费者）**：`std::future`、`std::shared_future`。

**共享状态**是生产者和消费者之间通信的“桥梁”。它包含了：
*   一个用来存放结果值（或异常）的空间。
*   一个标志位，表示结果是否就绪。
*   一个条件变量或类似机制，用于阻塞和唤醒等待的线程。
*   **一个原子引用计数器**（这就是你问的引用计数）。

### 二、 引用计数的运作机制

共享状态的引用计数，记录了当前有多少个对象（生产者和消费者）正在引用这个共享状态。

#### 1. `std::future`（独占所有权，单消费者）

`std::future` 是**独占的**。它类似于 `std::unique_ptr`。
*   **不可拷贝**：你不能拷贝一个 `std::future`。因为如果允许拷贝，意味着两个 `future` 指向同一个共享状态，而 `future::get()` 的语义是“取出结果并销毁共享状态”，如果有多份拷贝，这会引起悬垂指针或多次释放。
*   **可以移动**：你可以通过 `std::move` 转移 `future` 的所有权。转移后，原 `future` 变为无效，新 `future` 接管共享状态。**注意：移动操作不会改变共享状态的引用计数**，因为引用它的对象总数仍然是 1（原持有者释放，新持有者获得）。

#### 2. `std::shared_future`（共享所有权，多消费者）

`std::shared_future` 是**共享的**。它类似于 `std::shared_ptr`。
*   **可以拷贝**：当你拷贝一个 `shared_future` 时，底层的共享状态的**引用计数会加 1**。
*   **析构减一**：当一个 `shared_future` 被析构时，共享状态的**引用计数减 1**。
*   **安全读取**：`shared_future::get()` 只是返回结果的引用或拷贝，**不会消耗或销毁共享状态**。因此，多个线程可以各自持有一份 `shared_future` 的拷贝，并安全地同时调用 `get()`。

#### 3. 生产者的参与

不仅消费者持有引用，**生产者也持有共享状态的引用**。
*   当你创建一个 `std::promise` 时，内部就生成了一个共享状态，此时引用计数至少为 1（由 `promise` 持有）。
*   当你调用 `promise.get_future()` 时，引用计数变为 2（`promise` 持有一个，`future` 持有一个）。
*   当 `promise` 被析构时（通常发生在设置完结果后），引用计数减 1。如果此时 `future` 也被析构了，引用计数归零，共享状态在堆上被自动销毁。

### 三、 引用计数的生命周期演示

我们通过一段代码和注释，来看引用计数的变化：

```cpp
#include <iostream>
#include <future>
#include <thread>

void shared_state_lifecycle() {
    // 1. 创建 promise，堆上生成共享状态
    // 引用计数 = 1 (promise 持有)
    std::promise<int> p; 

    // 2. 获取 future
    // 引用计数 = 2 (promise 持有, future 持有)
    std::future<int> f = p.get_future(); 

    // 3. 转换为 shared_future (通过 share() 方法)
    // f 内部交出所有权，f 变为无效
    // 引用计数 = 2 (promise 持有, shared_future 持有)
    std::shared_future<int> sf = f.share(); 

    // 4. 拷贝 shared_future
    // 引用计数 = 3 (promise 持有, sf 持有, sf2 持有)
    std::shared_future<int> sf2 = sf; 

    // 5. 启动线程执行任务
    std::thread t([&p]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        p.set_value(42); // 设置结果，唤醒等待者
        // 线程结束时，p 析构
        // 引用计数 = 2 (sf 持有, sf2 持有)
    });
    t.detach();

    // 6. 多个线程安全读取
    std::thread reader1([&sf]() {
        std::cout << "Reader 1 got: " << sf.get() << std::endl; // 安全，不消耗状态
    });
    
    std::thread reader2([&sf2]() {
        std::cout << "Reader 2 got: " << sf2.get() << std::endl; // 安全，不消耗状态
    });

    reader1.join();
    reader2.join();

    // 7. 函数结束，sf 和 sf2 析构
    // 引用计数 = 0，共享状态在堆上被自动销毁，内存释放。
}
```

### 四、 为什么 C++ 不提供 `future::use_count()` 方法？

熟悉 `std::shared_ptr` 的人可能会问：既然有引用计数，为什么 `std::future` 或 `std::shared_future` 没有像 `use_count()` 这样的方法来查看当前有多少个引用？

这是因为 C++ 标准委员会将**共享状态抽象成了一个黑盒**。
标准只规定了行为（例如：`future::get()` 只能调用一次；`shared_future::get()` 可以多次调用），而没有将底层的实现细节（确切的引用计数值）暴露给用户。用户只需要知道：只要还有一个 `shared_future` 指向共享状态，底层的内存和结果就不会被释放。

### 五、 总结

1.  **`std::future` 是 Move-only 的**：它内部并不真正做“引用计数”的加减，而是像 `unique_ptr` 一样转移底层共享状态指针的所有权。
2.  **`std::shared_future` 是基于引用计数的**：它的拷贝和析构会对应底层共享状态引用计数的增减，机制类似 `shared_ptr`。
3.  **共享状态是堆对象**：只要生产者或任意一个消费者还持有它，它就存活；当所有引用方都销毁时，它自动释放。