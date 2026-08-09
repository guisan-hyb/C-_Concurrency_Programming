#pragma once
#include <iostream>
#include <memory>
#include <atomic>

template<typename T, size_t Cap>
class CircularQueSync : private std::allocator<T>
{
public:
    CircularQueSync() : _max_size(Cap + 1),
        _data(_alloc.allocate(_max_size)),
        _head(0), _tail(0), _tail_update(0) {
    }

    CircularQueSync(const CircularQueSync&) = delete;
    CircularQueSync(CircularQueSync&&) = delete;
    CircularQueSync& operator=(const CircularQueSync&) = delete;
    CircularQueSync& operator=(CircularQueSync&&) = delete;

    ~CircularQueSync()
    {
        size_t h = _head.load(std::memory_order_relaxed);
        size_t t = _tail.load(std::memory_order_relaxed);
        while (h != t) {
            std::destroy_at(_data + h);
            h = (h + 1) % _max_size;
        }
        _alloc.deallocate(_data, _max_size);
    }

    bool push(const T& val)
    {
        size_t t;
        // 1. 抢占位置
        while (true)
        {
            t = _tail.load(std::memory_order_relaxed);
            // 判断满：需要 acquire 来看到消费者 pop 释放的空间
            if ((t + 1) % _max_size == _head.load(std::memory_order_acquire))
            {
                return false;
            }

            // 抢占成功则跳出，失败则自旋重试
            // 注意：这里用 relaxed，因为此时数据还没写，不需要向其他线程同步任何内存状态
            if (_tail.compare_exchange_weak(t, (t + 1) % _max_size,
                std::memory_order_relaxed, std::memory_order_relaxed))
            {
                break;
            }
        }

        // 2. 写入数据 (此时本线程独占 _data[t] 的使用权)
        _data[t] = val;

        // 3. 提交数据 (两阶段提交的第二阶段)
        size_t tailup;
        while (true)
        {
            tailup = t;
            // 必须用 release：保证上面的 _data[t] = val 的写入，对拿到此更新的消费者可见
            if (_tail_update.compare_exchange_weak(tailup, (tailup + 1) % _max_size,
                std::memory_order_release, std::memory_order_relaxed))
            {
                break;
            }
            // 【性能优化点】：此处失败说明前序生产者还没写完，自旋等待是必要的。
            // 在极高并发下，可插入 _mm_pause() 或 std::this_thread::yield() 让出CPU
        }

        return true;
    }

    bool pop(T& val)
    {
        size_t h;
        while (true)
        {
            h = _head.load(std::memory_order_relaxed);
            // 判断空
            if (h == _tail.load(std::memory_order_relaxed))
            {
                return false;
            }

            // 必须用 acquire：与 push 中 _tail_update 的 release 配对，确保能看到数据写入
            while (h == _tail_update.load(std::memory_order_acquire))
            {
                // 数据未就绪，前序生产者正在写。自旋等待，绝不能 return false!
                // 【性能优化点】：同上，可插入 _mm_pause() 或 yield 避免总线风暴
            }

            // 此时数据绝对就绪，读取数据
            val = _data[h];

            // 4. 尝试移动头指针
            // 用 release：通知 push 线程这个位置已经 pop 完毕，可以重新写入了
            if (_head.compare_exchange_weak(h, (h + 1) % _max_size,
                std::memory_order_release, std::memory_order_relaxed))
            {
                break; // 成功拿到数据，退出
            }
            // CAS 失败说明被其他消费者抢先拿走了，继续循环重试即可
        }

        return true;
    }

private:
    size_t _max_size;
    T* _data;
    std::atomic<size_t>  _head;
    std::atomic<size_t> _tail;
    std::atomic<size_t> _tail_update;
    std::allocator<T> _alloc;
};

