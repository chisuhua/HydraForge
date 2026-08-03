// include/agenticdsl/contract/event_builder.h
// 功能描述：统一 BusEvent 构造方言 (ADR-0068)
//          args 放 schema 必填业务字段, meta 放 trace_id/session_id/debug 上下文
//          timestamp 由 build() 自动填充 steady_clock::now()
// 设计依据：openspec/changes/adr-0068-event-emission-contract/design.md Decision 1
// 作者：HydraForge ADR-0068
// 最后修改日期：2026-08-03
#pragma once

#include "agenticdsl/contract/bus_event.h"
#include "core/types/tool_result.h"
#include <chrono>
#include <string>
#include <utility>

namespace agenticdsl {

// 链式构造器，统一三种 BusEvent 构造方言：
// 1) 工具事件：EventBuilder("tool.x").args({...}).meta({...}).build()
// 2) 审计事件：EventBuilder("audit.x").meta({...}).build()
// 3) 生命周期事件：EventBuilder("lifecycle.x").args({...}).build()
class EventBuilder {
 public:
  explicit EventBuilder(std::string topic)
      : topic_(std::move(topic)),
        payload_(ToolResult::success(nlohmann::json::object(),
                                      nlohmann::json::object())) {}

  EventBuilder& args(nlohmann::json args) {
    payload_.data = std::move(args);
    return *this;
  }

  EventBuilder& meta(nlohmann::json meta) {
    payload_.meta = std::move(meta);
    return *this;
  }

  BusEvent build() const {
    return BusEvent{topic_, payload_, std::chrono::steady_clock::now()};
  }

 private:
  std::string topic_;
  ToolResult payload_;
};

}  // namespace agenticdsl
