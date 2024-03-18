#include "PostOffice.h"

#include <fstream>

#include "ps/Base.h"
#include "internal/Env.h"
#include "internal/Customer.h"
#include "utility/JSONParser.hpp"

#include "../Config.h"

namespace {

/**
 * @brief 读取文件内容并转为 string
 */
std::string readFileToString(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		LOG(ERROR) << "Unable to open config file: " << filename
			<< ". using environment vars.";
		return "";
	}
	std::string content;
	std::string line;
	while (std::getline(file, line)) {
		content += line + '\n';
	}
	file.close();
	return content;
}

// 读取当前目录下的 config_filename.json 初始化 Environment
void ReadLocalConfig(std::string config_name) {
	if (config_name.length() > 5) {
		std::string suffix = config_name.substr(config_name.length() - 5);
		if (suffix != ".json") {
			config_name += ".json";
		}
	}
	std::string content = readFileToString(config_name);
	if (content.empty()) {
		return;
	}
	json::JSON json = json::JSON::Load(content);
	std::unordered_map<std::string, std::string> cfg;

	auto AddKey = [&json, &cfg](const std::string& key, bool isEssential = true) {
		if (!json.hasKey(key)) {
			if (isEssential) {
				CHECK(json.hasKey(key)) << "config.json should contain key " << key;
			}
			return;
		}
		switch (json[key].JSONType()) {
			case json::JSON::Class::String:
				cfg[key] = json[key].ToString();
				break;
			case json::JSON::Class::Integral:
				cfg[key] = std::to_string(json[key].ToInt());
				break;
			default:
				LOG(ERROR) << "Unsupported config type: " << static_cast<int>(json[key].JSONType());
		}
		using ps::PostOffice;
	};

	AddKey("PS_NUM_WORKER");
	AddKey("PS_NUM_SERVER");
	AddKey("PS_ROLE");
	AddKey("PS_SCHEDULER_URI");
	AddKey("PS_SCHEDULER_PORT");

	AddKey("PS_VERBOSE", false);

	ps::Environment::Init(cfg);
}

} // namespace

