#pragma once
#include <cmath>
#include <chrono>
#include <cstdlib>

#include "ps/KVApp.h"
#include "internal/Env.h"

#include "./Adam.h"
#include "./LRServer.h"
#include "./DataLoader.h"

namespace lr {

/**
 * @brief 逻辑回归。
 * @tparam UsePS 是否使用 PS 进行分布式训练
 */
template <bool UsePS = true>
class LRWorker {
 public:
	explicit LRWorker(ps::KVWorker<FType>* worker)
			: worker_(worker) {
		if constexpr (UsePS) {
			CHECK(worker != nullptr);
		}

		int num_feature = ps::Environment::GetIntOrFail("NUM_FEATURE");
		weight_.resize(num_feature);
		key_.resize(num_feature);
		for (int i = 0; i < num_feature; ++i) {
			key_[i] = i;
		}

		current_iteration_ = 0;
		total_iteration_ = ps::Environment::GetInt("ITERATION");

		learning_rate_ = std::stof(std::string(ps::Environment::Get("LEARNING_RATE")));
		C_ = std::stof(std::string(ps::Environment::GetOrDefault("C", "1")));

		if constexpr (!UsePS) {
			// 如果不使用 PS，则将 server 的逻辑放到 worker 内
			if (ps::Environment::Get("USE_ADAM") != nullptr) {
				adam_ = new Adam(num_feature, learning_rate_);
			}
			// 获取 weight
			InitWeight(weight_, 0, total_iteration_, current_iteration_);
		}

		std::cout << "new Worker: "
			<< "learning_rate: " << learning_rate_
			<< ", C: " << C_ << std::endl;
	}

	~LRWorker() {
		delete worker_;
	}

	/**
	 * @brief 分布式进行一轮小批量训练（重复拉取梯度、训练、推送梯度）。默认在 Pull, Push 时阻塞。
	 * @param data 训练集数据
	 */
	void Train(DataLoader& data, int batch_size = 100, bool block = true) requires UsePS {
		std::vector<FType> grad(weight_.size());
		while (data.HasNextBatch()) {
			Pull(block);

			Batch batch = data.GetNextBatch(batch_size);
			for (size_t i = 0; i < weight_.size(); ++i) {
				double sum = 0;
				for (size_t j = 0; j < batch.size(); ++j) {
					const Sample& sample = batch[j];
					sum += (Sigmoid(sample.GetAllFeatures()) - sample.GetLabel()) * sample.GetFeature(i);
				}
				grad[i] = 1. * sum / batch.size() + C_ * weight_[i] / batch.size();
				// C：正则化，考虑一部分原模型的影响，避免过拟合
			}

			Push(grad, block, data.HasNextBatch() ? 0 : 1);
		}
		++current_iteration_;
	}
	/**
	 * @brief 本地进行一轮小批量训练（训练、更新梯度）。
	 * @param data 训练集数据
	 */
	void Train(DataLoader& data, int batch_size = 100, bool block = true) requires (!UsePS) {
		std::vector<FType> grad(weight_.size());
		while (data.HasNextBatch()) {
			// train
			Batch batch = data.GetNextBatch(batch_size);
			for (size_t i = 0; i < weight_.size(); ++i) {
				double sum = 0;
				for (size_t j = 0; j < batch.size(); ++j) {
					const Sample& sample = batch[j];
					sum += (Sigmoid(sample.GetAllFeatures()) - sample.GetLabel()) * sample.GetFeature(i);
				}
				grad[i] = 1. * sum / batch.size() + C_ * weight_[i] / batch.size();
			}
			// update
			for (size_t i = 0; i < weight_.size(); ++i) {
				if (adam_) {
					grad[i] = adam_->GetGrad(grad[i], i, current_iteration_);
				}
				weight_[i] -= learning_rate_ * grad[i];
			}
		}
		++current_iteration_;
	}
	/**
	 * @brief 读取数据集进行预测，输出准确率。
	 * @param iteration 当前进行的迭代次数，会显示在输出中
	 */
	void Test(DataLoader& data, int iteration) {
		if constexpr (UsePS) {
			Pull();
		}

		Batch batch = data.GetNextBatch(-1);
		int correct = 0;
		for (size_t i = 0; i < batch.size(); ++i) {
			const Sample& sample = batch[i];
			if (Predict(sample.GetAllFeatures()) == sample.GetLabel()) {
				++correct;
			}
		}
		// get time
		std::chrono::time_point now = std::chrono::system_clock::now();
		std::time_t tmNow = std::chrono::system_clock::to_time_t(now);

		std::ostringstream out;
		out << "TEST " << std::put_time(std::localtime(&tmNow), "%F %T")
			<< "\n\titeration: " << iteration
			<< ", correct: " << correct << "/" << batch.size()
			<< "\n\taccuracy: " << 1. * correct / batch.size()
			<< "\n\tlearning_rate: " << learning_rate_
			<< ", C: " << C_;
		// std::cout << out.str() << std::endl;

		// iteration correct accuracy
		test_result_ << current_iteration_ << '\t' << correct << '\t' << 1. * correct / batch.size() << '\n';
	}

