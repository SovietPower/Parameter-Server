/**
 * @file ZMQVan.h
 */
#pragma once

#include "../internal/Van.h"

namespace ps {

class ZMQVan: public Van {

 protected:
	void Connect(const Node &node) override {}

	int Bind(const Node &node, int max_retry) override {}

	int SendMsg(const Message &msg) override {}

	int ReceiveMsg(Message *msg) override {}

};


} // namespace ps