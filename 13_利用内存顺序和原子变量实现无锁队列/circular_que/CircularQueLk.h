#pragma once

#include <iostream>
#include <mutex>
#include <memory>

template <typename T, std::size_t Cap>
class CircularQueLk {
public:
    CircularQueLk() 
        : _max_size(Cap + 1), 
          _data(_alloc.allocate(_max_size)),
          _head(0), 
          _tail(0) 
    {}

    CircularQueLk(const CircularQueLk&) = delete;
    CircularQueLk& operator=(const CircularQueLk&) = delete;
    CircularQueLk(CircularQueLk&&) = delete;
    CircularQueLk& operator=(CircularQueLk&&) = delete;

    ~CircularQueLk() {
        std::lock_guard<std::mutex> lk(_mtx);
        while (_head != _tail) {
            std::destroy_at(_data + _head);
            _head = (_head + 1) % _max_size;
        }
        _alloc.deallocate(_data, _max_size);
    }

    template <typename... Args>
    bool emplace(Args&&... args) {
        std::lock_guard<std::mutex> lk(_mtx);
        if ((_tail + 1) % _max_size == _head) {
            std::cout << "circular que full!" << std::endl;
            return false;
        }
        std::construct_at(_data + _tail, std::forward<Args>(args)...);
        _tail = (_tail + 1) % _max_size;
        return true;
    }

    bool push(const T& val) {
        std::cout << "called push const T& version" << std::endl;
        return emplace(val);
    }

    bool push(T&& val) {
        std::cout << "called push T&& version" << std::endl;
        return emplace(val);
    }

    bool pop(T& val) {
        std::lock_guard<std::mutex> lk(_mtx);
        if (_tail == _head) {
            std::cout << "circular que empty!" << std::endl;
            return false;
        }
        
        val = std::move(_data[_head]);
        std::destroy_at(_data + _head);
        _head = (_head + 1) % _max_size;
        return true;
    }

private:
    //成员变量的初始化顺序严格按照它们在类中声明的顺序，而不是初始化列表中的顺序
    std::size_t _max_size;
    T* _data;
    std::mutex _mtx;
    std::size_t _head;
    std::size_t _tail;

    //分配器作成员变量
    std::allocator<T> _alloc;
};

