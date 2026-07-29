#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <mutex>
#include <chrono>
#include <thread>
#include <stack>

std::mutex mtx1;
int shared_data = 100;

void use_lock() {
	for (;;) {
		mtx1.lock();
		shared_data++;
		std::cout << "current thread is: " << std::this_thread::get_id() << std::endl;
		std::cout << "shared data is: " << shared_data << std::endl;
		mtx1.unlock();
		std::this_thread::sleep_for(std::chrono::microseconds(10));
	}
}

void test_lock() {
	std::thread t1(use_lock);
	std::thread t2([]() {
		for (;;) {
			{
				std::lock_guard<std::mutex> lk_guard(mtx1);
				shared_data--;
				std::cout << "current thread is: " << std::this_thread::get_id() << std::endl;
				std::cout << "shared data is: " << shared_data << std::endl;
			}
			// lock_guard在 } 析构，解锁
			//这样，该线程就不会持有锁睡10微秒了
			std::this_thread::sleep_for(std::chrono::microseconds(10));
		}
	});

	t1.join();
	t2.join();
}


template<typename T>
class threadsafe_stack1 {
public:
	threadsafe_stack1() {}
	threadsafe_stack1(const threadsafe_stack1& other) {
		std::lock_guard<std::mutex> lock(other.m);//防止拷贝过程中other被修改
		_data = other._data;
	}
	threadsafe_stack1& operator=(const threadsafe_stack1&) = delete;

	void push(T new_value) {
		std::lock_guard<std::mutex> lock(m);
		_data.push(std::move(new_value));
	}

	//问题代码
	T pop() {
		std::lock_guard<std::mutex> lock(m);
		auto element = _data.top();
		_data.pop();
		return element;
	}

	//危险
	//一个只读函数返回了一个结果，那么这个结果在没有锁的情况下可能在延迟调用中出现一些问题
	bool empty() const {
		std::lock_guard<std::mutex> lock(m);
		return _data.empty();
	}

private:
	std::stack<T> _data;
	mutable std::mutex m;//这里声明mutable是为了在只读的成员函数中可以加锁(只读的成员函数指const修饰)
};

void test_threadsafe_stack1() {
	threadsafe_stack1<int> safe_stack;
	safe_stack.push(1);

	std::thread t1([&safe_stack]() {
		if (!safe_stack.empty()) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			safe_stack.pop();
		}
	});

	std::thread t2([&safe_stack]() {
		if (!safe_stack.empty()) {
			safe_stack.pop();
		}
	});

	t1.join();
	t2.join();
}


struct empty_stack : public std::exception {
	const char* what() const throw();
};

template<typename T>
class threadsafe_stack {
public:
	threadsafe_stack() {}
	threadsafe_stack(const threadsafe_stack& other) {
		std::lock_guard<std::mutex> lock(other.m);//防止拷贝过程中other被修改
		_data = other._data;
	}
	threadsafe_stack& operator=(const threadsafe_stack&) = delete;

	void push(T new_value) {
		std::lock_guard<std::mutex> lock(m);
		_data.push(std::move(new_value));
	}

	std::shared_ptr<T> pop() {
		std::lock_guard<std::mutex> lock(m);
		if (_data.empty()) return nullptr;

		std::shared_ptr<T> const res(std::make_shared<T>(_data.top()));
		_data.pop();
		return res;
	}

	void pop(T& val) {
		std::lock_guard<std::mutex> lock(m);
		if (_data.empty()) throw empty_stack();

		val = _data.top();
		_data.pop();
	}

	bool empty() const {
		std::lock_guard<std::mutex> lock(m);
		return _data.empty();
	}

private:
	std::stack<T> _data;
	mutable std::mutex m;
};




std::mutex t_lock1;
std::mutex t_lock2;
int m_1 = 0;
int m_2 = 0;

void dead_lock1() {
	for (;;) {
		std::cout << "dead lock1 begin " << std::endl;
		t_lock1.lock();
		m_1 = 1024;
		t_lock2.lock();
		m_2 = 2048;
		//解锁优先邻近的先去解锁
		t_lock2.unlock();
		t_lock1.unlock();
		std::cout << "dead lock2 end " << std::endl;
	}
}

