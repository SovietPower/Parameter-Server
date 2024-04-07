#include <thread>
#include <chrono>

#include "ps/ps.h"
#include "./src/LRServer.h"
#include "./src/LRWorker.h"

using lr::FType;

void RunServer() {
	if (!ps::IsServer()) {
		return;
	}
	auto server = new lr::LRServer();
	ps::RegisterExitCallback([server](){ delete server; });
}

void RunWorker(int customer_id) {
	if (!ps::IsWorker()) return;
	auto* kv_worker = new ps::KVWorker<FType>(0, customer_id);

	int rank = ps::MyRank();
	srand(rank); // 默认为 srand(1)

	// get config
	std::string data_dir = ps::Environment::GetOrFail("DATA_DIR");
	int num_feature = ps::Environment::GetIntOrFail("NUM_FEATURE");

	int iteration = ps::Environment::GetInt("ITERATION");
	int batch_size = ps::Environment::GetIntOrDefault("BATCH_SIZE", -1);
	int test_period = ps::Environment::GetInt("TEST_PERIOD");

	int sync_mode = ps::Environment::GetInt("SYNC_MODE");
	bool is_async = sync_mode == 1;
	bool output_result = is_async || rank == 0; // 只让一个 worker 输出信息

	constexpr bool track_comm = true; // 记录每轮的通信量

	// setup lr worker
	lr::LRWorker<true, track_comm> lr_worker(kv_worker);

	// if (rank == 0) { // 为 server 设置初始参数
	// 	lr_worker.Push(lr_worker.GetWeight());
	// }
	ps::Barrier(customer_id, ps::kWorkerGroup);

	lr::DataLoader train_data(data_dir + "/train/worker-0" + std::to_string(rank), num_feature);
	lr::DataLoader test_data(data_dir + "/test/full", num_feature);

	{
		auto now = std::chrono::system_clock::now();
		std::time_t tmNow = std::chrono::system_clock::to_time_t(now);
		std::ostringstream out;
		out << "Worker[" << rank << "] starts training at " << std::put_time(std::localtime(&tmNow), "%F %T");
		std::cout << out.str() << std::endl;
		LOG(WARNING) << out.str();
	}

	auto start_tm = std::chrono::system_clock::now();
	for (int i = 1; i <= iteration; ++i) {
		lr_worker.Train(train_data, batch_size);
		train_data.Reset();

		if (output_result && (i == 1 || (test_period > 0 && i % test_period == 0 && i < iteration))) {
			lr_worker.Test(test_data, i);
			test_data.Reset();
		}

		// for debug
		// 随机延迟某些 worker，测试同步与异步的差异
		if (rank > 0 && rand() % 4 == 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(rank * 200)); // delay 200ms 400ms 600ms
		}
	}
	auto end_tm = std::chrono::system_clock::now();

	if (output_result) {
		lr_worker.Test(test_data, iteration);
	}
	lr_worker.SaveModel(data_dir + "/model/worker-0" + std::to_string(rank), output_result ? data_dir + "/model/worker-0" + std::to_string(rank) + "_test" : "");

	{
		std::time_t tmNow = std::chrono::system_clock::to_time_t(end_tm);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_tm - start_tm);

		std::ostringstream out;
		out << "Worker[" << rank << "] finished training at "  << std::put_time(std::localtime(&tmNow), "%F %T")
			<< "\n\ttime: " << ms
			<< ", iteration: " << iteration
			<< ", batch_size: " << batch_size;
		std::cout << out.str() << std::endl;
		LOG(WARNING) << out.str();
	}

	if (output_result && track_comm) {
		const auto& bytes_sent = lr_worker.bytes_sent_;
		const auto& bytes_received = lr_worker.bytes_received_;
		std::ofstream comm(data_dir + "/model/network");
		for (size_t i = 0; i < bytes_sent.size(); ++i) {
			int s = bytes_sent[i] * 4;
			int r = bytes_received[i] * 4;
			comm << s << '\t' << r << '\t' << (s + r) << '\n';
		}
	}
}

void RunMultiWorker(int customer_id, int argc, char* argv[]) {
	// 每个 worker 需要分别调用对应 customer_id 的 start 与 Finalize
	ps::Start(customer_id, argc, argv);
	RunWorker(customer_id);
	ps::Finalize(customer_id, true);
}

// whether run multi worker nodes in one process
#define USE_MULTI_WORKER 0

int main(int argc, char* argv[]) {
#if USE_MULTI_WORKER == 0
	// start system
	ps::Start(0, argc, argv);
	// setup server nodes
	RunServer();
	// run worker nodes
	RunWorker(0);
	// stop system
	ps::Finalize(0, true);

#else
#error "目前不能使用多 customer, 因为内部获取 rank 的实现有误"
	if (argc < 4) {
		std::cout << "param error:\n"
			<< "usage: " << argv[0] << " config_filename log_filename role\n";
		return 0;
	}
	bool isWorker = (strcmp(argv[3], "worker") == 0);
	// server and scheduler
	if (!isWorker) {
		ps::Start(0, argc, argv);
		RunServer();
		ps::Finalize(0, true);
		return 0;
	}
	// worker
	int num_workers = 2;
	std::vector<std::thread> v;
	for (int i = 0; i < num_workers; ++i) {
		v.emplace_back(RunMultiWorker, i, argc, argv);
	}
	for (auto& t: v) {
		t.join();
	}
#endif

}