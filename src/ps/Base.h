/**
 * @file Base.h
 * @brief 对 PS 系统的配置
 */
#pragma once
#include <limits>
#include <cstdint>

namespace ps {

/* scheduler 的节点 ID */
static constexpr int kScheduler = 1;

/* 节点组的 ID，可通过 + 或 | 组合 */
/* server 所在组的 ID */
static constexpr int kServerGroup = 2;
/* worker 所在组的 ID */
static constexpr int kWorkerGroup = 4;

/* Key 的类型 */
using Key = uint64_t;
/* 可用 Key 的最大值 */
static constexpr Key kMaxKey = std::numeric_limits<Key>::max();

} // namespace ps
