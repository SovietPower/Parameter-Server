/**
 * @file Van.h
 */
#pragma once
#include <mutex>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "../internal/Message.h"

namespace ps {

class PBMeta;
class Resender;

/**
 * @brief 具体执行信息收发的对象。
 * 使用前后，必须调用 Start 和 Stop 来启动或结束 Van（由 PostOffice 保证）。
 * 除非特殊声明，否则所有接口都是线程安全的。
 */
class Van {
 protected:
	/**
	 * @brief 使用工厂函数 Create 创建 Van。
	 */
	Van() = default;

 public:
	virtual ~Van() = default;

	/**
	 * @brief 创建指定类型的 Van。
	 */
	static Van* Create(std::string van_type);
	/**
	 * @brief 启动 Van：
	 * 初始化节点信息；与 scheduler 建立链接；让 scheduler 添加自己；分别启动接收消息、发送心跳（如果不是 scheduler）、超时重发（如果设置）的线程。
	 * 通过 PostOffice::Start 调用。
	 */
	virtual void Start(int customer_id);
	/**
	 * @brief 结束 Van：
	 * 停止接收、心跳、重发线程，清空成员。
	 */
	virtual void Stop();

	/**
	 * @brief 发送一条消息的外部接口。可能被多个线程同时执行。
	 * 实际调用 SendMsg、更新 send_bytes、触发 resender.OnSend。
	 * @return 返回发送的字节数。失败则返回-1。
	 */
	int Send(const Message& msg);
	/**
	 * @brief 获取下个可用的时间戳。
	 */
	int GetAvailableTimestamp() {
		return timestamp_++;
	}
	/**
	 * @brief 检查 Van 是否已启动完成、可以进行发送消息。
	 */
	bool IsReady() {
		return ready_;
	}
	/**
	 * @brief
	 */
	const Node& my_node() const {
		return my_node_;
	}

 protected:
	/**
	 * @brief 将当前节点绑定到某个端口。
	 * 为了保证成功，除了尝试绑定到 node.port 外，还会随机选择多个端口尝试绑定。
	 * @param max_retry 最大尝试次数。
	 * @return 返回实际绑定到的端口号。失败则返回-1。
	 */
	virtual int Bind(const Node& node, int max_retry) = 0;
	/**
	 * @brief 与某个节点建立连接。
	 */
	virtual void Connect(const Node& node) = 0;
	/**
	 * @brief 发送一条消息的内部实现。
	 * @return 返回发送的字节数。失败则返回-1。
	 */
	virtual int SendMsg(const Message& msg) = 0;
	/**
	 * @brief 接收一条消息的内部实现。会阻塞直到收到消息。
	 * @param msg 空初始化的 Message。
	 * @return 返回收到的字节数。失败则返回-1。
	 */
	virtual int ReceiveMsg(Message* msg) = 0;

	/**
	 * @brief 将消息元信息打包到 C string。
	 * @param meta_buf
	 * @param buf_size
	 */
	void PackMetaToString(const Meta& meta, char** meta_buf, int* buf_size);
	/**
	 * @brief 从 C string 解包获取消息元信息。
	 */
	void UnpackMetaFromString(const char* meta_buf, int buf_size, Meta* meta);
	/**
	 * @brief 将消息元信息打包到 Protobuf。
	 */
	// void PackMetaToPB(const Meta& meta, PBMeta* pb);

	Node my_node_;
	Node scheduler_;
	bool is_scheduler_;

	/* 系统当前启动阶段 */
	int start_stage_{0};
	std::mutex start_mu_;

