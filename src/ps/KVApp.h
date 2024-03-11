/**
 * @file KVApp.h
 * @brief 一个自定义的 App 示例，可用于简单的机器学习。
 */
#pragma once
#include <vector>
#include <algorithm>
#include <unordered_map>

#include "../ps/Base.h"
#include "../ps/Range.h"
#include "../ps/SimpleApp.h"
#include "../internal/Customer.h"
#include "../internal/PostOffice.h"
#include "../utility/SVector.h"

namespace ps {

// 均为默认实现

/**
 * @brief 包含一系列 KV 及每个 value 的长度（可选）。
 * 每个 key 必须唯一，且升序排列。
 * 如果 lens 未指定，则每个 value 的长度为 vals.size()/keys.size()，否则为 lens[i]。
 * @tparam Value 值类型
 */
template <typename Value>
struct KVPairs {
	/* key */
	SVector<Key> keys;
	/* value */
	SVector<Value> values;
	/* 每个 key 的 value 长度（可选） */
	SVector<int> lens;
	/* priority */
	int priority{0};
};

/**
 * @brief 一个 worker 节点。
 */
template <typename Value>
class KVWorker: public SimpleApp {
 public:
	/* KVPairs，即通信的数据：一系列键值对和可选的长度 */
	using Data = KVPairs<Value>;

	/**
	 * @brief Server 写入完成后 (push) 或请求的数据返回后 (pull) 执行的回调函数。
	 * It is called by the data receiving thread of this instance when the push or
	 * pull is actually finished. Namely the kv pairs have already written into
	 * servers' data structure or the kv pairs have already pulled back.
	 */
	using Callback = std::function<void()>;

	/**
	 * @brief constructor
	 * @param app_id 应用 ID，需要与 KVServer 的 app_id 对应
	 * @param customer_id customer id
	 */
	explicit KVWorker(int app_id, int customer_id) : SimpleApp() {
		using namespace std::placeholders;
		slicer_ = std::bind(&KVWorker::DefaultSlicer, this, _1, _2, _3);
		customer_ = new Customer(app_id, customer_id, std::bind(&KVWorker::OnReceive, this, _1));
	}

	virtual ~KVWorker() { delete customer_; customer_ = nullptr; }

	/**
	 * @brief 向 server 推送数据（键值对）。
	 * 数据会根据 server 实际负责的 key 区间被切分，再发送给对应 server。
	 * 该调用不阻塞，可以通过 Wait 或 callback 得知推送何时完成。
	 *
	 * This function pushes a KV list specified by \a keys and \a vals to all server nodes.
	 *
	 * Sample usage: the following codes push two KV pairs `{1, (1.1, 1.2)}` and `{3,
	 * (3.1,3.2)}` to server nodes, where the value is a length-2 float vector
	 * \code
	 *	 KVWorker<float> w;
	 *	 std::vector<Key> keys = {1, 3};
	 *	 std::vector<float> vals = {1.1, 1.2, 3.1, 3.2};
	 *	 w.Push(keys, vals);
	 * \endcode
	 *
	 * @param keys 键，必须唯一且升序排序
	 * @param vals 值
	 * @param lens 可选，保存每个 value 的长度
	 * @param cmd 可选，要发送给 server 的命令
	 * @param cb 可选，推送完成后要执行的回调
	 * @return 本次请求的时间戳（即 request_id）
	 */
	int Push(const std::vector<Key>& keys,
				const std::vector<Value>& vals,
				const std::vector<int>& lens = {},
				int cmd = 0,
				const Callback& cb = nullptr,
				int priority = 0) {
		// 此处通过 vector 构造 SVector 需要拷贝。如果想要零拷贝，用 SVector 做参数调用 ZPush
		return ZPush(
				SVector<Key>(keys), SVector<Value>(vals), SVector<int>(lens), cmd, cb,
				priority);
	}

