#pragma once

#include "dispatcher.h"
#include "withdraw_msg.h"
#include <functional>
#include <iostream>


class atm {
public:
	atm(messaging::sender bank_, messaging::sender interface_hardware_)
		: bank(bank_), interface_hardware(interface_hardware_) {}

	messaging::sender get_sender() {
		return incoming;
	}

	void done() {
		get_sender().send(messaging::close_queue());
	}

	void run() {
		state = &atm::waiting_for_card;
		try {
			for (;;) {
				(this->*state)();
			}
		}
		catch (const messaging::close_queue&) {

		}
	}

private:
	void process_withdrawal() { // 具体处理
		incoming.wait().handle<withdraw_ok, std::function<void(withdraw_ok const& msg)>,
			messaging::dispatcher >(
				[&](withdraw_ok const& msg)
				{
					interface_hardware.send(
						issue_money(withdrawal_amount));
					bank.send(
						withdrawal_processed(account, withdrawal_amount));
					state = &atm::done_processing;
				}, "withdraw_ok").handle<withdraw_denied, std::function<void(withdraw_denied const& msg)>>(
					[&](withdraw_denied const& msg)
					{
						interface_hardware.send(display_insufficient_funds());
						state = &atm::done_processing;
					}, "withdraw_denied").handle<cancel_pressed, std::function<void(cancel_pressed const& msg)>>(
						[&](cancel_pressed const& msg)
						{
							bank.send(
								cancel_withdrawal(account, withdrawal_amount));
							interface_hardware.send(
								display_withdrawal_cancelled());
							state = &atm::done_processing;
						}, "cancel_pressed"
					);
	}

	void process_balance() { // 具体处理
		incoming.wait()
			.handle<balance, std::function<void(balance const& msg)>,
			messaging::dispatcher>(
				[&](balance const& msg)
				{
					interface_hardware.send(display_balance(msg.amount));
					state = &atm::wait_for_action;
				}, "balance"
			).handle < cancel_pressed, std::function<void(cancel_pressed const& msg) >>(
				[&](cancel_pressed const& msg)
				{
					state = &atm::done_processing;
				}, "cancel_pressed"
			);
	}

	void wait_for_action() { // (4)
		interface_hardware.send(display_withdrawal_options());
		incoming.wait()
			.handle<withdraw_pressed, std::function<void(withdraw_pressed const& msg)>,
			messaging::dispatcher>(
				[&](withdraw_pressed const& msg)
				{
					withdrawal_amount = msg.amount;
					bank.send(withdraw(account, msg.amount, incoming));
					state = &atm::process_withdrawal;
				}, "withdraw_pressed"
			).handle < balance_pressed, std::function<void(balance_pressed const& msg) >>(
				[&](balance_pressed const& msg)
				{
					bank.send(get_balance(account, incoming));
					state = &atm::process_balance;
				}, "balance_pressed"
			).handle<cancel_pressed, std::function<void(cancel_pressed const& msg) >>(
				[&](cancel_pressed const& msg)
				{
					state = &atm::done_processing;
				}, "cancel_pressed"
			);
	}

	void verifying_pin() { // (3)
		incoming.wait()
			.handle<pin_verified, std::function<void(pin_verified const& msg)>,
			messaging::dispatcher>(
				[&](pin_verified const& msg)
				{
					state = &atm::wait_for_action;
				}, "pin_verified"
			).handle<pin_incorrect, std::function<void(pin_incorrect const& msg)>>(
				[&](pin_incorrect const& msg)
				{
					interface_hardware.send(
						display_pin_incorrect_message());
					state = &atm::done_processing;// 回归
				}, "pin_incorrect"
			).handle<cancel_pressed, std::function<void(cancel_pressed const& msg)>>(
				[&](cancel_pressed const& msg)
				{
					state = &atm::done_processing;// 回归
				}, "cancel_pressed"
			);
	}

	void getting_pin() { // (2)
		incoming.wait().handle<digit_pressed, std::function<void(const digit_pressed& msg)>, messaging::dispatcher>(
			[&](const digit_pressed& msg) {
				const unsigned pin_length = 6;
				pin += msg.digit;
				if (pin.length() == pin_length) {
					bank.send(verify_pin(account, pin, incoming));
					state = &atm::verifying_pin;
				}
			},"digit_pressed"
		).handle<clear_last_pressed, std::function<void(const clear_last_pressed& msg)>>(
			[&](const clear_last_pressed& msg) {
				if (!pin.empty()) {
					pin.pop_back();
				}
			}, "clear_last_pressed"
		).handle<cancel_pressed, std::function<void(const cancel_pressed& msg)>>(
			[&](const cancel_pressed& msg) {
				state = &atm::done_processing;//回归
			}, "cancel_pressed"
		);
	}

	void waiting_for_card() { // (1)
		interface_hardware.send(display_enter_card());
		incoming.wait().handle<card_inserted, std::function<void(const card_inserted& msg)>, messaging::dispatcher>(
			[&](const card_inserted& msg) {
				account = msg.account;
				pin = "";
				interface_hardware.send(display_enter_pin());
				state = &atm::getting_pin;
			}, "card_inserted"
		);
	}

	void done_processing() {
		interface_hardware.send(eject_card());
		state = &atm::waiting_for_card;// 回到开头
	}

	atm(const atm&) = delete;
	atm& operator=(const atm&) = delete;

private:
	messaging::receiver incoming;
	messaging::sender bank;
	messaging::sender interface_hardware;
	void (atm::* state) (); //using state = void (atm::*)(); + state ptr;
	std::string account;
	unsigned withdrawal_amount;
	std::string pin;
};