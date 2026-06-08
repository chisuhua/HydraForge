// src/common/contract/inmemory_bus.cpp
// 文件头注释
// 功能描述：InMemoryBus 实现（基于 mutex + queue + 多 subscriber 列表）
//          关键约束：callback 调用严格在锁外完成，防止死锁
// 设计依据：ADR-0019 + plan §14
// 作者：AgenticDSL Phase 0 / Track A
// 最后修改日期：2026-06-08

#include "agenticdsl/contract/inmemory_bus.h"

#include <algorithm>
#include <utility>

namespace agenticdsl {

// =====================================================================
// emit: 入队 + 通知所有 subscribers
// =====================================================================
void InMemoryBus::emit(const std::string& event_type,
                       const ToolResult& payload) {
  // Step 1: 加锁 → 入队 + 复制 subscriber 列表
  std::vector<std::function<void(const ToolResult&)>> callbacks;
  {
    std::lock_guard<std::mutex> lock(mtx_);
    queue_.push({event_type, payload});
    cv_.notify_one();  // 唤醒阻塞在 try_pop 上的消费者
    auto it = subscribers_.find(event_type);
    if (it != subscribers_.end()) {
      callbacks.reserve(it->second.size());
      for (const auto& [token, cb] : it->second) {
        (void)token;
        callbacks.push_back(cb);
      }
    }
  }  // 锁已释放

  // Step 2: 锁外调用 callbacks（防止 callback 中递归 emit 导致死锁）
  for (auto& cb : callbacks) {
    cb(payload);
  }
}

// =====================================================================
// subscribe: 分配 token + 存储 callback
// =====================================================================
size_t InMemoryBus::subscribe(const std::string& event_type,
                              std::function<void(const ToolResult&)> callback) {
  std::lock_guard<std::mutex> lock(mtx_);
  size_t token = next_token_++;
  subscribers_[event_type].push_back({token, std::move(callback)});
  return token;
}

// =====================================================================
// unsubscribe: 移除匹配 token 的订阅
// =====================================================================
void InMemoryBus::unsubscribe(size_t token) {
  std::lock_guard<std::mutex> lock(mtx_);
  for (auto& [event_type, vec] : subscribers_) {
    (void)event_type;
    auto it = std::remove_if(vec.begin(), vec.end(),
                              [token](const auto& pair) {
                                return pair.first == token;
                              });
    if (it != vec.end()) {
      vec.erase(it, vec.end());
      return;  // token 唯一，找到即可返回
    }
  }
  // token 不存在则静默忽略（符合预期行为）
}

// =====================================================================
// try_pop: 非阻塞取队首事件
// =====================================================================
bool InMemoryBus::try_pop(std::string& event_type, ToolResult& payload) {
  std::lock_guard<std::mutex> lock(mtx_);
  if (queue_.empty()) {
    return false;
  }
  auto front = std::move(queue_.front());
  queue_.pop();
  event_type = std::move(front.first);
  payload = std::move(front.second);
  return true;
}

} // namespace agenticdsl
