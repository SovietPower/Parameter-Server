#pragma once

#include <fmt/core.h>

inline int pow(int x) {
	fmt::println("fmt from base.h!");
	return x * x * x;
}