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

    // V1 简化：直接解析为 JSON (假设文件是 JSON 格式)
    try {
        config.config_ = nlohmann::json::parse(content);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::invalid_argument("CrossCuttingConfig: invalid JSON format");
    }

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
    if (!config_.contains("patterns") || !config_["patterns"].is_array()) {
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
