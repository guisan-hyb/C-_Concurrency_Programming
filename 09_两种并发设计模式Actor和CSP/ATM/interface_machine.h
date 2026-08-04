#pragma once

#include "withdraw_msg.h"
#include <mutex>
#include <functional>

class interface_machine {
public:
	messaging::sender get_sender() {
		return incoming;
	}

	void done() {
		get_sender().send(messaging::close_queue());
	}

	void run() {
		try {
			for (;;) {
				incoming.wait().handle<issue_money, std::function<void(const issue_money& msg)>, messaging::dispatcher>(
					[&](const issue_money& msg) {
						{
							std::lock_guard<std::mutex> lk(iom);
							std::cout << "Issuing: " << msg.amount << std::endl;
						}
					},"issue_money"
				).handle<display_insufficient_funds, std::function<void(const display_insufficient_funds& msg)>>(
					[&](const display_insufficient_funds& msg) {
						{
							std::lock_guard<std::mutex> lk(iom);
							std::cout << "Insufficient funds" << std::endl;
						}
					},"display_insufficient_funds"
				).handle<display_enter_pin, std::function<void(const display_enter_pin& msg)>>(
					[&](const display_enter_pin& msg) {
						{
							std::lock_guard<std::mutex> lk(iom);
							std::cout << "Please enter your PIN(0-9)" << std::endl;
						}
					},"display_enter_pin"
				).handle<display_enter_card, std::function<void(const display_enter_card& msg)>>(
					[&](const display_enter_card& msg) {
						{
							std::lock_guard<std::mutex> lk(iom);
							std::cout << "Please enter your card (I)" << std::endl;
						}
					},"display_enter_card"
				).handle<display_balance, std::function<void(const display_balance& msg)>>(
					[&](const display_balance& msg) {
						{
							std::lock_guard<std::mutex> lk(iom);
							std::cout << "The balance of your account is: " << msg.amount << std::endl;
						}
					},"display_balance"
				).handle<display_withdrawal_options, std::function<void(const display_withdrawal_options& msg)>>(
					[&](const display_withdrawal_options& msg) {
						{
							std::lock_guard<std::mutex> lk(iom);
							std::cout << "Withdraw 50? (w)" << std::endl;
							std::cout << "Display Balance? (b)" << std::endl;
							std::cout << "Cancel? (c)" << std::endl;
						}
					},"display_withdrawal_options"
				).handle<display_withdrawal_cancelled, std::function<void(const display_withdrawal_cancelled& msg)>>(
					[&](const display_withdrawal_cancelled& msg) {
						{
							std::lock_guard<std::mutex> lk(iom);
							std::cout << "Withdrawal cancelled" << std::endl;
						}
					},"display_withdrawal_cancelled"
				).handle<display_pin_incorrect_message, std::function<void(const display_pin_incorrect_message& msg)>>(
					[&](const display_pin_incorrect_message& msg) {
						{
							std::lock_guard<std::mutex> lk(iom);
							std::cout << "PIN incorrect" << std::endl;
						}
					}, "display_pin_incorrect_message"
				).handle<eject_card, std::function<void(const eject_card& msg)>>(
					[&](const eject_card& msg) {
						{
							std::lock_guard<std::mutex> lk(iom);
							std::cout << "eject_card" << std::endl;
						}
					}, "eject_card"
				);
			}
		}
		catch (const messaging::close_queue&) {

		}
	}

private:
	std::mutex iom;
	messaging::receiver incoming;
};

