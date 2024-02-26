#include <iostream>

#include <fmt/core.h>

#include "base/Log.h"
#include "ps/PostOffice.h"
#include "ps/Message.h"

int main(int, char**){
    std::cout << "Hello, from my-ps!\n";

	fmt::println("Hello fmt!");

	LOG(DEBUG) << "this is a debug msg!";
	LOG(INFO) << "this is a info msg!";

	po_print();

	puts("------");
	ps::Message msg;
	std::cout << msg.DebugString() << std::endl;
	ps::Meta meta;
	std::cout << meta.DebugString() << std::endl;
}
