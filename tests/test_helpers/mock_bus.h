// tests/test_helpers/mock_bus.h
// 功能描述：Canonical MockBus fixture — 统一 9 处重复 MockBus 实现。
//          兼容：
//            - #1 test_skill_interpreter.cpp   (string emit only)
//            - #2 test_budget_agent_hooks.cpp   (topics vector)
//            - #3 test_context_compactor.cpp    (full BusEvent)
//            - #4 test_escalation_triggers.cpp  (MockBusForEscalation: topics vector)
//            - #5 test_tool_coordinator.cpp     (emit_log: topics vector)
//            - #6 test_tool_coordinator_hooks.cpp (emit_log + payloads via events[i].payload)
//            - #7 test_session_persistence.cpp  (events pair<topic,meta> + subscribers)
//            - #8 test_e2e_mock.cpp             (events pair + subscribers)
//            - #9 test_budget_alert.cpp         (EventRecord via events[i].payload)
// 设计依据：openspec/changes/mock-bus-canonical-extract (P12) + ADR-0019 IInteractionBus
// 作者：P12 mock-bus-canonical-extract change
// 最后修改日期：2026-08-20
#pragma once

#include "agenticdsl/contract/bus_event.h"
#include "agenticdsl/contract/iinteraction_bus.h"

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace agenticdsl {
namespace test {

/**
 * @brief Canonical MockBus fixture for IInteractionBus in tests.
 *
 * 提供三种事件访问方式（同时记录，互不冲突）：
 * - events[i]: 完整 BusEvent（含 payload.data / payload.meta / payload.ok）
 * - topics[i]: 仅 topic 字符串（按 emit 顺序）
 *
 * 完整订阅支持（兼容 #7/#8/#9）：subscribe 注册回调，emit 同步通知同 topic 订阅者。
 * unsubscribe 为 noop（与 pdk_chat_demo 现有 mock 一致，不做精确 token 追踪）。
 *
 * Helper API：
 * - count(topic): 事件计数（按 topic 精确匹配）
 * - last(topic): 最近一个 topic 匹配事件指针；找不到返回 nullptr
 * - clear(): 清空 events/topics；保留 subscribers（与 proposal §关键场景一致）
 */
class MockBus : public IInteractionBus {
 public:
  // 公开存储（兼容 9 处现有访问模式）
  std::vector<BusEvent> events;
  std::vector<std::string> topics;

  // --- IInteractionBus 接口实现 ---

  /// emit(BusEvent) 主路径：存储 + 通知同 topic subscribers + 通配符 "*"
  void emit(const BusEvent& event) override {
    events.push_back(event);
    topics.push_back(event.topic);
    auto it = subscribers_.find(event.topic);
    if (it != subscribers_.end()) {
      for (auto& cb : it->second) {
        cb(event);
      }
    }
    auto wildcard_it = subscribers_.find("*");
    if (wildcard_it != subscribers_.end()) {
      for (auto& cb : wildcard_it->second) {
        cb(event);
      }
    }
  }

  /// emit(string, string) 重载：包装为 BusEvent + 委托给主路径
  /// 兼容 #1 (test_skill_interpreter.cpp) 的 string_emits 模式
  void emit(const std::string& event_type,
            const std::string& content) override {
    BusEvent event;
    event.topic = event_type;
    event.payload.ok = true;
    event.payload.meta = nlohmann::json{{"content", content}};
    emit(event);
  }

  /// subscribe: 注册精确匹配回调，返回递增 token (1, 2, 3, ...)
  size_t subscribe(
      const std::string& event_type,
      std::function<void(const BusEvent&)> callback) override {
    subscribers_[event_type].push_back(std::move(callback));
    return next_token_++;
  }

  /// unsubscribe: noop（与 pdk_chat_demo 现有 mock 行为一致）
  void unsubscribe(size_t /*token*/) override {
    // 不做精确 token 追踪 — 同 pdk_chat_demo test_budget_alert.cpp:53
  }

  // --- Helper API (proposal §关键场景验收) ---

  /// count(topic) — 统计 topic 精确匹配事件数
  std::size_t count(const std::string& topic) const {
    std::size_t n = 0;
    for (const auto& e : events) {
      if (e.topic == topic) ++n;
    }
    return n;
  }

  /// last(topic) — 返回最后一个 topic 匹配事件指针；无匹配返回 nullptr
  const BusEvent* last(const std::string& topic) const {
    for (auto it = events.rbegin(); it != events.rend(); ++it) {
      if (it->topic == topic) return &(*it);
    }
    return nullptr;
  }

  /// clear() — 清空 events 和 topics；保留 subscribers
  void clear() {
    events.clear();
    topics.clear();
    // subscribers_ 保留：clear 不取消订阅（proposal §关键场景："events_ 清空 + 所有 subscribe handler 保留"）
  }

 private:
  std::size_t next_token_ = 1;  // token 从 1 开始（避开 0）
  std::unordered_map<std::string,
                     std::vector<std::function<void(const BusEvent&)>>
                     > subscribers_;
};

}  // namespace test
}  // namespace agenticdsl
