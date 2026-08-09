#pragma once

template <typename T, std::size_t Cap>
class CircularQueLight {
public:
	CircularQueLight() :
		_max_size(Cap + 1),
		_data(_alloc.allocate(_max_size)),
		_head(0), _tail(0), _tail_update(0)
	{ }
	CircularQueLight(const CircularQueLight&) = delete;
	CircularQueLight& operator=(const CircularQueLight&) = delete;
	CircularQueLight(CircularQueLight&&) = delete;
	CircularQueLight& operator=(CircularQueLight&&) = delete;

	~CircularQueLight() {
		while (_head != _tail) {
			std::destroy_at(_data + _head);
			_head = (_head + 1) % _max_size;
		}
		_alloc.deallocate(_data, _max_size);
	}

	bool push(const T& val) {
		std::size_t t;
		do {
			t = _tail.load();
			if ((t + 1) % _max_size == _head) {
				std::cout << "circular que full !" << std::endl;
				return false;
			}
		} while (!_tail.compare_exchange_strong(t, (t + 1) % _max_size));

		_data[t] = val;

		std::size_t tailup;
		do {
			tailup = t;
		} while (!_tail_update.compare_exchange_strong(tailup, (tailup + 1) % _max_size));

		std::cout << "called push data success " << val << std::endl;
		return true;
	}

	bool pop(T& val) {
		std::size_t h;
		do {
			h = _head;
			if (h == _tail.load()) {
				std::cout << "circular que empty!" << std::endl;
				return false;
			}

			if (h == _tail_update.load()) {
				continue;
			}

			val = _data[h];
		} while (!_head.compare_exchange_strong(h, (h + 1) % _max_size));
		std::cout << "pop data success, data is: " << val << std::endl;
		return true;
	}

private:
	std::size_t _max_size;
	T* _data;
	std::atomic<std::size_t> _head;
	std::atomic<std::size_t> _tail;
	std::atomic<std::size_t> _tail_update;//负责确认写入完成
	std::allocator<T> _alloc;
};

