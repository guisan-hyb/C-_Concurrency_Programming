# 浅谈原子操作其他RMW

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





## 浅谈exchange

好的，我们接着来谈谈 `std::atomic` 的另一个非常核心的成员函数：**`exchange`**。

如果说 `fetch_add` 和 `fetch_sub` 是原子地进行“加减法”，那么 `exchange` 就是原子地进行**“替换并取出”**。它同样属于“读-改-写”（RMW）操作家族。

---

### 1. 核心概念：狸猫换太子

`exchange` 的逻辑非常直接：**将一个新值写入原子变量，并返回该变量在写入之前的旧值。**

其执行逻辑等价于以下伪代码：
```text
原值 = 当前值;
当前值 = 新值;
return 原值;
```
当然，这个过程也是硬件级别原子完成的，不可被打断。

你会发现，它和 `fetch_add` 返回旧值的逻辑是一脉相承的。区别在于：`fetch_add` 是在旧值的基础上做加法，而 `exchange` 是直接无视旧值，强行塞入一个新值。

---

### 2. 语法与特性

#### 语法
```cpp
T exchange(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept;
```
*   **`desired`**: 你想要写入的新值。
*   **`order`**: 内存序，默认是顺序一致性（最强保证）。
*   **返回值**: 原子变量在被替换**之前**的值。

#### 重要特性：支持所有类型
`fetch_add` 和 `fetch_sub` 通常只支持整数和指针。但 **`exchange` 支持任何可平凡复制的类型**（TriviallyCopyable），比如 `bool`、浮点数，甚至是结构体！

---

### 3. 基础示例

#### 整数替换
```cpp
#include <iostream>
#include <atomic>

int main() {
    std::atomic<int> val(100);

    // 将 val 替换为 200，并返回旧值 100
    int old_val = val.exchange(200);

    std::cout << "Old value: " << old_val << std::endl; // 输出 100
    std::cout << "New value: " << val.load() << std::endl; // 输出 200

    return 0;
}
```

#### 布尔类型（常用于状态切换）
```cpp
std::atomic<bool> flag(false);

// 原子地将 true 写入，并获取旧值
bool old_value = flag.exchange(true);

// 如果旧值是 false，说明以前没被占用，是我们成功抢到了状态切换的权力
if (!old_value) { // 或者 old_value == false
    // 成功抢到权力，执行相关逻辑
    std::cout << "Success! We got the lock/power." << std::endl;
} else {
    // 旧值是 true，说明之前已经被别人占用了
    std::cout << "Failed. Someone else already had it." << std::endl;
}
```

---

### 4. 为什么不直接用 `store`？

你可能会问：既然我只是想写入一个新值，我用 `store` 不就好了吗？为什么要用 `exchange`？

**核心区别在于：你是否需要知道旧值。**
*   `store`: 只写不读（“我只管覆盖，以前是什么我不关心”）。
*   `exchange`: 边写边读（“我要覆盖它，但我必须知道我覆盖掉的是什么”）。

如果你在多线程下既要写入新值又要获取旧值，**绝对不能用 `load()` + `store()`**，因为这两步之间会被其他线程打断。必须用 `exchange` 保证原子性。

---

### 5. `exchange` 与 `operator=` 的区别

`std::atomic` 重载了 `=` 运算符。它们的区别同样是**返回值**：

*   `exchange`: 返回**旧值**。
*   `operator=`: 返回**新值**（即你赋的值）。

```cpp
std::atomic<int> a(10);
std::atomic<int> b(10);

int ret_a = a.exchange(5); // ret_a = 10 (旧值), a 变成 5
int ret_b = b = 5;         // ret_b = 5  (新值), b 变成 5
```

---

### 6. 经典应用场景

#### 场景 1：实现自旋锁

`exchange` 是实现自旋锁最经典的底层原语。

```cpp
class SpinLock {
    std::atomic<bool> flag_{false};
public:
    void lock() {
        // 尝试将 false 替换为 true。
        // 如果旧值是 false，说明以前没被锁，exchange 返回 false，!false == true，跳出循环。
        // 如果旧值是 true，说明以前就被锁了，exchange 返回 true，!true == false，继续死循环。
        while (flag_.exchange(true, std::memory_order_acquire)) {
            // 死循环等待 (自旋)
        }
    }
    void unlock() {
        // 解锁只需直接设为 false 即可
        flag_.store(false, std::memory_order_release);
    }
};
```

#### 场景 2：安全地移交所有权 / 指针交接

假设有一个单生产者、单消费者的无锁队列，或者你需要在线程间安全地传递一个对象的所有权。

```cpp
std::atomic<Widget*> active_widget{nullptr};

// 线程 A (生产者)：创建新 widget，替换掉旧的，并拿到旧 widget 准备清理
Widget* new_w = new Widget();
Widget* old_w = active_widget.exchange(new_w);
delete old_w; // 安全清理旧对象

// 线程 B (消费者)：如果拿到非空指针，就开始处理
Widget* current = active_widget.exchange(nullptr); // 取出后置空
if (current) {
    current->do_something();
}
```

#### 场景 3：一次性状态切换

比如某个资源只能被初始化一次，多个线程尝试去初始化，但只有一个能成功。

```cpp
std::atomic<bool> initialized{false};

void init_if_needed() {
    // 原子地将 false 替换为 true
    // 只有第一个调用此函数的线程，exchange 会返回 false
    if (!initialized.exchange(true)) {
        // 执行真正的初始化工作
        do_expensive_init();
    }
}
```

---

### 7. 性能考量与替代方案

在 x86 架构上，`exchange` 通常会被编译为带有 `LOCK` 前缀的指令（如 `XCHG`，它自带 `LOCK` 语义），它是一个完整的内存屏障（即使你传入了 `memory_order_relaxed`，在底层硬件层面依然有较大开销）。

**优化提示：CAS (Compare-Exchange)**
如果你只是想把旧值换成新值，但前提是“**只有当旧值等于某个预期值时才换**”，那么 `exchange` 就显得“太暴力”了。此时应该使用更高级、更常用的原子操作：**`compare_exchange_weak` / `compare_exchange_strong`**（即 CAS 操作）。我们后续可以再详细谈谈 CAS。



### 顺便一提：Compare-Exchange (CAS) 的优势

其实在 C++ 中，如果我们的逻辑是 **“只有当它是 false 时，我才把它变成 true，并认为我抢到了权力”** ，使用 `exchange` 并不是最完美的做法。

使用 `exchange(true)` 会导致：即使它之前已经是 `true`，我们也会强制再写入一次 `true`（虽然结果没变，但在底层硬件层面会产生不必要的写操作，可能会影响性能，尤其是在缓存一致性协议上）。

对于这种 **“条件替换”** 的场景，更标准且高效的做法是使用 **CAS (Compare-Exchange)**：

```cpp
std::atomic<bool> flag(false);

bool expected = false;

// 如果 flag 当前的值等于 expected (false)，
// 则将 flag 修改为 true，并返回 true (表示修改成功)。
// 如果 flag 当前的值不等于 expected (比如已经是 true)，
// 则不修改 flag，把 flag 当前的真实值写入 expected，并返回 false。
if (flag.compare_exchange_strong(expected, true)) {
    // 成功从 false 变成 true，抢到权力
    std::cout << "Success!" << std::endl;
} else {
    // expected 现在变成了 true (因为别人已经改过它了)
    std::cout << "Failed. It was already true." << std::endl;
}
```



