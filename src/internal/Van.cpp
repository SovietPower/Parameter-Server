#include "Van.h"
#include <random>

#include "base/Log.h"
#include "ps/Base.h"
#include "internal/Env.h"
#include "internal/ZMQVan.h"
#include "internal/Customer.h"
#include "internal/Resender.h"
#include "internal/PostOffice.h"
#include "utility/NetworkUtils.h"

#include "./meta.pb.h"

namespace {

std::mt19937 rd(std::time(nullptr));

} // namespace

namespace ps {

Van* Van::Create(std::string van_type) {
	if (van_type == "zmq") {
		return new ZMQVan();
	} else if (van_type == "p3") {

	} else if (van_type == "ibverbs") {

	}
	LOG(FATAL) << "Unsupported van: " << van_type;
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
	if (msg.meta.request) {
		// Server/Worker 发送的 Barrier 请求
		DCHECK(is_scheduler_);

		int group = msg.meta.control.barrier_group;
		++barrier_count_[group];
		LOG(DEBUG) << "Increase barrier_count[" << group << "] to " << barrier_count_[group];

		if (static_cast<size_t>(barrier_count_[group]) == PostOffice::Get()->GetNodeIDs(group).size()) {
			barrier_count_[group] = 0;
			LOG(DEBUG) << "Release group [" << group << "] from barrier";

			Message rel; // release
			rel.meta.request = false;
			rel.meta.control.cmd = Control::BARRIER;
			rel.meta.app_id = msg.meta.app_id;
			rel.meta.customer_id = msg.meta.customer_id;
			for (int id: PostOffice::Get()->GetNodeIDs(group)) {
				if (shared_node_mapping_.find(id) == shared_node_mapping_.end()) {
					rel.meta.receiver = id;
					rel.meta.timestamp = GetAvailableTimestamp();
					Send(rel);
				}
			}
		}
	} else {
		// Scheduler 发出的结束 Barrier 指令
		DCHECK(!is_scheduler_);
		PostOffice::Get()->ExitBarrier(msg);
	}
}

void Van::HandleHeartbeatCmd(const Message& msg) {
	// 更新心跳时间
	std::time_t now = std::time(nullptr);
	for (const auto& node: msg.meta.control.nodes) {
		PostOffice::Get()->UpdateHeartbeat(node.id, now);
		LOG(DEBUG) << "Update heartbeat of node " << node.ShortDebugString() << " to time " << now;
	}

	// 发送心跳回复
	if (is_scheduler_) {
		static Message hb_back;
		if (hb_back.meta.control.IsEmpty()) [[unlikely]] {
			// init
			hb_back.meta.control.cmd = Control::HEARTBEAT;
			hb_back.meta.control.nodes.push_back(my_node_);
		}
		for (const auto& node: msg.meta.control.nodes) {
			hb_back.meta.receiver = node.id;
			hb_back.meta.timestamp = GetAvailableTimestamp();
			Send(hb_back);
		}
	}
}

void Van::HandleDataMsg(const Message& msg) {
	CHECK_NE(msg.meta.app_id, Meta::kEmpty);
	CHECK_NE(msg.meta.sender, Meta::kEmpty);
	CHECK_NE(msg.meta.receiver, Meta::kEmpty);

	int app_id = msg.meta.app_id;
	int customer_id = PostOffice::Get()->is_worker() ? msg.meta.customer_id : app_id; // 只有 worker 有多个 customer
	Customer* customer = PostOffice::Get()->GetCustomer(app_id, customer_id, 5);
	CHECK(customer) << "Cannot find customer with app_id: " << app_id << ", customer_id: " << customer_id
		<< " after waiting for 5s";
	customer->OnReceive(msg);
}

void Van::HandleAddNodeCmd(Message& msg, std::vector<Node>& nodes, std::vector<Node>& recovered_nodes) {
	UpdateNodeID(msg, nodes, recovered_nodes);
	if (is_scheduler_) {
		HandleAddNodeCmdAtScheduler(nodes, recovered_nodes);
	} else {
		HandleAddNodeCmdAtSAndW(msg);
	}
}

void Van::UpdateNodeID(Message& msg, std::vector<Node>& nodes, std::vector<Node>& recovered_nodes) {
	auto& msg_nodes = msg.meta.control.nodes;
	if (msg.meta.sender == Meta::kEmpty) {
		// 某个尚未分配 ID 的节点 msg_nodes[0] 请求加入与获得 ID
		// 为新加入的节点分配节点 ID，并将其加入到 nodes 或 recovered_nodes 中（取决于是否它是故障恢复的节点）
		CHECK(is_scheduler_);
		CHECK_EQ(msg_nodes.size(), 1);

		auto& new_node = msg_nodes[0];
		size_t num_nodes = PostOffice::Get()->num_servers() + PostOffice::Get()->num_workers();
		if (nodes.size() < num_nodes) {
			// 目前还不是所有节点都已注册，直接放入 nodes
			nodes.push_back(new_node);

			LOG(INFO) << "UpdateNodeID: New node added (now: " << nodes.size() << " nodes)";
		} else {
			// 所有节点都已注册，从 nodes 中找一个 dead node 将其替换，相当于重启一个故障节点
			CHECK(ready_);

			const auto& dead = PostOffice::Get()->GetDeadNodes(heartbeat_timeout_);
			std::unordered_set<int> dead_nodes(dead.begin(), dead.end());

			// 当前 nodes 已包含系统的所有节点，包括 scheduler
			// 注意 scheduler 会在 nodes 最后面添加，所以要跳过最后一个节点
			for (size_t i = 0; i + 1 < nodes.size(); ++i) {
				auto& node = nodes[i];
				if (dead_nodes.find(node.id) != dead_nodes.end() && node.role == new_node.role) {
					// 复用 dead node 的 ID
					new_node.id = node.id;
					new_node.is_recovered = true;
					recovered_nodes.push_back(new_node);

					LOG(INFO) << "UpdateNodeID: Replace dead node " << node.DebugString()
						<< " with new node " << new_node.DebugString();
					node = new_node;
				}
			}
		}
	}

	// 更新自己的节点 ID 为从 scheduler 那里获得的 ID
	for (const auto& node: msg_nodes) {
		if (my_node_.hostname == node.hostname && my_node_.port == node.port) {
			if (my_node_.id == Meta::kEmpty) {
				my_node_ = node;
				LOG(INFO) << "UpdateNodeID: Got node ID: " << node.ShortDebugString();
			}
			// TODO: 需要 DMLC_RANK 吗？
		}
	}
}

void Van::HandleAddNodeCmdAtScheduler(std::vector<Node>& nodes, std::vector<Node>& recovered_nodes) {
	std::time_t now = std::time(nullptr);
	size_t num_nodes = PostOffice::Get()->num_servers() + PostOffice::Get()->num_workers();
	if (nodes.size() == num_nodes) {
		// 所有节点均已注册
		// 只会在系统第一次完全启动时执行。在这之后 nodes.size = servers + workers + 1 (scheduler)
		// TODO: 可删除
		static bool flag {false};
		DCHECK(!flag);
		if (!flag) flag = true;

		// 将所有节点按地址排序，分配节点 ID
		// ip 降序，port 升序？
		std::sort(nodes.begin(), nodes.end(), [](const Node& a, const Node& b) {
			int ret = a.hostname.compare(b.hostname);
			return ret != 0 ? ret > 0 : a.port < b.port;
		});
		DCHECK_EQ(num_servers_, 0);
		for (auto& node: nodes) {
			// 分配节点 ID
			CHECK_EQ(node.id, Node::kEmpty);
			int new_id;
			if (node.role == Node::SERVER) {
				new_id = PostOffice::ServerRankToID(num_servers_++);
			} else {
				new_id = PostOffice::WorkerRankToID(num_workers_++);
			}

			// 建立连接（如果需要）
			std::string addr = node.hostname + ":" + std::to_string(node.port);
			if (connected_nodes_.find(addr) == connected_nodes_.end()) {
				// 与未连接的节点建立连接
				node.id = new_id;
				Connect(node);
				connected_nodes_[addr] = node.id;
				// 更新一次心跳
				PostOffice::Get()->UpdateHeartbeat(node.id, now);

				LOG(INFO) << "HandleAddNodeCmdAtScheduler: " << "Scheduler connects to a new node: " << node.DebugString();
			} else {
				// 该节点之前已连接，即该节点与之前某个节点互为 customer
				node.id = connected_nodes_[addr];
				shared_node_mapping_[new_id] = node.id;

				LOG(INFO) << "HandleAddNodeCmdAtScheduler: Scheduler knows a already connected node: " << node.DebugString();
			}
		}

		// 在 nodes 的最后放入 scheduler 节点自己
		nodes.push_back(my_node_);

		// 通知其它节点整个系统的所有节点信息（即 nodes）
		Message notify;
		notify.meta.control.cmd = Control::ADD_NODE;
		notify.meta.control.nodes = nodes;
		// TODO: 为什么这里不需要更新消息的 sender？
		// notify.meta.sender = kScheduler;

		for (int id: PostOffice::Get()->GetNodeIDs(kWorkerGroup | kServerGroup)) {
			// 只给每个进程发一次即可。同一进程的其它 customer 共享 PostOffice 信息，不必再发
			if (shared_node_mapping_.find(id) == shared_node_mapping_.end()) {
				notify.meta.receiver = id;
				notify.meta.timestamp = GetAvailableTimestamp();
				Send(notify);
			}
		}

		ready_ = true;
		LOG(INFO) << "HandleAddNodeCmdAtScheduler: " << "Scheduler connects to " << num_servers_ << " servers and " << num_workers_ << " num_workers";
	} else if (!recovered_nodes.empty()) {
		// 在系统完全启动后，有一个节点故障并重新加入，需要与节点重新建立连接
		// 这个节点由 AddNode 触发，并在 UpdateNodeID 中加入到了 recovered_nodes 中
		CHECK_EQ(recovered_nodes.size(), 1);
		auto& new_node = recovered_nodes[0];
		Connect(new_node);
		// 更新一次心跳
		PostOffice::Get()->UpdateHeartbeat(new_node.id, now);

		// 将恢复的节点信息 (new_node) 告知其它节点
		// 将系统的所有节点信息 (nodes) 告知 new_node
		Message notify;
		notify.meta.control.cmd = Control::ADD_NODE;
		// TODO: 为什么这里不需要更新消息的 sender？
		// notify.meta.sender = kScheduler;

		const auto& dead = PostOffice::Get()->GetDeadNodes(heartbeat_timeout_);
		std::unordered_set<int> dead_nodes(dead.begin(), dead.end());

		for (int id: PostOffice::Get()->GetNodeIDs(kWorkerGroup | kServerGroup)) {
			// 不需要给故障的节点发送，否则会大量重发
			if (id == new_node.id || dead_nodes.find(id) == dead_nodes.end()) {
				notify.meta.control.nodes = id == new_node.id ? nodes : recovered_nodes;
				notify.meta.receiver = id;
				notify.meta.timestamp = GetAvailableTimestamp();
				Send(notify);
			}
		}
	}
}

void Van::HandleAddNodeCmdAtSAndW(const Message& msg) {
	for (const auto& node: msg.meta.control.nodes) {
		std::string addr = node.hostname + ":" + std::to_string(node.port);
		if (connected_nodes_.find(addr) == connected_nodes_.end()) {
			Connect(node);
			connected_nodes_[addr] = node.id;
		}

		if (!node.is_recovered) {
			if (node.role == Node::WORKER) {
				++num_workers_;
			} else if (node.role == Node::SERVER) {
				++num_servers_;
			}
		}
	}
	LOG(INFO) << "HandleAddNodeCmdAtSAndW: " << "node " << my_node_.ShortDebugString()
		<< " connects to " << msg.meta.control.nodes.size() << " nodes";

	// server/worker 第一次接收到 ADD_NODE 时代表系统启动
	// if (!ready_)
	ready_ = true;
}

// ---
void Van::ReceiveThread() {
	std::vector<Node> nodes; // 所有已注册的节点
	std::vector<Node> recovered_nodes; // 所有后续加入的从故障中恢复的节点
	while (true) {
		Message msg;
		int received = ReceiveMsg(&msg);
		CHECK_NE(received, -1);

		// 随机丢弃信息，用于调试（不能丢弃节点加入前的 AddNode msg）
		if (ready_.load() && drop_rate_ > 0) {
			if (rd() % 100 < static_cast<unsigned int>(drop_rate_)) {
				LOG(WARNING) << "Dropped msg: " << msg.DebugString();
			}
		}

		receive_bytes_ += received;
		DLOG(DEBUG) << "Received a msg (" << received << "B): " << msg.DebugString(0, 1);

		// 发送 ACK，并检查：如果消息已接收过、无需重复处理，或只是 ACK 消息，则跳过处理
		if (resender_ && resender_->OnReceive(msg)) {
			continue;
		}

		if (msg.meta.control.IsEmpty()) {
			// 数据消息（请求或响应）
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
	// 默认实现
	// convert into protobuf
	PBMeta pb;
	pb.set_head(meta.head);
	if (meta.app_id != Meta::kEmpty) pb.set_app_id(meta.app_id);
	if (meta.timestamp != Meta::kEmpty) pb.set_timestamp(meta.timestamp);
	if (meta.body.size()) pb.set_body(meta.body);
	pb.set_push(meta.push);
	pb.set_pull(meta.pull);
	pb.set_request(meta.request);
	pb.set_simple_app(meta.simple_app);
	pb.set_priority(meta.priority);
	pb.set_customer_id(meta.customer_id);
	for (auto d : meta.data_type) pb.add_data_type(d);
	if (!meta.control.IsEmpty()) {
		auto ctrl = pb.mutable_control();
		ctrl->set_cmd(meta.control.cmd);
		if (meta.control.cmd == Control::BARRIER) {
			ctrl->set_barrier_group(meta.control.barrier_group);
		} else if (meta.control.cmd == Control::ACK) {
			pb.set_msg_sign(meta.msg_sign);
			// ctrl->set_msg_sig(meta.control.msg_sig);
		}
		for (const auto& n : meta.control.nodes) {
			auto p = ctrl->add_node();
			p->set_id(n.id);
			p->set_role(n.role);
			p->set_port(n.port);
			p->set_hostname(n.hostname);
			p->set_is_recovered(n.is_recovered);
			p->set_customer_id(n.customer_id);
		}
	}

	// to string
	*buf_size = pb.ByteSizeLong(); // ByteSize()
	*meta_buf = new char[*buf_size + 1];
	CHECK(pb.SerializeToArray(*meta_buf, *buf_size))
			<< "failed to serialize protobuf";
}

void Van::UnpackMetaFromString(const char* meta_buf, int buf_size, Meta* meta) {
	// 默认实现
	// to protobuf
	PBMeta pb;
	CHECK(pb.ParseFromArray(meta_buf, buf_size))
			<< "failed to parse string into protobuf";

	// to meta
	meta->head = pb.head();
	meta->app_id = pb.has_app_id() ? pb.app_id() : Meta::kEmpty;
	meta->timestamp = pb.has_timestamp() ? pb.timestamp() : Meta::kEmpty;
	meta->request = pb.request();
	meta->push = pb.push();
	meta->pull = pb.pull();
	meta->simple_app = pb.simple_app();
	meta->priority = pb.priority();
	meta->body = pb.body();
	meta->customer_id = pb.customer_id();
	meta->data_type.resize(pb.data_type_size());
	for (int i = 0; i < pb.data_type_size(); ++i) {
		meta->data_type[i] = static_cast<DataType>(pb.data_type(i));
	}
	if (pb.has_control()) {
		const auto& ctrl = pb.control();
		meta->control.cmd = static_cast<Control::Command>(ctrl.cmd());
		meta->control.barrier_group = ctrl.barrier_group();
		meta->msg_sign = pb.msg_sign();
		// meta->control.msg_sig = ctrl.msg_sig();
		for (int i = 0; i < ctrl.node_size(); ++i) {
			const auto& p = ctrl.node(i);
			Node n;
			n.role = static_cast<Node::Role>(p.role());
			n.port = p.port();
			n.hostname = p.hostname();
			n.id = p.has_id() ? p.id() : Node::kEmpty;
			n.is_recovered = p.is_recovered();
			n.customer_id = p.customer_id();
			meta->control.nodes.push_back(n);
		}
	} else {
		meta->control.cmd = Control::EMPTY;
	}
}

} // namespace ps