// src/common/governance/cross_cutting/bus_pattern.cpp
// 文件头注释
// 功能描述：Bus Pattern 实现 (ADR-0085 V1)。
//          事件总线订阅。
// 设计依据：ADR-0085 Cross-Cutting Pattern PDK v1.2
// 作者：AgenticDSL Phase 1
// 最后修改日期：2026-08-28

#include "agenticdsl/pdk/cross_cutting/bus_pattern.h"
#include "agenticdsl/contract/iinteraction_bus.h"

#include <stdexcept>

namespace hydraforge::pdk {

const std::string& BusPattern::name() const {
    static const std::string name = cross_cutting_pattern::Bus;
    return name;
}

void BusPattern::apply(const nlohmann::json& pattern_config,
                       CrossCuttingContext& ctx) {
    // 读取 subscriptions 数组
    if (!pattern_config.contains("subscriptions") || !pattern_config["subscriptions"].is_array()) {
        throw std::invalid_argument("BusPattern: 'subscriptions' array required");
    }

    auto& subscriptions = pattern_config["subscriptions"];
    for (const auto& topic : subscriptions) {
        if (!topic.is_string()) {
            throw std::invalid_argument("BusPattern: topic must be string");
        }

        std::string topic_str = topic.get<std::string>();

        // V1 简化：订阅空回调
        ctx.bus->subscribe(topic_str, [](const agenticdsl::BusEvent&) {
            // V1: 仅记录计数，不执行实际操作
        });
    }
}

} // namespace hydraforge::pdk
