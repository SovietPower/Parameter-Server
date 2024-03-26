#pragma once
#include <cmath>
#include <vector>

namespace lr {

/**
 * @brief Adam 优化。
 * 在梯度下降过程中使用一阶矩估计和二阶矩估计来动态调整每个参数的学习率。
 */
class Adam {
 public:
	explicit Adam(int num_feature, double learning_rate = 0.01, double beta1 = 0.9, double beta2 = 0.999,
	double epsilon = 1e-8): learning_rate(learning_rate), beta1(beta1), beta2(beta2), epsilon(epsilon)
	{
		m.resize(num_feature, 0);
		v.resize(num_feature, 0);
	}

	~Adam() = default;

	/**
	 * @brief 获取优化后的梯度。
	 * @param gradient 原梯度值
	 * @param index 当前特征的下标
	 * @param iteration 迭代次数
	 */
	double GetGrad(double gradient, int index, int iteration) {
		m[index] = beta1 * m[index] + (1 - beta1) * gradient;
		v[index] = beta2 * v[index] + (1 - beta2) * gradient * gradient;
		double m_hat = m[index] / (1 - std::pow(beta1,iteration + 1));
		double v_hat = v[index] / (1 - std::pow(beta2,iteration + 1));
		return learning_rate * m_hat / (std::sqrt(v_hat) + epsilon);
	}

 private:
	double learning_rate; // alpha
	double beta1;
	double beta2;
	double epsilon;
	// int iteration; // 不在这里统计了

	std::vector<double> m; // 一阶矩估计的累积变量
	std::vector<double> v; // 二阶矩估计的累积变量
};

} // namespace lr