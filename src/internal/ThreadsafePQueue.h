/**
 * @file ThreadsafePQueue.h
 */
#pragma once
#include <queue>
#include <mutex>
#include <vector>
#include <condition_variable>

#include "internal/Message.h"

namespace ps {

/**
 * @brief 线程安全、支持插入、阻塞等待获取元素的优先队列。
 * 仅用于 Message。
 */
class ThreadsafePQueue {
 public:
	ThreadsafePQueue() = default;
	~ThreadsafePQueue() = default;

	/**
	 * @brief 插入元素
	 * TODO: 根据使用情况看是否需要区分左值右值，还是直接 copy move
	 */
	void Push(Message msg) {
		mu_.lock();
		queue_.push(std::move(msg));
		mu_.unlock();
		cv_.notify_all(); // TODO: 可以改成 notify_one 吗
	}

	/**
	 * @brief 阻塞等待，直到队列非空、成功获取到一个元素
	 */
	Message WaitAndPop() {
		std::unique_lock<std::mutex> lock(mu_);
		while (queue_.empty()) {
			cv_.wait(lock);
		}
		auto ret = std::move(queue_.top());
		queue_.pop();
		return ret; // NRVO
	}

 private:
	struct Comparator {
		bool operator() (const Message& x, const Message& y) {
			return x.meta.priority <= y.meta.priority;
		}
	};

	mutable std::mutex mu_;
	std::condition_variable cv_;
	std::priority_queue<Message, std::vector<Message>, Comparator> queue_;
};

} // namespace ps