#pragma once

#include <atomic>
#include <memory>


template <typename T>
class single_ref_stack {
private:
	struct ref_node;

	struct node {
		std::shared_ptr<T> _data;
		ref_node _next;
		std::atomic<int> _dec_count; // 减少计数量

		node(const T& data) : _data(std::make_shared<T>(data)) {}
	};

	struct ref_node {
		int _ref_count; // 引用计数
		node* _node_ptr;

		ref_node() : _node_ptr(nullptr), _ref_count(0) {}
		ref_node(const T& data) : _node_ptr(new node(data)), _ref_count(1) {}
	};

	std::atomic<ref_node> head;// 头部节点

public:
	single_ref_stack() {}
	~single_ref_stack() {
		//循环出栈
		while (pop());
	}

	void push(const T& val) {
		auto new_node = ref_node(val);
		new_node._node_ptr->_next = head.load();
		while (!head.compare_exchange_weak(new_node._node_ptr->_next, new_node));
	}

	std::shared_ptr<T> pop() {
		ref_node old_head = head.load();
		for (;;) {
			//1 只要执行pop就对引用计数+1并更新到head中
			ref_node new_head;//作为草稿纸, 为了隔离 CAS 失败时对入参变量的隐式修改，保证“修改副本”和“比对原始值”这两个动作互不干扰
			do {
				new_head = old_head;
				new_head._ref_count++;
			} while (!head.compare_exchange_weak(old_head, new_head));

			old_head = new_head;

			auto* node_ptr = old_head._node_ptr;
			if (node_ptr == nullptr) return std::shared_ptr<T>();

			//2 比较head和old_head想等则交换否则说明head已经被其他线程更新
			if (head.compare_exchange_strong(old_head, node_ptr->_next)) {
				std::shared_ptr<T> ret;
				ret.swap(node_ptr->_data);

				int increase_count = old_head._ref_count - 2;// 代表扣除当前线程和栈本身后，还有多少个其他线程正在盯着这个节点
				if (node_ptr->_dec_count.fetch_add(increase_count) == -increase_count) {
					delete node_ptr;
				}

				return ret;
			}
			else {
				if (node_ptr->_dec_count.fetch_sub(1) == 1) {
					delete node_ptr;
				}
			}
		}
	}
};

