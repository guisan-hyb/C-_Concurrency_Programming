#include <iostream>
#include <thread>
#include <memory>
#include <functional>
#include <future>

class TestCopy {
public:
	TestCopy() {}
	TestCopy(const TestCopy& tp) {
		std::cout << "Test Copy Copy " << std::endl;
	}
	TestCopy(TestCopy&& cp) {
		std::cout << "Test Copy Move " << std::endl;
	}
};

TestCopy TestCp() {
	TestCopy tp;
	return tp;
}


std::unique_ptr<int> ReturnUniquePtr() {
	std::unique_ptr<int>  uq_ptr = std::make_unique<int>(100);
	return  uq_ptr;
}

std::thread ReturnThread() {
	std::thread t([]() {
		int i = 0;
		while (true) {
			std::cout << "i is " << i << std::endl;
			i++;
			if (i == 5) {
				break;
			}
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		});

	return t;
}

void ChangeValue() {
	int m = 100;
	std::thread  t1{ [](int& rm) {
		rm++;
	}, std::ref(m) };

	t1.join();
}


void ThreadOp() {

	std::thread t1([]() {
		int i = 0;
		while (i < 5) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			i++;
		}
		});

	std::thread t2([]() {
		int i = 0;
		while (i < 10) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			i++;
		}
		});

	//不能将一个线程归属权绑定给一个已经绑定线程的变量，否则会触发terminate导致崩溃
	t1 = std::move(t2);
	t1.join();
	t2.join();
}


void BlockAsync() {
	std::cout << "begin block async" << std::endl;
	{
		std::future<void> futures = std::async(std::launch::async, []() {
			std::this_thread::sleep_for(std::chrono::seconds(3));
			std::cout << "std::async called " << std::endl;
			});
	}
	std::cout << "end block async" << std::endl;
}


void DeadLock() {
	std::mutex  mtx;
	std::cout << "DeadLock begin " << std::endl;
	std::lock_guard<std::mutex>  dklock(mtx);
	{
		std::future<void> futures = std::async(std::launch::async, [&mtx]() {
			std::cout << "std::async called " << std::endl;
			std::lock_guard<std::mutex>  dklock(mtx);
			std::cout << "async working...." << std::endl;
			});
	}

	std::cout << "DeadLock end " << std::endl;
}

int asyncFunc() {
	std::this_thread::sleep_for(std::chrono::seconds(3));
	std::cout << "this is asyncFunc" << std::endl;
	return 0;
}

void func1(std::future<int>& future_ref) {
	std::cout << "this is func1" << std::endl;
	future_ref = std::async(std::launch::async, asyncFunc);
}

void func2(std::future<int>& future_ref) {
	std::cout << "this is func2" << std::endl;
	auto future_res = future_ref.get();
	if (future_res == 0) {
		std::cout << "get asyncFunc result success !" << std::endl;
	}
	else {
		std::cout << "get asyncFunc result failed !" << std::endl;
		return;
	}
}

//提供多种思路，这是第一种
void first_method() {
	std::future<int> future_tmp;
	func1(future_tmp);
	func2(future_tmp);
}


template<typename Func, typename... Args  >
auto  ParallenExe(Func&& func, Args && ... args) -> std::future<decltype(func(args...))> {
	typedef    decltype(func(args...))  RetType;
	std::function<RetType()>  bind_func = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
	std::packaged_task<RetType()> task(bind_func);
	auto rt_future = task.get_future();
	std::thread t(std::move(task));

	t.detach();

	return rt_future;
}

void TestParallen1() {
	int i = 0;
	std::cout << "Begin TestParallen1 ..." << std::endl;
	{
		ParallenExe([](int i) {
			while (i < 3) {
				i++;
				std::cout << "ParllenExe thread func " << i << " times" << std::endl;
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
			}, i);
	}

	std::cout << "End TestParallen1 ..." << std::endl;
}

void TestParallen2() {
	int i = 0;
	std::cout << "Begin TestParallen2 ..." << std::endl;

	auto rt_future = ParallenExe([](int i) {
		while (i < 3) {
			i++;
			std::cout << "ParllenExe thread func " << i << " times" << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		}, i);

	std::cout << "End TestParallen2 ..." << std::endl;

	rt_future.wait();
}

int main()
{
	//TestCp();
	//auto rt_ptr = ReturnUniquePtr();
	//std::cout << "rt_ptr value is " << *rt_ptr << std::endl;
	//std::thread rt_thread = ReturnThread();
	//rt_thread.join();
	//ThreadOp();
	//BlockAsync();

	//ChangeValue();
	//DeadLock();
	//first_method();
	//TestParallen1();
	//std::this_thread::sleep_for(std::chrono::seconds(4));
	//TestParallen2();
	std::cout << "Main Exited!\n";
}