	/**
	 * @brief 从 server 拉取指定 key 的 value。
	 * 该调用不阻塞，可以通过 Wait 或 callback 得知拉取何时完成。
	 *
	 * This function pulls the values of the keys specified in \a keys from the
	 * server nodes. The format is same to \ref KVPairs
	 *
	 * Sample usage: the following codes pull the values of keys \a 1 and \a 3
	 * from the server nodes.
	 * \code
	 *	 KVWorker<float> w;
	 *	 std::vector<Key> keys = {1, 3};
	 *	 std::vector<float> vals;
	 *	 w.Pull(keys, &vals);
	 * \endcode
	 *
	 * @param keys 键，必须唯一且升序排序
	 * @param vals 拉取后的值要保存到的位置。大小需要为 0 或等于要拉取的值数量。
	 * ? 为什么用指针不用引用？
	 * @param lens 可选，拉取后的值的长度要保存到的位置。如果非空，大小需要为 0 或等于键的数量。
	 * @param cmd 可选，要发送给 server 的命令
	 * @param cb 可选，拉取完成后要执行的回调
	 * @return 本次请求的时间戳（即 request_id）
	 */
	int Pull(const std::vector<Key>& keys,
				std::vector<Value>* vals,
				std::vector<int>* lens = nullptr,
				int cmd = 0,
				const Callback& cb = nullptr,
				int priority = 0) {
		// 此处通过 vector 构造 SVector 需要拷贝。如果想要零拷贝，在外部构造 SVector，然后用 SVector 做参数调用 ZPush
		SVector<Key> skeys(keys);
		int ts = AddPullCB(skeys, vals, lens, cmd, cb);
		Data kvs;
		kvs.keys = skeys;
		kvs.priority = priority;
		Send(ts, false, true, cmd, kvs);
		return ts;
	}

	/**
	 * @brief 向 server 推送然后拉取数据。
	 * 该调用不阻塞，可以通过 Wait 或 callback 得知推送何时完成。
	 *
	 * This function pushes the values of the keys specified in \a keys to the
	 * server nodes and subsequently pulls and updates the values in \a vals.
	 *
	 * Sample usage: the following code pushes and pulls the values of keys
	 * \a 1 and \a 3 to and from the server nodes.
	 * \code
	 *	 KVWorker<float> w;
	 *	 std::vector<Key> keys = {1, 3};
	 *	 std::vector<float> vals;
	 *	 w.PushPull(keys, &vals);
	 * \endcode
	 *
	 * @param keys 键，必须唯一且升序排序
	 * @param vals 值
	 * @param puts 拉取后的值要保存到的位置。大小需要为 0 或等于要拉取的值数量。
	 * @param lens 可选，拉取后的值的长度要保存到的位置。如果非空，大小需要为 0 或等于键的数量。
	 * @param cmd 可选，要发送给 server 的命令
	 * @param cb 可选，拉取完成后要执行的回调
	 * @return 本次请求的时间戳（即 request_id）
	 */
	int PushPull(const std::vector<Key>& keys,
					const std::vector<Value>& vals,
					std::vector<Value>* outs,
					std::vector<int>* lens = nullptr,
					int cmd = 0,
					const Callback& cb = nullptr,
					int priority = 0) {
		CHECK_NOTNULL(outs);
		if (outs->empty())
			outs->resize(vals.size());
		else
			CHECK_EQ(vals.size(), outs->size());

		SVector<Key> skeys(keys);
		SVector<Value> svals(vals);
		auto souts = new SVector<Value>(outs->data(), outs->size());
		SVector<int>* slens = lens ?
				new SVector<int>(lens->data(), lens->size()) : nullptr;
		int ts = ZPushPull(skeys, svals, souts, slens, cmd,
				[this, cb, souts, slens]() {
					delete souts;
					delete slens;
					if (cb) cb();
				}, priority);
		return ts;
	}

