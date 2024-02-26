/**
 * @file Range.h
 */
#pragma once
#include <cstdint>

namespace ps {

/**
 * @brief 一段区间 [begin, end)
 */
struct Range {
	Range(): Range(0, 0) {}
	Range(uint64_t begin, uint64_t end): begin(begin), end(end) {}

	/**
	 * @brief 区间长度
	 */
	uint64_t size() {
		return end - begin;
	}

	uint64_t begin;
	uint64_t end;
};

} // namespace ps
