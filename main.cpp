#include <iostream>

#include <fmt/core.h>

#include "base/base.h"
#include "ps/PostOffice.h"
#include "utility/Test.hpp"

int main(int, char**){
    std::cout << "Hello, from my-ps!\n";

	fmt::println("Hello fmt!");

	fmt::println("pow: {}", pow(3));

	test_print();

	po_print();
}
