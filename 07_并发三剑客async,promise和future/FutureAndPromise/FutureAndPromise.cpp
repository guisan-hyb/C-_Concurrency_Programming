#include <iostream>
#include <future>
#include <chrono>
#include <string>
#include "thread_pool.h"

// 定义一个异步任务
std::string fetchDataFromDB(std::string query) {
	// 模拟一个异步任务，比如从数据库中获取数据
	std::this_thread::sleep_for(std::chrono::seconds(5));
	return "Data: " + query;
}

void use_async() {
	// 使用 std::async 异步调用 fetchDataFromDB
	std::future<std::string> resultFromDB = std::async(std::launch::async, fetchDataFromDB, "Data");

	// 在主线程中做其他事情
	std::cout << "Doing something else..." << std::endl;

	// 从 future 对象中获取数据
	std::string dbData = resultFromDB.get();
	std::cout << dbData << std::endl;

}

int my_task() {
	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::cout << "my task run 5 s" << std::endl;
	return 42;
}

void use_package() {
	// 创建一个包装了任务的 std::packaged_task 对象  
	std::packaged_task<int()> task(my_task);

	// 获取与任务关联的 std::future 对象  
	std::future<int> result = task.get_future();

	// 在另一个线程上执行任务  
	std::thread t(std::move(task));
	t.detach(); // 将线程与主线程分离，以便主线程可以等待任务完成  

	// 等待任务完成并获取结果  
	int value = result.get();
	std::cout << "The result is: " << value << std::endl;

}

void myFunction(std::promise<int>&& promise) {
	// 模拟一些工作
	std::this_thread::sleep_for(std::chrono::seconds(1));
	promise.set_value(42); // 设置 promise 的值
}

void threadFunction(std::shared_future<int> future) {
	try {
		int result = future.get();
		std::cout << "Result: " << result << std::endl;
	}
	catch (const std::future_error& e) {
		std::cout << "Future error: " << e.what() << std::endl;
	}
}

void use_shared_future() {
	std::promise<int> promise;
	std::shared_future<int> future = promise.get_future();

	std::thread myThread1(myFunction, std::move(promise)); // 将 promise 移动到线程中

	// 使用 share() 方法获取新的 shared_future 对象  

	std::thread myThread2(threadFunction, future);

	std::thread myThread3(threadFunction, future);

	myThread1.join();
	myThread2.join();
	myThread3.join();
}

void use_shared_future_error() {
	std::promise<int> promise;
	std::shared_future<int> future = promise.get_future();

	std::thread myThread1(myFunction, std::move(promise)); // 将 promise 移动到线程中

	// 使用 share() 方法获取新的 shared_future 对象  

	std::thread myThread2(threadFunction, std::move(future));

	std::thread myThread3(threadFunction, std::move(future));

	myThread1.join();
	myThread2.join();
	myThread3.join();
}


void set_value(std::promise<int> prom) {
	// 设置 promise 的值
	std::this_thread::sleep_for(std::chrono::seconds(5));
	prom.set_value(10);
	std::cout << "promise set value success" << std::endl;
}

void use_promise() {
	// 创建一个 promise 对象
	std::promise<int> prom;
	// 获取与 promise 相关联的 future 对象
	std::future<int> fut = prom.get_future();
	// 在新线程中设置 promise 的值
	std::thread t(set_value, std::move(prom));
	// 在主线程中获取 future 的值
	std::cout << "Waiting for the thread to set the value...\n";
	std::cout << "Value set by the thread: " << fut.get() << '\n';
	t.join();
}

void set_exception(std::promise<void> prom) {
	try {
		// 抛出一个异常
		throw std::runtime_error("An error occurred!");
	}
	catch (...) {
		// 设置 promise 的异常
		prom.set_exception(std::current_exception());
	}
}

void use_promise_exception() {
	std::promise<void> prom;
	// 获取与 promise 相关联的 future 对象
	std::future<void> fut = prom.get_future();
	// 在新线程中设置 promise 的异常
	std::thread t(set_exception, std::move(prom));
	// 在主线程中获取 future 的异常
	try {
		std::cout << "Waiting for the thread to set the exception...\n";
		fut.get();
	}
	catch (const std::exception& e) {
		std::cout << "Exception set by the thread: " << e.what() << '\n';
	}
	t.join();
}

void use_promise_destruct() {
	std::thread t;
	std::future<int> fut;
	{
		// 创建一个 promise 对象
		std::promise<int> prom;
		// 获取与 promise 相关联的 future 对象
		fut = prom.get_future();
		// 在新线程中设置 promise 的值
		t = std::thread(set_value, std::move(prom));
	}
	// 在主线程中获取 future 的值
	std::cout << "Waiting for the thread to set the value...\n";
	std::cout << "Value set by the thread: " << fut.get() << '\n';
	t.join();
}

void may_throw()
{
	// 这里我们抛出一个异常。在实际的程序中，这可能在任何地方发生。
	throw std::runtime_error("Oops, something went wrong!");
}

void use_future_exception() {
	// 创建一个异步任务
	std::future<void> result(std::async(std::launch::async, may_throw));

	try
	{
		// 获取结果（如果在获取结果时发生了异常，那么会重新抛出这个异常）
		result.get();
	}
	catch (const std::exception& e)
	{
		// 捕获并打印异常
		std::cerr << "Caught exception: " << e.what() << std::endl;
	}

}

int main()
{
	//use_async();
	//use_package();
	//use_shared_future();
	//use_shared_future_error();
	//use_promise();
	//use_promise_exception();
	//use_promise_destruct();
	//use_future_exception();
	auto& m1 = ThreadPool::instance();
	auto& j = ThreadPool::instance();
	std::cout << "m is " << &m1 << std::endl;
	std::cout << "j is " << &j << std::endl;

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

	return 1;
}


