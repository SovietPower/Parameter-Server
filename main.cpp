#include <iostream>

#include "base/Log.h"
#include "internal/Message.h"

int main(int, char**){
    std::cout << "Hello, from my-ps!\n";

	LOG(DEBUG) << "this is a debug msg!";
	LOG(INFO) << "this is a info msg!";

	puts("------");
	ps::Message msg;
	std::cout << msg.DebugString() << std::endl;
	ps::Meta meta;
	std::cout << meta.DebugString() << std::endl;
}
