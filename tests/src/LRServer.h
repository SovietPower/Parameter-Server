#pragma once
#include <functional>

#include "ps/ps.h"
#include "ps/KVApp.h"
#include "internal/Env.h"

#include "./Adam.h"
#include "./DataLoader.h"

namespace lr {

/**
 * @brief 使用随机数或已有模型初始化权重。
*/
inline void InitWeight(std::vector<FType>& weight, int seed, int& total_iteration, int& current_iteration) {
	auto ptr = ps::Environment::Get("USE_OLD_MODEL");
	if (ptr == nullptr) {
		std::srand(seed);
		std::cout << "Generating random model with seed: " << seed << std::endl;

		for (size_t i = 0; i < weight.size(); ++i) {
			weight[i] = static_cast<FType>(rand()) / static_cast<FType>(RAND_MAX) - 0.5; // (-0.5, 0.5)
		}
	} else {
		std::string model = std::string(ps::Environment::GetOrFail("DATA_DIR")) + "/model/" + std::string(ptr);
		std::cout << "Using old model: " << model << std::endl;

		std::ifstream input(model);
		CHECK(input.is_open()) << "Old model doesn't exist in path: " << model;

		int iter; input >> iter;
		total_iteration += iter; // 累积之前的迭代次数
		current_iteration = iter;

		size_t num_feature; input >> num_feature;
		CHECK_EQ(num_feature, weight.size()) << "The dimension of old model [" << model << "] doesn't match";

		for (size_t i = 0; i < num_feature; ++i) {
			input >> weight[i];
		}
	}
}

class LRServer {
 public:
	explicit LRServer(int seed = 0): seed_(seed) {
		using namespace std::placeholders;
		ps_server_ = new ps::KVServer<FType>(0);
		ps_server_->SetRequestHandle(
			std::bind(&LRServer::RequestHandle, this, _1, _2, _3));

		// 其实配置应该在 main 中读取，然后通过构造传进来。现在是什么都不传，按需自行读取
		total_iteration_ = ps::Environment::GetInt("ITERATION");
		current_iteration_ = 0;

		sync_mode_ = ps::Environment::GetInt("SYNC_MODE");
		learning_rate_ = std::stof(std::string(ps::Environment::GetOrFail("LEARNING_RATE")));

		int num_feature = ps::Environment::GetIntOrFail("NUM_FEATURE");
		weight_.resize(num_feature);

		if (ps::Environment::Get("USE_ADAM") != nullptr) {
			adam_ = new Adam(num_feature, learning_rate_);
		}

		InitWeight(weight_, seed_, total_iteration_, current_iteration_);

		std::string mode = sync_mode_ == 0 ? "sync" : "async";
		std::cout << "new Server: mode: " << mode
			<< ", learning_rate: " << learning_rate_
			<< ", seed: " << seed_ << std::endl;
	}

	~LRServer() {
		delete ps_server_;
		delete adam_;

		// 输出模型参数
		auto ptr = ps::Environment::Get("DATA_DIR");
		SaveModel(std::string(ptr) + "/model/lr_ps");
	}

	/**
	 * @brief 将模型参数保存到指定文件。
	 */
	void SaveModel(const std::string& filename) {
		std::ofstream fout(filename);
		fout << total_iteration_ << '\n';
		fout << weight_.size() << '\n';
		for (auto w: weight_) {
			fout << w << ' ';
		}
		fout << '\n';
	}

	const std::vector<FType>& GetWeight() {
		return weight_;
	}

 private:
	void RequestHandle(const ps::KVMeta& req_meta,
					const ps::KVPairs<FType>& req_data,
					ps::KVServer<FType>* server) {
		// Customer 每次仅取出一个 handle 执行，所以线程安全
		size_t n = req_data.keys.size();
		if (req_meta.push) {
			CHECK_EQ(n, req_data.vals.size());
			CHECK(!weight_.empty()) << "Weights haven't been inited!";
			// if (weight_.empty()) {
			// 	// 第一次 push 进行参数初始化
			// 	std::cout << "Initialize weight." << std::endl;
			// 	weight_.resize(n);
			// 	for (size_t i = 0; i < n; ++i) {
			// 		weight_[i] = req_data.vals[i];
			// 	}
			// 	server->Response(req_meta);
			// }
			if (sync_mode_ == 0) {
				// 同步更新梯度：将 worker 推送的梯度缓存到 merge_buf_
				// 在所有 worker 均完成推送后，更新梯度，再通知 worker 请求完成
				if (merge_buf_.vals.empty()) {
					merge_buf_.vals.resize(n, 0);
				}
				// 汇总梯度再更新以减少计算量（不过 LR 计算少、worker 少，影响不大）
				for (size_t i = 0; i < n; ++i) {
					merge_buf_.vals[i] += req_data.vals[i];
				}

				merge_buf_.request.push_back(req_meta);
				if (merge_buf_.request.size() == static_cast<size_t>(ps::NumWorkers())) {
					// 所有 worker 均推送完成
					for (size_t i = 0; i < n; ++i) {
						// 梯度下降
						double grad = learning_rate_ * merge_buf_.vals[i]; // / merge_buf_.request.size()
						if (adam_) {
							grad = adam_->GetGrad(grad, i, current_iteration_);
						}
						weight_[i] -= grad;
					}
					for (const auto& req : merge_buf_.request) {
						server->Response(req);
					}
					merge_buf_.request.clear();
					merge_buf_.vals.clear();
				}
			} else {
				// 异步更新梯度：每次收到 worker 推送，就进行梯度下降，然后通知 worker 请求完成
				for (size_t i = 0; i < n; ++i) {
					double grad = learning_rate_ * req_data.vals[i];
					if (adam_) {
						grad = adam_->GetGrad(grad, i, current_iteration_);
					}
					weight_[i] -= grad;
				}
				server->Response(req_meta);
			}
			// cmd = 1 代表一轮迭代结束
			// 仅在 0 号 worker 发送 cmd 1 时更新迭代次数
			if (req_meta.cmd == 1 && req_meta.sender == ps::PostOffice::WorkerRankToID(0)) {
				++current_iteration_;
			}
		}
		if (req_meta.pull) {
			CHECK(!weight_.empty()) << "weight hasn't been init";

			ps::KVPairs<FType> res;
			res.keys = req_data.keys;
			res.vals.resize(n);
			for (size_t i = 0; i < n; ++i) {
				res.vals[i] = weight_[i]; // 需要拷贝一份，不能零拷贝，因为还会更新
			}
			server->Response(req_meta, res);
		}
	}
	/* 同步模式。0：同步；1：异步 */
	int sync_mode_;
	/* 学习率 */
	float learning_rate_;

	struct Buffer {
		std::vector<ps::KVMeta> request;
		std::vector<FType> vals;
	};
	/* 用于同步模式下，汇总合并 worker 的梯度 */
	Buffer merge_buf_;

	/* 模型参数 */
	std::vector<FType> weight_;

	ps::KVServer<FType>* ps_server_;

	/* 生成初始模型的种子 */
	int seed_;
	/* 当前已进行的迭代次数。用于 Adam */
	int current_iteration_;
	/* 当前模型最终训练的迭代次数。用于最终输出 */
	int total_iteration_;

	Adam* adam_{nullptr};
};

} // namespace lr