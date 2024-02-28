#pragma once
#include <mutex>
#include <vector>
#include <functional>
#include <unordered_map>
#include <condition_variable>

#include "base/Log.h"
#include "ps/Range.h"

namespace ps {

class Van;
class Customer;

/**
 * @brief 单例类，系统的主要组件，处理当前进程的信息收发(?)，保存整个系统的状态信息。
 * 除非特殊声明，否则所有接口都是线程安全的。
 */
class PostOffice {
 public:
	using Callback = std::function<void()>;

	/**
	 * @brief 启动系统。
	 * @param customer_id 当前 customer_id。
	 * @param argv0 程序名，用于初始化 glog。
	 * @param need_barrier 是否需要阻塞当前节点，直到所有节点都启动完成。
	 */
	void Start(int customer_id, const char* argv0, bool need_barrier = true);
	/**
	 * @brief 结束系统（当前节点退出系统）。
	 * 所有节点在退出前都需调用，以结束整个系统。
	 * @param customer_id 当前 customer_id。
	 * @param need_barrier 是否需要阻塞当前节点，直到所有节点都退出系统。
	 */
	void Finalize(int customer_id, bool need_barrier = true);

	/**
	 * @brief 在系统中添加一个 customer。
	 */
	void AddCustomer(Customer* customer);
	/**
	 * @brief 从系统中移除某个 customer（通过 customer_id 识别）。
	 */
	void RemoveCustomer(Customer* customer);
	/**
	 * @brief 从系统中获取某个 customer。
	 * 如果在指定时间后仍不存在对应的 customer，返回 nullptr。
	 * @param timeout_in_sec 超时时间。为 0 则要求立刻返回。
	 */
	Customer* GetCustomer(int app_id, int customer_id, int timeout_in_sec = 0);

	/**
	 * @brief 获取每个服务器存储的 key 的区间。
	 */
	const std::vector<Range>& GetServerRanges();

	/**
	 * @brief 获取某个组所包含的节点 ID。
	 * 如果 group_id 只是某个节点的 id，则返回 {group_id}。
	 */
	const std::vector<int>& GetNodeIDs(int group_id) const {
		const auto it = node_ids_.find(group_id);
		CHECK(it != node_ids_.cend()) << "Get non-existed node [" << group_id << "]";
		return it->second;
	}

	/**
	 * @brief 注册系统退出时要执行的回调函数。
	 */
	void RegisterExitCallback(const Callback& cb) {
		exit_callback_ = cb;
	}
	void RegisterExitCallback(Callback&& cb) {
		exit_callback_ = std::move(cb);
	}

	static PostOffice* Get() {
		static PostOffice po;
		return &po;
	}

	Van* van() {
		return van_;
	}

 private:
	PostOffice() = default;
	~PostOffice();

	/**
	 * @brief 读取环境变量，配置初始化系统。
	 */
	void InitEnv();

	Van* van_;
	/* 当前节点身份 */
	bool is_worker_, is_server_, is_scheduler_;
	int num_servers_, num_workers_;

	/* 系统当前启动阶段 */
	int start_stage_ = 0;
	mutable std::mutex start_mu_;
	/* 系统运行起始时间 */
	std::time_t start_time_;
	/* 系统退出时需要执行的回调 */
	Callback exit_callback_;

	std::condition_variable barrier_cond_;
	std::mutex barrier_mu_;
	/* 每个服务器存储的 key 的区间 */
	std::vector<Range> server_key_ranges_;
	std::mutex server_key_ranges_mu_;
	/* 每个节点上次收到心跳的时间 */
	std::unordered_map<int, time_t> heartbeats_;
	std::mutex heartbeat_mu_;

	/* app_id -> (customer_id -> customer pointer).
	通过 (app_id, customer_id) 获取指定 customer */
	std::unordered_map<int, std::unordered_map<int, Customer*>> customers_;

	/* app_id -> (customer_id -> 该 customer 是否同步完成) */
	std::unordered_map<int, std::unordered_map<int, bool>> barrier_done_;

	/* group_id -> node_ids which belong to this group.
	获取某个组所包含的节点 ID */
	std::unordered_map<int, std::vector<int>> node_ids_;

	DISABLE_COPY_AND_ASSIGN(PostOffice);
};


} // namespace ps