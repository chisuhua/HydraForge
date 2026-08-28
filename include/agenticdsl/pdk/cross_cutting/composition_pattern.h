// include/agenticdsl/pdk/cross_cutting/composition_pattern.h
// 文件头注释
// 功能描述：Composition Pattern PDK (ADR-0085 V1)。
//          实现 Agent 注册和 hook 注入。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 0
// 最后修改日期：2026-08-28

#pragma once

#include "agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h"

namespace hydraforge::pdk {

// Composition Pattern: 注册 Agent 并注入 hook
class CompositionPattern : public ICrossCuttingPattern {
public:
    const std::string& name() const override;

    void apply(const nlohmann::json& pattern_config,
               CrossCuttingContext& ctx) override;
};

} // namespace hydraforge::pdk
