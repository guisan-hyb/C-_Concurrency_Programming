## 小结C++并发API

在C++11中，并发编程引入了一套基于**“期望”**的高级抽象机制。这套机制将“任务的执行”与“结果的获取”进行了优雅的解耦。

要理解它们之间的联系，最核心的概念是**共享状态**。这五个组件都是围绕共享状态展开的。下面先分别详细总结，然后再进行深度的对比与联系。

---

### 一、 详细总结

#### 1. `std::promise`（承诺/生产者）
*   **角色**：异步结果的**生产者**。
*   **原理**：`promise` 对象内部持有一个共享状态。它允许在一个线程中设置一个值（或异常），然后另一个线程可以通过与之关联的 `future` 来获取这个值。
*   **核心接口**：
    *   `set_value()`：设置正常结果。
    *   `set_exception()`：设置异常结果。
    *   `get_future()`：返回与之关联的 `future` 对象，用于将共享状态交由消费者读取。
*   **应用场景**：底层线程间通信，例如将后台线程的计算结果或捕获的异常传递给主线程。

#### 2. `std::future`（期望/消费者）
*   **角色**：异步结果的**消费者**。
*   **原理**：`future` 是访问共享状态的只读通道。调用 `get()` 方法会阻塞当前线程，直到共享状态被生产者（promise/async/packaged_task）就绪。
*   **核心接口**：
    *   `get()`：阻塞等待并获取结果。**注意：`get()` 只能调用一次**，调用后共享状态被销毁，`future` 变为无效。
    *   `wait()`：阻塞等待结果就绪，但不读取。
    *   `valid()`：检查是否与共享状态关联。
*   **应用场景**：获取异步任务的返回值。

#### 3. `std::shared_future`（共享期望/多消费者）
*   **角色**：支持多个消费者的异步结果访问器。
*   **原理**：由于 `std::future` 的 `get()` 只能调用一次（因为结果是按移动语义取出的），如果有多个线程需要等待并获取同一个异步结果，`future` 就无能为力了。`shared_future` 允许多个线程共享同一个共享状态，每个线程都可以调用 `get()` 获取结果的拷贝。
*   **获取方式**：通常由一个 `std::future` 通过 `share()` 方法转换而来。
*   **应用场景**：广播模式，例如多个工作线程等待同一个初始化任务完成（如配置文件加载）后再开始工作。

#### 4. `std::packaged_task`（打包任务/包装器）
*   **角色**：可调用对象的包装器，连接了任务和异步结果。
*   **原理**：它将一个普通的可调用对象（如函数、Lambda表达式）包装起来，并自动为其创建一个共享状态。当包装的任务被执行时，其返回值或抛出的异常会自动存入这个共享状态中。
*   **核心接口**：
    *   构造函数：接受一个可调用对象。
    *   `get_future()`：获取关联的 `future`。
    *   `operator()` / `make_ready_at_thread_exit()`：执行被包装的任务。
*   **应用场景**：当你已经有了现成的函数，想把它扔到线程池里异步执行，并需要获取返回值时。它比手动用 `promise` 传值更方便。

#### 5. `std::async`（异步启动器/高层接口）
*   **角色**：最顶层的异步任务启动接口。
*   **原理**：它是一个函数模板，接受一个可调用对象和参数，自动完成“创建任务、创建共享状态、启动线程执行（或延迟执行）”的全部流程，并直接返回一个 `future` 对象。
*   **启动策略（`std::launch`）**：
    *   `std::launch::async`：立即在新线程中异步执行。
    *   `std::launch::deferred`：延迟执行，直到调用关联 `future` 的 `get()` 或 `wait()` 时，才在当前线程同步执行。
    *   默认策略（`async | deferred`）：由编译器/运行时决定，通常不推荐直接用默认，应显式指定。
*   **应用场景**：需要并发执行任务并获取结果的最快、最简单的写法，无需手动管理线程或 `promise`。

---

### 二、 对比与联系

这五者的关系可以从**抽象层级**和**数据流向**两个维度来理解。

#### 1. 抽象层级的对比（从底到顶）

