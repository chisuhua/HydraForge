// include/agenticdsl/contract/iinteraction_bus.h
// 文件头注释
// 功能描述：交互总线抽象接口（ADR-0019）。
//          Phase 1 异步交互层契约：emit/subscribe/unsubscribe + 非阻塞 try_pop。
//          MVP 实现由 InMemoryBus（include/agenticdsl/contract/inmemory_bus.h）提供。
// 设计依据：ADR-0019（IInteractionBus 设计）+ plan §12。
// 作者：AgenticDSL Phase 0 / Track A
// 最后修改日期：2026-07-26 (Change A: BusEvent migration)
#pragma once

#include "agenticdsl/contract/bus_event.h"

#include <cstddef>
#include <functional>
#include <string>

namespace agenticdsl {

/**
 * @brief 交互总线抽象接口 (BusEvent unified contract)
 *
 * Change A (Phase 6a): emit/subscribe 一次性迁移到 BusEvent，
 * 后续所有 EventBus 扩展 (glob subscribe, causal clock) 均为增量非破坏。
 */
class IInteractionBus {
 public:
  virtual ~IInteractionBus() = default;

  /**
   * @brief 发射 BusEvent 事件
   * @param event 事件信封 (topic + ToolResult payload + timestamp + ...)
   */
  virtual void emit(const BusEvent& event) = 0;

  /**
   * @brief 发射字符串事件 (向后兼容入口)
   * @param event_type 事件类型
   * @param content    字符串载荷
   *
   * 实现内部将 std::string 包装为 ToolResult 信封再构造 BusEvent
   */
  virtual void emit(const std::string& event_type,
                    const std::string& content) = 0;

  /**
   * @brief 订阅事件
   * @param event_type 事件类型
   * @param callback   事件回调 void(const BusEvent&)
   * @return 订阅 token（用于 unsubscribe）
   */
  virtual size_t subscribe(
      const std::string& event_type,
      std::function<void(const BusEvent&)> callback) = 0;

  /**
   * @brief 取消订阅
   * @param token subscribe() 返回的 token
   */
  virtual void unsubscribe(size_t token) = 0;
};

} // namespace agenticdsl