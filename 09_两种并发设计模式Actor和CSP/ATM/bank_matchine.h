#pragma once

#include "withdraw_msg.h"
#include "dispatcher.h"
#include <functional>

class bank_matchine {
public:
	bank_matchine() : balance(199) {}

	messaging::sender get_sender() {
		return incoming;
	}

	void done() {
		get_sender().send(messaging::close_queue());
	}

	void run() {
		try {
			for (;;) {
				incoming.wait().handle<verify_pin, std::function<void(const verify_pin& msg)>, messaging::dispatcher>(
					[&](const verify_pin& msg) {
						if (msg.pin == "521024") {
							msg.atm_queue.send(pin_verified());
						}
						else {
							msg.atm_queue.send(pin_incorrect());
						}
					}, "verify_pin"
				).handle<withdraw, std::function<void(const withdraw& msg)>>(
					[&, this](const withdraw& msg) {
						if (this->balance >= msg.amount) {
							msg.atm_queue.send(withdraw_ok());
							this->balance -= msg.amount;
						}
						else {
							msg.atm_queue.send(withdraw_denied());
						}
					},"withdraw"
				).handle<get_balance, std::function<void(const get_balance& msg)>>(
					[&](const get_balance& msg) {
						msg.atm_queue.send(::balance(balance));
					},"get_balance"
				).handle<withdrawal_processed, std::function<void(const withdrawal_processed& msg)>>(
					[&](const withdrawal_processed& msg) {
						std::cout << "withdrawal_processed !!!" << std::endl;
					}, "withdrawal_processed"
				).handle<cancel_withdrawal, std::function<void(const cancel_withdrawal& msg)>>(
					[&](const cancel_withdrawal& msg) {
						std::cout << "cancel_withdrawal !!!" << std::endl;
					}, "cancel_withdrawal"
				);
			}
		}
		catch (const messaging::close_queue&) {

		}
	}

public:
	messaging::receiver incoming;
	unsigned balance;
};