 private:
	/**
	 * @brief 处理 Terminate 命令的逻辑。
	 */
	void HandleTerminateCmd();
	/**
	 * @brief 处理 Barrier 命令的逻辑。
	 * Scheduler 端：接收 Barrier 请求，更新对应组的 Barrier 计数；在计数达到指定值时通知该组节点结束 Barrier。
	 * Server/Worker 端：接收结束 Barrier 指令，通过 PostOffice 退出 Barrier。
	 */
	void HandleBarrierCmd(const Message& msg);
	/**
	 * @brief 处理 Heartbeat 命令的逻辑。
	 * 通过 PostOffice 更新心跳时间。如果是 scheduler 还需回复心跳，供其它节点更新时间。
	 */
	void HandleHeartbeatCmd(const Message& msg);
	/**
	 * @brief 处理数据信息 (EmptyCmd) 的逻辑。
	 */
	void HandleDataMsg(const Message& msg);
	/**
	 * @brief 处理 AddNode 消息的逻辑。
	 * 调用 UpdateNodeID，然后根据身份调用HandleAddNodeCmdAtScheduler 或 HandleAddNodeCmdAtSAndW
	 */
	void HandleAddNodeCmd(Message& msg, std::vector<Node>& nodes, std::vector<Node>& recovered_nodes);
	/**
	 * @brief 更新节点 ID。在执行 AddNode 时调用。
	 * Scheduler 端：为新加入的节点分配节点 ID；
	 * Server/Worker 端：更新自己的节点 ID 为从 scheduler 那里获得的 ID。
	 */
	void UpdateNodeID(Message& msg, std::vector<Node>& nodes, std::vector<Node>& recovered_nodes);
	/**
	 * @brief 处理 scheduler 端 AddNode 消息的具体逻辑。
	 */
	void HandleAddNodeCmdAtScheduler(std::vector<Node>& nodes, std::vector<Node>& recovered_nodes);
	/**
	 * @brief 处理 server, worker 端 AddNode 消息的具体逻辑：
	 * 如果节点之前没有和请求节点建立连接，则建立。
	 * 节点不是和所有节点都要建立连接，比如 worker 实际只会和 server 建立连接。
	 */
	void HandleAddNodeCmdAtSAndW(const Message& msg);

	/**
	 * @brief 接收线程的执行逻辑。接收消息是单线程的。
	 */
	void ReceiveThread();
	/**
	 * @brief 发送心跳线程的执行逻辑。
	 */
	void HeartbeatThread();

	/* Van 是否已成功加入系统、可以进行发送消息 */
	std::atomic<bool> ready_{false};
	/* 第一个可用的时间戳。暂时先用 int。 */
	std::atomic<int> timestamp_{0};
	/* 收到消息时，丢弃消息的概率。用于测试 */
	int drop_rate_{0};

	/* 与当前节点建立了连接的 server/worker 数量 */
	int num_servers_{0};
	int num_workers_{0};

	/* Resender 需要在 Stop 时手动释放，以等待 resend 线程正常发送完再退出 */
	Resender* resender_{nullptr};
	std::unique_ptr<std::thread> receive_thread_;
	std::unique_ptr<std::thread> heartbeat_thread_;
	/* 心跳超时时间，从配置中读取。单位为秒。为0则不检查 */
	int heartbeat_timeout_;

	/* 历史总共发送的字节数 */
	std::atomic<size_t> send_bytes_{0};
	/* 历史总共接收的字节数。由接收线程单线程处理，无需原子 */
	size_t receive_bytes_{0};

	/* 每个组的 barrier 计数。即当前有多少属于该组的节点进入了 barrier 阻塞 */
	std::array<int, 8> barrier_count_ {0, 0, 0, 0, 0, 0, 0, 0};

	/* 节点地址 -> node_id.
	* 通过节点地址（比如 IP:port）获取对应的节点 ID。包括所有已建立连接 (Connected) 的节点。
	* 只会在第一次建立连接时被更新。 */
	std::unordered_map<std::string, int> connected_nodes_;

	/* node_id -> 最早加入的与当前节点同地址的 node_id.
	* 将后续加入的节点 ID 映射到最初加入的、有相同地址的节点 ID。
	* 如果某节点 ID 在该映射中，则代表存在另一个节点与其有相同地址，即它们是同一进程上的不同 customer。
	* （注意节点指逻辑节点 customer，一个物理节点即同一个地址上可以有多个逻辑节点）。
	* 可用来减少某些消息的发送次数（相同节点的不同 customer 只需要发一次）。 */
	std::unordered_map<int, int> shared_node_mapping_;

	DISABLE_COPY_AND_ASSIGN(Van);
};


} // namespace ps