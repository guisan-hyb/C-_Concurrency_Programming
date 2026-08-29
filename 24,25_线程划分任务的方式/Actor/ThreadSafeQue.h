#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>

template <typename T>
class ThreadSafeQue {
private:
	struct node {
		std::shared_ptr<T> _data;
		std::unique_ptr<node> _next;
	};

	std::mutex head_mtx;
	std::mutex tail_mtx;
	std::condition_variable data_cond;
	std::unique_ptr<node> head;
	node* tail;
	std::atomic<bool> _is_stop;

private:
	node* get_tail() {
		std::lock_guard<std::mutex> lk(tail_mtx);
		return tail;
	}

	std::unique_ptr<node> pop_head() {
		std::unique_ptr<node> old_head = std::move(head);
		head = std::move(old_head->_next);
		return old_head;
	}

	std::unique_lock<std::mutex> wait_for_data() {
		std::unique_lock<std::mutex> head_lock(head_mtx);
		data_cond.wait(head_lock, [&]() {
			return (_is_stop.load() == true) || (head.get() != get_tail());
		});
		return head_lock;// NRVO”≈ªØ
	}

	std::unique_ptr<node> wait_pop_head() {
		std::unique_lock<std::mutex> ulk(wait_for_data());
		if (_is_stop.load()) {
			return nullptr;
		}

		return pop_head();
	}

	std::unique_ptr<node> wait_pop_head(T& value) {
		std::unique_lock<std::mutex> ulk(wait_for_data());
		if (_is_stop.load()) {
			return nullptr;
		}

		value = std::move(*head->_data);
		return pop_head();
	}

	std::unique_ptr<node> try_pop_head() {
		std::lock_guard<std::mutex> head_lk(head_mtx);
		if (head.get() == get_tail()) return std::unique_ptr<node>();
		return pop_head();
	}

	std::unique_ptr<node> try_pop_head(T& value) {
		std::lock_guard<std::mutex> head_lk(head_mtx);
		if (head.get() == get_tail()) return std::unique_ptr<node>();
		value = std::move(*head->_data);
		return pop_head();
	}

public:
	ThreadSafeQue() : head(new node), tail(head.get()), _is_stop(false) {}
	~ThreadSafeQue() = default;
	ThreadSafeQue(const ThreadSafeQue&) = delete;
	ThreadSafeQue& operator=(const ThreadSafeQue&) = delete;

	void NotifyStop() {
		_is_stop.store(true);
		data_cond.notify_one();
	}

	std::shared_ptr<T> WaitAndPop() {
		std::unique_ptr<node> old_head = wait_pop_head();
		if (old_head == nullptr) return nullptr;
		return old_head->_data;
	}

	void WaitAndPop(T& val) {
		std::unique_ptr<node> old_head = wait_pop_head(val);
	}

	std::shared_ptr<T> Try() {
		std::unique_ptr<node> old_head = try_pop_head();
		if (old_head == nullptr) return nullptr;
		return old_head->_data;
	}

	bool try_pop(T& val) {
		std::unique_ptr<node> old_head = try_pop_head(val);
		return old_head;
	}

	bool empty() {
		std::lock_guard<std::mutex> head_lk(head_mtx);
		return (head.get() == tail);
	}

	void push(T new_value) {
		std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
		std::unique_ptr<node> new_next = std::make_unique<node>();
		{
			std::lock_guard<std::mutex> lk(tail_mtx);
			tail->_data = new_data;
			tail->_next = std::move(new_next);
			tail = tail->_next.get();
		}
		data_cond.notify_one();
	}
};

