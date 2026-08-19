#pragma once

#include <memory>
#include <atomic>

template <typename T>
class lock_free_stack {
private:
	struct node {
		std::shared_ptr<T> _data;
		std::shared_ptr<node> _next;

		explicit node(const T& val): _data(std::make_shared<T>(val)), _next(nullptr) {}
	};

	lock_free_stack(const lock_free_stack&) = delete;
	lock_free_stack& operator=(const lock_free_stack&) = delete;

	std::atomic<std::shared_ptr<node>> _head;

public:
	lock_free_stack() = default;

	void push(const T& val) {
		std::shared_ptr<node> new_node = std::make_shared<node>(val);
		new_node->_next = _head.load();
		while (!_head.compare_exchange_weak(new_node->_next, new_node));
	}

	std::shared_ptr<T> pop() {
		std::shared_ptr<node> old_head = _head.load();
		while (old_head && !_head.compare_exchange_weak(old_head, old_head->_next));

		if (!old_head) {
			return nullptr;
		}
		return old_head->_data;
	}
};

