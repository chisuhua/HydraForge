// include/agenticdsl/pdk/cross_cutting/bus_pattern.h
// 文件头注释
// 功能描述：Bus Pattern PDK (ADR-0085 V1)。
//          实现事件总线订阅。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 0
// 最后修改日期：2026-08-28

#pragma once

#include "agenticdsl/pdk/cross_cutting/icross_cutting_pattern.h"

namespace hydraforge::pdk {

// Bus Pattern: 事件总线订阅
class BusPattern : public ICrossCuttingPattern {
public:
    const std::string& name() const override;

    void apply(const nlohmann::json& pattern_config,
               CrossCuttingContext& ctx) override;
};

} // namespace hydraforge::pdk
