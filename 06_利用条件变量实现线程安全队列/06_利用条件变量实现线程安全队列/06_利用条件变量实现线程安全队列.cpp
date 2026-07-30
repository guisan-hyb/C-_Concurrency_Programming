#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include <queue>

//本文介绍如何使用条件变量控制并发的同步操作
//试想有一个线程A一直输出1，另一个线程B一直输出2。
//我想让两个线程交替输出1，2，1，2...之类的效果，该如何实现？
//有的同学可能会说不是有互斥量mutex吗？可以用一个全局变量num表示应该哪个线程输出，
//比如num为1则线程A输出1，num为2则线程B输出2，mutex控制两个线程访问num，如果num和线程不匹配，就让该线程睡一会，
//这不就实现了吗？
//比如线程A加锁后发现当前num为2则表示它不能输出1，就解锁，将锁的使用权交给线程A，线程B就sleep一会。


int num = 1;
std::mutex mtx_num;
std::condition_variable cvA;
std::condition_variable cvB;


void PoorImplemention() {
	std::thread t1([]() {
		for (;;) {
			{
				std::lock_guard<std::mutex> lk(mtx_num);
				if (num == 1) {
					std::cout << num << std::endl;
					num++;
					continue;
				}
			}
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	});

	std::thread t2([]() {
		for (;;) {
			{
				std::lock_guard<std::mutex> lk(mtx_num);
				if (num == 2) {
					std::cout << num << std::endl;
					num--;
					continue;
				}
			}
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	});

	t1.join();
	t2.join();
}



void ReasonableImplemention() {
	std::thread t1([]() {
		for (;;) {
			std::unique_lock<std::mutex> ulk(mtx_num);

			//写法一
			/*while (num != 1) {
				cvA.wait(ulk);
			}*/

			//写法二
			cvA.wait(ulk, []() {
				return num == 1;
				});

			std::cout << num << std::endl;
			num++;
			cvB.notify_one();
		}
	});

	std::thread t2([]() {
		for (;;) {
			std::unique_lock<std::mutex> ulk(mtx_num);
			cvB.wait(ulk, []() {
				return num == 2;
			});

			std::cout << num << std::endl;
			num--;
			cvA.notify_one();
		}
	});

	t1.join();
	t2.join();
}


template <typename T>
class threadsafe_queue {
public:
	threadsafe_queue() = default;
	threadsafe_queue(const threadsafe_queue& other) {
		std::lock_guard<std::mutex> lk(other._mtx);
		_data_que = other._data_que;
	}

	void push(T new_val) {
		std::lock_guard<std::mutex> lk(_mtx);
		_data_que.push(new_val);
		_data_cond.notify_one();
	}

	void wait_and_pop(T& val) {
		std::unique_lock<std::mutex> ulk(_mtx);
		_data_cond.wait(ulk, [this]() {return !_data_que.empty(); });
		val = _data_que.front();
		_data_que.pop();
	}

	std::shared_ptr<T> wait_and_pop() {
		std::unique_lock<std::mutex> ulk(_mtx);
		_data_cond.wait(ulk, [this]() {return !_data_que.empty(); });
		std::shared_ptr<T> res(std::make_shared<T>(_data_que.front()));
		_data_que.pop();
		return res;
	}

	bool try_pop(T& val) {
		std::lock_guard<std::mutex> lk(_mtx);
		if (_data_que.empty()) {
			return false;
		}

		val = _data_que.front();
		_data_que.pop();
		return true;
	}

	std::shared_ptr<T> try_pop() {
		std::lock_guard<std::mutex> lk(_mtx);
		if (_data_que.empty()) {
			return std::make_shared<T>();
		}

		std::shared_ptr<T> res(std::make_shared<T>(_data_que.front()));
		_data_que.pop();
		return res;
	}

	bool empty() const {
		std::lock_guard<std::mutex> lk(_mtx);
		return _data_que.empty();
	}

private:
	std::queue<T> _data_que;
	mutable std::mutex _mtx;
	std::condition_variable _data_cond;
};

void test_safe_que() {
	threadsafe_queue<int> safe_que;
	std::mutex  mtx_print;
	std::thread producer(
		[&]() {
			for (int i = 0; ; i++) {
				safe_que.push(i);
				{
					std::lock_guard<std::mutex> printlk(mtx_print);
					std::cout << "producer push data is " << i << std::endl;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		}
	);

	std::thread consumer1(
		[&]() {
			for (;;) {
				auto data = safe_que.wait_and_pop();
				{
					std::lock_guard<std::mutex> printlk(mtx_print);
					std::cout << "consumer1 wait and pop data is " << *data << std::endl;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}
	);

	std::thread consumer2(
		[&]() {
			for (;;) {
				auto data = safe_que.try_pop();
				if (data != nullptr) {
					{
						std::lock_guard<std::mutex> printlk(mtx_print);
						std::cout << "consumer2 try_pop data is " << *data << std::endl;
					}

				}

				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}
	);

	producer.join();
	consumer1.join();
	consumer2.join();
}


int main() {
	//PoorImplemention();
	//ReasonableImplemention();
	test_safe_que();

	return 0;
}