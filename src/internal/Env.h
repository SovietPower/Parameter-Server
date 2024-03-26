/**
 * @file Env.h
 */
#pragma once
#include <string>
#include <cstdlib>
#include <unordered_map>

#include "../base/Log.h"

namespace ps {

// 关闭 getenv 的警告
#if defined(_MSC_VER)
#pragma warning(disable: 4996) // _CRT_SECURE_NO_WARNINGS
#endif

/**
 * @brief 保存配置信息的单例类。
 * 如果未调用 Init 或配置未设置，则从环境变量读取。
 * 与 ps-lite 不同，不使用 shared_ptr，但应该不会出现对象被销毁的问题。
 */
class Environment {
 public:
	/**
	 * @brief 初始化配置信息。非线程安全。
	 */
	static void Init(const std::unordered_map<std::string, std::string>& cfg) {
		Environment* env = GetEnvironment();
		env->cfg_ = cfg;
	}

	/**
	 * @brief 获取指定配置的值。不存在则返回 nullptr。
	 */
	static const char* Get(const char* key) {
		auto& cfg = GetEnvironment()->cfg_;
		auto it = cfg.find(std::string(key));
		if (it == cfg.end()) {
			return std::getenv(key);
		}
		return (it->second).c_str();
	}
	/**
	 * @brief 获取指定配置的值。不存在则返回 default_val。
	 */
	static const char* GetOrDefault(const char* key, const char* default_val) {
		auto ret = Get(key);
		return ret ? ret : default_val;
	}
	/**
	 * @brief 获取指定配置的值。不存在则报错。
	 */
	static const char* GetOrFail(const char* key) {
		auto ret = Get(key);
		CHECK(ret != nullptr) << "Set valid config: " << key << " first!";
		return ret;
	}

	/**
	 * @brief 获取指定整数类型配置的值。不存在则返回 0。
	 */
	static int GetInt(const char* key) {
		return GetIntOrDefault(key, 0);
	}
	/**
	 * @brief 获取指定整数类型配置的值。不存在则返回 default_val。
	 */
	static int GetIntOrDefault(const char* key, int default_val) {
		auto ret = Get(key);
		return ret ? std::atoi(ret) : default_val;
	}
	/**
	 * @brief 获取指定配置的值。不存在则报错。
	 */
	static int GetIntOrFail(const char* key) {
		auto ret = Get(key);
		CHECK(ret != nullptr) << "Set valid config: " << key << " first!";
		return ret ? std::atoi(ret) : 0;
	}

	~Environment() = default; // 析构必须 public 供 shared_ptr 调用

 private:
	Environment() = default;

	/**
	 * @brief 获取单例对象。不直接使用。
	 */
	static Environment* GetEnvironment() {
		static Environment env;
		return &env;
	}

	std::unordered_map<std::string, std::string> cfg_;
};

/**
 * @brief 读取某目录下的 config 文件初始化 Environment。
 */
void ReadLocalConfigToEnv(std::string config_filename);

} // namespace ps
