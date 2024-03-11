#include <iostream>

#include "ps/PS.h"
#include "internal/Message.h"

int main(int, char**){
	using std::cout;

	cout << ps::IsScheduler() << '\n';

    std::cout << "Hello, from my-ps!\n";

	LOG(DEBUG) << "this is a debug msg!";
	LOG(INFO) << "this is a info msg!";

	puts("------");
	ps::Message msg;
	std::cout << msg.DebugString() << std::endl;
	ps::Meta meta;
	std::cout << meta.DebugString() << std::endl;
}
