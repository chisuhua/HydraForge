// pdk/temporal_agent/src/temporal_client_pool.h
// 功能描述：Temporal gRPC 连接池 - round-robin 负载均衡 + 故障切换 + 恢复
//          管理多个 Temporal server target (host:port), 提供 channel 获取接口。
//          采用 ChannelHandle 字符串抽象 (零 gRPC 编译期依赖), 适用于:
//            - InMemoryTemporalBackend (不需要真实 channel)
//            - GrpcTemporalBackend (Task 5 激活时, 内部用 target 创建 grpc::Channel)
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.1
//          .rddf/plans/pkgm-temporal-agent.md Task 1
// 线程安全：atomic round-robin index + mutex 保护 channels_ vector
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace pdk_temporal_agent {

// === Channel 句柄 (轻量级, 零 gRPC 依赖) ===
// 当 GrpcTemporalBackend (Task 5) 激活时, 用 target 字段创建真实 grpc::Channel。
// healthy 字段供调用方参考 (pool 内部已过滤不健康 target)。
struct ChannelHandle {
  std::string target;       // host:port
  bool healthy = true;      // 当前健康状态
};

// === gRPC 连接池 ===
// 多 target round-robin 负载均衡 + 不健康 target 自动跳过 + 恢复。
//
// 用法:
//   TemporalClientPool pool({"host1:7233", "host2:7233"});
//   auto ch = pool.acquire_channel();  // std::optional<ChannelHandle>
//   if (ch) { /* 使用 ch->target 创建 grpc::Channel */ }
//
// 故障切换:
//   pool.mark_unhealthy("host1:7233");  // 标记不可用, acquire 时自动跳过
//   pool.mark_healthy("host1:7233");    // 恢复, 重新加入 round-robin
class TemporalClientPool {
 public:
  // 构造: 传入 target 列表 (host:port), 至少 1 个
  // 空列表将抛 std::invalid_argument
  explicit TemporalClientPool(std::vector<std::string> targets);

  // 获取一个健康的 channel (round-robin 负载均衡)
  // 返回 std::nullopt 当所有 target 均不健康时
  std::optional<ChannelHandle> acquire_channel();

  // 标记 target 为不健康 (故障切换, acquire 时跳过)
  void mark_unhealthy(const std::string& target);

  // 标记 target 为健康 (恢复, 重新加入 round-robin)
  void mark_healthy(const std::string& target);

  // 查询: 是否存在指定 target
  bool has_target(const std::string& target) const;

  // 查询: 当前健康 target 数量
  size_t healthy_count() const;

  // 查询: 总 target 数量
  size_t total_count() const;

 private:
  struct ChannelEntry {
    std::string target;
    std::unique_ptr<std::atomic<bool>> healthy;
    // Phase 2 Task 5 激活时: std::shared_ptr<grpc::Channel> channel;
  };

  std::vector<ChannelEntry> channels_;
  std::atomic<size_t> rr_index_{0};
  mutable std::mutex mu_;  // 保护 channels_ vector 结构性修改

  // 查找 target 索引 (不存在返回 SIZE_MAX)
  size_t find_index(const std::string& target) const;
};

}  // namespace pdk_temporal_agent