	/**
	 * @brief 阻塞，直到指定的操作完成。
	 *
	 * Sample usage:
	 * \code
	 *	 int ts = w.Pull(keys, &vals);
	 *	 Wait(ts);
	 *	 // now vals is ready for use
	 * \endcode
	 *
	 * @param timestamp 标识操作请求的时间戳（即 request_id）
	 */
	void Wait(int timestamp) { customer_->WaitRequest(timestamp); }

	/**
	 * @brief 向 server 推送数据，零拷贝。
	 * 与 Push 类似，但需要传递 SVector 以实现零拷贝。
	 * 调用者需要保证在调用完成前，SVector 的内容不会被更改。
	 */
	int ZPush(const SVector<Key>& keys,
				const SVector<Value>& vals,
				const SVector<int>& lens = {},
				int cmd = 0,
				const Callback& cb = nullptr,
				int priority = 0) {
		int ts = customer_->NewRequest(kServerGroup);
		AddCallback(ts, cb);
		Data kvs;
		kvs.keys = keys;
		kvs.vals = vals;
		kvs.lens = lens;
		kvs.priority = priority;
		Send(ts, true, false, cmd, kvs);
		return ts;
	}

	/**
	 * @brief 从 server 拉取数据，零拷贝。
	 * 与 Pull 类似，但需要传递 SVector 以实现零拷贝。
	 * 调用者需要保证在调用完成前，SVector 的内容不会被更改。
	 */
	int ZPull(const SVector<Key>& keys,
				SVector<Value>* vals,
				SVector<int>* lens = nullptr,
				int cmd = 0,
				const Callback& cb = nullptr,
				int priority = 0) {
		int ts = AddPullCB(keys, vals, lens, cmd, cb);
		Data kvs;
		kvs.keys = keys;
		kvs.priority = priority;
		Send(ts, false, true, cmd, kvs);
		return ts;
	}

	/**
	 * @brief 向 server 推送并拉取数据，零拷贝。
	 * 与 PushPull 类似，但需要传递 SVector 以实现零拷贝。
	 * 调用者需要保证在调用完成前，SVector 的内容不会被更改。
	 */
	int ZPushPull(const SVector<Key>& keys,
					const SVector<Value>& vals,
					SVector<Value>* outs,
					SVector<int>* lens = nullptr,
					int cmd = 0,
					const Callback& cb = nullptr,
					int priority = 0) {
		int ts = AddPullCB(keys, outs, lens, cmd, cb);
		Data kvs;
		kvs.keys = keys;
		kvs.vals = vals;
		kvs.priority = priority;
		if (lens)
			kvs.lens = *lens;
		Send(ts, true, true, cmd, kvs);
		return ts;
	}

	/* 切分结果。SlicedKVs[i] 的值为：(切分数据是否有属于 server[i] 的部分, 属于 server[i] 部分的数据) */
	using SlicedKVs = std::vector<std::pair<bool, Data>>;
	/**
	 * @brief slicer 根据 key 区间将若干键值对切分成多组，每组对应一个 server
	 * @param send 需要划分的键值对
	 * @param ranges 各个 server 的 key 区间，ranges[i] 为 server i 的 key 区间
	 * @param sliced 切分结果。sliced[i] 会保存 server i 所负责的所有键值
	 */
	using Slicer =
		std::function<void(const Data& send,
							const std::vector<Range>& ranges,
							SlicedKVs* sliced)>;

	/**
	 * @brief 设置一个自定义 slicer
	 */
	void set_slicer(const Slicer& slicer) {
		CHECK(static_cast<bool>(slicer));
		slicer_ = slicer;
	}

