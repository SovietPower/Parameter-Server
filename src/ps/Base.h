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

/* 包含所有节点的组 ID */
static constexpr int kAllNodes = kScheduler + kServerGroup + kWorkerGroup;

/* Key 的类型 */
using Key = uint64_t;
/* 可用 Key 的最大值 */
static constexpr Key kMaxKey = std::numeric_limits<Key>::max();

/* Value 的类型。
* 为了避免大量模板类实现，且保证类型安全，只能将其固定。
* ps-lite 中使用 char 存储，之后再将其强转为想要的类型。
* 好像并不需要，因为转为 uchar 发送，接收后再转回来再使用，好像也是安全的？
* 类型安全应该由 pb 保证，不管了直接用 char。 */
// using Value = int;

} // namespace ps
