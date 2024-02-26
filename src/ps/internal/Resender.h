/**
 * @file Resender.h
 */
#pragma once
#include <thread>

namespace ps {

/**
 * @brief 当一条消息没有在指定时间内收到时，进行重发。
 * 需要在收发消息时调用 OnReceive/OnSend。
 */
class Resender {
 public:


 private:

	std::thread* resender_;
};

} // namespace ps
