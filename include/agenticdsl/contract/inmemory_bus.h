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
// 最后修改日期：2026-07-26 (Change A: BusEvent queue migration)
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

class InMemoryBus : public IInteractionBus {
 public:
  InMemoryBus();
  ~InMemoryBus() override;

  InMemoryBus(const InMemoryBus&) = delete;
  InMemoryBus& operator=(const InMemoryBus&) = delete;
  InMemoryBus(InMemoryBus&&) = delete;
  InMemoryBus& operator=(InMemoryBus&&) = delete;

  void emit(const BusEvent& event) override;

  void emit(const std::string& event_type,
            const std::string& content) override;

  size_t subscribe(const std::string& event_type,
                   std::function<void(const BusEvent&)> callback) override;

  void unsubscribe(size_t token) override;

  bool try_pop(BusEvent& event);

  void wait_for_drain();

 private:
  void dispatch_loop();

  mutable std::mutex mtx_;
  std::condition_variable cv_;
  std::queue<BusEvent> queue_;

  std::unordered_map<
      std::string,
      std::vector<std::pair<size_t, std::function<void(const BusEvent&)> > >
  > subscribers_;

  size_t next_token_ = 0;
  size_t in_flight_callbacks_ = 0;

  std::atomic<bool> stop_{false};
  std::thread dispatch_thread_;
};

} // namespace agenticdsl