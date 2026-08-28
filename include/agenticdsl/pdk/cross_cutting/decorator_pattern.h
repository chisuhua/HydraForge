// include/agenticdsl/pdk/cross_cutting/decorator_pattern.h
// 文件头注释
// 功能描述：Decorator Pattern PDK (ADR-0085 V1)。
//          实现 ILLMProvider 装饰器链配置。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 0
// 最后修改日期：2026-08-28

#pragma once

#include "agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h"

namespace hydraforge::pdk {

// Decorator Pattern: 配置 ILLMProvider 装饰器链
class DecoratorPattern : public ICrossCuttingPattern {
public:
    const std::string& name() const override;

    void apply(const nlohmann::json& pattern_config,
               CrossCuttingContext& ctx) override;
};

} // namespace hydraforge::pdk
