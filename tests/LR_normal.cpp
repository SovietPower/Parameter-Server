#include "internal/Env.h"

#include "./src/LRWorker.h"

void Run() {
	// get config
	std::string data_dir = ps::Environment::GetOrFail("DATA_DIR");
	int num_feature = ps::Environment::GetIntOrFail("NUM_FEATURE");

	int iteration = ps::Environment::GetInt("ITERATION");
	int batch_size = ps::Environment::GetIntOrDefault("BATCH_SIZE", -1);
	int test_period = ps::Environment::GetInt("TEST_PERIOD");

	// setup lr worker
	lr::LRWorker<false> lr_worker(nullptr);

	lr::DataLoader train_data(data_dir + "/train/full", num_feature);
	lr::DataLoader test_data(data_dir + "/test/full", num_feature);

	auto start_tm = std::chrono::system_clock::now();
	for (int i = 1; i <= iteration; ++i) {
		lr_worker.Train(train_data, batch_size);
		train_data.Reset();

		if (i == 1 || (test_period > 0 && i % test_period == 0 && i < iteration)) {
			lr_worker.Test(test_data, i);
			test_data.Reset();
		}
	}
	auto end_tm = std::chrono::system_clock::now();

	lr_worker.Test(test_data, iteration);
	lr_worker.SaveModel(data_dir + "/model/lr_normal", data_dir + "/model/lr_normal_test");

	{
		std::time_t tmNow = std::chrono::system_clock::to_time_t(end_tm);
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_tm - start_tm);

		std::ostringstream out;
		out << "LR_normal finished training at "  << std::put_time(std::localtime(&tmNow), "%F %T")
			<< "\n\ttime: " << ms
			<< ", iteration: " << iteration
			<< ", batch_size: " << batch_size;
		std::cout << out.str() << std::endl;
	}
}

int main(int argc, char** argv) {
	// 初始化配置
	ps::ReadLocalConfigToEnv("./log/config_H.json");

	Run();
}