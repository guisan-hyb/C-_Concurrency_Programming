#define _CRT_SECURE_NO_WARNINGS

#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <iostream>
#include <vector>

template <typename T>
class BufferedChannel {
public:
    // 强制要求容量必须大于 0
    explicit BufferedChannel(std::size_t capacity) : _capacity(capacity), _closed(false) {
        if (_capacity == 0) {
            throw std::invalid_argument("Capacity must be greater than 0 for a buffered channel");
        }
    }

    // 生产者：发送数据
    bool send(T val) {
        std::unique_lock<std::mutex> ulk(_mtx);

        // 等待直到队列有空位，或者通道关闭
        _cv_producer.wait(ulk, [this]() {
            return _queue.size() < _capacity || _closed;
            });

        // 如果通道已关闭，拒绝发送
        if (_closed) {
            return false;
        }

        _queue.push(std::move(val)); // 使用 move 语义提高效率
        _cv_consumer.notify_one();    // 通知一个等待的消费者
        return true;
    }

    // 消费者：接收数据
    bool receive(T& val) {
        std::unique_lock<std::mutex> ulk(_mtx);

        // 等待直到队列有数据，或者通道关闭
        _cv_consumer.wait(ulk, [this]() {
            return !_queue.empty() || _closed;
            });

        // 如果通道关闭但队列还有数据，必须继续取出数据！
        if (_closed && _queue.empty()) {
            return false;
        }

        val = std::move(_queue.front()); // 使用 move 语义
        _queue.pop();
        _cv_producer.notify_one();       // 通知一个等待的生产者
        return true;
    }

    // 关闭通道
    void close() {
        std::unique_lock<std::mutex> ulk(_mtx);
        _closed = true;
        // 唤醒所有正在等待的线程，让它们检查关闭状态并退出
        _cv_producer.notify_all();
        _cv_consumer.notify_all();
    }

private:
    std::queue<T> _queue;
    std::mutex _mtx;
    std::condition_variable _cv_producer;
    std::condition_variable _cv_consumer;
    std::size_t _capacity;
    bool _closed;
};


int main() {
    // 创建一个容量为 5 的有缓冲通道
    BufferedChannel<int> ch(5);

    // 启动 3 个生产者
    std::vector<std::thread> producers;
    for (int i = 0; i < 3; ++i) {
        producers.emplace_back([&ch, i]() {
            for (int j = 0; j < 5; ++j) {
                int val = i * 100 + j;
                if (ch.send(val)) {
                    std::cout << "Producer " << i << " sent: " << val << "\n";
                }
            }
            });
    }

    // 启动 2 个消费者
    std::vector<std::thread> consumers;
    for (int i = 0; i < 2; ++i) {
        consumers.emplace_back([&ch, i]() {
            int val;
            while (ch.receive(val)) {
                std::cout << "  Consumer " << i << " received: " << val << "\n";
            }
            std::cout << "  Consumer " << i << " exited.\n";
            });
    }

    // 等待所有生产者发送完毕
    for (auto& p : producers) {
        p.join();
    }

    std::cout << "All producers done. Closing channel...\n";
    // 生产者全部结束，关闭通道
    ch.close();

    // 等待所有消费者消费完剩余数据并退出
    for (auto& c : consumers) {
        c.join();
    }

    std::cout << "All consumers done. Program exited.\n";
    return 0;
}