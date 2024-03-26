#include "./Env.h"

#include <fstream>

#include "utility/JSONParser.hpp"

namespace ps {

/**
 * @brief 读取文件内容并转为 string
 */
std::string readFileToString(const std::string& filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		LOG(ERROR) << "Unable to open config file: " << filename
			<< ". using environment vars.";
		return "";
	}
	std::string content;
	std::string line;
	while (std::getline(file, line)) {
		content += line + '\n';
	}
	file.close();
	return content;
}

void ReadLocalConfigToEnv(std::string config_name) {
	if (config_name.length() > 5) {
		std::string suffix = config_name.substr(config_name.length() - 5);
		if (suffix != ".json") {
			config_name += ".json";
		}
	}
	std::string content = readFileToString(config_name);
	if (content.empty()) {
		return;
	}
	json::JSON json = json::JSON::Load(content);
	std::unordered_map<std::string, std::string> cfg;

	auto AddKey = [&json, &cfg](const std::string& key, bool isEssential = true) {
		if (cfg.find(key) != cfg.end()) {
			return;
		}
		if (!json.hasKey(key)) {
			if (isEssential) {
				CHECK(json.hasKey(key)) << "config.json should contain key " << key;
			}
			return;
		}
		switch (json[key].JSONType()) {
			case json::JSON::Class::String:
				cfg[key] = json[key].ToString();
				break;
			case json::JSON::Class::Integral:
				cfg[key] = std::to_string(json[key].ToInt());
				break;
			case json::JSON::Class::Floating:
				cfg[key] = std::to_string(json[key].ToFloat());
				break;
			default:
				LOG(ERROR) << "Unsupported config type: " << static_cast<int>(json[key].JSONType());
		}
		// using ps::PostOffice;
		// PS_LOG_DEBUG << "Set config: " << key << ": " << cfg[key];
	};

	AddKey("PS_NUM_WORKER");
	AddKey("PS_NUM_SERVER");
	AddKey("PS_ROLE");
	AddKey("PS_SCHEDULER_URI");
	AddKey("PS_SCHEDULER_PORT");

	// AddKey("PS_VERBOSE", false);
	// traversal config
	const auto& map = json.ObjectRange();
	for (const auto& [key, json]: map) {
		AddKey(key);
	}

	ps::Environment::Init(cfg);
}

} // namespace ps