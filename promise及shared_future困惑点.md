## promise及shared_future困惑点

```cpp
void myFunction(std::promise<int>&& promise) {
	std::this_thread::sleep_for(std::chrono::seconds(1));
	promise.set_value(32);
}

void threadFunction(std::shared_future<int> future) {
	try {
		int result = future.get();
		std::cout << "Result: " << result << std::endl;
	}
	catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
}

void use_shared_future() {
	std::promise<int> promise;
	std::shared_future<int> fut = promise.get_future();
	std::thread t1(myFunction, std::move(promise));

	std::thread t2(threadFunction, fut);
	std::thread t3(threadFunction, fut);

	t1.join();
	t2.join();
	t3.join();
}


为什么要std::move(promise)，thread线程内部不是拷贝了一份右值吗，为什么还要移动
void threadFunction(std::shared_future<int> future)为什么按值传递参数，按左值引用不行吗
```

这是一个非常深入且切中C++多线程编程核心的问题。我们分两部分来解答：

### 1. 为什么要 `std::move(promise)`？`std::thread` 内部不是拷贝了一份右值吗？

这里有一个关键的误解：**`std::promise` 是不可拷贝的，只能移动。**

*   **为什么 `promise` 不可拷贝？**
    `std::promise` 的设计初衷是为了在**一个**特定的线程中设置一个值或异常。它内部持有一个与 `std::future` 共享的共享状态。如果允许拷贝，就会导致两个 `promise` 对象试图向同一个共享状态写入数据，这会引发数据竞争和逻辑混乱。因此，C++ 标准库删除了 `std::promise` 的拷贝构造函数和拷贝赋值运算符。
*   **`std::thread` 构造函数的行为：**
    当你调用 `std::thread t1(myFunction, promise)` 时，`std::thread` 的构造函数确实需要将参数**转移**到新线程的内存空间中（为了避免主线程局部变量销毁导致悬空引用）。但这个转移过程，对于不可拷贝的对象，**必须使用移动语义**。
*   **如果不加 `std::move` 会怎样？**
    `promise` 在这里是一个左值。如果你直接写 `std::thread t1(myFunction, promise)`，编译器会尝试调用 `promise` 的**拷贝构造函数**来将其传递给线程。但因为拷贝构造函数是被删除的，**编译会直接报错**。
*   **加上 `std::move` 的作用：**
    `std::move(promise)` 将 `promise` 强制转换为右值引用。此时，`std::thread` 的内部机制会调用 `std::promise` 的**移动构造函数**，将 `promise` 的所有权（及其内部的共享状态）转移给 `std::thread` 内部存储的参数副本中。然后，新线程启动时，再通过右值引用（`std::promise<int>&&`）将这个内部副本传递给你的 `myFunction`。

**总结：** `std::thread` 内部确实拷贝了一份，但它是通过**移动构造**来拷贝的，而不是传统的拷贝构造。你必须用 `std::move` 告诉编译器：“我允许你移动这个对象，转移它的所有权”。

---

### 2. `void threadFunction(std::shared_future<int> future)` 为什么按值传递参数，按左值引用不行吗？

按值传递在这里是**最安全、最标准**的做法，按左值引用虽然能编译，但会带来极大的风险。

*   **为什么 `std::shared_future` 可以按值传递？**
    与 `std::promise` 不同，`std::shared_future` 是**可拷贝**的。它的设计初衷就是为了被多个消费者共享。当你拷贝一个 `shared_future` 时，它们内部共享同一个同步状态，拷贝的代价非常小（只是增加了一个引用计数），且 `future.get()` 是线程安全的（多个线程可以同时调用同一个 `shared_future` 的 `get()` 而不会引发数据竞争）。
*   **为什么不用左值引用 (`std::shared_future<int>& future`)？**
    如果你使用左值引用，意味着线程函数直接引用主线程中的 `fut` 对象。这会带来两个严重问题：

    1.  **生命周期风险（悬空引用）：**
        `std::thread` 的构造函数默认是按值传递参数的。如果你想让线程函数接受引用，你必须使用 `std::ref` 来包装：
        `std::thread t2(threadFunction, std::ref(fut));`
        这样做的后果是，`t2` 和 `t3` 线程内部持有的是主线程 `fut` 的引用。如果主线程在 `t2`/`t3` 执行 `future.get()` 之前，由于某种原因销毁了 `fut`（比如离开作用域），线程内部就会访问一个已销毁的对象，导致**未定义行为（UB）**，通常是段错误。
    2.  **违背多线程设计的职责分离：**
        按值传递意味着每个线程都拥有自己的一份 `shared_future` 副本。这保证了线程的独立性——它不依赖于主线程中该对象的生命周期。只要线程启动了，它自己手里的那份副本就是安全的，主线程随时可以销毁它自己的 `fut`，而不会影响子线程获取结果。

**总结：**
对于 `std::shared_future`，按值传递不仅性能开销极小，而且**保证了线程拥有独立的状态副本，避免了主线程对象生命周期提前结束导致的悬空引用问题**。这是 C++ 多线程编程中传递可共享对象的最佳实践。

