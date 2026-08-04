#pragma once

#include "message.h"
#include <string>
#include <iostream>

namespace messaging {
	class close_queue {}; //示意关闭队列的消息


	class dispatcher {
		// 准许TemplateDispatcher的实例访问内部数据
		template <typename Dispatcher, typename Msg, typename Func>
		friend class TemplateDispatcher;

	public:
		dispatcher(dispatcher&& other) : _q(other._q), chained(other.chained) {
			//上游的消息分发者不会等待消息
			other.chained = true;// 把临时对象的链尾身份夺走，防止它在析构时擅自启动等待
		}

		explicit dispatcher(queue* q) : _q(q), chained(false) {}

		//根据TemplateDispatcher处理某种具体类型的消息
		template <typename Message, typename Func, typename dispatcher>
		TemplateDispatcher<dispatcher, Message, Func>
			handle(Func&& f, std::string info_msg)
		{
			//std::cout << "Dispatcher handle msg is: " << info_msg << std::endl;
			return TemplateDispatcher<dispatcher, Message, Func>(
				_q, this, std::forward<Func>(f), info_msg
			);
		}

		//析构函数可能抛出异常
		~dispatcher() noexcept(false) { //链尾的析构是唯一启动点
			if (!chained) { //当前节点是链尾
				wait_and_dispatch();// 阻塞等待消息
			}
		}

	private:
		dispatcher(const dispatcher&) = delete;
		dispatcher& operator=(const dispatcher&) = delete;

		void wait_and_dispatch() {
			for (;;) { //无限循环，等待消息并发送消息
				auto msg = _q->wait_and_pop();
				dispatch(msg);
			}
		}

		//dispatch()判别消息是否属于close_queue类型，若属于，则抛出异常
		bool dispatch(const std::shared_ptr<message_base>& msg) {
			if (dynamic_cast<wrapped_message<close_queue>*>(msg.get())) {
				throw close_queue();
			}
			return false;
		}

	private:
		queue* _q;
		bool chained;// 标记我不是链尾: true -> 不是链尾 ； false -> 是链尾
	};


	template <typename PreviousDispatcher, typename Msg, typename Func>
	class TemplateDispatcher {
		//声明：根据类模板TemplateDispatcher<>具现化而成的各种类型互为友类
		template <typename Dispatcher, typename OtherMsg, typename OtherFunc>
		friend class TemplateDispatcher;

	public:
		TemplateDispatcher(TemplateDispatcher&& other)
			: q(other.q), prev(other.prev), f(std::move(other.f)), chained(other.chained), _msg(other._msg) {
			other.chained = true;// 把临时对象的链尾身份夺走，防止它在析构时擅自启动等待
		}

		TemplateDispatcher(queue* q_, PreviousDispatcher* prev_, Func&& f_, std::string msg)
			: q(q_), prev(prev_), f(std::forward<Func>(f_)), chained(false), _msg(msg) {
			prev->chained = true;
		}

		//按衔接成链的方式引入更多处理函数
		template <typename OtherMsg, typename OtherFunc>
		TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>
			handle(OtherFunc&& of, std::string info_msg)
		{
			//std::cout << "TemplateDispatcher handle msg is: " << info_msg << std::endl;
			return TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>(
				q, this, std::forward<OtherFunc>(of), info_msg
			);
		}

		~TemplateDispatcher() noexcept(false) {
			if (!chained) {
				wait_and_dispatch();
			}
		}

	private:
		TemplateDispatcher(const TemplateDispatcher&) = delete;
		TemplateDispatcher& operator=(const TemplateDispatcher&) = delete;

		void wait_and_dispatch() {
			for (;;) {
				auto msg = q->wait_and_pop();
				//如果消息已经妥善处理，则跳出无限循环
				if (dispatch(msg)) {
					break;
				}
			}
		}

		bool dispatch(const std::shared_ptr<message_base>& msg) {
			//检查消息类别并调用相应的处理函数
			if (wrapped_message<Msg>* wrapper = dynamic_cast<wrapped_message<Msg>*>(msg.get())) { //转换成功
				f(wrapper->contents);
				return true;
			}
			else {
				//衔接前一个dispatcher对象，形成连锁调用
				return prev->dispatch(msg);
			}
		}

	private:
		std::string _msg;// 调试用的字符串标签，记录"我这个节点处理的消息叫什么名字"
		queue* q;
		PreviousDispatcher* prev;
		Func f;
		bool chained;// 标记我不是链尾: true -> 不是链尾, 析构时啥也别做 ； false -> 是链尾, 析构时触发等待
	};


	class sender {
	public:
		sender() : _q(nullptr) {}
		explicit sender(queue* q) : _q(q) {}

		template <typename Message>
		void send(const Message& msg) {
			if (_q) {
				_q->push(msg);
			}
		}

	private:
		queue* _q;// 只能借用消息队列，相当于引用
	};


	class receiver {
	public:
		operator sender() { // receiver对象准许隐式转换为sender对象，前者拥有的队列被后者引用
			return sender(&_q);
		}

		dispatcher wait() { 
			return dispatcher(&_q);
		}

	private:
		queue _q;// 完全拥有消息队列
	};
}

