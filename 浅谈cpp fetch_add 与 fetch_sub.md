## 浅谈cpp fetch_add 与 fetch_sub 

在 C++11 引入的 `<atomic>` 库中，`fetch_add` 和 `fetch_sub` 是 `std::atomic` 类模板的两个非常核心的成员函数。它们用于在多线程环境下**原子地**进行加法和减法操作。

这两个函数主要用于整数类型（如 `int`, `long`, `size_t` 等）和指针类型（`std::atomic<T*>`）。

下面为您详细解析这两个函数。

---

### 1. 核心概念：返回值是“旧值”

这是 `fetch_add` 和 `fetch_sub` 最重要的特性：
**它们返回的是修改之前的值**，而不是修改之后的值。

其执行逻辑等价于以下伪代码：
```text
原值 = 当前值;
当前值 = 当前值 +/- 参数;
return 原值;
```
当然，整个过程是硬件级别原子完成的，不会被其他线程打断。

---

### 2. fetch_add 详解

`fetch_add` 用于原子地将一个值加到当前原子变量上。

#### 语法
```cpp
T fetch_add(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept;
```
*   **`arg`**: 要加上的数值。
*   **`order`**: 内存序，默认是顺序一致性（最强保证）。
*   **返回值**: 原子变量在加上 `arg` **之前**的值。

#### 示例代码
```cpp
#include <iostream>
#include <atomic>

int main() {
    std::atomic<int> counter(10);

    int old_val = counter.fetch_add(5); // 原子操作：counter = counter + 5

    std::cout << "Old value: " << old_val << std::endl; // 输出 10
    std::cout << "New value: " << counter.load() << std::endl; // 输出 15

    return 0;
}
```

---

### 3. fetch_sub 详解

`fetch_sub` 用于原子地从当前原子变量中减去一个值。

#### 语法
```cpp
T fetch_sub(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept;
```
*   **`arg`**: 要减去的数值。
*   **`order`**: 内存序。
*   **返回值**: 原子变量在减去 `arg` **之前**的值。

#### 示例代码
```cpp
#include <iostream>
#include <atomic>

int main() {
    std::atomic<int> counter(10);

    int old_val = counter.fetch_sub(3); // 原子操作：counter = counter - 3

    std::cout << "Old value: " << old_val << std::endl; // 输出 10
    std::cout << "New value: " << counter.load() << std::endl; // 输出 7

    return 0;
}
```

---

### 4. 易混淆点：`fetch_add` vs `operator+=` / `operator++`

`std::atomic` 重载了 `+=`, `-=` , `++`, `--` 运算符。它们和 `fetch_xxx` 的区别**仅在于返回值**：

*   `fetch_add` / `fetch_sub`: 返回**旧值**（操作前的值）。
*   `operator+=` / `operator-=`: 返回**新值**（操作后的值）。

```cpp
std::atomic<int> a(10);
std::atomic<int> b(10);

int ret_a = a.fetch_add(5); // ret_a = 10 (旧值), a 变成 15
int ret_b = b += 5;         // ret_b = 15 (新值), b 变成 15
```
*底层实现上，`operator+=` 其实就是调用 `fetch_add` 然后把返回值加上参数再返回。*

---

### 5. 内存序

这两个函数接受 `std::memory_order` 参数。这在高级无锁编程中非常重要：

*   `std::memory_order_relaxed`: 只保证当前操作的原子性，不提供任何同步和排序约束（常用于简单的计数器，如统计请求数）。
*   `std::memory_order_acquire`: 用于读取时，保证该操作之后的读写操作不会被重排到该操作之前。
*   `std::memory_order_release`: 用于写入时，保证该操作之前的读写操作不会被重排到该操作之后。
*   `std::memory_order_acq_rel`: **`fetch_add` 和 `fetch_sub` 最常用的自定义内存序**。因为它们是“读-改-写”(RMW)操作，既读取又写入，使用 `acq_rel` 可以同时保证 acquire 语义和 release 语义（常用于实现自旋锁或无锁队列）。
*   `std::memory_order_seq_cst`: 默认值，最强保证，全局顺序一致。

**性能提示**：如果在不需要强制内存同步的场景下（比如仅仅是多线程累加一个计数器），使用 `memory_order_relaxed` 会比默认的 `seq_cst` 性能好很多。

```cpp
// 宽松内存序：只保证加法原子，不管指令重排
counter.fetch_add(1, std::memory_order_relaxed);
```

---

### 6. 指针类型的特殊用法

当 `std::atomic<T*>` 使用 `fetch_add` 或 `fetch_sub` 时，它们执行的是**指针算术运算**。

```cpp
int arr[5] = {0, 0, 0, 0, 0};
std::atomic<int*> ptr(arr);

// 指针向后移动 2 个 int 的距离
int* old_ptr = ptr.fetch_add(2); 

// old_ptr 指向 arr[0]
// ptr 现在指向 arr[2]
```
这在实现无锁环形队列时非常有用。

---

### 7. 实际应用场景：多线程安全计数器

这是最常见的使用场景。假设有多个线程同时向一个变量进行加减操作，使用 `fetch_add` 和 `fetch_sub` 可以避免数据竞争。

```cpp
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

std::atomic<int> shared_counter(0);

void worker(bool is_adder) {
    for (int i = 0; i < 10000; ++i) {
        if (is_adder) {
            shared_counter.fetch_add(1, std::memory_order_relaxed);
        } else {
            shared_counter.fetch_sub(1, std::memory_order_relaxed);
        }
    }
}

int main() {
    std::vector<std::thread> threads;
    
    // 5个线程加，5个线程减
    for (int i = 0; i < 5; ++i) threads.emplace_back(worker, true);
    for (int i = 0; i < 5; ++i) threads.emplace_back(worker, false);

    for (auto& t : threads) t.join();

    std::cout << "Final counter value: " << shared_counter.load() << std::endl; 
    // 结果必定是 0，如果不用原子操作，结果将是不可预期的随机数。
    return 0;
}
```

### 总结

*   `fetch_add` / `fetch_sub` 是原子的加减法。
*   返回**旧值**。
*   支持整数和指针类型。
*   可配合 `memory_order` 优化性能。
*   如果你想获取操作后的新值，直接使用 `+=` 或 `-=` 运算符更方便。



