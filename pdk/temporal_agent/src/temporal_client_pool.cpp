// pdk/temporal_agent/src/temporal_client_pool.cpp
// 功能描述：Temporal gRPC 连接池实现 - round-robin + 故障切换 + 恢复
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.1
//          .rddf/plans/pkgm-temporal-agent.md Task 1
// 线程安全：atomic rr_index_ (fetch_add) + mutex 保护 channels_ 结构性操作
//          healthy 标志为 atomic<bool>, 读取无需加锁
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "temporal_client_pool.h"

#include <algorithm>
#include <stdexcept>

namespace pdk_temporal_agent {

TemporalClientPool::TemporalClientPool(std::vector<std::string> targets) {
  if (targets.empty()) {
    throw std::invalid_argument(
        "TemporalClientPool: targets list must not be empty");
  }
  for (auto& t : targets) {
    channels_.emplace_back(ChannelEntry{std::move(t),
                                        std::make_unique<std::atomic<bool>>(true)});
  }
}

std::optional<ChannelHandle> TemporalClientPool::acquire_channel() {
  // 读取 channels_ 大小需要锁 (防止并发修改)
  std::lock_guard<std::mutex> lock(mu_);
  if (channels_.empty()) {
    return std::nullopt;
  }

  const size_t n = channels_.size();

  // round-robin 起始索引 (atomic, 无锁)
  size_t start = rr_index_.fetch_add(1, std::memory_order_relaxed) % n;

  // 从 start 开始遍历一圈, 找到第一个健康的 target
  for (size_t i = 0; i < n; ++i) {
    size_t idx = (start + i) % n;
    if (channels_[idx].healthy->load(std::memory_order_relaxed)) {
      ChannelHandle handle;
      handle.target = channels_[idx].target;
      handle.healthy = true;
      return handle;
    }
  }

  // 所有 target 均不健康
  return std::nullopt;
}

void TemporalClientPool::mark_unhealthy(const std::string& target) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& entry : channels_) {
    if (entry.target == target) {
      entry.healthy->store(false, std::memory_order_relaxed);
      return;
    }
  }
  // target 不存在: 静默忽略 (幂等操作)
}

void TemporalClientPool::mark_healthy(const std::string& target) {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& entry : channels_) {
    if (entry.target == target) {
      entry.healthy->store(true, std::memory_order_relaxed);
      return;
    }
  }
  // target 不存在: 静默忽略 (幂等操作)
}

bool TemporalClientPool::has_target(const std::string& target) const {
  std::lock_guard<std::mutex> lock(mu_);
  return find_index(target) != SIZE_MAX;
}

size_t TemporalClientPool::healthy_count() const {
  std::lock_guard<std::mutex> lock(mu_);
  size_t count = 0;
  for (const auto& entry : channels_) {
    if (entry.healthy->load(std::memory_order_relaxed)) {
      ++count;
    }
  }
  return count;
}

size_t TemporalClientPool::total_count() const {
  std::lock_guard<std::mutex> lock(mu_);
  return channels_.size();
}

size_t TemporalClientPool::find_index(const std::string& target) const {
  // 调用方持锁
  for (size_t i = 0; i < channels_.size(); ++i) {
    if (channels_[i].target == target) {
      return i;
    }
  }
  return SIZE_MAX;
}

}  // namespace pdk_temporal_agent
