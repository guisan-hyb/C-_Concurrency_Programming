#include <iostream>
#include "atm.h"
#include "bank_matchine.h"
#include "interface_machine.h"
#include <thread>

int main() {
	bank_matchine bank;
	interface_machine interface_hardware;
	atm machine(bank.get_sender(), interface_hardware.get_sender());

	std::thread bank_thread(&bank_matchine::run, &bank);
	std::thread interface_thread(&interface_machine::run, &interface_hardware);
	std::thread atm_thread(&atm::run, &machine);

	messaging::sender atm_que(machine.get_sender());
	bool quit_pressed = false;

	while (!quit_pressed) {
		char c = getchar();
		switch (c)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			atm_que.send(digit_pressed(c));
			break;
		case 'b':
			atm_que.send(balance_pressed());
			break;
		case 'w':
			atm_que.send(withdraw_pressed(50));
			break;
		case 'c':
			atm_que.send(cancel_pressed());
			break;
		case 'q':
			quit_pressed = true;
			break;
		case 'i':
			atm_que.send(card_inserted("accdsa"));
			break;

		default:
			break;
		}
	}

	bank.done();
	machine.done();
	interface_hardware.done();

	atm_thread.join();
	bank_thread.join();
	interface_thread.join();

	return 0;
}

