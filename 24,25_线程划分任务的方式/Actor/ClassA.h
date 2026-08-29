#pragma once

#include "ActorSingle.h"
#include "ClassB.h"

struct MsgClassA {
	std::string name;
	friend std::ostream& operator<< (std::ostream& os, const MsgClassA& ca) {
		os << ca.name;
		return os;
	}
};

class ClassA : public ActorSingle<ClassA, MsgClassA> {
	friend class ActorSingle<ClassA, MsgClassA>;
public:
	~ClassA() {
		_is_stop = true;
		_que.NotifyStop();
		_t.join();
		std::cout << "Class A destruct " << std::endl;
	}

	void DealMsg(std::shared_ptr<MsgClassA> data) {
		std::cout << "class A deal msg is: " << *data << std::endl;

		MsgClassB msga;
		msga.name = "llfc";
		ClassB::Inst().PostMsg(msga);
	}

private:
	ClassA() {
		_t = std::thread([this]() {
			for (; (_is_stop.load() == false);) {
				std::shared_ptr<MsgClassA> data = _que.WaitAndPop();
				if (data == nullptr) continue;
				DealMsg(data);
			}

			std::cout << "Class A thread exit " << std::endl;
		});
	}
};
