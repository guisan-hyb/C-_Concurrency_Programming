#define _CRT_SECURE_NO_WARNINGS

#include <thread>
#include <iostream>
#include <string>
#include <chrono>

void thread_work1(std::string str) {
	std::cout << "str is: " << str << std::endl;
}

class background_task {
public:
	void operator()() {
		std::cout << "background_task called" << std::endl;
	}
};

struct func {
	func(int& i):_i(i) {}
	void operator()() {
		for (int k = 0; k < 3; k++) {
			_i = k;
			std::cout << "_i is: " << _i << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}

	int& _i;
};

void oops() {
	int some_local_state = 0;
	func myfunc(some_local_state);
	std::thread func_thread(myfunc);
	//隐患，访问局部变量，局部变量可能会随着}结束而回收或随着主线程退出而回收
	func_thread.detach();
}

void use_join() {
	int some_local_state = 0;
	func my_func(some_local_state);
	std::thread functhread(my_func);
	functhread.join();
}

void catch_exception() {
	int some_local_state = 0;
	func my_func(some_local_state);
	std::thread functhread(my_func);
	try {
		//本线程处理一些事情，例如：
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	catch (const std::exception& e) {
		functhread.join();
		throw;
	}

	functhread.join();
}

class thread_guard {
public:
	explicit thread_guard(std::thread& t) : _t(t) {}
	~thread_guard() {
		//join只能调用一次
		if (_t.joinable()) {
			_t.join();
		}
		std::cout << "this thread destructed" << std::endl;
	}

	thread_guard(const thread_guard&) = delete;
	thread_guard& operator=(const thread_guard&) = delete;

private:
	std::thread& _t;
};

void auto_guard() {
	int some_local_state = 0;
	func my_func(some_local_state);
	std::thread t(my_func);
	thread_guard g(t);
	//本线程做一些事情，例如：
	std::cout << "auto guard finished " << std::endl;
}

void print_str(int i, std::string const& s) {
	std::cout << "i is: " << i << " str is: " << s << std::endl;
}

void danger_oops(int som_param) {
	char buffer[1024];
	sprintf(buffer, "%i", som_param);
	std::thread t(print_str, 3, buffer);
	t.detach();
	std::cout << "danger oops finished " << std::endl;
}

void safe_oops(int som_param) {
	char buffer[1024];
	sprintf(buffer, "%i", som_param);
	std::thread t(print_str, 3, std::string(buffer));
	t.detach();
}

void change_param(int& param) {
	param++;
}

void ref_oops(int some_param) {
	std::cout << "before change, param is: " << some_param << std::endl;
	//需使用引用显示转换
	std::thread t(change_param, std::ref(some_param));
	t.join();
	std::cout << "after change, param is: " << some_param << std::endl;
}

class X
{
public:
	void do_lengthy_work() {
		std::cout << "do_lengthy_work " << std::endl;
	}
};

void bind_class_oops() {
	X my_x;
	std::thread t(&X::do_lengthy_work, &my_x);
	t.join();
}

void deal_unique(std::unique_ptr<int> p) {
	std::cout << "unique ptr data is: " << *p << std::endl;
	(*p)++;

	std::cout << "after, unique ptr data is: " << *p << std::endl;
}

void move_oops() {
	auto p = std::make_unique<int>(100);
	std::thread t(deal_unique, std::move(p));
	t.join();
	//不能再使用p了，p已经被move废弃
    // std::cout << "after unique ptr data is " << *p << std::endl;
}

int main() {
	// 1.线程发起
	std::string hellostr = "hello world!";
	std::thread t1(thread_work1, hellostr);// 通过()初始化并启动一个线程
	t1.join();// 主线程等待子线程退出


	// 2.仿函数作为参数
	// 编译器的理解：
	// t2 是一个函数，返回 std::thread，
	// 参数是一个函数指针：指向返回 background_task、无参数的函数
	// std::thread t2(background_task(*pf)());
	//
	/*std::thread t2(background_task());
	t2.join();*/

	//解决方法：
	//1.可多加一层()
	std::thread t3((background_task()));
	t3.join();
	//2.可使用{}
	std::thread t4{ background_task() };
	t4.join();


	// 3.lambda表达式
	std::thread t5([](std::string str) {
		std::cout << "str is: " << str << std::endl;
	}, hellostr);
	t5.join();


	// 4.detach 注意事项
	oops();
	//防止主线程退出过快，需要停顿一下，让子线程跑起来detach
	std::this_thread::sleep_for(std::chrono::seconds(1));

	
	// 5.join用法
	use_join();


	// 6.捕获异常
	catch_exception();


	// 7.自动守卫
	auto_guard();


	// 8.危险可能存在崩溃
	//当我们定义一个线程变量thread t时，传递给这个线程的参数buffer会被保存到thread的成员变量中
	//而在线程对象t内部启动并运行线程时，参数才会被传递给调用函数print_str
	//而此时buffer可能随着 }运行结束而释放了
	//
	//改进的方式很简单，我们将参数传递给thread时显示转换为string就可以了，
	//这样thread内部保存的是string类型
	danger_oops(100);
	std::this_thread::sleep_for(std::chrono::seconds(1));

	// 安全，提前转化
	safe_oops(100);
	std::this_thread::sleep_for(std::chrono::seconds(1));


	// 9.绑定引用
	ref_oops(100);


	// 10.绑定类成员函数
	bind_class_oops();


	// 11.通过move()传递参数
	move_oops();

	return 0;
}

