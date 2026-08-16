#pragma once

#include <memory>
#include <mutex>

template <typename T>
class threadsafelist {
private:
	struct node
	{
		std::mutex m;
		std::shared_ptr<T> data;
		std::unique_ptr<node> next;

		node() : next() {}
		node(const T& value) : data(std::make_shared<T>(value)) {}
	};

	node head;//Ðé½Úµã

public:
	threadsafelist() {}
	~threadsafelist() {
		remove_if([&](const T&) {return true; });
	}

	threadsafelist(const threadsafelist& other) = delete;
	threadsafelist& operator=(const threadsafelist& other) = delete;

	template <typename Predicate>
	void remove_if(Predicate p) {
		node* cur = &head;
		std::unique_lock<std::mutex> lk(cur->m);
		while (node* next = cur->next.get()) {
			std::unique_lock<std::mutex> next_lk(next->m);
			if (p(*next->data)) {
				auto tmp = std::move(cur->next);
				cur->next = std::move(tmp->next);
				next_lk.unlock();
			}
			else {
				lk.unlock();
				cur = next;
				lk = std::move(next_lk);
			}
		}
	}

	void push_front(const T& value) {
		std::unique_ptr<node> new_node = std::make_unique<node>(value);
		std::lock_guard<std::mutex> lk(head.m);
		new_node->next = std::move(head.next);
		head.next = std::move(new_node);
	}

	template <typename Predicate>
	std::shared_ptr<T> find_first_if(Predicate p) {
		node* cur = &head;
		std::unique_lock<std::mutex> lk(cur->m);
		while (node* next = cur->next.get()) {
			std::unique_lock<std::mutex> next_lk(next->m);
			lk.unlock();
			if (p(*(next->data))) {
				return next->data;
			}
			cur = next;
			lk = std::move(next_lk);
		}
		return std::make_shared<T>();
	}

	template <typename Function>
	void for_each(Function f) {
		node* cur = &head;
		std::unique_lock<std::mutex> lk(cur->m);
		while (node* next = cur->next.get()) {
			std::unique_lock<std::mutex> next_lk(next->m);
			lk.unlock();
			f(*next->data);
			cur = next;
			lk = std::move(next_lk);
		}
	}
};

