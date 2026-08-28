// include/agenticdsl/pdk/cross_cutting/cross_cutting_config.h
// 文件头注释
// 功能描述：Cross-Cutting Config DSL 加载器 (ADR-0085 V1)。
//          YAML → JSON 简化解析。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 2
// 最后修改日期：2026-08-28

#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace hydraforge::pdk {

class CrossCuttingConfig {
public:
    static CrossCuttingConfig load(const std::string& yaml_path);
    nlohmann::json to_json() const;
    bool is_valid() const;

private:
    nlohmann::json config_;
};

} // namespace hydraforge::pdk