namespace ps {

PostOffice::~PostOffice() {
	delete van_;
}

void PostOffice::InitEnv(const char* config_filename) {
#ifdef USE_CONFIG_FILE
	CHECK_NOTNULL(config_filename);
	ReadLocalConfig(config_filename);
#endif

	van_ = Van::Create(Environment::GetOrDefault("PS_VAN_TYPE", "zmq"));

	num_workers_ = std::atoi(CHECK_NOTNULL(
		Environment::Get("PS_NUM_WORKER")
	));
	num_servers_ = std::atoi(CHECK_NOTNULL(
		Environment::Get("PS_NUM_SERVER")
	));

	std::string role = CHECK_NOTNULL(
		Environment::Get("PS_ROLE")
	);
	is_worker_ = role == "worker";
	is_server_ = role == "server";
	is_scheduler_ = role == "scheduler";

	verbose_ = Environment::GetIntOrDefault("PS_VERBOSE", 0);
}


void PostOffice::Start(int customer_id, const char* config_filename, const char* log_filename, bool need_barrier) {
	start_mu_.lock();
	if (start_stage_ == 0) {
		// 初始化 glog
		ps_log::InitLogging(log_filename);

		InitEnv(config_filename);

		// 初始化 node_ids_
		for (int i = 0; i < num_servers_; ++i) {
			int id = ServerRankToID(i);
			for (int g : {id, kServerGroup,
					kServerGroup + kScheduler,
					kServerGroup + kWorkerGroup,
					kServerGroup + kWorkerGroup + kScheduler}) {
				node_ids_[g].push_back(id);
			}
		}
		for (int i = 0; i < num_workers_; ++i) {
			int id = WorkerRankToID(i);
			for (int g : {id, kWorkerGroup,
					kWorkerGroup + kScheduler,
					kWorkerGroup + kServerGroup,
					kWorkerGroup + kServerGroup + kScheduler}) {
				node_ids_[g].push_back(id);
			}
		}
		for (int g : {kScheduler,
				kScheduler + kServerGroup,
				kScheduler + kWorkerGroup,
				kScheduler + kServerGroup + kWorkerGroup}) {
			node_ids_[g].push_back(kScheduler);
		}

		++start_stage_;
	}
	start_mu_.unlock();

	CHECK_NOTNULL(van_);
	van_->Start(customer_id);

	start_mu_.lock();
	if (start_stage_ == 1) {
		start_time_ = std::time(nullptr);
		++start_stage_;
	}
	start_mu_.unlock();

	if (need_barrier) {
		Barrier(customer_id, kAllNodes);
	}
}

void PostOffice::Finalize(int customer_id, bool need_barrier) {
	if (need_barrier) {
		Barrier(customer_id, kAllNodes);
	}
	// 只需要 0 号 customer 终止进程的成员
	if (customer_id == 0) {
		van_->Stop();
		num_servers_ = 0;
		num_workers_ = 0;
		start_stage_ = 0;
		server_key_ranges_.clear();
		heartbeats_.clear();
		customers_.clear();
		barrier_done_.clear();
		node_ids_.clear();
		if (exit_callback_) {
			exit_callback_();
		}
	}
}

void PostOffice::AddCustomer(Customer* customer) {
	CHECK_NOTNULL(customer);
	std::lock_guard<std::mutex> lock(customers_mu_);
	int app_id = customer->app_id();
	int customer_id = customer->customer_id();
	CHECK_EQ(customers_[app_id].count(customer_id), 0) << "customer_id " << customer_id << " already exists";
	customers_[app_id][customer_id] = customer;

	std::unique_lock<std::mutex> ulock(barrier_mu_);
	barrier_done_[app_id][customer_id] = false;
}

void PostOffice::RemoveCustomer(Customer* customer) {
	CHECK_NOTNULL(customer);
	std::lock_guard<std::mutex> lock(customers_mu_);
	int app_id = customer->app_id();
	int customer_id = customer->customer_id();
	customers_[app_id].erase(customer_id);
	if (customers_[app_id].empty()) {
		customers_.erase(app_id);
	}
}

Customer* PostOffice::GetCustomer(int app_id, int customer_id, int timeout_in_sec) {
	Customer* ret = nullptr;
	for (int i = 0, lim = 500 * timeout_in_sec; i < lim; ++i) {
		{
			std::lock_guard<std::mutex> lock(customers_mu_);
			const auto it = customers_.find(app_id);
			if (it != customers_.end()) {
				ret = (it->second)[customer_id];
				break; // ? 不需要检查 c_id 是否存在吗
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	return ret;
}

void PostOffice::Barrier(int customer_id, int group_id) {
	if (GetNodeIDs(group_id).size() <= 1) {
		return;
	}
	switch (van_->my_node().role) {
		case Node::SERVER:
			CHECK(group_id & kServerGroup); break;
		case Node::WORKER:
			CHECK(group_id & kWorkerGroup); break;
		case Node::SCHEDULER:
			CHECK(group_id & kScheduler); break;
	}

	std::unique_lock<std::mutex> ulock(barrier_mu_);
	// 在 0 号 app_id 进行 barrier？
	barrier_done_[0][customer_id] = false;

	Message msg;
	msg.meta.app_id = 0;
	msg.meta.customer_id = customer_id;
	msg.meta.receiver = kScheduler;
	msg.meta.request = true;
	msg.meta.timestamp = van_->GetAvailableTimestamp();
	msg.meta.control.cmd = Control::BARRIER;
	msg.meta.control.barrier_group = group_id;
	van_->Send(msg);

	while (!barrier_done_[0][customer_id]) {
		barrier_cond_.wait(ulock);
	}
}

void PostOffice::ExitBarrier(const Message& msg) {
	CHECK_EQ(msg.meta.control.cmd, Control::BARRIER);
	// if (msg.meta.control.cmd == Control::BARRIER)
	if (!msg.meta.request) {
		// Scheduler 发出的结束 Barrier 指令
		int app_id = msg.meta.app_id;
		barrier_mu_.lock();
		int num_customers = barrier_done_[app_id].size();
		for (int customer_id = 0; customer_id < num_customers; ++customer_id) {
			barrier_done_[app_id][customer_id] = true;
		}
		barrier_mu_.unlock();
		barrier_cond_.notify_all();
	}
}


int PostOffice::my_rank() const  {
	return IDToRank(van_->my_node().id);
}

bool PostOffice::is_recovered() const {
	return van_->my_node().is_recovered;
}

const std::vector<Range>& PostOffice::GetServerRanges() {
	std::lock_guard lock(server_key_ranges_mu_);
	if (server_key_ranges_.empty()) {
		for (int i = 0; i < num_servers_; ++i) {
			Key begin = kMaxKey / num_servers_ * i;
			Key end = i != num_servers_ - 1 ? kMaxKey / num_servers_ * (i + 1) : kMaxKey;
			server_key_ranges_.emplace_back(begin, end);
		}
	}
	return server_key_ranges_;
}

std::vector<int> PostOffice::GetDeadNodes(int time_in_sec) {
	if (!van_->IsReady() || time_in_sec == 0) {
		return {};
	}
	std::time_t now = std::time(nullptr);
	// 如果系统还没运行那么长时间，自然不会有心跳超时的节点
	if (start_time_ + time_in_sec > now) {
		return {};
	}

	std::vector<int> dead_nodes;
	const auto& nodes = is_scheduler_ ? GetNodeIDs(kServerGroup + kWorkerGroup) : GetNodeIDs(kScheduler);

	std::lock_guard<std::mutex> lock(heartbeat_mu_);
	for (int id: nodes) {
		auto it = heartbeats_.find(id);
		if (it == heartbeats_.end() || it->second + time_in_sec < now) {
			dead_nodes.push_back(id);
		}
	}
	return dead_nodes;
}

} // namespace ps


