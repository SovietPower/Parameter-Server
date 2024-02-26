/**
 * @file Node.h
 */
#pragma once
#include <string>
#include <sstream>

namespace ps {
/**
 * @brief 表示一个节点的信息。
 */
struct Node {
	Node(): id(kEmpty), port(kEmpty), is_recovered(false) {}
	~Node() = default;

	enum Role {
		SERVER, WORKER, SCHEDULER,
	};

	/* 空值 */
	static constexpr int kEmpty = -998244353; // std::numeric_limits<int>::max();

	/* 节点角色 */
	Role role;
	/* 节点 ID */
	int id;
	/* 节点的 customer_id */
	int customer_id;

	/* 节点的域名或 IP */
	std::string hostname;
	/* 节点绑定的端口号 */
	int port;
	/* 节点是否是通过故障重启加入的，而非最初创建是节点 */
	bool is_recovered;

	std::string DebugString(size_t tab = 0) const {
		std::stringstream ss;
		ss << "{ Node\n";
		std::string tabStr(tab + 1, '\t');
		#define NewLine(str) ss << tabStr << str
		NewLine("Role: ") << (role == SERVER ? "server" : role == WORKER ? "worker" : "scheduler")
			<< ", id: " << (id == kEmpty ? -1 : id)
			<< ", customer_id: " << customer_id << ",\n";
		NewLine(", ip: ") << hostname
			<< ", port: " << port
			<< ", is_recovered: " << is_recovered << ",\n";
		#undef NewLine
		ss << std::string(tab, '\t') << "}";
		return ss.str();
	}
	std::string ShortDebugString() const {
		std::string s = (role == SERVER ? "S" : role == WORKER ? "W" : "H");
		s += "[" + std::to_string(id == kEmpty ? -1 : id) + "]";
		return s;
	}
};

} // namespace ps

