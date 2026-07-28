### thread作参数传递困惑点

```cpp
class joining_thread {
	std::thread  _t;
public:
	joining_thread() noexcept = default;
	template<typename Callable, typename ...  Args>
	explicit  joining_thread(Callable&& func, Args&& ...args) :
		_t(std::forward<Callable>(func), std::forward<Args>(args)...) {
	}
	explicit joining_thread(std::thread  t) noexcept : _t(std::move(t)) {}
	joining_thread(joining_thread&& other) noexcept : _t(std::move(other._t)) {}
	joining_thread& operator=(joining_thread&& other) noexcept
	{
		//如果当前线程可汇合，则汇合等待线程完成再赋值
		if (joinable()) {
			join();
		}
		_t = std::move(other._t);
		return *this;
	}

	joining_thread& operator=(std::thread other) noexcept
	{
		//如果当前线程可汇合，则汇合等待线程完成再赋值
		if (joinable()) {
			join();
		}
		_t = std::move(other);
		return *this;
	}

	~joining_thread() noexcept {
		if (joinable()) {
			join();
		}
	}

	void swap(joining_thread& other) noexcept {
		_t.swap(other._t);
	}

	std::thread::id   get_id() const noexcept {
		return _t.get_id();
	}

	bool joinable() const noexcept {
		return _t.joinable();
	}

	void join() {
		_t.join();
	}

	void detach() {
		_t.detach();
	}

	std::thread& as_thread() noexcept {
		return _t;
	}

	const std::thread& as_thread() const noexcept {
		return _t;
	}
};

Q:
joining_thread& operator=(std::thread other) noexcept
thread不是没有拷贝构造吗，为什么这里不用thread&
```

这是一个非常经典且直击C++核心机制的问题。

你的直觉很敏锐：`std::thread` 确实**没有拷贝构造函数**（它被标记为 `= delete`）。但这并不意味着你不能按值传递 `std::thread`，因为 `std::thread` 有**移动构造函数**。

在这个代码中，`operator=(std::thread other)` 使用的是**按值传递**，这里的核心逻辑是利用了 C++ 的移动语义。下面为你详细拆解原因：

### 1. 为什么不使用 `std::thread&`（左值引用）？

如果你写成 `joining_thread& operator=(std::thread& other)`，会导致严重的编译错误。

因为 `std::thread` 是**不可拷贝**的，你通常是这样创建线程的：
```cpp
// 创建一个临时线程对象（右值）
std::thread t([](){ /* do something */ });

joining_thread jt;
jt = t; // 【情况A】如果用的是 std::thread&，这里会报错！
jt = std::thread([](){ /* do something */ }); // 【情况B】这里也会报错！
```
* **情况A**：虽然 `t` 是左值，但因为你试图把它传给 `std::thread&`，函数内部如果要把 `t` 赋给 `_t`，必须进行拷贝（因为 `t` 本身没有标记为即将销毁），而 `std::thread` 不能拷贝，所以编译失败。
* **情况B**：临时对象（右值）不能绑定到非 const 的左值引用 `std::thread&` 上，直接编译失败。

### 2. 为什么不使用 `const std::thread&`（常量左值引用）？

如果改成 `const std::thread& other`，情况B（传临时对象）可以编译通过，但函数内部的代码会报错：
```cpp
joining_thread& operator=(const std::thread& other) noexcept {
    // ...
    _t = other; // 【报错】不能拷贝 std::thread
    return *this;
}
```
因为 `other` 是 const 引用，你无法对它使用 `std::move` 将其转为右值，所以 `_t = other` 会尝试调用拷贝赋值，导致编译失败。

### 3. 为什么不使用 `std::thread&&`（右值引用）？

你可能会想，那用右值引用 `std::thread&& other` 总行了吧？
```cpp
joining_thread& operator=(std::thread&& other) noexcept {
    // ...
    _t = std::move(other);
    return *this;
}
```
这**完全可以**，并且也是合法的 C++ 写法。但这需要调用者总是显式地传递右值：
```cpp
std::thread t([](){});
joining_thread jt;
jt = std::move(t); // 必须显式写 std::move
```

### 4. 为什么作者选择了按值传递 `std::thread other`？

这是 C++11 之后非常推荐的一种编程技巧，称为 **"Sink" 参数（沉水式参数）**。

当函数内部想要**获取对象的所有权**（Ownership）并保存它时，按值传递是最优雅的做法。原因如下：

1. **自动处理左值和右值**：
   * 如果调用者传的是右值（如 `jt = std::thread(...)`），编译器会调用 `std::thread` 的**移动构造函数**将资源转移到参数 `other` 中。
   * 如果调用者传的是左值（如 `jt = std::move(t)`，注意这里必须用 move，因为 thread 不能拷贝），同样会触发移动构造。
