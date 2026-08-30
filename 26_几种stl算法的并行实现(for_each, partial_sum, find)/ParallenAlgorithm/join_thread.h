#pragma once

#include <thread>
#include <vector>

class join_thread {
public:
	explicit join_thread(std::vector<std::thread>& threads) : _threads(threads) {}
	~join_thread() {
		for (unsigned i = 0; i < _threads.size(); i++) {
			if (_threads[i].joinable()) {
				_threads[i].join();
			}
		}
	}

private:
	std::vector<std::thread>& _threads;
};

