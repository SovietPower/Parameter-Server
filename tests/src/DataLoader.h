/**
 * @file DataLoader.h
 * @brief 包括：Sample, Batch, DataLoader。
 */
#pragma once
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <charconv>
#include <string_view>

namespace lr {

/* 特征类型 */
using FType = float;

/**
 * @brief 一个样本，包括：特征、标签。特征的长度即为特征数量/维度。
 * 方法均返回 const&，不应也不需要修改。
 */
class Sample {
 public:
	explicit Sample(const std::vector<FType>& feature, int label)
		: feature_(feature), label_(label) {}

	explicit Sample(std::vector<FType>&& feature, int label)
		: feature_(std::move(feature)), label_(label) {}

	~Sample() = default;

	/**
	 * @brief 返回所有特征。
	 */
	const std::vector<FType>& GetAllFeatures() const {
		return feature_;
	}
	/**
	 * @brief 返回指定特征。
	 */
	FType GetFeature(int index) const {
		return feature_[index];
	}
	/**
	 * @brief 返回指定特征。
	 */
	FType operator [](int index) const {
		return feature_[index];
	}
	/**
	 * @brief 返回样本的标签。
	 */
	int GetLabel() const {
		return label_;
	}

	std::string DebugString() const {
		std::string str = std::to_string(label_);
		for (size_t i = 0; i < feature_.size(); ++i) {
			if (feature_[i]) {
				str += " " + std::to_string(i) + ":" + std::to_string(feature_[i]);
			}
		}
		str += "\n";
		return str;
	}

 private:
	/* 特征 */
	std::vector<FType> feature_;
	/* 标签 */
	int label_;
};

/**
 * @brief 一组样本。
 * 是所有样本中的一个切片，以避免拷贝。
 */
class Batch {
 public:
	explicit Batch(size_t begin, size_t end, const std::vector<Sample>& samples)
		: begin_(begin), end_(end), samples_(samples) {}

	~Batch() = default;

	size_t size() const {
		return begin_ < end_ ? end_ - begin_ : samples_.size() - begin_ + end_;
	}

	const Sample& operator [](size_t index) const {
		if (begin_ < end_) {
			return samples_[begin_ + index];
		} else {
			size_t tail = samples_.size() - begin_;
			if (index < tail) {
				return samples_[begin_ + index];
			} else {
				return samples_[index - tail];
			}
		}
	}

	const Sample& at(size_t index) const {
		if (index > size()) {
			throw std::out_of_range(std::to_string(index) + " exceeds size: " + std::to_string(size()));
		}
		return operator [](index);
	}

 private:
	/* 该组样本的起始位置 */
	size_t begin_;
	/* 该组样本的结束位置 */
	size_t end_;
	/**/
	const std::vector<Sample>& samples_;
};

/**
 * @brief 根据指定分割符切分字符串。
 * 使用和返回 string_view，因此需保证参数 s 的生命期不短于返回值。
 */
inline std::vector<std::string_view> Split(std::string_view s, char sep) {
	std::vector<std::string_view> ret;
	size_t last = 0;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == sep) {
			if (last < i) {
				ret.emplace_back(s.data() + last, i - last);
			}
			last = i + 1;
		}
	}
	if (last < s.size()) {
		ret.emplace_back(s.data() + last, s.size() - last);
	}
	return ret;
}

/**
 * @brief 加载指定文件中的样本保存。
 */
class DataLoader {
 public:
	explicit DataLoader(std::string filename, int num_feature) : num_feature_(num_feature) {
		std::ifstream input(filename);
		std::string line, buf;
		while (std::getline(input, line)) {
			std::istringstream in(line);
			in >> buf;
			int label = std::stoi(buf) == 1 ? 1 : 0;

			std::vector<FType> feature(num_feature_, 0);
			while (in >> buf) {
				auto sv = Split(buf, ':');
				int index{1}; FType value{0};
				if (auto ret = std::from_chars(sv[0].data(), sv[0].data() + sv[0].size(), index);
					ret.ec == std::errc::invalid_argument) {
					std::cout << "Error: Could not convert " << sv[0] << " to int!";
				}
				if (auto ret = std::from_chars(sv[1].data(), sv[1].data() + sv[1].size(), value);
					ret.ec == std::errc::invalid_argument) {
					std::cout << "Error: Could not convert " << sv[1] << " to FType!";
				}
				feature[index - 1] = value;
			}
			samples_.emplace_back(std::move(feature), label);
		}
	}

	~DataLoader() = default;

	/**
	 * @brief 获取下一个 batch。
	 * @param batch_size 如果为 -1，则获取整个数据集
	 */
	Batch GetNextBatch(int batch_size = 100) {
		if (batch_size < 0) {
			batch_size = samples_.size();
		}
		size_t begin = next_sample_;
		size_t end = next_sample_ + batch_size;
		if (end > samples_.size()) {
			end -= samples_.size();
			wrap_around_ = true;
		}
		// std::cout << "DEBUG: GetNextBatch: batch_size: " << batch_size
		// 	<< ", total samples: " << samples_.size()
		// 	<<  ", got range: " << begin << "~" << end << '\n';
		next_sample_ = end;
		return Batch(begin, end, samples_);
	}

	/**
	 * @brief 返回是否还有下一个 batch。
	 */
	bool HasNextBatch() const {
		return !wrap_around_;
	}

	/**
	 * @brief 重置为未读取状态。
	 */
	void Reset() {
		next_sample_ = 0;
		wrap_around_ = false;
	}

 private:
	/* 特征数量 */
	int num_feature_;
	/* 样本 */
	std::vector<Sample> samples_;

	/* 下一个要取的样本 */
	size_t next_sample_{0};
	/* 是否已经取完一轮样本，绕回到原点 */
	bool wrap_around_{false};
};

} // namespace lr