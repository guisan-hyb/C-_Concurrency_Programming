#pragma once

#include <memory>
#include <mutex>

template <typename T>
class double_push_list {
private:
	struct node
	{
		std::mutex m;
		std::shared_ptr<T> data;
		std::unique_ptr<node> next;

		node() : next() {}
		node(const T& value) : data(std::make_shared<T>(value)) {}
	};

	node head;
	node* tail;
	std::mutex tail_mtx;

public:
	double_push_list() {
		tail = &head;
	}

	~double_push_list() {
		remove_if([&](const T&) {return true; });
	}

	double_push_list(const double_push_list&) = delete;
	double_push_list& operator=(const double_push_list&) = delete;

	void push_front(const T& val) {
		std::unique_ptr<node> new_node(std::make_unique<node>(val));
		std::lock_guard<std::mutex> lk(head.m);
		new_node->next = std::move(head.next);
		head.next = std::move(new_node);

		//更新尾节点
		if (head.next->next == nullptr) {
			std::lock_guard<std::mutex> tail_lk(tail_mtx);
			tail = head.next.get();
		}
	}

	void push_back(const T& val) {
		//防止于push_head同时进行
		//并且保证头部或者删除节点更新last_node_ptr唯一, 所以同时加锁
		std::unique_ptr<node> new_node = std::make_unique<node>(val);
		/*std::lock(tail->m, tail_mtx);
		std::unique_lock<std::mutex> lk(tail->m, std::adopt_lock);
		std::unique_lock<std::mutex> last_lk(tail_mtx, std::adopt_lock);*/
		std::unique_lock<std::mutex> tail_lk(tail_mtx);
		tail->next = std::move(new_node);
		tail = tail->next.get();//由于上方已经std::move()了，所以不能写 tail = new_node.get()
	}

	template <typename Function>
	void for_each(Function f) {
		node* cur = &head;
		std::unique_lock<std::mutex> lk(head.m);
		while (node* next = cur->next.get()) {
			std::unique_lock<std::mutex> next_lk(next->m);
			lk.unlock();
			f(*next->data);
			cur = next;
			lk = std::move(next_lk);
		}
	}

	template <typename Predicate>
	std::shared_ptr<T> find_first_if(Predicate p) {
		node* cur = &head;
		std::unique_lock<std::mutex> lk(head.m);
		while (node* next = cur->next.get()) {
			std::unique_lock<std::mutex> next_lk(next->m);
			lk.unlock();
			if (p(*next->data)) {
				return next->data;
			}
			cur = next;
			lk = std::move(next_lk);
		}
		return std::make_shared<T>();
	}

	template <typename Predicate>
	void remove_if(Predicate p) {
		node* cur = &head;
		std::unique_lock<std::mutex> lk(cur->m);
		while (node* next = cur->next.get()) {
			std::unique_lock<std::mutex> next_lk(next->m);
			if (p(*next->data)) {
				auto tmp = std::move(cur->next);
				cur->next = std::move(tmp->next);
				if (cur->next == nullptr) {
					std::lock_guard<std::mutex> tail_lk(tail_mtx);
					tail = cur;
				}
				next_lk.unlock();//处理完毕，别忘了解锁
			}
			else {
				lk.unlock();
				cur = next;
				lk = std::move(next_lk);
			}
		}
	}

	template <typename Predicate>
	bool remove_first(Predicate p) {
		node* cur = &head;
		std::unique_lock<std::mutex> lk(cur->m);
		while (node* next = cur->next.get()) {
			std::unique_lock<std::mutex> next_lk(next->m);
			if (p(*next->data)) {
				auto tmp = std::move(cur->next);
				cur->next = std::move(tmp->next);
				if (cur->next == nullptr) {
					std::lock_guard<std::mutex> tail_lk(tail_mtx);
					tail = cur;
				}
				next_lk.unlock();//处理完毕，别忘了解锁
				return true;
			}
			lk.unlock();
			cur = next;
			lk = std::move(next_lk);
		}
		return false;
	}

	template <typename Predicate>
	void insert_if(Predicate p, const T& val) {
		node* cur = &head;
		std::unique_lock<std::mutex> lk(cur->m);
		while (node* next = cur->next.get()) {
			std::unique_lock<std::mutex> next_lk(next->m);
			if (p(*next->data)) {
				std::unique_ptr<node> new_node = std::make_unique<node>(val);
				new_node->next = std::move(cur->next);
				cur->next = std::move(new_node);
				return;
			}

			lk.unlock();
			cur = next;
			lk = std::move(next_lk);
		}
	}
};

