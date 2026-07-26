// include/agenticdsl/contract/bus_event.h
// 功能描述：BusEvent 公开契约类型（ADR-0002 EventBus 基础设施链 — Change A）。
//          Phase 1 统一事件信封：topic + ToolResult payload + timestamp +
//          causal_time(预留) + priority(预留)。
//          emit/subscribe 一次性迁移到此类型后，后续 EventBus 扩展
//          (glob subscribe、causal clock) 均为增量非破坏变更。
// 设计依据：Oracle 评审 (2026-07-26) + ADR-0002 + ADR-0019。
// 作者：HydraForge Phase 6a / EventBus Chain
// 最后修改日期：2026-07-26
#pragma once

#include "core/types/tool_result.h"

#include <chrono>
#include <cstdint>
#include <string>

namespace agenticdsl {

enum class EventPriority { Critical = 0, Normal = 1, Low = 2 };

struct BusEvent {
    std::string topic;
    ToolResult payload;                               // ADR-0023 标准载荷
    std::chrono::steady_clock::time_point timestamp;  // monotonic clock (not wall time)
    uint64_t causal_time{0};                          // 预留，Change C 填充
    EventPriority priority{EventPriority::Normal};     // 预留，Phase 2 使用
};

}  // namespace agenticdsl