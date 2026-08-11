#pragma once

#include <mutex>
#include <queue>
#include <condition_variable>
#include <memory>

template<typename T>
class threadsafe_queue
{
private:

    mutable std::mutex mut;
    std::queue<T> data_queue;
    std::condition_variable data_cond;

public:
    threadsafe_queue() {}

    void push(T new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(std::move(new_value));
        data_cond.notify_one();    
    }

    void wait_and_pop(T& value)    
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] {return !data_queue.empty(); });
        value = std::move(data_queue.front());
        data_queue.pop();
    }

    std::shared_ptr<T> wait_and_pop()   
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] {return !data_queue.empty(); });   
        std::shared_ptr<T> res(std::make_shared<T>(std::move(data_queue.front())));
        data_queue.pop();
        return res;
    }

    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return false;
        value = std::move(data_queue.front());
        data_queue.pop();
        return true;
    }

    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return std::shared_ptr<T>();    
        std::shared_ptr<T> res(std::make_shared<T>(std::move(data_queue.front())));
        data_queue.pop();
        return res;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};



template<typename T>
class threadsafe_queue_ptr
{
private:
    mutable std::mutex mut;
    std::queue<std::shared_ptr<T>> data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_queue_ptr() {}
    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] {return !data_queue.empty(); });
        value = std::move(*data_queue.front());    
        data_queue.pop();
    }
    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty())
            return false;
        value = std::move(*data_queue.front());   
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this] {return !data_queue.empty(); });
        std::shared_ptr<T> res = data_queue.front();  
        data_queue.pop();
        return res;
    }
    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if (data_queue.empty()) return std::shared_ptr<T>();
        std::shared_ptr<T> res = data_queue.front();  
        data_queue.pop();
        return res;
    }
    void push(T new_value)
    {
        std::shared_ptr<T> data(std::make_shared<T>(std::move(new_value)));  
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(data);
        data_cond.notify_one();
    }
    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};




//静态虚拟节点
template<typename T>
class threadsafe_queue_ht_demo {
private:
    struct Node {
        std::shared_ptr<T> data;
        std::unique_ptr<Node> next;

        Node() = default; // 1. 必须提供默认构造函数以创建虚拟节点
        explicit Node(std::shared_ptr<T> d) : data(std::move(d)) {}
    };

    std::mutex head_mutex;
    std::mutex tail_mutex;
    std::condition_variable data_cond;
    std::unique_ptr<Node> head; // 拥有整个链表（从虚拟节点开始）
    Node* tail;                 // 2. 必须是裸指针！不能是 unique_ptr，避免双重释放

    // 3. 必须加锁读取 tail，防止与 push 产生数据竞争
    Node* get_tail() {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }

    // 4. 提取统一的出队逻辑，处理复杂的指针摘除和 tail 重置
    void unlink_head() {
        // 调用此函数前必须已经持有 head_mutex，且队列不为空
        std::unique_ptr<Node> old_next = std::move(head->next);
        head->next = std::move(old_next->next);

        // 5. 核心修正：如果摘除后队列为空，必须将 tail 重置指向虚拟节点
        if (head->next == nullptr) {
            std::lock_guard<std::mutex> tail_lock(tail_mutex);
            tail = head.get();
        }
    }

public:
    threadsafe_queue_ht_demo() : head(std::make_unique<Node>()), tail(head.get()) {}

    threadsafe_queue_ht_demo(const threadsafe_queue_ht_demo&) = delete;
    threadsafe_queue_ht_demo& operator=(const threadsafe_queue_ht_demo&) = delete;

    void push(T new_value) {
        std::shared_ptr<T> new_data = std::make_shared<T>(std::move(new_value));
        std::unique_ptr<Node> p = std::make_unique<Node>(new_data);

        // 6. 使用 lock_guard 而不是 unique_lock，因为不需要配合 cv.wait
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        tail->next = std::move(p);
        tail = tail->next.get(); // 7. 正确更新裸指针 tail
        data_cond.notify_one();
    }

    void wait_and_pop(T& val) {
        std::unique_lock<std::mutex> ulk(head_mutex);
        // 8. 使用 get_tail() 读取 tail，确保线程安全
        data_cond.wait(ulk, [this]() {
            return head.get() != get_tail();
            });
        val = std::move(*(head->next->data));
        unlink_head();
    }

    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> ulk(head_mutex);
        data_cond.wait(ulk, [this]() {
            return head.get() != get_tail();
            });
        std::shared_ptr<T> ret = std::move(head->next->data);
        unlink_head();
        return ret;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(head_mutex);
        if (head.get() == get_tail()) return false;
        value = std::move(*(head->next->data));
        unlink_head();
        return true;
    }

    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(head_mutex);
        // 9. 修正返回值类型错误，返回 nullptr 而不是 false
        if (head.get() == get_tail()) return nullptr;
        std::shared_ptr<T> ret = std::move(head->next->data);
        unlink_head();
        return ret;
    }

    bool empty() {
        std::lock_guard<std::mutex> lk(head_mutex);
        return head.get() == get_tail();
    }
};



template <typename T>
class threadsafe_queue_ht {
public:
    threadsafe_queue_ht() : head(std::make_unique<node>()), tail(head.get()) {}
    threadsafe_queue_ht(const threadsafe_queue_ht&) = delete;
    threadsafe_queue_ht& operator=(const threadsafe_queue_ht&) = delete;

    void push(T new_value) {
        auto new_data = std::make_shared<T>(std::move(new_value));
        auto p = std::make_unique<node>();
        node* new_tail = p.get();
        std::lock_guard<std::mutex> lk(tail_mutex);
        tail->data = new_data;
        tail->next = std::move(p);
        tail = new_tail;
        data_cond.notify_one();
    }

    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(head_mutex);
        if (head.get() == get_tail()) return nullptr;

        auto old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head->data;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(head_mutex);
        if (head.get() == get_tail()) return false;
        value = std::move(*head->data);
        auto old_head = std::move(head);
        head = std::move(old_head->next);
        return true;
    }

    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> ulk(head_mutex);
        data_cond.wait(ulk, [this]() {
            return head.get() != tail;
        });
        auto old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head->data;
    }

    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> ulk(head_mutex);
        data_cond.wait(ulk, [this]() {
            return head.get() != tail;
        });
        value = std::move(*head->data);
        auto old_head = std::move(head);
        head = std::move(old_head->next);
    }

    bool empty() {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return head.get() == tail;
    }

private:
    struct node {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };

    node* get_tail() {
        std::lock_guard<std::mutex> lk(tail_mutex);
        return tail;
    }

    std::mutex head_mutex;
    std::mutex tail_mutex;
    std::condition_variable data_cond;
    std::unique_ptr<node> head;
    node* tail;
};

