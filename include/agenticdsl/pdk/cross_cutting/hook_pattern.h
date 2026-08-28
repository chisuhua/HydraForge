// include/agenticdsl/pdk/cross_cutting/hook_pattern.h
// 文件头注释
// 功能描述：Hook Pattern PDK (ADR-0085 V1)。
//          实现工具/Agent/审批 hook 注册。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 0
// 最后修改日期：2026-08-28

#pragma once

#include "agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h"

namespace hydraforge::pdk {

// Hook Pattern: 注册工具/Agent/审批 hook
class HookPattern : public ICrossCuttingPattern {
public:
    const std::string& name() const override;

    void apply(const nlohmann::json& pattern_config,
               CrossCuttingContext& ctx) override;
};

} // namespace hydraforge::pdk
