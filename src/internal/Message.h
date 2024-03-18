/**
 * @file Message.h
 */
#pragma once
#include "../base/Log.h"
#include "../internal/Node.h"
#include "../utility/SVector.h"

namespace ps {

/**
 * @brief Message 可保存的 data 类型。
 */
enum DataType: int32_t {
	CHAR, UCHAR,
	INT8, INT16, INT32, INT64,
	UINT8, UINT16, UINT32, UINT64,
	FLOAT, DOUBLE, OTHER
};
/**
 * @brief for debug output
 */
static const char* DataTypeName[] = {
	"CHAR", "UCHAR",
	"INT8", "INT16", "INT32", "INT64",
	"UINT8", "UINT16", "UINT32", "UINT64",
	"FLOAT", "DOUBLE", "OTHER"
};

template <class T>
constexpr DataType GetDataType() {
	using std::is_same_v;
	if constexpr (is_same_v<char, T>) {
		return CHAR;
	} else if constexpr (is_same_v<unsigned char, T>) {
		return UCHAR;
	} else if constexpr (is_same_v<int8_t, T>) {
		return INT8;
	} else if constexpr (is_same_v<int16_t, T>) {
		return INT16;
	} else if constexpr (is_same_v<int32_t, T>) {
		return INT32;
	} else if constexpr (is_same_v<int64_t, T>) {
		return INT64;
	} else if constexpr (is_same_v<uint8_t, T>) {
		return UINT8;
	} else if constexpr (is_same_v<uint16_t, T>) {
		return UINT16;
	} else if constexpr (is_same_v<uint32_t, T>) {
		return UINT32;
	} else if constexpr (is_same_v<uint64_t, T>) {
		return UINT64;
	} else if constexpr (is_same_v<float, T>) {
		return FLOAT;
	} else if constexpr (is_same_v<double, T>) {
		return DOUBLE;
	} else {
		return OTHER;
	}
}

/**
 * @brief 系统控制消息的元信息，影响系统状态。
 * 当 Command 为 EMPTY 时，代表消息为 server/worker 端的数据请求或响应。
 */
struct Control {
	Control(): cmd(EMPTY) {}
	~Control() = default;

	enum Command: int32_t {
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
	/* 操作涉及的节点 */
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
	/* msg.data 各成员的数据类型。
	* 可用于恢复类型，但只要保证发送接收的类型一致（本来也要保证），就不需要它 */
	std::vector<DataType> data_type;

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
		if (!data_type.empty()) {
			NewLine("data_type: { ");
			for (const auto& t: data_type) {
				ss << DataTypeName[t] << ", ";
			}
			ss << "},\n";
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
	std::vector<SVector<char>> data;

	template <typename T>
	void AddData(const SVector<T>& value) {
		CHECK_EQ(data.size(), meta.data_type.size());
		meta.data_type.push_back(GetDataType<T>());

		SVector<char> new_data(value);
		meta.data_size += new_data.size();
		data.push_back(std::move(new_data));
	}

	SVector<char>& GetKeys() {
		CHECK_GE(data.size(), 2);
		return data[0];
	}
	SVector<char>& GetValues() {
		CHECK_GE(data.size(), 2);
		return data[1];
	}
	SVector<char>& GetLens() {
		CHECK_EQ(data.size(), 3);
		return data[2];
	}

 public:
	/**
	 * 输出 data 时，仅输出各元素的长度。
	 * @param verbose ~为 0 则输出 data 的内容，否则仅输出 data 的长度~ 不使用。否则要根据 DataType 转回类型太麻烦。
	 */
	std::string DebugString(size_t tab = 0, int verbose = 0) const {
		std::stringstream ss;
		ss << "{ Message\n";
		std::string tabStr(tab + 1, '\t');
		#define NewLine(str) ss << tabStr << str
		NewLine("meta: ") << meta.DebugString(tab + 1) << ",\n";
		if (!data.empty()) {
			CHECK_GE(data.size(), 2);
			NewLine("data: [ ");
			if (true) { // verbose
				ss << " sizes: "
					<< data[0].size() << ", "
					<< data[1].size() << ", ";
				if (data.size() == 3) {
					ss << data[2].size() << ", ";
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