| 组件                     | 抽象层级   | 职责                 | 开发者需要关心什么                                           |
| :----------------------- | :--------- | :------------------- | :----------------------------------------------------------- |
| **`promise` + `future`** | **最底层** | 纯粹的线程间数据通道 | 需要手动创建线程、手动调用 `set_value` 传递结果。            |
| **`packaged_task`**      | **中间层** | 封装了任务和结果传递 | 需要手动创建线程去执行这个 `packaged_task`，但不用手动 `set_value` 了。 |
| **`async`**              | **最高层** | 一站式异步方案       | 不需要手动创建线程，只需提供函数和参数，直接拿 `future`。    |

*   **向下兼容**：`async` 内部通常是通过 `packaged_task` 实现的，而 `packaged_task` 内部则是通过 `promise` 实现的。

#### 2. 生产者-消费者模型联系

在共享状态的视角下，它们扮演着明确的角色：

*   **生产端（写入共享状态）**：
    *   `std::promise`：手动写入（`set_value()`）。
    *   `std::packaged_task`：调用任务执行后自动写入。
    *   `std::async`：启动新线程执行后自动写入。
*   **消费端（读取共享状态）**：
    *   `std::future`：单消费者读取（一次性）。
    *   `std::shared_future`：多消费者读取（可多次读，要求结果可拷贝）。

#### 3. `future` vs `shared_future` 对比

| 特性         | `std::future`               | `std::shared_future`            |
| :----------- | :-------------------------- | :------------------------------ |
| **所有权**   | 独占（Move-only，不可拷贝） | 共享（可拷贝）                  |
| `get()` 行为 | 移动出结果，调用一次后失效  | 返回结果的引用/拷贝，可多次调用 |
| **适用场景** | 单一等待者获取一次性结果    | 多个线程需要等待同一个结果      |

#### 4. 代码演示：同一任务的四种实现方式

为了更直观地对比，假设我们要计算一个数的平方并异步获取结果。

**方式一：最底层 `promise` + `future`**
```cpp
#include <iostream>
#include <future>

void compute_promise(std::promise<int>&& p, int x) {
    p.set_value(x * x); // 手动设置值
}

int main() {
    std::promise<int> p;
    std::future<int> f = p.get_future();
    std::thread t(compute_promise, std::move(p), 5);
    std::cout << "Result: " << f.get() << std::endl; // 阻塞等待
    t.join();
}
```

**方式二：中间层 `packaged_task`**
```cpp
#include <iostream>
#include <future>

int compute(int x) { return x * x; }

int main() {
    std::packaged_task<int(int)> task(compute); // 包装任务
    std::future<int> f = task.get_future();
    std::thread t(std::move(task), 5); // 扔进线程执行
    std::cout << "Result: " << f.get() << std::endl; // 阻塞等待
    t.join();
}
```

**方式三：最高层 `async`**
```cpp
#include <iostream>
#include <future>

int compute(int x) { return x * x; }

int main() {
    // 一行代码搞定：自动创建线程、自动执行、自动返回 future
    std::future<int> f = std::async(std::launch::async, compute, 5);
    std::cout << "Result: " << f.get() << std::endl;
}
```

**方式四：多消费者 `shared_future`**
```cpp
#include <iostream>
#include <future>

int compute(int x) { return x * x; }

int main() {
    std::future<int> f = std::async(std::launch::async, compute, 5);
    
    // 转换为 shared_future，原 future 失效
    std::shared_future<int> sf = f.share(); 

    // 启动多个线程读取同一个结果
    auto worker = [sf]() {
        std::cout << "Worker got: " << sf.get() << std::endl;
    };

    std::thread t1(worker);
    std::thread t2(worker);
    
    t1.join();
    t2.join();
}
```

### 三、 总结与选型建议

1.  **简单异步任务**：首选 **`std::async`**。代码最简洁，自动管理线程生命周期（虽然C++11的async可能有资源耗尽风险，但在不涉及复杂线程池的场景下最方便）。
2.  **自定义线程池或任务调度**：使用 **`std::packaged_task`**。线程池可以保存 `packaged_task` 队列，主线程提交任务获取 `future`，工作线程取出任务执行。
3.  **流式/管道式处理或线程间复杂通信**：使用 **`std::promise`**。当任务不是简单的一次性函数调用，而是需要在长时间运行的线程中分多次、或者在不同条件下设置结果甚至异常时，`promise` 提供了最底层的控制力。
4.  **多线程等待同一结果**：使用 **`std::shared_future`**。通过 `future.share()` 获取，解决 `future` 只能 `get` 一次的限制。