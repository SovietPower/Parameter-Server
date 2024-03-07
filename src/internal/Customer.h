/**
 * @file Customer.h
 */
#pragma once
#include <functional>

#include "../internal/ThreadsafePQueue.h"

namespace ps {

/**
 * @brief worker 或 server 线程用于发送请求和接收数据消息的代理。
 * ! 只处理数据相关消息的请求与响应（拉取数据、推送数据、等待前两个完成、响应数据请求），系统控制消息由 PostOffice 与 Van 内部进行，不需要 worker 和 server 考虑。
 * ! 数据消息的响应由用户定义，与系统 ACK 无关，ACK 是保证请求与响应不丢失，两者的 Command 均为 EMPTY。
 * request ID 会被保存在消息的 timestamp 中。
 * 接收线程会持续接收 customer_id 等于当前 customer ID 的消息。
 */
class Customer final {
 public:
	/**
	 * @brief 收到消息后要执行的回调。
	 */
	using ReceiveHandle = std::function<void(const Message& received)>;

	/**
	 * @param app_id 当前进程正在服务的应用 ID
	 * @param customer_id 当前进程中某个 customer ID
	 * @param handle 收到消息后要执行的回调
	 */
	Customer(int app_id, int customer_id, const ReceiveHandle& handle);

	~Customer();

	/**
	 * @brief 获取一个 request ID，用于发起新的数据请求。
	 * @param receiver 接收节点的 ID（可以是 group ID）
	 * @return int 新请求使用的 request ID
	 */
	int NewRequest(int receiver);

	/**
	 * @brief 阻塞直到指定数据请求完成，已接收到所有指定节点的响应。
	 * @param request_id 请求所使用的 request ID
	 */
	void WaitRequest(int request_id);

	/**
	 * @brief 返回指定请求已有多少个节点收到并回复确认。
	 * @param request_id 请求所使用的 request ID
	 */
	int GetResponse(int request_id);

	/**
	 * @brief 增加收到并确认指定请求的节点数量。
	 * @param request_id 请求所使用的 request ID
	 */
	void AddResponse(int request_id, int cnt = 1);

	/**
	 * @brief 当系统收到*数据消息*时会执行的函数。由 Van 调用。
	 * @param received
	 */
	void OnReceive(const Message& received) {
		receive_queue_.Push(received);
	}

	int app_id() const {
		return app_id_;
	}
	int customer_id() const {
		return customer_id_;
	}

 private:
	void ReceiveThread();

	int app_id_;
	int customer_id_;

	/* 收到消息后要执行的回调 */
	ReceiveHandle receive_handle_;

	/* 存储已收到的消息 */
	ThreadsafePQueue receive_queue_;
	/* 接收线程 */
	std::unique_ptr<std::thread> receive_thread_;

	/* tracker_[request_id] = pair(该请求被发送给了多少个节点, 已有多少个节点收到并回复确认).
	* 记录所有请求的完成情况，以能够阻塞直到请求完成 */
	std::vector<std::pair<int, int>> tracker_;
	std::condition_variable tracker_cond_;
	std::mutex tracker_mu_; // cv 用不了读写锁

	DISABLE_COPY_AND_ASSIGN(Customer);
};


} // namespace ps