#pragma once

#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>

struct message_base
{
	virtual ~message_base() = default;
};


template <typename Msg>
struct wrapped_message : message_base
{
	Msg contents;
	explicit wrapped_message(const Msg& other) : contents(other) {}
};


class queue {
public:
	template <typename T>
	void push(const T& msg) {
		std::unique_lock<std::mutex> ulk(m);
		q.push(std::make_shared<wrapped_message<T>>(msg));
		c.notify_all();
	}

	std::shared_ptr<message_base> wait_and_pop() {
		std::unique_lock<std::mutex> ulk(m);
		c.wait(ulk, [&]() {
			return !q.empty();
		});

		auto ret = q.front();
		q.pop();
		return ret;
	}

private:
	std::queue<std::shared_ptr<message_base>> q;
	std::mutex m;
	std::condition_variable c;
};


