/**
 * @file ZMQVan.h
 */
#pragma once

#include "../internal/Van.h"

namespace ps {

class ZMQVan: public Van {
 public:
	ZMQVan() {}
	virtual ~ZMQVan() {}

 protected:
	void Start(int customer_id) override;

	void Stop() override;

	void Connect(const Node& node) override;

	int Bind(const Node& node, int max_retry) override;

	int SendMsg(const Message& msg) override;

	int ReceiveMsg(Message* msg) override;

 private:
	void* context_{nullptr};
	/* 接收 socket */
	void* receiver_{nullptr};

	std::mutex mu_;

	/* node_id -> 给该节点发送数据的 socket. */
	std::unordered_map<int, void*> senders_;
};


} // namespace ps