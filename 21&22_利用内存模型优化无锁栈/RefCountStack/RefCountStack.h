#pragma once

#include <atomic>
#include <memory>

template <typename T>
class ref_count_stack {
private:
	struct count_node;

	struct counted_node_ptr
	{
		int external_count;// 外部引用计数
		count_node* ptr;

		counted_node_ptr() : external_count(0), ptr(nullptr) {}
		counted_node_ptr(const T& val) : ptr(new count_node(val)), external_count(1) {}
	};

	struct count_node
	{
		std::shared_ptr<T> _data;
		std::atomic<int> internal_count;// 节点内部引用计数
		counted_node_ptr _next;

		count_node(const T& val) : _data(std::make_shared<T>(val)), internal_count(0) {}
	};

	std::atomic<counted_node_ptr> head;

private:
	void increase_head_count(counted_node_ptr& old_counter) {
		counted_node_ptr new_counter = old_counter;
		do {
			new_counter = old_counter;
			new_counter.external_count++;
		} while (!head.compare_exchange_weak(old_counter, new_counter,
			std::memory_order_acquire, std::memory_order_relaxed));
		old_counter = new_counter;
	}

public:
	ref_count_stack() {}
	~ref_count_stack() {
		while (pop());
	}

	void push(const T& val) {
		auto new_node = counted_node_ptr(val);
		new_node.ptr->_next = head.load();
		while (!head.compare_exchange_weak(new_node.ptr->_next, new_node,
			std::memory_order_release, std::memory_order_relaxed));
	}

	std::shared_ptr<T> pop() {
		counted_node_ptr old_head = head.load();
		for (;;) {
			increase_head_count(old_head);
			count_node* ptr = old_head.ptr;

			if (!ptr) return std::shared_ptr<T>();

			if (head.compare_exchange_strong(old_head, ptr->_next, std::memory_order_relaxed)) {
				std::shared_ptr<T> res;
				res.swap(ptr->_data);
				int increase_count = old_head.external_count - 2;// 减少外部引用计数，先统计到目前为止增加了多少外部引用
				if (ptr->internal_count.fetch_add(increase_count, std::memory_order_release) == -increase_count) {
					delete ptr;
				}
				return res;
			}
			else {
				if (ptr->internal_count.fetch_sub(1, std::memory_order_acquire) == 1) {
					//如果当前线程操作的head节点已经被别的线程更新，则减少内部引用计数
					//当前线程减少内部引用计数，返回之前值为1说明指针仅被当前线程引用
					ptr->internal_count.load(std::memory_order_acquire);
					delete ptr;
				}
			}
		}
	}
};

