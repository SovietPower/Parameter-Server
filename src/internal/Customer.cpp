#include "Customer.h"

#include "internal/PostOffice.h"

namespace ps {

Customer::Customer(int app_id, int customer_id, const ReceiveHandle& handle)
		: app_id_(app_id), customer_id_(customer_id), receive_handle_(handle) {
	PostOffice::Get()->AddCustomer(this);
	receive_thread_ = std::unique_ptr<std::thread>(new std::thread(&Customer::ReceiveThread, this));
}

Customer::~Customer() {
	PostOffice::Get()->RemoveCustomer(this);

	Message term;
	term.meta.control.cmd = Control::TERMINATE;
	receive_queue_.Push(term);
	receive_thread_->join();
}

int Customer::NewRequest(int receiver) {
	std::lock_guard lock(tracker_mu_);
	int num_nodes = PostOffice::Get()->GetNodeIDs(receiver).size();
	tracker_.emplace_back(num_nodes, 0);
	return tracker_.size() - 1;
}

void Customer::WaitRequest(int request_id) {
	std::unique_lock ulock(tracker_mu_);
	while (tracker_[request_id].first != tracker_[request_id].second) {
		tracker_cond_.wait(ulock);
	}
	// TODO: tracker_[rid] 不会被删除，vector 会一直变大。
	// 考虑记录 Wait 的数量，然后在解除 Wait、数量降低到0时尝试删除。还需要保证后续进入 Wait 的能直接返回。
	// 由于是 vector，可以记录一个偏移量 base，tracker[t] 实际对应 base+t 的时间戳
	// ts < base 的请求都已完成，ts >= base 的请求保证仍在 vector 内
	// 当“Wait 数量降低到0时”触发一次检查：如果 ts 之前的请求都已完成（可能还要一个 bool 数组/bitset）
	// 且 ts-base 即当前下标较大，加锁将 vector 后面的对象移动到前面、减少其最大大小，更新 base
}

int Customer::GetResponse(int request_id) {
	std::lock_guard<std::mutex> lock(tracker_mu_);
	return tracker_[request_id].second;
}

void Customer::AddResponse(int request_id, int cnt) {
	std::lock_guard<std::mutex> lock(tracker_mu_);
	tracker_[request_id].second += cnt;
}

void Customer::ReceiveThread() {
	while (true) {
		Message msg = receive_queue_.WaitAndPop();
		if (msg.meta.control.cmd == Control::TERMINATE) [[unlikely]] {
			break;
		}
		receive_handle_(msg);

		if (!msg.meta.request) {
			// 该消息是一条回复
			// 回复的 request_id 肯定是之前发送的有效的下标
			std::lock_guard<std::mutex> lock(tracker_mu_);
			int r_id = msg.meta.timestamp;
			if (++tracker_[r_id].second == tracker_[r_id].first) {
				tracker_cond_.notify_all();
			}
		}
	}
}

} // namespace ps