 private:
	/**
	 * @brief Pull 的内部实现。C/D 是模板以能同时接收 SVector 和 std::vector。
	 * Pull 的 cb 相比 Push 还有其它逻辑：需要将切分后从不同 Server 收到的数据汇总、按 key 排序，
	 * 然后才能依次放入 vals、lens
	 */
	template <typename C, typename D>
	int AddPullCB(const SVector<Key>& keys, C* vals, D* lens,
						int cmd, const Callback& cb);
	/**
	 * @brief 为指定请求添加回调。
	 * @param cb callback
	 * @param timestamp 标识请求的时间戳
	 */
	void AddCallback(int timestamp, const Callback& cb) {
		if (!cb) return;
		std::lock_guard<std::mutex> lk(mu_);
		callbacks_[timestamp] = cb;
	}

	/**
	 * @brief 运行并移除指定请求的回调。
	 * @param timestamp 标识请求的时间戳
	 */
	void RunCallback(int timestamp);
	/**
	 * @brief 向所有 server 发送数据。
	 * 数据会根据 slicer 进行切分，被发送到对应负责的 server 上。
	 * @param timestamp 标识请求的时间戳
	 * @param push 是否是 push 请求
	 * @param pull 是否是 pull 请求
	 * @param cmd command
	 */
	void Send(int timestamp, bool push, bool pull, int cmd, const Data& kvs);

	/**
	 * @brief 接收到消息时执行的逻辑
	 */
	void OnReceive(const Message& msg);

	/**
	 * @brief 默认的 slicer
	 */
	void DefaultSlicer(const KVPairs<Value>& send,
						const std::vector<Range>& ranges,
						SlicedKVs* sliced);

	/* timestamp -> 对应 pull 请求已接收到的数据.
	* 收到 pull 请求的回应时，将数据暂存到这里。
	* 只有在收到的回复数量等于 num_servers 时，才可调用 callback，调用前的回复需先缓存 */
	std::unordered_map<int, std::vector<KVPairs<Value>>> recv_kvs_;
	/* timestamp -> 对应请求的回调.
	* 每次请求的回调 */
	std::unordered_map<int, Callback> callbacks_;
	/* 其实可分为两个：callback mu 与 recv mu */
	std::mutex mu_;
	/* 数据使用的 slicer */
	Slicer slicer_;
};

/**
 * @brief 一次请求 (push, pull, push_pull) 的元信息
 */
struct KVMeta {
	/* cmd */
	int cmd;
	/* 是否为 push 请求。
	* 如果是，则用消息中的数据更新存储 */
	bool push;
	/* 是否为 push 请求。
	* 如果是，则要求返回 Key 中的最新数据 */
	bool pull;
	/* 发送者的节点 ID */
	int sender;
	/* 请求的时间戳（即 request_id） */
	int timestamp;
	/* 相关 worker 的 customer_id */
	int customer_id;
};

/**
 * @brief 维护键值对的一个 server 节点。
 */
template <typename Value>
class KVServer : public SimpleApp {
 public:
	/**
	 * @brief constructor
	 * @param app_id 应用 ID，需要与 KVWorker 的 app_id 对应
	 */
	explicit KVServer(int app_id) : SimpleApp() {
		using namespace std::placeholders;
		customer_ = new Customer(app_id, app_id, std::bind(&KVServer<Value>::OnReceive, this, _1));
	}

	virtual ~KVServer() { delete customer_; customer_ = nullptr; }

	/**
	 * @brief 处理 worker 请求 (push, pull, push_pull) 的 handle
	 * @param req_meta 本次请求的元信息
	 * @param req_data 本次请求的数据
	 * @param server this
	 */
	using ReqHandle =
		std::function<void(const KVMeta& req_meta,
							const KVPairs<Value>& req_data,
							KVServer* server)>;

	void set_request_handle(const ReqHandle& request_handle) {
		CHECK(static_cast<bool>(request_handle)) << "invalid request handle";
		request_handle_ = request_handle;
	}

	/**
	 * @brief 回复 worker 请求
	 * @param req_meta 本次请求的元信息
	 * @param res 要发给 worker 的数据
	 */
	void Response(const KVMeta& req, const KVPairs<Value>& res = KVPairs<Value>());

