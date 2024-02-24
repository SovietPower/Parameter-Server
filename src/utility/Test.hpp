#include <fmt/core.h>

#include "base/base.h"

inline void test_print() {
	fmt::println("test ok in Test.h! VERBOSE: {}", VERBOSE);
}