	/**
	 * @brief 将模型参数保存到指定文件。
	 * @param test_result_filename 可选，将测试结果输出到某文件中。
	 */
	void SaveModel(const std::string& filename, const std::string& test_result_filename = "") {
		std::ofstream fout(filename);
		fout << total_iteration_ << '\n'; // 注意 worker 自己输出的迭代次数不包括旧模型的迭代次数
		fout << weight_.size() << '\n';
		for (auto w: weight_) {
			fout << w << ' ';
		}
		fout << '\n';

		if (!test_result_filename.empty()) {
			std::ofstream tr_out(test_result_filename);
			tr_out << test_result_.str();
		}
	}

	/**
	 * @brief 拉取最新的模型参数。默认阻塞。
	 * 返回请求的时间戳。
	 * 可以把 weights_ 改为 SVector 实现零拷贝。
	 */
	int Pull(bool block = true) requires UsePS {
		int ts = worker_->Pull(key_, &weight_);
		if (block) {
			worker_->Wait(ts);
		}
		return ts;
	}
	/**
	 * @brief 向 server 推送梯度。默认阻塞。
	 * 返回请求的时间戳。
	 * 可以把 grad 改为 SVector 实现零拷贝（注意 grad 需要每次创建，以避免在非阻塞的情况下覆盖 grad 底层数组）。
	 * @param cmd 如果为 1，则通知 server 一轮迭代完成
	 */
	int Push(const std::vector<FType>& grad, bool block = true, int cmd = 0) requires UsePS {
		int ts = worker_->Push(key_, grad, {}, cmd);
		if (block) {
			worker_->Wait(ts);
		}
		return ts;
	}

	/**
	 * @brief 预测某个样本的结果：计算 W*X，判断正负（等价于传入 sigmoid，判断是否 > 0.5）。
	 */
	int Predict(const std::vector<FType>& feature) {
		FType ret = 0;
		for (size_t i = 0; i < weight_.size(); ++i) {
			ret += weight_[i] * feature[i];
		}
		return ret > 0;
	}
	/**
	 * @brief 计算 W*X，传入 sigmoid。
	 */
	double Sigmoid(const std::vector<FType>& feature) {
		FType ret = 0;
		for (size_t i = 0; i < weight_.size(); ++i) {
			ret += weight_[i] * feature[i];
		}
		return 1. / (1. + exp(-ret));
	}

	const std::vector<FType>& GetWeight() {
		return weight_;
	}
	ps::KVWorker<FType>* GetKVWorker() requires UsePS {
		return worker_;
	}

	std::string DebugString() {
		std::ostringstream out;
		out << "num_feature: " << weight_.size()
			<< ", learning_rate: " << learning_rate_
			<< ", C: " << C_ << '\n';
		out << "weights: ";
		for (size_t i = 0; i < weight_.size(); ++i) {
			out << weight_[i] << ", ";
		}
		out << '\n';
		return out.str();
	}

 private:
	/* 学习率 */
	float learning_rate_;
	/* 正则化系数 */
	float C_;

	/* 模型参数 */
	std::vector<FType> weight_;
	/* 模型的 key（每个特征的编号） */
	std::vector<ps::Key> key_;

	ps::KVWorker<FType>* worker_;

	/* 当前已进行的迭代次数。用于 lr_normal 时的 Adam */
	int current_iteration_;
	/* 当前模型最终训练的迭代次数。用于最终输出 */
	int total_iteration_;

	/* 将测试结果输出到文件 */
	std::ostringstream test_result_;

	Adam* adam_{nullptr};
};

} // namespace lr
