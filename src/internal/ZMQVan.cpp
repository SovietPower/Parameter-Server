#include "ZMQVan.h"

#include <zmq.h>

#include "internal/Env.h"
#include "internal/PostOffice.h"

// 均为默认实现

namespace {

/**
 * @brief 根据情况释放接收的数据。
 * hint 为 nullptr 时，data 为用于 protobuf 打包、解包传输数据的数组，用完后直接 delete[] 释放即可。
 * 否则，hint 为 SArray<char>*。
 */
void FreeData(void *data, void *hint) {
	if (hint == NULL) {
		delete[] static_cast<char*>(data);
	} else {
		// 将 T 设为 char 调用析构应该是安全的
		delete static_cast<ps::SVector<char>*>(hint);
	}
}

/**
 * @brief return the node id given the received identity
 * @return -1 if not find
 */
int GetNodeID(const char* buf, size_t size) {
	if (size > 2 && buf[0] == 'p' && buf[1] == 's') {
		int id = 0;
		size_t i = 2;
		for (; i < size; ++i) {
			if (buf[i] >= '0' && buf[i] <= '9') {
				id = id * 10 + buf[i] - '0';
			} else {
				break;
			}
		}
		if (i == size) return id;
	}
	return ps::Meta::kEmpty;
}

} // namespace

