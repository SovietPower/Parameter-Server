#include "SimpleApp.h"

#include "internal/Customer.h"
#include "internal/PostOffice.h"

namespace ps {

SimpleApp::SimpleApp(int app_id, int customer_id)
		: ps::SimpleApp() {
	customer_ = new Customer(app_id, customer_id, std::bind(&SimpleApp::OnReceive, this, std::placeholders::_1));
}

SimpleApp::~SimpleApp() {
	delete customer_;
}

int SimpleApp::Request(int request_head, const std::string request_body, int receiver) {
	// 将 SimpleData 转为 Message，然后调用 van 发送
	Message msg;
	msg.meta.head = request_head;
	if (!request_body.empty()) {
		msg.meta.body = request_body;
	}
	msg.meta.request = true;
	msg.meta.simple_app = true;
	msg.meta.app_id = customer_->app_id();
	msg.meta.customer_id = customer_->customer_id();

	int request_id = customer_->NewRequest(receiver);
	msg.meta.timestamp = request_id;

	// 注意 receiver 可能是组，而非一个节点的 ID
	for (int id: PostOffice::Get()->GetNodeIDs(receiver)) {
		msg.meta.receiver = id;
		PostOffice::Get()->van()->Send(msg);
	}
	return request_id;
}

void SimpleApp::Response(const SimpleData& request_msg, const std::string& response_body) {
	// 将 SimpleData 转为 Message，然后调用 van 发送
	Message msg;
	msg.meta.head = request_msg.head;
	if (!response_body.empty()) {
		msg.meta.body = response_body;
	}
	msg.meta.request = false;
	msg.meta.simple_app = true;
	msg.meta.app_id = customer_->app_id();
	msg.meta.customer_id = request_msg.customer_id;

	msg.meta.timestamp = request_msg.request_id;
	msg.meta.receiver = request_msg.sender;

	PostOffice::Get()->van()->Send(msg);
}

void SimpleApp::Wait(int request_id) {
	customer_->WaitRequest(request_id);
}

void SimpleApp::SetRequestHandle(const Handle& request_handle) {
	CHECK(static_cast<bool>(request_handle)) << "Handle shouldn't be empty";
	request_handle_ = request_handle;
}

void SimpleApp::SetResponseHandle(const Handle& response_handle) {
	CHECK(static_cast<bool>(response_handle)) << "Handle shouldn't be empty";
	response_handle_ = response_handle;
}

void SimpleApp::OnReceive(const Message& msg) {
	// 将 Message 转为 SimpleData，调用指定回调
	SimpleData received {
		msg.meta.head,
		msg.meta.sender,
		msg.meta.customer_id,
		msg.meta.timestamp,
		msg.meta.body
	};
	if (msg.meta.request) {
		request_handle_(this, received);
	} else {
		response_handle_(this, received);
	}
}

} // namespace ps