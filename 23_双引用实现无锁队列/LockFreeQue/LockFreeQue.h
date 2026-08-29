#pragma once

#include <memory>
#include <atomic>
#include <iostream>

template <typename T>
class lock_free_queue {
private:
	struct node_counter {
		unsigned internal_count : 30;// 位域，分配30位 -> 当前节点被多少个操作（如push/pop的中间状态）在内部持有
		unsigned external_counters : 2;// 位域，分配2位 -> 当前节点被多少个“外部引用”（即head或tail）指向。因为队列中最多只有head和tail两个指针指向同一个节点，所以2位（最大值3）足够了
	};

	struct node;

	struct counted_node_ptr {
		int external_count;
		node* ptr;

		counted_node_ptr(): external_count(0), ptr(nullptr) {}
	};

	struct node {
		std::atomic<T*> data;
		std::atomic<node_counter> count;
		std::atomic<counted_node_ptr> next;

		node(int external_count = 2) {
			node_counter new_count;
			new_count.internal_count = 0;
			new_count.external_counters = external_count;
			count.store(new_count);

			counted_node_ptr node_ptr;
			node_ptr.external_count = 0;
			node_ptr.ptr = nullptr;
			next.store(node_ptr);
		}

		// 释放内部引用
		void release_ref() {
			//std::cout << "call release ref " << std::endl;

			node_counter old_counter = count.load(std::memory_order_relaxed);
			node_counter new_counter;
			do {
				new_counter = old_counter;
				new_counter.internal_count--;
			} while (!count.compare_exchange_strong(old_counter, new_counter,
				std::memory_order_acquire, std::memory_order_relaxed));

			if (!new_counter.internal_count && !new_counter.external_counters) {
				delete this;
				
				// std::cout << "release_ref delete success" << std::endl;
				destruct_count.fetch_add(1);
			}
		}
	};

	std::atomic<counted_node_ptr> head;
	std::atomic<counted_node_ptr> tail;

private:
	// 更新尾部节点
	void set_new_tail(counted_node_ptr& old_tail, const counted_node_ptr& new_tail) {
		node* current_tail_ptr = old_tail.ptr;
		// 为防止失败的线程重试导致tail被再次更新所以添加了后面的 && 判断
		// 如果tail和old_tail不等说明引用计数不同或者tail已经被移动，如果tail已经被移动那么old_tail的ptr和current_tail_ptr不同，则可以直接退出。
		// 所以一旦tail被设置为new_tail，那么另一个线程在重试时判断tail和old_tail不等，会修改old_tail, 此时old_tail已经和current_tail不一致了，所以没必要再重试。
		// 如不加后续判断， 会造成重复设置newtail，引发多插入节点的问题。
		while (!tail.compare_exchange_weak(old_tail, new_tail) &&
			old_tail.ptr == current_tail_ptr);

		if (old_tail.ptr == current_tail_ptr) {
			free_external_counter(old_tail);
		}
		else {
			current_tail_ptr->release_ref();
		}
	}

	// 增加外部引用
	static void increase_external_count(std::atomic<counted_node_ptr>& counter, counted_node_ptr& old_counter) {
		counted_node_ptr new_counter;
		do {
			new_counter = old_counter;
			new_counter.external_count++;
		} while (!counter.compare_exchange_weak(old_counter, new_counter,
			std::memory_order_acquire, std::memory_order_relaxed));
		old_counter.external_count = new_counter.external_count;
	}

	//释放外部引用
	static void free_external_counter(counted_node_ptr& old_node_ptr) {
		std::cout << "call  free_external_counter " << std::endl;

		node* ptr = old_node_ptr.ptr;
		int increase_count = old_node_ptr.external_count - 2;
		node_counter old_counter = ptr->count.load(std::memory_order_relaxed);
		node_counter new_counter;

		do {
			new_counter = old_counter;
			new_counter.external_counters--;
			new_counter.internal_count += increase_count;
		} while (!ptr->count.compare_exchange_strong(old_counter, new_counter,
			std::memory_order_acquire, std::memory_order_relaxed));

		if (!new_counter.external_counters && !new_counter.internal_count) {
			
			std::cout << "free_external_counter delete success" << std::endl;
			destruct_count.fetch_add(1);

			delete ptr;
		}
	}

public:
	lock_free_queue() {
		counted_node_ptr new_next;// 哨兵节点
		new_next.ptr = new node();
		new_next.external_count = 1;
		tail.store(new_next);
		head.store(new_next);
	}

	~lock_free_queue() {
		while (pop());
		auto head_counted_node = head.load();
		delete head_counted_node.ptr;
	}

	void push(T new_value) {
		std::unique_ptr<T> new_data = std::make_unique<T>(new_value);
		counted_node_ptr new_next;
		new_next.external_count = 1;
		new_next.ptr = new node();
		
		counted_node_ptr old_tail = tail.load();
		for (;;) {
			increase_external_count(tail, old_tail);
			T* old_data = nullptr;
			if (old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get())) {
				counted_node_ptr old_next;

				if (!old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
					delete new_next.ptr;
					new_next = old_next;
				}
				set_new_tail(old_tail, new_next);
				new_data.release();
				break;
			}
			else {
				counted_node_ptr old_next;
				if (old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
					old_next = new_next;
					new_next.ptr = new node;
				}
				set_new_tail(old_tail, old_next);
			}
		}

		construct_count++;
	}

	std::unique_ptr<T> pop() {
		counted_node_ptr old_head = head.load(std::memory_order_relaxed);
		for (;;) {
			increase_external_count(head, old_head);
			node* ptr = old_head.ptr;
			if (ptr == tail.load().ptr) {
				// 头尾相等说明队列为空，要减少内部引用计数
				ptr->release_ref();
				return std::unique_ptr<T>();
			}

			counted_node_ptr next = ptr->next.load();
			if (head.compare_exchange_strong(old_head, next)) {
				T* res = ptr->data.exchange(nullptr);
				free_external_counter(old_head);
				return std::unique_ptr<T>(res);
			}
			ptr->release_ref();
		}
	}

	
	// 调试用：
	static std::atomic<int> destruct_count;
	static std::atomic<int> construct_count;
};

template <typename T>
std::atomic<int> lock_free_queue<T>::destruct_count = 0;

template <typename T>
std::atomic<int> lock_free_queue<T>::construct_count = 0;

