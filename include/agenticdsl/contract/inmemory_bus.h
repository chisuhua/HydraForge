// include/agenticdsl/contract/inmemory_bus.h
// 文件头注释
// 功能描述：IInteractionBus 的内存实现（mutex + queue + 多 subscriber）。
//          MVP 实现：不引入 lock-free；callback 调用严格在锁外完成以防止死锁。
// 设计依据：ADR-0019 + plan §13、§14。
// 作者：AgenticDSL Phase 0 / Track A
// 最后修改日期：2026-06-08
#pragma once

#include "agenticdsl/contract/iinteraction_bus.h"

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace agenticdsl {

/**
 * @brief 基于内存的 IInteractionBus 实现（线程安全）
 *
 * 关键约束：
 *  - emit() 在锁内入队 + 复制 callback 列表；callback 必须在锁外调用（C++ Core Guidelines CP.22）
 *  - subscribe() 返回的 token 单调递增，作为唯一标识
 *  - try_pop() 不阻塞；队列空时返回 false
 *
 * 不引入 lock-free 结构（MVP 阶段复杂度收益不成正比）。
 */
class InMemoryBus : public IInteractionBus {
 public:
  InMemoryBus() = default;
  ~InMemoryBus() override = default;

  // 禁止拷贝/移动（MVP：避免 mutex + queue 的移动语义复杂度）
  InMemoryBus(const InMemoryBus&) = delete;
  InMemoryBus& operator=(const InMemoryBus&) = delete;
  InMemoryBus(InMemoryBus&&) = delete;
  InMemoryBus& operator=(InMemoryBus&&) = delete;

  /**
   * @brief 实现 IInteractionBus::emit
   * 1) 加锁 → 入队
   * 2) 复制 subscriber 列表（锁内）
   * 3) 解锁
   * 4) 在锁外依次调用每个 callback（防死锁）
   */
  void emit(const std::string& event_type,
            const ToolResult& payload) override;

  /**
   * @brief 实现 IInteractionBus::subscribe
   * 加锁 → 分配 token → 存储 (token, callback) → 返回 token
   */
  size_t subscribe(const std::string& event_type,
                   std::function<void(const ToolResult&)> callback) override;

  /**
   * @brief 实现 IInteractionBus::unsubscribe
   * 加锁 → 遍历 subscribers_ → 移除匹配 token 的项
   */
  void unsubscribe(size_t token) override;

  /**
   * @brief 非阻塞取队首事件
   * @param event_type [out] 队首事件类型
   * @param payload    [out] 队首事件载荷
   * @return 成功弹出返回 true；空队列返回 false
   */
  bool try_pop(std::string& event_type, ToolResult& payload);

 private:
  // 互斥锁（mutable 以支持 const 成员）
  mutable std::mutex mtx_;
  std::condition_variable cv_;

  // 事件队列
  std::queue<std::pair<std::string, ToolResult>> queue_;

  // 订阅表：event_type -> [(token, callback)]
  std::unordered_map<
      std::string,
      std::vector<std::pair<size_t, std::function<void(const ToolResult&)>>>
  > subscribers_;

  // 单调递增 token
  size_t next_token_ = 0;
};

} // namespace agenticdsl