 private:
	/**
	 * @brief 接收到消息时执行的逻辑
	 */
	void OnReceive(const Message& msg);

	/* 处理请求的 handle */
	ReqHandle request_handle_;
};


/**
 * @brief 处理 worker 请求的默认 handle：将 value 累加到 store 中。
 * @param req_meta 本次请求的元信息
 * @param req_data 本次请求的数据
 */
template <typename Value>
struct KVServerDefaultHandle {
	void operator() (
			const KVMeta& req_meta, const KVPairs<Value>& req_data, KVServer<Value>* server) {
		size_t n = req_data.keys.size();
		KVPairs<Value> res;
		if (!req_meta.pull) {
			// 为什么不是判 push？push 和 pull 不能同时为 true？
			CHECK_EQ(n, req_data.vals.size());
		} else {
			res.keys = req_data.keys;
			res.vals.resize(n);
		}
		for (size_t i = 0; i < n; ++i) {
			Key key = req_data.keys[i];
			if (req_meta.push) {
				store[key] += req_data.vals[i];
			}
			if (req_meta.pull) {
				res.vals[i] = store[key];
			}
		}
		server->Response(req_meta, res);
	}
	std::unordered_map<Key, Value> store;
};


///////////////////////////////////////////////////////////////////////////////

template <typename Value>
void KVServer<Value>::OnReceive(const Message& msg) {
	if (msg.meta.simple_app) {
		SimpleApp::OnReceive(msg); return;
	}
	// 提取 Message 里的内容转成本地处理的 KVMeta 和 KVPairs
	KVMeta meta;
	meta.cmd	= msg.meta.head;
	meta.push	= msg.meta.push;
	meta.pull	= msg.meta.pull;
	meta.sender	= msg.meta.sender;
	meta.timestamp = msg.meta.timestamp;
	meta.customer_id = msg.meta.customer_id;
	KVPairs<Value> data;
	int n = msg.data.size();
	if (n) {
		CHECK_GE(n, 2);
		data.keys = msg.data[0];
		data.vals = msg.data[1];
		if (n > 2) {
			CHECK_EQ(n, 3);
			data.lens = msg.data[2];
			CHECK_EQ(data.lens.size(), data.keys.size());
		}
	}
	CHECK(static_cast<bool>(request_handle_));
	request_handle_(meta, data, this);
}

template <typename Value>
void KVServer<Value>::Response(const KVMeta& req, const KVPairs<Value>& res) {
	// 根据 KVMeta 和 KVPairs 生成一个 Message
	Message msg;
	msg.meta.app_id = customer_->app_id();
	msg.meta.customer_id = req.customer_id;
	msg.meta.request	 = false;
	msg.meta.push		 = req.push;
	msg.meta.pull		 = req.pull;
	msg.meta.head	 	 = req.cmd;
	msg.meta.timestamp	 = req.timestamp;
	msg.meta.receiver	 = req.sender;
	// Message 中的 SVector 会与 request_handle_ 中产生的要返回的 KVPairs 中的数组共享所有权，以减少拷贝
	// ~为什么不 move 而是引用计数？因为 Response 不一定能负责 res 的生命周期~
	if (res.keys.size()) {
		msg.AddData(res.keys);
		msg.AddData(res.vals);
		if (res.lens.size()) {
			msg.AddData(res.lens);
		}
	}
	PostOffice::Get()->van()->Send(msg);
}

template <typename Value>
void KVWorker<Value>::DefaultSlicer(
		const KVPairs<Value>& send, const std::vector<Range>& ranges,
		typename KVWorker<Value>::SlicedKVs* sliced) {
	sliced->resize(ranges.size());

	// 确定每次切分的位置
	size_t n = ranges.size();
	std::vector<size_t> pos(n+1);
	const Key* begin = send.keys.begin();
	const Key* end = send.keys.end();
	for (size_t i = 0; i < n; ++i) {
		if (i == 0) {
			pos[0] = std::lower_bound(begin, end, ranges[0].begin) - begin;
			begin += pos[0];
		} else {
			CHECK_EQ(ranges[i-1].end, ranges[i].begin);
		}
		size_t len = std::lower_bound(begin, end, ranges[i].end) - begin;
		begin += len;
		// pos[i] ~ pos[i+1] - 1 即 pos[i] ~ pos[i] + len - 1
		// 为属于第 i 个 Range（节点）的数据
		pos[i+1] = pos[i] + len;

		// 不含某个 server 的数据时，不需要发送
		sliced->at(i).first = (len != 0);
	}
	// ~Range[n - 1].end 一定是 MAX 吗？不会回绕吗~
	// key 区间在系统启动时就确定，不会动态更改
	CHECK_EQ(pos[n], send.keys.size());
	if (send.keys.empty()) return; // 如果在一开始就返回，注意 sliced 的 bool 和大小初始化？

	// the length of value
	size_t k = 0, val_begin = 0, val_end = 0;
	if (send.lens.empty()) {
		k = send.vals.size() / send.keys.size();
		CHECK_EQ(k * send.keys.size(), send.vals.size());
	} else {
		CHECK_EQ(send.keys.size(), send.lens.size());
	}

	// slice
	for (size_t i = 0; i < n; ++i) {
		if (pos[i+1] == pos[i]) {
			sliced->at(i).first = false;
			continue;
		}
		sliced->at(i).first = true;
		auto& kv = sliced->at(i).second;
		kv.keys = send.keys.segment(pos[i], pos[i+1]);
		if (send.lens.size()) {
			kv.lens = send.lens.segment(pos[i], pos[i+1]);
			for (int l : kv.lens) val_end += l;
			kv.vals = send.vals.segment(val_begin, val_end);
			val_begin = val_end;
		} else {
			kv.vals = send.vals.segment(pos[i]*k, pos[i+1]*k);
		}
	}
}

template <typename Value>
void KVWorker<Value>::Send(int timestamp, bool push, bool pull, int cmd, const KVPairs<Value>& kvs) {
	// slice the message
	SlicedKVs sliced;
	slicer_(kvs, PostOffice::Get()->GetServerRanges(), &sliced);

	// need to add response first, since it will not always trigger the callback
	int skipped = 0;
	for (size_t i = 0; i < sliced.size(); ++i) {
		if (!sliced[i].first) ++skipped;
	}
	customer_->AddResponse(timestamp, skipped);
	if ((size_t)skipped == sliced.size()) {
		RunCallback(timestamp);
		// 此时实际可以直接 return 了
		// 当没有消息被发时，也不会收到回复，即 OnReceive 不会被调用，也就不会再有人触发 callback，所以手动调用下
	}

	for (size_t i = 0; i < sliced.size(); ++i) {
		const auto& s = sliced[i];
		if (!s.first) continue;
		Message msg;
		msg.meta.app_id = customer_->app_id();
		msg.meta.customer_id = customer_->customer_id();
		msg.meta.request	 = true;
		msg.meta.push		 = push;
		msg.meta.pull		 = pull;
		msg.meta.head		 = cmd;
		msg.meta.timestamp	 = timestamp;
		msg.meta.receiver	 = PostOffice::Get()->ServerRankToID(i);
		msg.meta.priority	 = kvs.priority;
		const auto& kvs = s.second;
		if (kvs.keys.size()) {
			msg.AddData(kvs.keys);
			msg.AddData(kvs.vals);
			if (kvs.lens.size()) {
				msg.AddData(kvs.lens);
			}
		}
		PostOffice::Get()->van()->Send(msg);
	}
}

template <typename Value>
void KVWorker<Value>::OnReceive(const Message& msg) {
	if (msg.meta.simple_app) {
		SimpleApp::OnReceive(msg); return;
	}
	// store the data for pulling
	int ts = msg.meta.timestamp;
	if (msg.meta.pull) {
		CHECK_GE(msg.data.size(), (size_t)2);
		KVPairs<Value> kvs;
		kvs.keys = msg.data[0];
		kvs.vals = msg.data[1];
		if (msg.data.size() > (size_t)2) {
			kvs.lens = msg.data[2];
		}
		mu_.lock();
		recv_kvs_[ts].push_back(kvs);
		mu_.unlock();
	}

	// finished, run callbacks
	// Customer 接收消息的处理流程中，是先执行 recv_handle_ 即该函数，再执行 AddResponse、然后尝试唤醒
	// 因此最后的 Response 是 num-1 而非 num
	if (customer_->GetResponse(ts) == PostOffice::Get()->num_servers() - 1)	{
		RunCallback(ts);
	}
}
template <typename Value>
void KVWorker<Value>::RunCallback(int timestamp) {
	mu_.lock();
	auto it = callbacks_.find(timestamp);
	if (it != callbacks_.end()) {
		// rehash 会导致迭代器 it 失效。因此应该先把 it->second 存到临时变量，然后移除、解锁，再执行
		auto& cb = it->second;
		mu_.unlock();

		CHECK(static_cast<bool>(cb));
		cb();

		mu_.lock();
		callbacks_.erase(it);
	}
	mu_.unlock();
}

template <typename Value>
template <typename C, typename D>
int KVWorker<Value>::AddPullCB(
		const SVector<Key>& keys, C* vals, D* lens, int cmd,
		const Callback& cb) {
	int ts = customer_->NewRequest(kServerGroup);
	// ~keys 也要值捕获吗？~（SVector 的拷贝代价低，无所谓）
	AddCallback(ts, [this, ts, keys, vals, lens, cb]() mutable {
			mu_.lock();
			auto& kvs = recv_kvs_[ts]; // 检查一下是否为空
			mu_.unlock();

			// do check
			// 检查从各节点收到的 keys 是否对应原来的 keys
			size_t total_key = 0, total_val = 0;
			for (const auto& s : kvs) {
				// 将切分后的某一部分 key 映射到原来 key 的下标区间
				Range range = FindRange(keys, s.keys.front(), s.keys.back()+1);
				// s.keys 一定是原来 keys 的一个连续子区间
				CHECK_EQ(range.size(), s.keys.size())
						<< "unmatched keys size from one server";
				if (lens) CHECK_EQ(s.lens.size(), s.keys.size()); // ? 不可以为 0 吗？
				total_key += s.keys.size();
				total_val += s.vals.size();
			}
			CHECK_EQ(total_key, keys.size()) << "lost some servers?";

			// fill vals and lens
			std::sort(kvs.begin(), kvs.end(), [](const KVPairs<Value>& a, const KVPairs<Value>& b) {
				return a.keys.front() < b.keys.front();
			});
			CHECK_NOTNULL(vals);
			if (vals->empty()) {
				vals->resize(total_val);
			} else {
				CHECK_EQ(vals->size(), total_val); // GE 其实也可以
			}
			Value* p_vals = vals->data();
			int *p_lens = nullptr;
			if (lens) {
				if (lens->empty()) {
					lens->resize(keys.size());
				} else {
					CHECK_EQ(lens->size(), keys.size());
				}
				p_lens = lens->data();
			}
			for (const auto& s : kvs) {
				memcpy(p_vals, s.vals.data(), s.vals.size() * sizeof(Value));
				p_vals += s.vals.size();
				if (p_lens) {
					memcpy(p_lens, s.lens.data(), s.lens.size() * sizeof(int));
					p_lens += s.lens.size();
				}
			}

			mu_.lock();
			recv_kvs_.erase(ts);
			mu_.unlock();
			if (cb) cb();
		});

	return ts;
}

} // namespace ps