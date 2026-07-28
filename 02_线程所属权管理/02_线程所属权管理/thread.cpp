#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <numeric>

void some_function() {
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void some_other_function() {
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void dangerous_use() {
	//t1 绑定some_function
	std::thread t1(some_function);
	//2 转移t1管理的线程给t2，转移后t1无效
	std::thread t2 = std::move(t1);
	//3 t1 可继续绑定其他线程,执行some_other_function
	t1 = std::thread(some_other_function);
	//4  创建一个线程变量t3
	std::thread t3;
	//5  转移t2管理的线程给t3
	t3 = std::move(t2);
	//6  转移t3管理的线程给t1
	t1 = std::move(t3);
	std::this_thread::sleep_for(std::chrono::seconds(2000));
}

std::thread f() {
	return std::thread(some_function);
}

void param_function(int a) {
	while (true) {
		std::cout << "param is " << a << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

std::thread g() {
	std::thread t(param_function, 43);
	return t;
}

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

	std::thread::id get_id() const noexcept {
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

void use_jointhread() {
	//1 根据线程构造函数构造joiningthread
	joining_thread j1([](int maxindex) {
		for (int i = 0; i < maxindex; i++) {
			std::cout << "in thread id " << std::this_thread::get_id()
				<< " cur index is " << i << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		}, 10);

	//2 根据thread构造joiningthread
	joining_thread j2(std::thread([](int maxindex) {
		for (int i = 0; i < maxindex; i++) {
			std::cout << "in thread id " << std::this_thread::get_id()
				<< " cur index is " << i << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		}, 10));

	//3 根据thread构造j3
	joining_thread j3(std::thread([](int maxindex) {
		for (int i = 0; i < maxindex; i++) {
			std::cout << "in thread id " << std::this_thread::get_id()
				<< " cur index is " << i << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		}, 10));


	//4 把j3赋值给j1，joining_thread内部会等待j1汇合结束后
	//再将j3赋值给j1
	j1 = std::move(j3);

}

void use_vector() {
	std::vector<std::thread> threads;
	for (unsigned i = 0; i < 10; ++i) {
		threads.emplace_back(param_function, i);
	}

	for (auto& entry : threads) {
		entry.join();
	}
}


template<typename Iterator, typename T>
struct accumulate_block
{
	void operator()(Iterator first, Iterator last, T& result)
	{
		result = std::accumulate(first, last, result);
	}
};
template<typename Iterator, typename T>
T parallel_accumulate(Iterator first, Iterator last, T init)
{
	unsigned long const length = std::distance(first, last);
	if (!length)
		return init;    //⇽-- - ①
	unsigned long const min_per_thread = 25;
	unsigned long const max_threads =
		(length + min_per_thread - 1) / min_per_thread;    //⇽-- - ②
	unsigned long const hardware_threads =
		std::thread::hardware_concurrency();
	unsigned long const num_threads =
		std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);    //⇽-- - ③
	unsigned long const block_size = length / num_threads;    //⇽-- - ④
	std::vector<T> results(num_threads);
	std::vector<std::thread>  threads(num_threads - 1);   // ⇽-- - ⑤
	Iterator block_start = first;
	for (unsigned long i = 0; i < (num_threads - 1); ++i)
	{
		Iterator block_end = block_start;
		std::advance(block_end, block_size);    //⇽-- - ⑥
		threads[i] = std::thread(//⇽-- - ⑦
			accumulate_block<Iterator, T>(),
			block_start, block_end, std::ref(results[i]));
		block_start = block_end;    //⇽-- - ⑧
	}
	accumulate_block<Iterator, T>()(
		block_start, last, results[num_threads - 1]);    //⇽-- - ⑨

	for (auto& entry : threads)
		entry.join();    //⇽-- - ⑩
	return std::accumulate(results.begin(), results.end(), init);    //⇽-- - ⑪
}

void use_parallel_acc() {
	std::vector <int> vec(10000);
	for (int i = 0; i < 10000; i++) {
		vec.push_back(i);
	}
	int sum = 0;
	sum = parallel_accumulate<std::vector<int>::iterator, int>(vec.begin(),
		vec.end(), sum);

	std::cout << "sum is " << sum << std::endl;
}


void day02() {
	//dangerous_use();
   // use_jointhread();
	//use_vector();
	use_parallel_acc();
	std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main() {
	day02();
}
