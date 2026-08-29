#pragma once
#include "ActorSingle.h"

struct MsgClassC {
	std::string name;
	friend std::ostream& operator << (std::ostream& os, const MsgClassC& ca) {
		os << ca.name;
		return os;
	}
};


class ClassC : public ActorSingle<ClassC, MsgClassC> {
	friend class ActorSingle<ClassC, MsgClassC>;
public:
	~ClassC() {
		_is_stop = true;
		_que.NotifyStop();
		_t.join();
		std::cout << "ClassC destruct " << std::endl;
	}

	void DealMsg(std::shared_ptr<MsgClassC> data) {
		std::cout << "class C deal msg is " << *data << std::endl;
	}
private:
	ClassC() {
		_t = std::thread([this]() {
			for (; (_is_stop.load() == false);) {
				std::shared_ptr<MsgClassC> data = _que.WaitAndPop();
				if (data == nullptr) {
					continue;
				}

				DealMsg(data);
			}

			std::cout << "ClassC thread exit " << std::endl;
			});
	}
};

