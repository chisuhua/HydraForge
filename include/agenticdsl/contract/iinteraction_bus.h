// include/agenticdsl/contract/iinteraction_bus.h
// 文件头注释
// 功能描述：交互总线抽象接口（ADR-0019）。
//          Phase 1 异步交互层契约：emit/subscribe/unsubscribe + 非阻塞 try_pop。
//          MVP 实现由 InMemoryBus（include/agenticdsl/contract/inmemory_bus.h）提供。
// 设计依据：ADR-0019（IInteractionBus 设计）+ plan §12。
// 作者：AgenticDSL Phase 0 / Track A
// 最后修改日期：2026-06-08
#pragma once

#include "core/types/tool_result.h"

#include <cstddef>
#include <functional>
#include <string>

namespace agenticdsl {

/**
 * @brief 交互总线抽象接口
 *
 * 用于 Cognitive / Executor / TraceExporter 等模块之间的轻量解耦事件传递。
 *
 * 关键不变量：
 *  - subscribe() 返回的 token 用于后续 unsubscribe()
 *  - emit() 同步通知所有 subscribers；实现必须在锁外调用回调（防止死锁）
 *  - try_pop() 用于消费队列中的事件（不依赖订阅）
 *
 * 线程安全：由实现决定（InMemoryBus 保证多线程安全）。
 */
class IInteractionBus {
 public:
  virtual ~IInteractionBus() = default;

  /**
   * @brief 发射事件
   * @param event_type 事件类型（如 "tool_call_started"）
   * @param payload    事件载荷（ToolResult）
   *
   * 实现应：
   *  1. 入队到内部队列
   *  2. 同步通知所有 subscribers（复制 callback 后在锁外调用）
   */
  virtual void emit(const std::string& event_type,
                    const ToolResult& payload) = 0;

  /**
   * @brief 订阅事件
   * @param event_type 事件类型
   * @param callback   事件回调
   * @return 订阅 token（用于 unsubscribe）
   */
  virtual size_t subscribe(
      const std::string& event_type,
      std::function<void(const ToolResult&)> callback) = 0;

  /**
   * @brief 取消订阅
   * @param token subscribe() 返回的 token
   */
  virtual void unsubscribe(size_t token) = 0;
};

} // namespace agenticdsl