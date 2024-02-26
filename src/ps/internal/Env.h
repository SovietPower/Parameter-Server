/**
 * @file Env.h
 */
#pragma once
#include <string>
#include <cstdlib>
#include <unordered_map>

namespace ps {

// 关闭 getenv 的警告
#if defined(_MSC_VER)
#pragma warning(disable: 4996) // _CRT_SECURE_NO_WARNINGS
#endif

/**
 * @brief 保存配置信息的单例类。
 * 如果未调用 Init 或配置未设置，则从环境变量读取。
 * 与 ps-lite 不同，不使用 shared_ptr，应该不会出现对象被销毁的问题。
 */
class Environment {
 public:
	/**
	 * @brief 初始化配置信息。非线程安全。
	 */
	static void Init(const std::unordered_map<std::string, std::string>& cfg) {
		Environment* env = Get();
		env->cfg_ = cfg;
	}

	/**
	 * @brief 获取指定配置的值。不存在则返回 nullptr。
	 */
	static const char* GetValue(const char* key) {
		auto& cfg = Get()->cfg_;
		auto it = cfg.find(std::string(key));
		if (it == cfg.end()) {
			return std::getenv(key);
		}
		return (it->second).c_str();
	}

	/**
	 * @brief 获取单例对象。没什么用。
	 */
	static Environment* Get() {
		static Environment env;
		return &env;
	}

	~Environment() = default; // 析构必须 public 供 shared_ptr 调用

 private:
	Environment() = default;

	std::unordered_map<std::string, std::string> cfg_;
};

} // namespace ps
