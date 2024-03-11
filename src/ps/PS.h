/**
 * @file PS.h
 * @brief 用户使用 PS 需要包含的唯一头文件。
 * 封装和提供 PS 的内部接口。
 */
#pragma once
#include "../ps/Base.h"
#include "../ps/KVApp.h"

#include "../internal/PostOffice.h"

namespace ps {

/**
 * @brief 启动系统。会阻塞当前节点，直到所有节点都启动完成。
 * @param customer_id 当前 customer_id
 * @param argv0 程序名，用于初始化 glog
 */
inline void Start(int customer_id, const char* argv0 = nullptr) {
	PostOffice::Get()->Start(customer_id, argv0, true);
}

/**
 * @brief 启动系统。不会阻塞当前节点。
 * @param customer_id 当前 customer_id
 * @param argv0 程序名，用于初始化 glog
 */
inline void StartAsync(int customer_id, const char* argv0 = nullptr) {
	PostOffice::Get()->Start(customer_id, argv0, false);
}

/**
 * @brief 结束系统（当前节点退出系统）。
 * 所有节点在退出前都需调用，以结束整个系统。
 * @param customer_id 当前 customer_id
 * @param need_barrier 是否需要阻塞当前节点，直到所有节点都退出系统
 */
inline void Finalize(int customer_id, bool need_barrier = true) {
	PostOffice::Get()->Finalize(customer_id, need_barrier);
}

/**
 * @brief 注册系统退出时（Finalize() 之后）要执行的回调函数。
 * 如果需要阻塞，则会在阻塞完后执行。
 * RegisterExitCallback(cb); Finalize();
 * 等价于
 * Finalize(); cb();
 */
inline void RegisterExitCallback(const std::function<void()>& cb) {
	PostOffice::Get()->RegisterExitCallback(cb);
}

/**
 * @brief 获取系统的 worker 数量。
 */
inline int NumWorkers() {
	return PostOffice::Get()->num_workers();
}
/**
 * @brief 获取系统的 server 数量。
 */
inline int NumServers() {
	return PostOffice::Get()->num_servers();
}
/**
 * @brief 检查当前节点是否是 worker。
 */
inline bool IsWorker() {
	return PostOffice::Get()->is_worker();
}
/**
 * @brief 检查当前节点是否是 server。
 */
inline bool IsServer() {
	return PostOffice::Get()->is_server();
}
/**
 * @brief 检查当前节点是否是 scheduler。
 */
inline bool IsScheduler() {
	return PostOffice::Get()->is_scheduler();
}
/**
 * @brief 获取当前节点的 rank。
 * rank 是节点在 worker/server 中的逻辑编号（取决于其身份），范围为 [0, num_workers/num_servers)。
 */
inline int MyRank() {
	return PostOffice::Get()->my_rank();
}

} // namespace ps