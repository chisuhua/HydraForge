// src/common/governance/cross_cutting/cross_cutting_config.cpp
// 文件头注释
// 功能描述：Cross-Cutting Config DSL 加载器实现 (ADR-0085 V1)。
//          YAML → JSON 简化解析。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 2
// 最后修改日期：2026-08-28

#include "agenticdsl/pdk/cross_cutting/cross_cutting_config.h"
#include "agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace hydraforge::pdk {

CrossCuttingConfig CrossCuttingConfig::load(const std::string& yaml_path) {
    CrossCuttingConfig config;

    // V1 简化：直接读取文件并解析
    std::ifstream file(yaml_path);
    if (!file.is_open()) {
        throw std::runtime_error("CrossCuttingConfig: cannot open file " + yaml_path);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // V1 简化：逐行解析，识别 ### AgenticDSL /<section> + 后续内容
    // 将 YAML 风格内容转为 JSON 对象
    std::istringstream iss(content);
    std::string line;
    bool in_cross_cutting = false;
    std::string yaml_buffer;

    nlohmann::json parsed;
    parsed["patterns"] = nlohmann::json::array();

    while (std::getline(iss, line)) {
        // 检测 section 标记
        if (line.find("### AgenticDSL") != std::string::npos) {
            in_cross_cutting = (line.find("/cross_cutting") != std::string::npos);
            continue;
        }

        // 跳过代码块围栏标记 (``` 或 ```yaml)
        if (line.find("```") != std::string::npos) {
            continue;
        }

        // 仅收集 /cross_cutting 节的非空内容
        if (in_cross_cutting) {
            if (line.find_first_not_of(" \t\r") != std::string::npos) {
                yaml_buffer += line + "\n";
            }
        }
    }

    // 简化 YAML → JSON：提取 patterns 配置
    // V1: 识别 "- type:" 开头行提取 pattern type
    std::istringstream yiss(yaml_buffer);
    std::string yline;
    nlohmann::json current_pattern;
    bool in_pattern = false;

    while (std::getline(yiss, yline)) {
        // 去除前导空格
        size_t start = yline.find_first_not_of(' ');
        if (start != std::string::npos) {
            yline = yline.substr(start);
        }

        if (yline.find("patterns:") != std::string::npos) {
            continue;
        }

        if (yline.find("- type:") != std::string::npos) {
            if (in_pattern && current_pattern.contains("type")) {
                parsed["patterns"].push_back(current_pattern);
            }
            current_pattern = nlohmann::json::object();
            current_pattern["config"] = nlohmann::json::object();
            size_t pos = yline.find("- type:");
            std::string type_val = yline.substr(pos + 7);
            // 去除引号和空格
            size_t first = type_val.find_first_not_of(" \"'");
            size_t last = type_val.find_last_not_of(" \"'");
            if (first != std::string::npos && last != std::string::npos) {
                type_val = type_val.substr(first, last - first + 1);
            }
            current_pattern["type"] = type_val;
            in_pattern = true;
        } else if (yline.find("decorators:") != std::string::npos && in_pattern) {
            // 简化：提取 decorators 数组
            current_pattern["config"]["decorators"] = nlohmann::json::array();
        } else if (yline.find("- \"") != std::string::npos && in_pattern &&
                   current_pattern["config"].contains("decorators")) {
            size_t pos = yline.find("- \"");
            size_t end = yline.find("\"", pos + 3);
            if (end != std::string::npos) {
                std::string val = yline.substr(pos + 3, end - pos - 3);
                current_pattern["config"]["decorators"].push_back(val);
            }
        } else if (yline.find("hooks:") != std::string::npos && in_pattern) {
            current_pattern["config"]["hooks"] = nlohmann::json::array();
        } else if (yline.find("- target:") != std::string::npos && in_pattern) {
            nlohmann::json hook_item = nlohmann::json::object();
            size_t pos = yline.find("- target:");
            std::string val = yline.substr(pos + 9);
            size_t first = val.find_first_not_of(" '");
            size_t last = val.find_last_not_of(" '");
            if (first != std::string::npos && last != std::string::npos) {
                val = val.substr(first, last - first + 1);
            }
            hook_item["target"] = val;
            current_pattern["config"]["hooks"].push_back(hook_item);
        } else if (yline.find("glob:") != std::string::npos && in_pattern &&
                   current_pattern["config"].contains("hooks") &&
                   !current_pattern["config"]["hooks"].empty()) {
            size_t pos = yline.find("glob:");
            std::string val = yline.substr(pos + 5);
            size_t first = val.find_first_not_of(" '");
            size_t last = val.find_last_not_of(" '");
            if (first != std::string::npos && last != std::string::npos) {
                val = val.substr(first, last - first + 1);
            }
            current_pattern["config"]["hooks"].back()["glob"] = val;
        } else if (yline.find("agents:") != std::string::npos && in_pattern) {
            current_pattern["config"]["agents"] = nlohmann::json::array();
        } else if (yline.find("- name:") != std::string::npos && in_pattern) {
            nlohmann::json agent_item = nlohmann::json::object();
            size_t pos = yline.find("- name:");
            std::string val = yline.substr(pos + 7);
            size_t first = val.find_first_not_of(" '");
            size_t last = val.find_last_not_of(" '");
            if (first != std::string::npos && last != std::string::npos) {
                val = val.substr(first, last - first + 1);
            }
            agent_item["name"] = val;
            current_pattern["config"]["agents"].push_back(agent_item);
        } else if (yline.find("scope:") != std::string::npos && in_pattern &&
                   current_pattern["config"].contains("agents") &&
                   !current_pattern["config"]["agents"].empty()) {
            size_t pos = yline.find("scope:");
            std::string val = yline.substr(pos + 6);
            size_t first = val.find_first_not_of(" '");
            size_t last = val.find_last_not_of(" '");
            if (first != std::string::npos && last != std::string::npos) {
                val = val.substr(first, last - first + 1);
            }
            current_pattern["config"]["agents"].back()["scope"] = val;
        } else if (yline.find("subscriptions:") != std::string::npos && in_pattern) {
            current_pattern["config"]["subscriptions"] = nlohmann::json::array();
        } else if (yline.find("- \"") != std::string::npos && in_pattern &&
                   current_pattern["config"].contains("subscriptions")) {
            size_t pos = yline.find("- \"");
            size_t end = yline.find("\"", pos + 3);
            if (end != std::string::npos) {
                std::string val = yline.substr(pos + 3, end - pos - 3);
                current_pattern["config"]["subscriptions"].push_back(val);
            }
        }
    }
    // 处理最后一个 pattern
    if (in_pattern && current_pattern.contains("type")) {
        parsed["patterns"].push_back(current_pattern);
    }

    config.config_ = parsed;

    // 字段校验
    if (!config.is_valid()) {
        throw std::invalid_argument("CrossCuttingConfig: schema validation failed");
    }

    return config;
}

nlohmann::json CrossCuttingConfig::to_json() const {
    return config_;
}

bool CrossCuttingConfig::is_valid() const {
    // 字段校验: patterns 数组必填
    if (!config_.contains("patterns") || !config_["patterns"].is_array() || config_["patterns"].empty()) {
        return false;
    }

    // 检查每个 pattern 的 type 和 config
    for (const auto& pattern : config_["patterns"]) {
        if (!pattern.contains("type") || !pattern["type"].is_string()) {
            return false;
        }
        if (!pattern.contains("config") || !pattern["config"].is_object()) {
            return false;
        }

        std::string type = pattern["type"].get<std::string>();
        if (type != hydraforge::pdk::cross_cutting_pattern::Decorator &&
            type != hydraforge::pdk::cross_cutting_pattern::Hook &&
            type != hydraforge::pdk::cross_cutting_pattern::Composition &&
            type != hydraforge::pdk::cross_cutting_pattern::Bus) {
            return false;
        }
    }

    return true;
}

} // namespace hydraforge::pdk
