/**
 * @file SimpleApp.h
 */
#pragma once
#include <functional>

#include "../internal/Message.h"

namespace ps {

/**
 * @brief SimpleApp 之间通信的信息。
 * App 内部会将其与 ps::Message 进行转换，使用户不需要了解和使用 ps::Message 的实现。
 */
struct SimpleData {
	/* 消息标识？ */
	int head;
	/* 发送者的节点 ID */
	int sender;
	/* 发送者的 customer ID */
	int customer_id;
	/* 当前请求的 ID */
	int request_id;
	/* 可选的消息体 */
	std::string body;
};

class Customer;

/**
 * @brief 对 PS 系统的简单对外封装。提供 PS 系统的基本通信能力。
 * 允许在通信时附带一个 head 与 body，并在收到请求/响应消息时会调用指定的回调函数。
 * 用户可继承重写 SimpleApp，实现自己的分布式系统逻辑。
 */
class SimpleApp {
 public:
	/**
	 * @brief 收到请求/响应时调用的回调。
	 */
	using Handle = std::function<void(SimpleApp* app, const SimpleData& received)>;

	/**
	 * @brief Construct a new Simple App object
	 * @param app_id 当前通信使用的 app
	 * @param customer_id 当前的 customer ID
	 */
	explicit SimpleApp(int app_id, int customer_id);

	virtual ~SimpleApp();

	/**
	 * @brief 发送一次请求。
	 * @param request_head 请求消息使用的标识
	 * @param request_body 请求体
	 * @param receiver 接收者的节点 ID 或组 ID
	 * @return 本次请求的 ID
	 */
	virtual int Request(int request_head, const std::string request_body, int receiver);

	/**
	 * @brief 向指定请求发送响应。
	 * 默认实现为：将 response_body 放入 body 然后发送 response。
	 * @param received 要进行回复的请求
	 * @param response_body 要回复的内容
	 */
	virtual void Response(const SimpleData& request_msg, const std::string& response_body = "");

	/**
	 * @brief 阻塞直到指定请求完成，即对方节点已进行响应。
	 * @param request_id 请求 ID
	 */
	virtual void Wait(int request_id);

	virtual void SetRequestHandle(const Handle& request_handle);

	virtual void SetResponseHandle(const Handle& response_handle);

	virtual Customer* GetCustomer() {
		return customer_;
	}

 protected:
	/**
	 * @brief 默认构造：将请求/响应回调设为默认逻辑。
	 * 子类可重写。
	 */
	SimpleApp() {
		// call response
		request_handle_ = [](SimpleApp* app, const SimpleData& received) {
			app->Response(received);
		};
		// do nothing
		response_handle_ = [](SimpleApp* app, const SimpleData& received) {};
	}

	/**
	 * @brief 接收到消息时执行的逻辑：
	 * 将 Message 转为 SimpleData，然后调用指定请求/响应回调。
	 */
	virtual void OnReceive(const Message& msg);

	Customer* customer_{nullptr};

 private:
	/* 收到请求时执行的回调。
	* 默认：执行 Response */
	Handle request_handle_;
	/* 收到响应时执行的回调。
	* 默认：不做任何事 */
	Handle response_handle_;
};

} // namespace ps