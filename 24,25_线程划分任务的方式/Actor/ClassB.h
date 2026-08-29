#pragma once
#include "ActorSingle.h"
#include "ClassC.h"

struct MsgClassB {
	std::string name;
	friend std::ostream& operator << (std::ostream& os, const MsgClassB& ca) {
		os << ca.name;
		return os;
	}
};


class ClassB : public ActorSingle<ClassB, MsgClassB> {
	friend class ActorSingle<ClassB, MsgClassB>;
public:
	~ClassB() {
		_is_stop = true;
		_que.NotifyStop();
		_t.join();
		std::cout << "ClassB destruct " << std::endl;
	}

	void DealMsg(std::shared_ptr<MsgClassB> data) {
		std::cout << "class B deal msg is " << *data << std::endl;

		MsgClassC msga;
		msga.name = "llfc";
		ClassC::Inst().PostMsg(msga);
	}
private:
	ClassB() {
		_t = std::thread([this]() {
			for (; (_is_stop.load() == false);) {
				std::shared_ptr<MsgClassB> data = _que.WaitAndPop();
				if (data == nullptr) {
					continue;
				}

				DealMsg(data);
			}

			std::cout << "ClassB thread exit " << std::endl;
			});
	}
};

