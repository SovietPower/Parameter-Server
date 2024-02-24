/**
 * @file ThreadsafePQueue.h
 * @date 2024-02-24
 */
#pragma once
#include <queue>
#include <mutex>
#include <vector>
#include <condition_variable>

namespace ps {

/**
 * @brief 支持插入、阻塞等待获取元素的优先队列。
 */
template <class Message>
class ThreadsafePQueue {
 public:

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