namespace ps {

void ZMQVan::Start(int customer_id) {
	// start zmq
	start_mu_.lock();
	if (context_ == nullptr) {
		context_ = zmq_ctx_new();
		CHECK(context_ != NULL) << "Create 0mq context failed";
		// 一个 context 上允许的 socket 数默认只有 1023
		zmq_ctx_set(context_, ZMQ_MAX_SOCKETS, 65536);
	}
	start_mu_.unlock();
	// zmq_ctx_set(context_, ZMQ_IO_THREADS, 4);
	Van::Start(customer_id);
}

void ZMQVan::Stop() {
	PS_LOG_INFO << my_node_.ShortDebugString() << " is stopping";
	Van::Stop();
	// close sockets
	int linger = 0;
	int rc = zmq_setsockopt(receiver_, ZMQ_LINGER, &linger, sizeof(linger));
	CHECK(rc == 0 || errno == ETERM);
	CHECK_EQ(zmq_close(receiver_), 0);
	for (auto& it : senders_) {
		int rc = zmq_setsockopt(it.second, ZMQ_LINGER, &linger, sizeof(linger));
		CHECK(rc == 0 || errno == ETERM);
		CHECK_EQ(zmq_close(it.second), 0);
	}
	senders_.clear();
	zmq_ctx_destroy(context_); // 已经改为 zmq_ctx_term
	context_ = nullptr;
}

void ZMQVan::Connect(const Node& node) {
	CHECK_NE(node.id, node.kEmpty);
	CHECK_NE(node.port, node.kEmpty);
	CHECK(node.hostname.size());
	int id = node.id;
	// sender：node id 映射到连接到它的 socket
	auto it = senders_.find(id);
	if (it != senders_.end()) {
		zmq_close(it->second);
	}
	// worker doesn't need to connect to the other workers. same for server
	if ((node.role == my_node_.role) && (node.id != my_node_.id)) {
		return;
	}
	void *sender = zmq_socket(context_, ZMQ_DEALER);
	CHECK(sender != NULL)
		<< zmq_strerror(errno)
		<< ". it often can be solved by \"sudo ulimit -n 65536\""
		<< " or edit /etc/security/limits.conf";
	if (my_node_.id != Node::kEmpty) {
		std::string my_id = "ps" + std::to_string(my_node_.id);
		zmq_setsockopt(sender, ZMQ_IDENTITY, my_id.data(), my_id.size());
		const char* watermark = Environment::Get("PS_WATER_MARK");
		if (watermark) {
			const int hwm = atoi(watermark);
			zmq_setsockopt(sender, ZMQ_SNDHWM, &hwm, sizeof(hwm));
		}
	}
	// connect
	std::string addr = "tcp://" + node.hostname + ":" + std::to_string(node.port);
	if (Environment::Get("PS_LOCAL") != nullptr) {
		addr = "ipc:///tmp/" + std::to_string(node.port);
	}
	if (zmq_connect(sender, addr.c_str()) != 0) {
		LOG(FATAL) <<	"Connect to " + addr + " failed: " + zmq_strerror(errno);
	}
	senders_[id] = sender;
}

int ZMQVan::Bind(const Node& node, int max_retry) {
	receiver_ = zmq_socket(context_, ZMQ_ROUTER);
	CHECK(receiver_ != NULL)
			<< "create receiver socket failed: " << zmq_strerror(errno);
	int local = Environment::Get("PS_LOCAL") != nullptr; // runs in local machines, no network is needed
	std::string hostname = node.hostname.empty() ? "*" : node.hostname;
	int use_kubernetes = Environment::Get("PS_USE_KUBERNETES") != nullptr; // TODO: 不需要？
	if (use_kubernetes && node.role == Node::SCHEDULER) {
		hostname = "0.0.0.0";
	}
	// 如果是本地发送，则绑定到路径 /tmp/
	std::string addr = local ? "ipc:///tmp/" : "tcp://" + hostname + ":";
	int port = node.port;
	std::srand(std::time(nullptr) + port);
	for (int i = 0; i < max_retry + 1; ++i) {
		auto address = addr + std::to_string(port);
		if (zmq_bind(receiver_, address.c_str()) == 0) break;
		if (i == max_retry) {
			port = -1;
		} else {
			port = 10000 + std::rand() % 40000;
		}
	}
	return port;
}

int ZMQVan::SendMsg(const Message& msg) {
	// 发送一条消息。先发送 Meta，再发送各个 Data。
	std::lock_guard<std::mutex> lk(mu_);
	// find the socket
	int id = msg.meta.receiver;
	CHECK_NE(id, Meta::kEmpty);
	auto it = senders_.find(id);
	if (it == senders_.end()) {
		LOG(WARNING) << "There is no socket to node " << id;
		return -1;
	}
	void *socket = it->second;

	// send meta
	int meta_size; char* meta_buf;
	PackMetaToString(msg.meta, &meta_buf, &meta_size);
	int tag = ZMQ_SNDMORE;
	int n = msg.data.size();
	if (n == 0) tag = 0;
	zmq_msg_t meta_msg;
	zmq_msg_init_data(&meta_msg, meta_buf, meta_size, FreeData, NULL);
	while (true) {
		if (zmq_msg_send(&meta_msg, socket, tag) == meta_size) break;
		if (errno == EINTR) continue;
		// TODO: 发送失败后，需要 zmq_msg_close 避免内存泄露？
		return -1;
	}
	// zmq_msg_close(&meta_msg);
	int send_bytes = meta_size;
	// send data
	for (int i = 0; i < n; ++i) {
		zmq_msg_t data_msg;
		SVector<char>* data = new SVector<char>(msg.data[i]);
		int data_size = data->size();
		zmq_msg_init_data(&data_msg, data->data(), data->size(), FreeData, data);
		if (i == n - 1) tag = 0;
		while (true) {
			if (zmq_msg_send(&data_msg, socket, tag) == data_size) break;
			if (errno == EINTR) continue;
			LOG(WARNING) << "Failed to send message to node [" << id
							<< "] errno: " << errno << " " << zmq_strerror(errno)
							<< ". " << i << "/" << n;
			// TODO: 发送失败后，需要 zmq_msg_close 避免内存泄露
			return -1;
		}
		// zmq_msg_close(&data_msg);
		send_bytes += data_size;
	}
	return send_bytes;
}

int ZMQVan::ReceiveMsg(Message* msg) {
	msg->data.clear();
	size_t recv_bytes = 0;
	for (int i = 0; ; ++i) {
		zmq_msg_t* zmsg = new zmq_msg_t;
		CHECK(zmq_msg_init(zmsg) == 0) << zmq_strerror(errno);
		while (true) {
			if (zmq_msg_recv(zmsg, receiver_, 0) != -1) break;
			if (errno == EINTR) {
				std::cout << "interrupted";
				continue;
			}
			LOG(WARNING) << "failed to receive message. errno: "
							<< errno << " " << zmq_strerror(errno);
			// 释放 zmsg
			return -1;
		}
		char* buf = CHECK_NOTNULL((char *)zmq_msg_data(zmsg));
		size_t size = zmq_msg_size(zmsg);
		recv_bytes += size;

		if (i == 0) {
			// identify
			msg->meta.sender = GetNodeID(buf, size);
			msg->meta.receiver = my_node_.id;
			// ? 为什么第1部分不是 Meta 而是 sender id？加点 log 测试看看输出内容
			CHECK(zmq_msg_more(zmsg));
			zmq_msg_close(zmsg);
			delete zmsg;
		} else if (i == 1) {
			// task
			UnpackMetaFromString(buf, size, &(msg->meta));
			zmq_msg_close(zmsg);
			bool more = zmq_msg_more(zmsg);
			delete zmsg;
			if (!more) break;
		} else {
			// zero-copy
			SVector<char> data;
			data.reset(buf, size, [zmsg](char* buf) {
				zmq_msg_close(zmsg);
				delete zmsg;
			});
			msg->data.push_back(data);
			if (!zmq_msg_more(zmsg)) {
				break;
			}
		}
	}
	return recv_bytes;
}

} // namespace ps