void dead_lock2() {
	for (;;) {
		std::cout << "dead lock2 begin " << std::endl;
		t_lock2.lock();
		m_1 = 1024;
		t_lock1.lock();
		m_2 = 2048;
		//解锁优先邻近的先去解锁
		t_lock1.unlock();
		t_lock2.unlock();
		std::cout << "dead lock1 end " << std::endl;
	}
}

void test_dead_lock() {
	std::thread t1(dead_lock1);
	std::thread t2(dead_lock2);
	t1.join();
	t2.join();
}



//加锁和解锁作为原子操作解耦合，各自只管理自己的功能
void atomic_lock1() {
	std::cout << "lock1 begin lock" << std::endl;
	t_lock1.lock();
	m_1 = 1024;
	t_lock1.unlock();
	std::cout << "lock1 end lock" << std::endl;
}

void atomic_lock2() {
	std::cout << "lock2 begin lock" << std::endl;
	t_lock2.lock();
	m_2 = 2048;
	t_lock2.unlock();
	std::cout << "lock2 end lock" << std::endl;
}

void safe_lock1() {
	while (true) {
		atomic_lock1();
		atomic_lock2();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}

void safe_lock2() {
	while (true) {
		atomic_lock2();
		atomic_lock1();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}

void test_safe_lock() {
	std::thread t1(safe_lock1);
	std::thread t2(safe_lock2);
	t1.join();
	t2.join();
}




//对于要使用两个互斥量，可以同时加锁
//如果不同时加锁，可能会死锁

//假设这是一个很复杂的数据结构，假设我们不建议执行拷贝构造
class some_big_object {
public:
	some_big_object(int data): _data(data) {}
	some_big_object(const some_big_object& other) : _data(other._data) {
		
	}
	some_big_object(const some_big_object&& other) noexcept : _data(std::move(other._data)) {

	}
	some_big_object& operator=(const some_big_object& other) {
		if (this == &other) {
			return *this;
		}

		_data = other._data;
		return *this;
	}
	some_big_object& operator=(const some_big_object&& other) noexcept {
		if (this == &other) {
			return *this;
		}

		_data = std::move(other._data);
		return *this;
	}

	//重载输出运算符
	friend std::ostream& operator<<(std::ostream& os, const some_big_object& big_obj) {
		os << big_obj._data;
		return os;
	}

	//交换数据
	friend void swap(some_big_object& b1, some_big_object& b2) {
		some_big_object tmp = std::move(b1);
		b1 = std::move(b2);
		b2 = std::move(tmp);
	}

private:
	int _data;
};

//假设这是一个结构，包含了复杂的成员对象和一个互斥量
class big_object_mgr {
public:
	big_object_mgr(int data = 0) :_obj(data) {}
	void printInfo() {
		std::cout << "current obj data is: " << _obj << std::endl;
	}

	friend void danger_swap(big_object_mgr& objm1, big_object_mgr& objm2);
	friend void safe_swap(big_object_mgr& objm1, big_object_mgr& objm2);
	friend void safe_swap_scope(big_object_mgr& objm1, big_object_mgr& objm2);


private:
	std::mutex _mtx;
	some_big_object _obj;
};

void danger_swap(big_object_mgr& objm1, big_object_mgr& objm2)
{
	std::cout << "thread [ " << std::this_thread::get_id() << " ] begin" << std::endl;
	if (&objm1 == &objm2) {
		return;
	}

	std::lock_guard<std::mutex> guard1(objm1._mtx);
	//此处为了让死锁必现，先睡一会
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::lock_guard<std::mutex> guard2(objm2._mtx);
	swap(objm1._obj, objm2._obj);
	std::cout << "thread [ " << std::this_thread::get_id() << " ] end" << std::endl;
}

void test_danger_swap() {
	big_object_mgr objm1(15);
	big_object_mgr objm2(100);
	std::thread t1(danger_swap, std::ref(objm1), std::ref(objm2));
	std::thread t2(danger_swap, std::ref(objm2), std::ref(objm1));
	t1.join();
	t2.join();

	objm1.printInfo();
	objm2.printInfo();
}

void safe_swap(big_object_mgr& objm1, big_object_mgr& objm2) {
	std::cout << "thread [ " << std::this_thread::get_id() << " ] begin" << std::endl;
	if (&objm1 == &objm2) {
		return;
	}

	std::lock(objm1._mtx, objm2._mtx);
	//领养锁管理互斥量的解锁
	std::lock_guard<std::mutex> guard1(objm1._mtx, std::adopt_lock);
	std::lock_guard<std::mutex> guard2(objm2._mtx, std::adopt_lock);
	swap(objm1._obj, objm2._obj);
	std::cout << "thread [ " << std::this_thread::get_id() << " ] end" << std::endl;
}

void test_safe_swap() {
	big_object_mgr objm1(15);
	big_object_mgr objm2(100);
	std::thread t1(safe_swap, std::ref(objm1), std::ref(objm2));
	std::thread t2(safe_swap, std::ref(objm2), std::ref(objm1));
	t1.join();
	t2.join();

	objm1.printInfo();
	objm2.printInfo();
}

void safe_swap_scope(big_object_mgr& objm1, big_object_mgr& objm2) {
	std::cout << "thread [ " << std::this_thread::get_id() << " ] begin" << std::endl;
	if (&objm1 == &objm2) {
		return;
	}

	std::scoped_lock guard(objm1._mtx, objm2._mtx);
	//等价于
	//std::scoped_lock<std::mutex, std::mutex> guard(objm1._mtx, objm2._mtx);
	swap(objm1._obj, objm2._obj);
	std::cout << "thread [ " << std::this_thread::get_id() << " ] end" << std::endl;
}

void test_safe_swap_scope() {
	big_object_mgr objm1(15);
	big_object_mgr objm2(100);
	std::thread t1(safe_swap_scope, std::ref(objm1), std::ref(objm2));
	std::thread t2(safe_swap_scope, std::ref(objm2), std::ref(objm1));
	t1.join();
	t2.join();

	objm1.printInfo();
	objm2.printInfo();
}






//层级锁
class hierarchical_mutex {
public:
	explicit hierarchical_mutex(unsigned long value): _hierarchy_value(value), _previous_hierarchy_value(0) {}

	hierarchical_mutex(const hierarchical_mutex&) = delete;
	hierarchical_mutex& operator=(const hierarchical_mutex&) = delete;

	void lock() {
		check_for_hierarchy_violation();
		_internal_mutex.lock();
		update_hierarchy_violation();
	}

	void unlock() {
		if (_this_thread_hierarchy_value != _hierarchy_value) {
			throw std::logic_error("mutex hierarchy violated");
		}

		_this_thread_hierarchy_value = _previous_hierarchy_value;
		_internal_mutex.unlock();
	}

	bool try_lock() {
		check_for_hierarchy_violation();
		if (!_internal_mutex.try_lock()) {
			return false;
		}

		update_hierarchy_violation();
		return true;
	}

private:
	std::mutex _internal_mutex;
	//当前层级值
	unsigned long _hierarchy_value;
	//上一级层级值
	unsigned long _previous_hierarchy_value;
	//本线程记录的层级值
	static thread_local unsigned long _this_thread_hierarchy_value;

	void check_for_hierarchy_violation() {
		if (_this_thread_hierarchy_value <= _hierarchy_value) {
			throw std::logic_error("mutex hierarchy violated");
		}
	}

	void update_hierarchy_violation() {
		_previous_hierarchy_value = _this_thread_hierarchy_value;
		_this_thread_hierarchy_value = _hierarchy_value;
	}
};

thread_local unsigned long hierarchical_mutex::_this_thread_hierarchy_value(ULONG_MAX);

void test_hierarchy_lock() {
	hierarchical_mutex hmtx1(1000);
	hierarchical_mutex hmtx2(500);

	std::thread t1([&]() {
		hmtx1.lock();
		hmtx2.lock();
		hmtx2.unlock();
		hmtx2.lock();
	});

	std::thread t2([&]() {
		hmtx2.lock();
		hmtx1.lock();
		hmtx1.unlock();
		hmtx2.unlock();
	});

	t1.join();
	t2.join();
}

int main() {
	//test_lock();
	//test_threadsafe_stack1();
	//test_dead_lock();
	//test_safe_lock();

	//test_danger_swap();
	//test_safe_swap();
	//test_safe_swap_scope();

	test_hierarchy_lock();

	return 0;
}

