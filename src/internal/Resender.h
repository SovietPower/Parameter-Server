/**
 * @file Resender.h
 */
#pragma once
// #include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

#include "../internal/Message.h"

namespace std {
class thread;
} // std

namespace ps {

class Van;

/**
 * @brief 当一条消息没有在指定时间内收到确认时，进行重发。
 * 需要在收发消息时调用 OnReceive/OnSend。
 */
class Resender {
 public:
	Resender(int timeout_in_ms, int max_retry, Van* van);
	~Resender();

	/**
	 * @brief 发送消息时需执行的逻辑：保存消息副本，以便定期检查和重发。
	 */
	void OnSend(const Message& msg);

	/**
	 * @brief 收到消息时需执行的逻辑：如果是 ACK 消息，则将原消息移出 tobe_acked；否则发送 ACK 消息，并将其加入 received。
	 * @return 如果消息已接收过、无需重复处理，或只是 ACK 消息，则返回 true
	 */
	bool OnReceive(const Message& msg);

 private:
	/**
	 * @brief 获取某条消息的签名。
	 */
	uint64_t GetSign(const Message& msg);

	/**
	 * @brief 重发线程的检查与重发逻辑。
	 */
	void ResendThread();

 	using Time = std::chrono::milliseconds;
	/**
	* @brief 获取当前时间的时间戳。单位为 chrono::milliseconds。
	*/
	Time Now();

	int timeout_;
	int max_retry_;
	Van* van_;
	std::atomic<bool> exit_;
	std::thread* resender_;

	/* 历史已接收过的消息的签名，用于消息去重，避免旧消息被多次处理。因此该集合无法清理 */
	std::unordered_set<uint64_t> received_;
	mutable std::mutex received_mu_;

	/* 待确认消息条目 */
	struct Entry {
		Message msg; // 暂存消息，用于可能的重发
		Time send; // 最初发送时间
		int retry{0}; // 已重发次数
	};
	/* 已发出但未确认的信息，用于可能的消息重发 */
	std::unordered_map<uint64_t, Entry> tobe_acked_;
	mutable std::mutex tobe_acked_mu_;
};


} // namespace ps
