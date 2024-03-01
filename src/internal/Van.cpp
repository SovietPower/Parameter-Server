#include "Van.h"
#include <random>

#include "base/Log.h"
#include "internal/Env.h"
#include "internal/ZMQVan.h"
#include "internal/Resender.h"
#include "internal/PostOffice.h"
#include "utility/NetworkUtils.h"

namespace {

std::mt19937 rd(std::time(nullptr));

} // namespace

namespace ps {

Van* Van::Create(Van::VanType van_type) {
	switch (van_type) {
		case ZMQ:
			return new ZMQVan();
		case P3:
		case IBVerbs:
			break;
	}
	LOG(FATAL) << "Unsupported van: " << [van_type]() {
		switch (van_type) {
			case ZMQ: return "ZMQ";
			case P3: return "P3";
			case IBVerbs: return "IBVerbs";
		}
		return "Unknown";
	}();
	return nullptr;
}

void Van::Start(int customer_id) {
	start_mu_.lock();
	// 初始化节点信息与配置；启动接收线程；与 scheduler 建立连接
	// 这些内容为多个 customer 所共享
	if (start_stage_ == 0) {
		// 读取 scheduler 信息
		scheduler_.id = kScheduler;
		scheduler_.role = Node::SCHEDULER;
		scheduler_.hostname = CHECK_NOTNULL(Environment::Get("PS_SCHEDULER_URI"));
		scheduler_.port = std::atoi(CHECK_NOTNULL(Environment::Get("PS_SCHEDULER_PORT")));

		// 读取当前节点的信息
		is_scheduler_ = PostOffice::Get()->is_scheduler();
		if (is_scheduler_) {
			my_node_ = scheduler_;
		} else {
			my_node_.id = Node::kEmpty; // 等待 scheduler 分配
			my_node_.role = PostOffice::Get()->is_server() ? Node::SERVER : Node::WORKER;
			my_node_.customer_id = customer_id;

			// ip
			const char* val = Environment::Get("PS_NODE_HOST");
			std::string ip;
			if (val) {
				ip = val;
			} else {
				const char* itf = Environment::Get("PS_INTERFACE");
				std::string interface;
				if (itf) {
					interface = std::string(itf);
				}
				if (interface.size()) {
					GetIP(interface, &ip);
				} else {
					GetAvailableInterfaceAndIP(&interface, &ip);
				}
				CHECK(!interface.empty()) << "Failed to get the interface";
			}
			CHECK(!ip.empty()) << "Failed to get IP";
			my_node_.hostname = ip;

			// port
			int port = Environment::GetInt("PS_PORT");
			if (!port) {
				port = GetAvailablePort();
			}
			CHECK(port) << "Failed to get port";
			my_node_.port = port;
		}
		heartbeat_timeout_ = Environment::GetInt("PS_HEARTBEAT_TIMEOUT");
		drop_rate_ = Environment::GetInt("PS_DROP_RATE");

		// 绑定到对应地址和端口
		Bind(my_node_, is_scheduler_ ? 0 : 30); // scheduler 必须位于指定端口上，其它节点无所谓
		CHECK_NE(my_node_.port, -1) << "Bind node failed";
		LOG(INFO) << "Node binds successfully: " << my_node_.DebugString();

		// 与 scheduler 建立连接
		Connect(scheduler_);

		// 启动接收线程
		receive_thread_ = std::unique_ptr<std::thread>(new std::thread(&Van::ReceiveThread, this));

		++start_stage_;
	}
	start_mu_.unlock();

	// 向 scheduler 发送 AddNode 请求
	// 每个 customer 都需执行（除了 scheduler 自己）
	if (!is_scheduler_) {
		CHECK_EQ(my_node_.customer_id, customer_id); // TODO: 后续可删
		Message msg;
		msg.meta.receiver = kScheduler;
		msg.meta.control.cmd = Control::ADD_NODE;
		msg.meta.control.nodes.push_back(my_node_);
		msg.meta.timestamp = GetAvailableTimestamp();
		Send(msg);
		// TODO: 这个消息没收到怎么办？
	}

	// 等待 scheduler 回复 AddNode 请求，即节点成功加入系统
	while (!ready_.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	start_mu_.lock();
	// 启动重发、心跳线程
	// 这些内容为多个 customer 所共享
	if (start_stage_ == 1) {
		// 启动心跳线程
		if (!is_scheduler_) {
			heartbeat_thread_ = std::unique_ptr<std::thread>(new std::thread(&Van::HeartbeatThread, this));
		}

		// 启动重发线程
		if (int timeout = Environment::GetInt("PS_RESEND_TIMEOUT"); timeout != 0) {
			resender_ = new Resender(timeout, 10, this);
		}

		++start_stage_;
	}
	start_mu_.unlock();
}

void Van::Stop() {
	// 给自己发送 TERMINATE 来结束接收线程
	Message term;
	term.meta.receiver = my_node_.id;
	// 多个 customer 共享接收线程，所以只需 customer 0 接收
	term.meta.customer_id = 0;
	term.meta.control.cmd = Control::TERMINATE;
	int sent = SendMsg(term);
	CHECK_NE(sent, -1);
	receive_thread_->join();

	// 结束心跳、重发线程
	if (!is_scheduler_) {
		heartbeat_thread_->join();
	}
	if (resender_) {
		delete resender_;
	}

	// 清空成员
	start_stage_ = 0;
	ready_ = false;
	timestamp_ = 0;
	send_bytes_ = 0;
	receive_bytes_ = 0;
	barrier_count_.fill(0);
	connected_nodes_.clear();
	shared_node_mapping_.clear();
	my_node_.id = Node::kEmpty;
}

int Van::Send(const Message& msg) {
	int sent = SendMsg(msg);
	CHECK_NE(sent, -1);
	send_bytes_ += sent;
	if (resender_) {
		resender_->OnSend(msg);
	}
	DLOG(DEBUG) << "Sent a msg (" << sent << "B): " << msg.DebugString(0, 1);
	return sent;
}

// ---
void Van::HandleTerminateCmd() {
	LOG(INFO) << my_node_.ShortDebugString() << " terminated";
	ready_ = false;
}

void Van::HandleBarrierCmd(const Message& msg) {

}

void Van::HandleHeartbeatCmd(const Message& msg) {

}

void Van::HandleDataMsg(const Message& msg) {

}

void Van::HandleAddNodeCmd(const Message& msg, const std::vector<Node>& nodes, const std::vector<Node>& recovery_nodes) {

}

void Van::UpdateNodeID(const Message& msg, const std::vector<Node>& nodes, const std::vector<Node>& recovery_nodes, const std::unordered_set<int>& dead_nodes) {

}

void Van::HandleAddNodeCmdAtScheduler(const Message& msg, const std::vector<Node>& nodes, const std::vector<Node>& recovery_nodes) {

}

void Van::HandleAddNodeCmdAtSAndW(const Message& msg, const std::vector<Node>& nodes, const std::vector<Node>& recovery_nodes) {

}

// ---
void Van::ReceiveThread() {
	std::vector<Node> nodes; // 所有已注册的节点
	std::vector<Node> recovered_nodes; // 所有从故障中恢复的节点
	while (true) {
		Message msg;
		int received = ReceiveMsg(&msg);
		CHECK_NE(received, -1);

		// 随机丢弃信息，用于调试（不能丢弃节点加入前的 AddNode msg）
		if (ready_.load() && drop_rate_ > 0) {
			if (rd() % 100 < drop_rate_) {
				LOG(WARNING) << "Dropped msg: " << msg.DebugString();
			}
		}

		receive_bytes_ += received;
		DLOG(DEBUG) << "Received a msg (" << received << "B): " << msg.DebugString(0, 1);

		// 消息已接收过、无需重复处理；或只是 ACK 消息
		if (resender_ && resender_->OnReceive(msg)) {
			continue;
		}

		if (msg.meta.control.IsEmpty()) {
			// 数据消息
			HandleDataMsg(msg);
		} else {
			// 控制消息
			auto cmd = msg.meta.control.cmd;
			if (cmd == Control::ADD_NODE) {
				HandleAddNodeCmd(msg, nodes, recovered_nodes);
			} else if (cmd == Control::HEARTBEAT) {
				HandleHeartbeatCmd(msg);
			} else if (cmd == Control::BARRIER) {
				HandleBarrierCmd(msg);
			} else if (cmd == Control::TERMINATE) {
				HandleTerminateCmd();
				break;
			} else {
				LOG(WARNING) << "Dropped msg due to invalid command: " << msg.DebugString();
			}
		}
	}
}

void Van::HeartbeatThread() {
	int hb_interval = Environment::GetInt("PS_HEARTBEAT_INTERVAL");
	if (hb_interval == 0) {
		return;
	}
	auto time = std::chrono::milliseconds(hb_interval);
	Message hb;
	hb.meta.receiver = kScheduler;
	hb.meta.timestamp = GetAvailableTimestamp();
	hb.meta.control.cmd = Control::HEARTBEAT;
	hb.meta.control.nodes.push_back(my_node_);
	while (ready_.load()) {
		Send(hb);
		std::this_thread::sleep_for(time);
	}
}

// ---
void Van::PackMetaToString(const Meta& meta, char** meta_buf, int* buf_size) {

}

void Van::UnpackMetaFromString(const char* meta_buf, int buf_size, Meta* meta) {

}



} // namespace ps