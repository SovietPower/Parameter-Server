/**
 * @file test_kv_app_multi_workers.cpp
 * @brief 测试包含两个 worker 的单节点。
 */
#include <cmath>
#include "ps/ps.h"
using namespace ps;

void StartServer() {
	if (!IsServer()) return;
	auto server = new KVServer<float>(0);
	server->SetRequestHandle(KVServerDefaultHandle<float>());
	RegisterExitCallback([server](){ delete server; });
}

void RunWorker(int customer_id, int argc, char** argv) {
	Start(customer_id, argc, argv);
	if (!IsWorker()) {
		return;
	}
	KVWorker<float> kv(0, customer_id);

	std::string tmp = "Customer " + std::to_string(customer_id) + ": rank: " + std::to_string(MyRank());
	std::cout << tmp << std::endl;

	// init
	int num = 10000;
	std::vector<Key> keys(num);
	std::vector<float> vals(num);

	int rank = MyRank();
	srand(rank + 7);
	for (int i = 0; i < num; ++i) {
		keys[i] = kMaxKey / num * i + customer_id;
		vals[i] = (rand() % 1000);
	}
	// push 更新50次
	int repeat = 50;
	std::vector<int> ts;
	for (int i = 0; i < repeat; ++i) {
		ts.push_back(kv.Push(keys, vals));

		// to avoid too frequency push, which leads huge memory usage
		if (i > 10) kv.Wait(ts[ts.size()-10]);
	}
	for (int t : ts) kv.Wait(t);

	// pull 获取 Server 当前的数据，应该等于 50*vals
	std::vector<float> rets;
	kv.Wait(kv.Pull(keys, &rets));

	// PushPull 再更新50次，应该等于 100*vals
	std::vector<float> outs;
	for (int i = 0; i < repeat; ++i) {
		kv.Wait(kv.PushPull(keys, vals, &outs));
	}

	float res = 0;
	float res2 = 0;
	for (int i = 0; i < num; ++i) {
		res += fabs(rets[i] - vals[i] * repeat);
		res += fabs(outs[i] - vals[i] * 2 * repeat);
	}
	CHECK_LT(res / repeat, 1e-5);
	CHECK_LT(res2 / (2 * repeat), 1e-5);
	std::cout << "got error value: " << res / repeat << ", " << res2 / (2 * repeat) << '\n';
	// stop system
	Finalize(customer_id, true);
}

int main(int argc, char* argv[]) {
	if (argc < 4) {
		std::cout << "param error:\n"
			<< "usage: " << argv[0] << " config_filename log_filename role\n";
		return 0;
	}
	// start system
	bool isWorker = (strcmp(argv[3], "worker") == 0);
	if (!isWorker) {
		Start(0, argc, argv);
		// setup server nodes
		StartServer();
		Finalize(0, true);
		return 0;
	}
	// run worker nodes
	std::thread t0(RunWorker, 0, argc, argv);
	std::thread t1(RunWorker, 1, argc, argv);

	t0.join();
	t1.join();
	return 0;
}