---

### 宏观对比

| 特性               | `std::promise`                  | `std::shared_future`                     |
| :----------------- | :------------------------------ | :--------------------------------------- |
| **角色**           | 生产者                          | 消费者                                   |
| **可拷贝性**       | 不可拷贝                        | 可拷贝                                   |
| **传递给线程方式** | 必须使用 `std::move` 转移所有权 | 按值传递（拷贝内部引用计数）             |
| **为什么这样设计** | 保证只有一个线程能设置值        | 允许多个线程安全地获取值                 |
| **生命周期安全**   | 移动后主对象失效，由子线程独占  | 按值传递后，子线程持有副本，与主对象解耦 |



**Q: cpp11参数按值传递时，如果不能拷贝构造，就会触发移动，而且promise已经有移动函数，为什么不会自动触发移动函数，需要手动move**

这是一个非常核心的C++问题！你提到的“如果不能拷贝构造，就会触发移动”这个理解，在C++的**重载决议**规则中是**不成立**的。

C++编译器**绝不会**仅仅因为拷贝构造函数被删除了，就自动把一个左值当作右值来触发移动构造。

下面我们来彻底剖析原因：

### 1. 核心规则：左值与右值的严格界限

在C++中，类型和值的类别是两套独立的系统：
*   **类型**：`std::promise`
*   **值的类别**：左值或右值

当你声明一个变量 `std::promise<int> promise;` 时，`promise` 有着明确的类型，同时它也是一个**左值**，因为它有名字，你可以取它的地址。

C++的重载决议规则非常死板：
*   **左值**只能绑定到**左值引用**（`T&` 或 `const T&`）。
*   **右值**才能绑定到**右值引用**（`T&&`）。

移动构造函数的签名是 `T(T&& other)`，它**只接受右值**。

### 2. 为什么编译器不“自动帮忙”移动？

假设你写了这样的伪代码：
```cpp
std::promise<int> promise; // promise 是左值
std::thread t1(myFunction, promise); // 试图把左值传进去
```

在 `std::thread` 的内部，它需要将你传进来的参数保存下来。它查看 `promise` 这个实参：
1.  `promise` 是一个左值。
2.  编译器去寻找匹配的构造函数：找到了拷贝构造 `promise(const promise&)` 和移动构造 `promise(promise&&)`。
3.  因为 `promise` 是左值，它**只能匹配**拷贝构造函数。
4.  编译器发现拷贝构造函数是被 `= delete` 删除的。
5.  **编译报错**：`call to deleted constructor`。

**编译器不会进行这样的推理**：“哦，拷贝构造被删了，而且移动构造可用，那我就把左值当作右值，调用移动构造吧”。

为什么C++标准要这样设计？**为了安全。**

移动操作是**掠夺性**的，它会导致源对象变得空心（比如内部的指针被置空）。如果编译器允许仅仅因为“拷贝不行”就偷偷把你的左值移走，这会引发极其恐怖的Bug。想象一下，你之后可能还会用到 `promise`，结果它被编译器偷偷移走了，你完全不知情。

**C++的设计哲学是：不隐式做任何可能改变对象状态的危险操作。** 你必须明确地告诉编译器：“我知道这个对象是左值，但我不再需要它了，请把它当作右值对待”。

### 3. `std::move` 的真正作用

`std::move` 本质上**不移动任何东西**，它只是一个**强制类型转换**。

它的源码极其简单：
```cpp
template <typename T>
constexpr typename std::remove_reference<T>::type&& move(T&& t) noexcept {
    return static_cast<typename std::remove_reference<T>::type&&>(t);
}
```

当你写 `std::move(promise)` 时，你其实是在向编译器声明：“**请无条件地把这个左值强制转换为右值引用**”。

有了这个强制转换：
1.  `std::move(promise)` 产生了一个右值（严格说是将亡值 xvalue）。
2.  编译器再次进行重载决议，发现这个右值可以完美匹配 `promise(promise&&)`。
3.  移动构造函数被成功触发。

### 4. 什么时候会“自动触发”移动？

你说的“不能拷贝就触发移动”，其实发生在一种特殊情况：**当对象本身就是右值时。**

```cpp
std::promise<int> createPromise() {
    std::promise<int> p;
    // ...
    return p; // 这里可能会触发移动（或者RVO/NRVO优化直接省略拷贝/移动）
}
```

在 `return p` 时，如果编译器没有做返回值优化（RVO），它会将 `p` 视为将亡值（右值），从而自动触发移动构造，而不是拷贝构造。这是C++11特意规定的例外（因为函数局部变量马上就要销毁了，移动它是安全的）。

### 总结

*   **左值就是左值**，编译器永远优先尝试拷贝它。
*   如果拷贝被删除，**编译器会报错**，而不会自作主张去调用移动构造。
*   **`std::move` 是程序员的承诺**：它告诉编译器“我放弃这个左值的所有权，请把它当右值看，触发移动构造”。
*   这不是技术上的限制（编译器完全可以实现自动降级），而是C++为了防止隐蔽Bug而设立的**安全防线**。