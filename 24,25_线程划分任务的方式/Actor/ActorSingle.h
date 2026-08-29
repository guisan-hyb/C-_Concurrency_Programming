#pragma once

#include "ThreadSafeQue.h"
#include <thread>
#include <atomic>
#include <iostream>

template <typename ClassType, typename QueType>
class ActorSingle {
public:
	static ClassType& Inst() {
		static ClassType as;
		return as;
	}

	~ActorSingle() {}

	void PostMsg(const QueType& data) {
		_que.push(data);
	}

protected:
	std::atomic<bool> _is_stop;
	ThreadSafeQue<QueType> _que;
	std::thread _t;

	ActorSingle() : _is_stop(false) {}

	ActorSingle(const ActorSingle&) = delete;
	ActorSingle(ActorSingle&&) = delete;
	ActorSingle& operator=(const ActorSingle&) = delete;
	ActorSingle& operator=(ActorSingle&&) = delete;
};

