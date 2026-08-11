#pragma once
#include <mutex>
#include <condition_variable>
#include <stack>
#include <exception>
#include <memory>

struct empty_stack : std::exception {
	const char* what() const noexcept override;
};

template <typename T>
class threadsafe_stack {
public:
	threadsafe_stack() {}
	threadsafe_stack(const threadsafe_stack& other) {
		std::lock_guard<std::mutex> lk(other.m);
		data = other.data;
	}

	threadsafe_stack& operator=(const threadsafe_stack&) = delete;

	void push(T new_value) {
		std::lock_guard<std::mutex> lk(m);
		data.push(std::move(new_value));
	}

	std::shared_ptr<T> pop() {
		std::lock_guard<std::mutex> lk(m);
		if (data.empty()) throw empty_stack();
		std::shared_ptr<T> ret(std::make_shared<T>(std::move(data.top())));
		data.pop();
		return ret;
	}

	void pop(T& value) {
		std::lock_guard<std::mutex> lk(m);
		if (data.empty()) throw empty_stack();
		value = std::move(data.top());
		data.pop();
	}

	bool empty() const {
		std::lock_guard<std::mutex> lk(m);
		return data.empty();
	}

private:
	std::stack<T> data;
	mutable std::mutex m;
};



template <typename T>
class threadsafe_stack_waitable {
public:
	threadsafe_stack_waitable() {}
	threadsafe_stack_waitable(const threadsafe_stack_waitable& other) {
		std::lock_guard<std::mutex> lk(other.m);
		data = other.data;
	}

	threadsafe_stack_waitable& operator=(const threadsafe_stack_waitable&) = delete;

	void push(T new_value) {
		std::unique_lock<std::mutex> ulk(m);
		data.push(std::move(new_value));
		cv.notify_one();
	}

	std::shared_ptr<T> wait_and_pop() {
		std::unique_lock<std::mutex> ulk(m);
		cv.wait(ulk, [this]() {
			return !data.empty();
		});
		std::shared_ptr<T> ret(std::make_shared<T>(std::move(data.top())));
		data.pop();
		return ret;
	}

	void wait_and_pop(T& val) {
		std::unique_lock<std::mutex> ulk(m);
		cv.wait(ulk, [this]() {
			return !data.empty();
		});
		val = std::move(data.top());
		data.pop();
	}

	bool empty() const {
		std::lock_guard<std::mutex> lk(m);
		return data.empty();
	}

	bool try_pop(T& val) {
		std::lock_guard<std::mutex> lk(m);
		if (data.empty()) return false;
		val = std::move(data.top());
		data.pop();
		return true;
	}

	std::shared_ptr<T> try_pop() {
		std::lock_guard<std::mutex> lk(m);
		if (data.empty()) return std::shared_ptr<T>();
		std::shared_ptr<T> res(std::make_shared<T>(std::move(data.top())));
		data.pop();
		return res;
	}

private:
	std::stack<T> data;
	mutable std::mutex m;
	std::condition_variable cv;
};

