// include/agenticdsl/contract/inmemory_bus.h
// 文件头注释
// 功能描述：IInteractionBus 的内存实现（mutex + queue + 多 subscriber）。
//          MVP 实现：不引入 lock-free；callback 调用严格在锁外完成以防止死锁。
// C2 Day 6-8 (2026-06-27, Sprint 12 P2, ADR-0030 V2):
//   改为 EventBus MPMC 有界队列后端: emit() 仅入队 + notify_one (非同步通知),
//   后台 dispatch 线程从队列取事件, 同步通知 subscribers.
//   解决 ADR-0030 V2 §风险 "bridge 背压" (慢 subscriber 不阻塞 emit)。
// 设计依据：ADR-0019 + plan §13、§14 + ADR-0030 V2 P2。
// 作者：AgenticDSL Phase 0 / Track A / C2 Day 6-8
// 最后修改日期：2026-06-27
#pragma once

#include "agenticdsl/contract/iinteraction_bus.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace agenticdsl {

/**
 * @brief 基于内存的 IInteractionBus 实现（线程安全, EventBus MPMC 后端）
 *
 * P2 关键约束：
 *  - emit() 仅入队 + notify_one (O(1) 摊销), 不直接调用 subscribers
 *  - 后台 dispatch 线程从队列 pop 事件, 同步通知 subscribers
 *  - 慢 subscriber 不阻塞 emit (只阻塞 dispatch 线程)
 *  - try_pop() 仍可直接从队列取事件 (与 dispatch 线程共享 mutex)
 *  - subscribe() 返回的 token 单调递增，作为唯一标识
 *
 * 不引入 lock-free 结构（MVP 阶段复杂度收益不成正比）。
 */
class InMemoryBus : public IInteractionBus {
 public:
  InMemoryBus();
  ~InMemoryBus() override;

  // 禁止拷贝/移动（mutex + queue + thread 的移动语义复杂）
  InMemoryBus(const InMemoryBus&) = delete;
  InMemoryBus& operator=(const InMemoryBus&) = delete;
  InMemoryBus(InMemoryBus&&) = delete;
  InMemoryBus& operator=(InMemoryBus&&) = delete;

  void emit(const std::string& event_type,
            const ToolResult& payload) override;

  void emit(const std::string& event_type,
            const std::string& content) override;

  size_t subscribe(const std::string& event_type,
                   std::function<void(const ToolResult&)> callback) override;

  void unsubscribe(size_t token) override;

  bool try_pop(std::string& event_type, ToolResult& payload);

  // C2 Day 6-8 (P2): 阻塞直到队列清空且所有 in-flight callbacks 完成
  // 测试和需要同步语义的场景使用, 生产环境通常不需要
  void wait_for_drain();

 private:
  void dispatch_loop();

  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::queue<std::pair<std::string, ToolResult>> queue_;

  std::unordered_map<
      std::string,
      std::vector<std::pair<size_t, std::function<void(const ToolResult&)> > >
  > subscribers_;

  size_t next_token_ = 0;

  // C2 Day 6-8: 后台 dispatch 线程 (EventBus MPMC 后端)
  std::thread dispatch_thread_;
  std::atomic<bool> stop_{false};
};

} // namespace agenticdsl