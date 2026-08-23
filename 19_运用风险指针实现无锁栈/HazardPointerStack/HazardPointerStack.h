#pragma once

//风险指针的核心思想非常简单：
//在线程访问一个可能被其他线程删除的节点之前，先把这个节点的地址“贴在公告栏上（风险指针数组）”。
//其他线程在删除节点前，会先看一眼公告栏，如果发现有人正在用这个节点，就暂时不删，丢进“待删列表”里等以后删。


#include <memory>
#include <atomic>
#include <thread>
#include <stdexcept>

//最大风险指针数量(假设最多100个线程并发)
const unsigned int max_hazard_pointers = 100;

struct hazard_pointer {
	std::atomic<std::thread::id> id; // 占用这个槽位的线程ID
	std::atomic<void*> pointer; // 该线程正在关注的节点地址
};

extern hazard_pointer hazard_pointers[max_hazard_pointers]; //在.cpp文件中定义


class hp_owner {
public:
	// 构造时，去全局数组里找一个没被占用的槽位
	hp_owner() : hp(nullptr) {
		bind_hazard_pointer();
	}

	// 析构时，释放槽位，将id置空，让给其他线程用
	~hp_owner() {
		hp->pointer.store(nullptr);
		hp->id.store(std::thread::id());
	}

	// 获取槽位中用于存储指针的引用
	std::atomic<void*>& get_pointer() {
		return hp->pointer;
	}

private:
	void bind_hazard_pointer() {
		for (unsigned i = 0; i < max_hazard_pointers; ++i) {
			std::thread::id old_id; // 默认构造的线程ID
			// 尝试用当前线程ID去替换空的槽位 (CAS操作)
			if (hazard_pointers[i].id.compare_exchange_strong(old_id, std::this_thread::get_id())) {
				hp = &hazard_pointers[i];
				break;
			}
		}

		if (!hp) {
			throw std::runtime_error("No hazard pointers available");// 超过100个并发线程就会报错
		}
	}

	hazard_pointer* hp;
};


std::atomic<void*>& get_hazard_pointer_for_current_thread() {
	//每个线程都具有自己的风险指针 线程本地变量
	thread_local static hp_owner hazard;
	return hazard.get_pointer();
}



template <typename T>
class hazard_pointer_stack {
private:
	// 栈节点
	struct node {
		std::shared_ptr<T> data;
		node* next;

		node(const T& data_) : data(std::make_shared<T>(data_)) {}
	};

	// 待删节点
	struct data_to_reclaim {
		node* data;
		data_to_reclaim* next;

		data_to_reclaim(node* p) : data(p), next(nullptr) {}
		~data_to_reclaim() {
			delete data;
		}
	};

	hazard_pointer_stack(const hazard_pointer_stack&) = delete;
	hazard_pointer_stack& operator=(const hazard_pointer_stack&) = delete;

	std::atomic<node*> head;
	std::atomic<data_to_reclaim*> nodes_to_reclaim;//全局待删链表头

private:
	void reclaim_later(node* old_head) {
		add_to_reclaim_list(new data_to_reclaim(old_head));
	}

	void add_to_reclaim_list(data_to_reclaim* reclaim_node) { // 将节点加入待删链表（无锁的链表头插法）
		reclaim_node->next = nodes_to_reclaim;
		while (!nodes_to_reclaim.compare_exchange_weak(reclaim_node->next, reclaim_node));
	}

	void delete_nodes_with_no_hazards() { // 遍历待删链表，把没人用的删掉，有人用的重新挂回待删链表
		// 1. 原子地把整个待删链表拿过来，原全局头置空（相当于把垃圾箱清空到本地处理）
		data_to_reclaim* current = nodes_to_reclaim.exchange(nullptr);

		while (current) {
			data_to_reclaim* next = current->next;
			// 2. 检查待删节点是否还有人正在用
			if (!outstanding_hazard_pointers_for(current->data)) {
				// 没人用了，直接 delete (会触发 data_to_reclaim 的析构，顺带 delete node)
				delete current;
			}
			else {
				// 还有人用，重新塞回全局待删链表，下次再说
				add_to_reclaim_list(current);
			}
			current = next;
		}
	}

	bool outstanding_hazard_pointers_for(void* p) { // 扫描全局100个风险指针，看有没有人正在指向 p
		for (unsigned i = 0; i < max_hazard_pointers; i++) {
			if (hazard_pointers[i].pointer.load() == p) {
				return true;
			}
		}
		return false;
	}

public:
	hazard_pointer_stack() {}

	void push(const T& val) {
		node* new_node = new node(val);
		new_node->next = head;
		while(!head.compare_exchange_weak(new_node->next, new_node));
	}

	std::shared_ptr<T> pop() {
		// 1. 获取当前线程专属的风险指针槽位
		std::atomic<void*>& hp = get_hazard_pointer_for_current_thread();
		node* old_head = head.load();

		do {
			node* tmp;
			do {
				tmp = old_head;
				// 2. 将当前head地址设置到风险指针中，声明：“我正在看这个节点，别删它！”
				hp.store(old_head);
				// 3. 再次加载全局head。如果此时head没变，说明我们保护成功了。
				old_head = head.load();

			} while (tmp != old_head);// 4. 如果old_head和temp不等，说明其他线程刚刚修改了head，我们必须重试

		} while (old_head && !head.compare_exchange_strong(old_head, old_head->next));// 5. 尝试用 old_head->next 替换掉全局的 head

		// 6. 重点！一旦 CAS 成功，这个节点已经从全局链表摘下来了，我们不再需要保护它了
		hp.store(nullptr);

		std::shared_ptr<T> res;
		if (old_head) {
			// 7. 把数据掏出来（swap后 old_head->data 变为空）
			res.swap(old_head->data);

			// 8. 检查是否还有其他线程正在保护这个节点
			if (outstanding_hazard_pointers_for(old_head)) {
				// 9a. 如果有，绝不能 delete，放进待删列表
				reclaim_later(old_head);
			}
			else {
				// 9b. 如果没有，直接安全删除
				delete old_head;
			}

			// 10. 顺便清理一下垃圾箱里积攒的待删节点
			delete_nodes_with_no_hazards();
		}

		return res;
	}
};

