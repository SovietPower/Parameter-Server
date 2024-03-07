#include "Resender.h"

#include <thread>
#include <vector>

#include "../internal/Van.h"

namespace ps {

Resender::Resender(int timeout_in_ms, int max_retry, Van* van): exit_(false) {
	timeout_ = timeout_in_ms;
	max_retry_ = max_retry;
	van_ = van;
	resender_ = new std::thread(&Resender::ResendThread, this);
}

Resender::~Resender() {
	exit_ = true;
	resender_->join();
	delete resender_;
}

void Resender::OnSend(const Message& msg) {
	if (msg.meta.control.IsACK()) {
		return;
	}
	auto sign = GetSign(msg);
	std::lock_guard<std::mutex> lock(tobe_acked_mu_);
	const auto& [it, inserted] =
		tobe_acked_.try_emplace(sign); // tobe_acked_.try_emplace(sign, msg, Now(), 0);
	if (!inserted) {
		return; // 被重发的、过去未确认的消息，忽略
	}
	Entry& entry = it->second;
	entry.msg = msg;
	entry.send = Now();
	entry.retry = 0;
	// 此时的消息仍可能是过去已确认的过时消息，会在 OnReceive 中忽略
}

bool Resender::OnReceive(const Message& msg) {
	if (msg.meta.control.cmd == Control::TERMINATE) [[unlikely]] {
		return false;
	} else if (msg.meta.control.IsACK()) {
		// 由于消息可能被发送多次，ACK 也可能被发送多次，但没有影响
		std::lock_guard<std::mutex> lock(tobe_acked_mu_);
		tobe_acked_.erase(msg.meta.msg_sign);
		return true;
	} else {
		auto sign = GetSign(msg);
		received_mu_.lock();
		const auto& [_, inserted] =
			received_.insert(sign);
		received_mu_.unlock();

		// 发送 ACK（即使消息重复也发送）
		Message ack;
		ack.meta.sender = msg.meta.receiver;
		ack.meta.receiver = msg.meta.sender;
		ack.meta.control.cmd = Control::ACK;
		ack.meta.msg_sign = sign;
		van_->Send(ack);

		// 重复消息
		if (!inserted) {
			LOG(WARNING) << "Received duplicated msg: " << msg.DebugString(0, 1);
		}
		return !inserted;
	}
}

uint64_t Resender::GetSign(const Message& msg) {
	CHECK_NE(msg.meta.timestamp, Meta::kEmpty) << msg.DebugString();
	// 签名组成：16位 app_id + 8位 senderID + 8位 receiverID + 31位时间戳 + 1位 request
	// TODO: why？注意当 sender 为空时，将当前 NodeID 作为 senderID
	const auto& meta = msg.meta;
	return (static_cast<uint64_t>(meta.app_id) << 48) |
		(static_cast<uint64_t>(static_cast<uint8_t>(meta.sender == Meta::kEmpty ? van_->my_node().id : meta.sender)) << 40) | // 需要清空低8位外的位
		(static_cast<uint64_t>(static_cast<uint8_t>(meta.receiver)) << 32) |
		(static_cast<uint32_t>(meta.timestamp) << 1) |
		meta.request;
}

void Resender::ResendThread() {
	std::vector<Message> tobe_send;
	const auto timeout_time = Time(timeout_);
	while (!exit_) {
		std::this_thread::sleep_for(timeout_time);
		Time now = Now();
		tobe_acked_mu_.lock();
		for (auto& pair: tobe_acked_) {
			auto& entry = pair.second;
			if (entry.send + timeout_time * (entry.retry + 1) < now) {
				// resend
				++entry.retry;
				tobe_send.push_back(entry.msg);

				LOG(WARNING) << van_->my_node().ShortDebugString()
					<< ": Resend msg due to timeout. retry time: " << entry.retry
					// << ", first send time: " << entry.send
					<< ",\nmsg: " << entry.msg.DebugString(0, 1);
				CHECK_LE(entry.retry, max_retry_);
			}
		}
		tobe_acked_mu_.unlock();

		for (auto& msg: tobe_send) {
			van_->Send(std::move(msg)); // Msg 的拷贝与移动代价差不多（基础类型多；有零拷贝）
		}
		tobe_send.clear();
	}
}

Resender::Time Resender::Now() {
	return std::chrono::duration_cast<Time>(
				std::chrono::high_resolution_clock::now().time_since_epoch());
}


} // namespace ps

