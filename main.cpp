#include <iostream>

#include <fmt/core.h>

#include "base/log.h"
#include "ps/PostOffice.h"
#include "utility/Test.hpp"

int main(int, char**){
    std::cout << "Hello, from my-ps!\n";

	fmt::println("Hello fmt!");

	LOG(DEBUG) << "this is a debug msg!";
	LOG(INFO) << "this is a info msg!";

	test_print();

	po_print();

	std::string a = "98121";
	std::string b = "021";
	CHECK_LT(a, b);
}
