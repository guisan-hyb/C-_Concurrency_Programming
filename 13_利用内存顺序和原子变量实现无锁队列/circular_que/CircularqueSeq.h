#pragma once

#include <iostream>
#include <atomic>
#include <memory>

template <typename T, std::size_t Cap>
class CircularqueSeq {
public:
	CircularqueSeq()
		: _max_size(Cap + 1),
		_data(_alloc.allocate(_max_size)),
		_atomic_using(false),
		_head(0), _tail(0)
	{ }

	CircularqueSeq(const CircularqueSeq&) = delete;
	CircularqueSeq& operator=(const CircularqueSeq&) = delete;
	CircularqueSeq(CircularqueSeq&&) = delete;
	CircularqueSeq& operator=(CircularqueSeq&&) = delete;

	~CircularqueSeq() {
		while (_head != _tail) {
			std::destroy_at(_data + _head);
			_head = (_head + 1) % _max_size;
		}
		_alloc.deallocate(_data, _max_size);
	}

	template <typename... Args>
	bool emplace(Args&& ... args) {
		bool use_expected = false;
		bool use_desired = true;
		do {
			use_expected = false;
			use_desired = true;
		} while (!_atomic_using.compare_exchange_strong(use_expected, use_desired));

		if ((_tail + 1) % _max_size == _head) {
			std::cout << "circular que full" << std::endl;
			do {
				use_expected = true;
				use_desired = false;
			} while (!_atomic_using.compare_exchange_strong(use_expected, use_desired));
			return false;
		}

		std::construct_at(_data + _tail, std::forward<Args>(args)...);
		_tail = (_tail + 1) % _max_size;

		do {
			use_expected = true;
			use_desired = false;
		} while (!_atomic_using.compare_exchange_strong(use_expected, use_desired));

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
		bool use_expected = false;
		bool use_desired = true;
		do {
			use_expected = false;
			use_desired = true;
		} while (!_atomic_using.compare_exchange_strong(use_expected, use_desired));

		if (_head == _tail) {
			std::cout << "circular que empty!" << std::endl;
			do {
				use_expected = true;
				use_desired = false;
			} while (!_atomic_using.compare_exchange_strong(use_expected, use_desired));
			return false;
		}

		val = std::move(_data[_head]);
		std::destroy_at(_data + _head);
		_head = (_head + 1) % _max_size;

		do {
			use_expected = true;
			use_desired = false;
		} while (!_atomic_using.compare_exchange_strong(use_expected, use_desired));
		
		return true;
	}

private:
	std::size_t _max_size;
	T* _data;
	std::atomic<bool> _atomic_using;
	std::size_t _head;
	std::size_t _tail;
	std::allocator<T> _alloc;
};

