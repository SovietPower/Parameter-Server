#include <cmath>
#include <iostream>
#include "ps/ps.h"
using namespace ps;

const int CustomerCount = 3;

void StartServer() {
	if (!IsServer()) {
		if (!IsScheduler()) {
			std::cout << "Worker enters RunServer!" << std::endl;
		}
		return;
	}
	auto server = new KVServer<float>(0);
	server->SetRequestHandle(KVServerDefaultHandle<float>());
	RegisterExitCallback([server](){ delete server; });
}

void RunWorker(int customer_id, int argc, char** argv) {
	std::cout << "RunWorker: c_id: " << customer_id << std::endl;
	Start(customer_id, argc, argv);
	if (!IsWorker()) {
		std::cout << "Server enters RunWorker!\n";
		return;
	}
	KVWorker<float> kv(0, customer_id);
	// init
	int num = 10000;
	std::vector<Key> keys(num);
	std::vector<float> vals(num);

	int rank = MyRank();
	srand(rank + 7);
	for (int i = 0; i < num; ++i) {
		keys[i] = kMaxKey / num * i + i;
		vals[i] = 5 * (i + customer_id);
	}
	// push 更新50次，则 vals[i] = 50*(i + c_id)
	int repeat = 50;
	std::vector<int> ts;
	for (int i = 0; i < repeat; ++i) {
		ts.push_back(kv.Push(keys, vals));

		// to avoid too frequency push, which leads huge memory usage
		if (i > 10) kv.Wait(ts[ts.size()-10]);
	}
	for (int t : ts) kv.Wait(t);
	PostOffice::Get()->Barrier(customer_id, kWorkerGroup);

	// pull 获取 Server 当前的数据
	// vals[i] = 50 * 5 * (i * CustomerCount + CC * (CC - 1) / 2)
	std::vector<float> rets;
	kv.Wait(kv.Pull(keys, &rets));

	// pushpull 再更新50次，应该等于 100 * ...
	// 但没有 Barrier 所以可能有误差
	std::vector<float> outs;
	for (int i = 0; i < repeat; ++i) {
		kv.Wait(kv.PushPull(keys, vals, &outs));
	}

	PostOffice::Get()->Barrier(customer_id, kWorkerGroup);

	std::vector<float> newData;
	kv.Wait(kv.Pull(keys, &newData));

	uint64_t res{0}, res2{0}, res3{0};
	for (int i = 0; i < num; ++i) {
		auto CC = CustomerCount;
		auto val = repeat * 5 * (i * CC + CC * (CC - 1) / 2);
		res += fabs(rets[i] - val);
		res2 += fabs(outs[i] - val * 2);
		res3 += fabs(newData[i] - val * 2);
	}
	// CHECK_LT(1.0 * res / repeat, 1e-5);
	// CHECK_LT(1.0 * res2 / (2 * repeat), 1e-5);
	std::cout << "got error value: " << res << ", " << res2 << ", " << res3 << std::endl;
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
	std::vector<std::thread> ths;
	for (int i = 0; i < CustomerCount; ++i) {
		ths.push_back(std::thread(RunWorker, i, argc, argv));
	}
	for (auto& th: ths) {
		th.join();
	}

	return 0;
}