2. **接口简洁且安全**：
   参数 `other` 就像是一个“容器”，在进入函数体之前，线程的所有权已经被安全地转移到了 `other` 中。
   在函数体内，`other` 作为一个局部变量，你随便怎么蹂躏它都行，最后 `_t = std::move(other)` 把所有权再转给类成员。
3. **异常安全性更好**：
   资源的转移在函数入口完成。如果后续的操作抛出异常，局部变量 `other` 会在函数退出时自动调用析构函数清理线程资源（如果还没被 move 走的话），不会造成资源泄露。

### 总结

* `std::thread` 不能**拷贝**，但可以**移动**。
* 按值传递 `std::thread other` 并不是在拷贝，而是在触发**移动构造**。
* 不用 `std::thread&` 是因为无法将右值绑定到它，且无法在内部转移所有权。
* 不用 `const std::thread&` 是因为无法转移所有权。
* 按值传递（配合函数体内的 `std::move`）是 C++ 中接收“只移动类型”（如 `std::thread`, `std::unique_ptr` 等）的**标准最佳实践**之一。

  



**Q: 按值传递不是会构造一个副本吗，但thread禁止拷贝**



你的疑惑非常精准，这正是从“传统 C++”过渡到“现代 C++ (C++11及以后)”时最容易碰到的认知冲突。

你的说法在 C++11 之前是完全正确的：**在 C++98/03 中，按值传递必然会调用拷贝构造函数。**

但是，自 C++11 引入了**移动语义**之后，这个规则被扩展了：**按值传递时，编译器会根据传入实参的类型，决定是调用“拷贝构造”还是“移动构造”。**

对于 `std::thread` 这样的对象，按值传递不仅没有违背“禁止拷贝”的原则，反而是在严格利用移动语义来转移所有权。下面为你详细拆解这个过程：

### 1. 按值传递到底发生了什么？

当你写下这样的函数签名时：
```cpp
joining_thread& operator=(std::thread other) noexcept
```
参数 `other` 是一个全新的、局部的 `std::thread` 对象。当函数被调用时， `other` 需要被**构造**出来。怎么构造呢？取决于你在调用时传给它的是什么：

#### 场景 A：传递一个右值（临时对象）
```cpp
jt = std::thread([]{ /* ... */ }); 
```
这里你传了一个临时对象（右值）。编译器看到实参是右值，就会去寻找 `std::thread` 的**移动构造函数**。
`std::thread` 虽然删除了拷贝构造，但它**有移动构造函数**。于是，底层线程的资源（线程句柄等）被“偷”到了参数 `other` 中。**全程没有发生任何拷贝。**

#### 场景 B：传递一个左值（已存在的对象）
```cpp
std::thread t([]{ /* ... */ });
jt = t; // 【注意：这会编译报错！】
```
这里你传了一个左值 `t`。编译器默认会尝试调用**拷贝构造函数**。但是 `std::thread` 禁止拷贝，所以编译器直接报错。
这恰恰证明了“按值传递并没有绕过禁止拷贝的限制”，它依然坚守了 `thread` 不可拷贝的底线。

#### 场景 C：强制转移一个左值
```cpp
std::thread t([]{ /* ... */ });
jt = std::move(t); // 正确
```
通过 `std::move`，你明确告诉编译器：“我不再需要 `t` 了，你可以把它当成右值处理”。此时，编译器再次调用 `std::thread` 的**移动构造函数**，将 `t` 的资源转移到参数 `other` 中。**依然没有发生拷贝。**

### 2. 用一张图理解内部流程

假设你执行了 `jt = std::move(t);`，整个流程是这样的：

1. **调用函数前**：`t` 持有真实线程资源。由于使用了 `std::move`，触发移动构造，`t` 的资源被“偷”走，构造出了形参 `other`。此时 `other` 持有资源，`t` 变成空壳。
2. **进入函数体**：执行 `_t = std::move(other);`，`other` 的资源被“偷”走，赋值给了成员变量 `_t`。
3. **函数返回时**：局部变量 `other` 析构。但因为它早就被 move 走了，是个空壳，所以析构不会做任何有害操作，非常安全。

### 总结

* 在现代 C++ 中，按值传递不再等同于“拷贝”。
* 对于像 `std::thread`、`std::unique_ptr` 这种**不可拷贝但可移动**的类型，按值传递实际上是强制调用者必须提供右值（或者显式 `std::move`），从而触发**移动构造**。
* 如果按值传递能成功编译，就说明底层 100% 走的是移动语义，绝对没有发生拷贝。如果调用者试图不 move 就传进去，编译器会直接拦截报错。



