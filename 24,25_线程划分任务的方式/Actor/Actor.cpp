#include <iostream>
#include "ClassA.h"
int main()
{
    MsgClassA msga;
    msga.name = "llfc";
    ClassA::Inst().PostMsg(msga);

    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "main process exited!\n";
}


