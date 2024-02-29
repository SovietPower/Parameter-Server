/**
 * @file Message.h
 */
#pragma once
#include "../base/Log.h"
#include "../ps/Base.h"
#include "../internal/Node.h"
#include "../utility/SVector.h"

namespace ps {

/**
 * @brief 系统控制消息的元信息，影响系统状态。
 */
struct Control {
	Control(): cmd(EMPTY) {}
	~Control() = default;

	enum Command {
		EMPTY, ADD_NODE, ACK, BARRIER, HEARTBEAT, TERMINATE,
	};

	bool IsEmpty() const {
		return cmd == EMPTY;
	}
	bool IsACK() const {
		return cmd == ACK;
	}

	/* 消息附带的操作 */
	Command cmd;
	/* 消息涉及的节点 */
	std::vector<Node> nodes;
	/* 如果是屏障操作，表示屏障所影响的组 */
	int barrier_group;

	std::string DebugString(size_t tab = 0) const {
		std::stringstream ss;
		if (IsEmpty()) {
			ss << "{ Control (EMPTY) }";
			return ss.str();
		}
		ss << "{ Control\n";
		std::string tabStr(tab + 1, '\t');
		#define NewLine(str) ss << tabStr << str
		NewLine("cmd: ") << [this]() {
			switch (cmd) {
				case EMPTY: return "EMPTY";
				case ADD_NODE: return "ADD_NODE";
				case ACK: return "ACK";
				case BARRIER: return "BARRIER";
				case HEARTBEAT: return "HEARTBEAT";
				case TERMINATE: return "TERMINATE";
			}
			return "IMPOSSIBLE";
		}();
		if (cmd == BARRIER) {
			ss << ", barrier_group: " << barrier_group;
		}
		ss << ",\n";

		NewLine("nodes: [ ");
		for (const Node& node: nodes) {
			ss << node.DebugString(tab + 1) << ", ";
		}
		ss << "],\n";
		#undef NewLine
		ss << std::string(tab, '\t') << "}";
		return ss.str();
	}
};

/**
 * @brief 一条消息的元信息。
 */
struct Meta {
	Meta() = default;
	~Meta() = default;

	/* 空值 */
	static constexpr int kEmpty = -1;

	// TODO: this is what?
	int head{kEmpty};
	/* 消息要发给和属于哪个应用 */
	int app_id{kEmpty};
	/* TODO: 消息发送还是接收？的节点的 customer_id */
	int customer_id{kEmpty};
	/* 发送者的节点 ID。为空则代表节点无 ID，需要 scheduler 分配 */
	int sender{kEmpty};
	/* 接收者的节点 ID */
	int receiver{kEmpty};

	/* 消息是一次请求还是回应。
	* 当消息类型为 Barrier 时，请求则代表节点进入阻塞，非请求代表 scheduler 通知节点退出阻塞。 */
	bool request{false};
	/* 消息是否需要提交数据 */
	bool push{false};
	/* 消息是否需要拉取数据 */
	bool pull{false};
	/* 消息是否是用于 SimpleApp */
	bool simple_app{false};
	/* 系统控制信息。如果为空，则为数据信息 */
	Control control;

	/* 消息的时间戳 */
	int timestamp{kEmpty};
	/* ACK 消息附带的签名。可唯一映射到某条消息，进行消息确认。
	* TODO: 是否需要像 ps-lite 放到 Control 而非 Meta 里？ */
	uint64_t msg_sign{0};
	/* 消息的优先级。默认 0 */
	int priority{0};
	/* 消息包含数据的总长度（指 Message::Data） */
	size_t data_size{0};
	/* 可选的消息体 */
	std::string body;
	/* msg.data 的数据类型。也许是为了恢复信息？ */
	// std::vector<DataType> data_type; // TODO: 应该不需要

	std::string DebugString(size_t tab = 0) const {
		std::stringstream ss;
		ss << "{ Meta\n";
		std::string tabStr(tab + 1, '\t');
		#define NewLine(str) ss << tabStr << str
		NewLine("S: ") << (sender == kEmpty ? -1: sender)
			<< ", R: " << receiver
			<< ", request: " << request
			<< ", timestamp: " << timestamp
			<< ", msg_sign: " << msg_sign
			<< ", head: " << (head == kEmpty ? -1 : kEmpty) << ",\n";
		if (control.IsEmpty()) {
			// 数据信息
			NewLine("contorl: EMPTY")
				<< ", app_id: " << app_id
				<< ", customer_id: " << customer_id
				<< ", push: " << push
				<< ", pull: " << pull
				<< ", simple_app: " << simple_app << ",\n";
		} else {
			// 系统控制信息
			NewLine("control: ") << control.DebugString(tab + 1) << ",\n";
		}
		if (!body.empty()) {
			NewLine("body: ") << body << ",\n";
		}
		#undef NewLine
		ss << std::string(tab, '\t') << "}";
		return ss.str();
	}
};

/**
 * @brief 在节点之间传输的消息，包含元信息和可选的数据。
 */
struct Message {
	Meta meta;

	/**
	 * @brief 消息可能附带的数据。包括三部分：K、V、每个 V 的长度（可选）。
	 */
	struct Data {
		SVector<Key> keys;
		SVector<Value> values;
		SVector<size_t> lens;
	};

	void SetData(const SVector<Key>& k, const SVector<Value>& v) {
		// 不暴露 data，因为访问时需要检查和更新 meta
		if (data == nullptr) {
			data = new Data();
		}
		data->keys = k;
		data->values = v;
		meta.data_size += k.size() * sizeof(Key);
		meta.data_size += v.size() * sizeof(Value);
		// meta.data_type.push_back... // TODO: 应该不需要
	}
	void SetData(const SVector<Key>& k, const SVector<Value>& v, const SVector<size_t>& l) {
		SetData(k, v);
		data->lens = l;
		meta.data_size += l.size() * sizeof(size_t);
	}
	SVector<Key>& GetKeys() {
		CHECK_NOTNULL(data);
		return data->keys;
	}
	SVector<Value>& GetValues() {
		CHECK_NOTNULL(data);
		return data->values;
	}
	SVector<size_t>& GetLens() {
		CHECK_NOTNULL(data);
		return data->lens;
	}

 private:
	Data* data{nullptr};

 public:
	/**
	 * @param verbose 为 0 则输出 data 的内容，否则仅输出 data 的长度。
	 */
	std::string DebugString(size_t tab = 0, int verbose = 0) const {
		std::stringstream ss;
		ss << "{ Message\n";
		std::string tabStr(tab + 1, '\t');
		#define NewLine(str) ss << tabStr << str
		NewLine("meta: ") << meta.DebugString(tab + 1) << ",\n";
		if (data != nullptr) {
			NewLine("data: [ ");
			if (verbose == 0) {
				ss << '\n';
				NewLine("keys: ") << data->keys.DebugString(tab + 1) << ",\n";
				NewLine("values: ") << data->values.DebugString(tab + 1) << ",\n";
				if (data->lens.has_value()) {
					NewLine("lens: ") << data->lens.DebugString(tab + 1) << ",\n";
				}
				NewLine("],\n");
			} else {
				ss << data->keys.size() << ", "
					<< data->values.size() << ", ";
				if (data->lens.has_value()) {
					ss << data->lens.size() << ", ";
				}
				ss << "],\n";
			}
		}
		#undef NewLine
		ss << std::string(tab, '\t') << "}";
		return ss.str();
	}
};

} // namespace ps
