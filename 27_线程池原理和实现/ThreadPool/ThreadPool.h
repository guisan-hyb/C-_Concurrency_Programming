#pragma once

#include <iostream>
#include <atomic>
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>

class NoneCopy {
public:
	~NoneCopy() {}

protected:
	NoneCopy() {}

private:
	NoneCopy(const NoneCopy&) = delete;
	NoneCopy& operator=(const NoneCopy&) = delete;
};


// 继承基类NoneCopy就不需要写如下删除了
// ThreadPool(const ThreadPool&) = delete;
// ThreadPool& operator=(const ThreadPool&) = delete;
class ThreadPool : public NoneCopy {
	using Task = std::packaged_task<void()>;
public:
	static ThreadPool& GetInst() {
		static ThreadPool inst;
		return inst;
	}

	~ThreadPool() {
		stop();
	}

	int idleThreadCount() {
		return _idle_count;
	}

	// 生产者
	template <typename F, typename... Args>
	auto commit(F&& f, Args&&... args) ->
		std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))> {
		using RetType = decltype((std::forward<F>(f))(std::forward<Args>(args)...));
		if (_is_stop.load()) return std::future<RetType>{};

		auto task = std::make_shared<std::packaged_task<RetType()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);

		std::future<RetType> ret = task->get_future();
		{
			std::lock_guard<std::mutex> lk(_cv_mt);
			_tasks.emplace([task]() {(*task)(); });
		}
		_cv_lock.notify_one();
		return ret;
	}

private:
	ThreadPool(unsigned int num = std::thread::hardware_concurrency()) : _is_stop(false) {
		if (num <= 1) _idle_count = 2;
		else _idle_count = num;

		start();
	}

	void stop() {
		_is_stop.store(true);
		_cv_lock.notify_all();
		for (auto& t : _pool) {
			if (t.joinable()) {
				std::cout << "join thread: " << t.get_id() << std::endl;
				t.join();
			}
		}
	}

	// 消费者
	void start() {
		for (unsigned int i = 0; i < _idle_count; i++) {
			_pool.emplace_back([this]() {
				while (!_is_stop.load()) {
					Task task;
					{
						std::unique_lock<std::mutex> ulk(_cv_mt);
						_cv_lock.wait(ulk, [this]() {
							return _is_stop.load() || !_tasks.empty();
						});

						if (this->_tasks.empty()) return;

						task = std::move(_tasks.front());
						_tasks.pop();
					}
					_idle_count--;
					task();
					_idle_count++;
				}
			});
		}
	}

private:
	std::mutex _cv_mt;
	std::condition_variable _cv_lock;
	std::atomic<bool> _is_stop;
	std::atomic<int> _idle_count;

	std::queue<Task> _tasks;
	std::vector<std::thread> _pool;
};

