/**
 * @file PS.h
 * @brief 用户使用 PS 需要包含的唯一头文件。
 * 封装和提供 PS 的内部接口。
 */
#pragma once
#include <cstdlib>
#include <iostream>

#include "../ps/Base.h"
#include "../ps/KVApp.h"
#include "../internal/PostOffice.h"

namespace ps {

// 以下均为默认实现

/**
 * @brief 启动系统。会阻塞当前节点，直到所有节点都启动完成。
 * @param customer_id 当前 customer_id
 * @param config_filename 要读取的配置文件名（json 格式，加不加后缀都可）。当使用本地文件配置时必须设置
 * @param log_filename 程序名，或日志输出文件名，用于初始化日志
 */
inline void Start(int customer_id, const char* config_filename, const char* log_filename = nullptr) {
	PostOffice::Get()->Start(customer_id, config_filename, log_filename, true);
}

/**
 * @brief 启动系统。会阻塞当前节点，直到所有节点都启动完成。
 * @param customer_id 当前 customer_id
 * @param argc 命令行参数
 * @param argv 命令行参数
 */
inline void Start(int customer_id, int argc, char* argv[]) {
	if (argc < 2) {
		std::cout << "param error:\n"
			<< "usage: " << argv[0] << " config_filename [log_filename] [args...]\n";
		exit(0);
	}
	const char* config_filename = argv[1];
	const char* log_filename = nullptr;
	if (argc > 2) {
		log_filename = argv[2];
	}
	ps::Start(customer_id, config_filename, log_filename);
}

/**
 * @brief 启动系统。不会阻塞当前节点。
 * @param customer_id 当前 customer_id
 * @param config_filename 要读取的配置文件名（json 格式，加不加后缀都可）。当使用本地文件配置时必须设置
 * @param log_filename 程序名，或日志输出文件名，用于初始化日志
 */
inline void StartAsync(int customer_id, const char* config_filename, const char* log_filename = nullptr) {
	PostOffice::Get()->Start(customer_id, config_filename, log_filename, false);
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
 * @brief 在指定组中建立屏障，阻塞该组的所有节点，直到全部进入屏障。
 * @param customer_id 发送同步请求的 customer
 * @param group_id 需要进行同步的范围
*/
inline void Barrier(int customer_id, int group_id) {
	PostOffice::Get()->Barrier(customer_id, group_id);
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