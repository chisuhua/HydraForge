// pdk/temporal_agent/mock_client.h
// 功能描述：MockTemporalClient 声明 - in-memory 状态机实现 ITemporalClient (Task 2)。
//          状态机: CREATED -> RUNNING -> COMPLETED / FAILED
//          特性:
//            - 幂等性: 重复 workflow_id 返回 idempotent_replay=true
//            - 延迟模拟: set_simulated_latency + advance_time (确定性虚拟时钟)
//            - signal 追加到 record (可 query 回查)
//            - 线程安全: std::mutex 保护 workflows_ map
// 设计依据：.rddf/plans/pkm-temporal-demo-scaffold.md Task 2 Step 3
// 作者：pkm-temporal-demo-scaffold Task 2
// 最后修改日期：2026-07-28

#pragma once

#include "agenticdsl/pdk/itemporal_client.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace agenticdsl {
namespace pdk {

// ============================================================================
// WorkflowState - Mock 状态机枚举
// ============================================================================
enum class WorkflowState {
  CREATED,
  RUNNING,
  COMPLETED,
  FAILED,
};

// ============================================================================
// MockTemporalClient - in-memory 状态机实现 ITemporalClient
// ============================================================================
// 用于测试和 Demo: 无需真实 Temporal server, 进程内确定性状态转换。
// 虚拟时钟 (current_time_) 通过 advance_time() 推进, 避免真实 sleep。
// ============================================================================
class MockTemporalClient : public ITemporalClient {
public:
  MockTemporalClient();

  // --- ITemporalClient 接口实现 (5 方法) ---
  nlohmann::json start_workflow_blocking(
      const std::string& workflow_id,
      const nlohmann::json& args) override;
  nlohmann::json start_workflow_async(
      const std::string& workflow_id,
      const nlohmann::json& args) override;
  nlohmann::json poll(const std::string& workflow_id) override;
  nlohmann::json signal(
      const std::string& workflow_id,
      const std::string& signal_name,
      const nlohmann::json& payload) override;
  nlohmann::json query(
      const std::string& workflow_id,
      const std::string& query_name) override;

  // --- 测试辅助 API (非 ITemporalClient) ---
  // 推进虚拟时钟 (测试用, 替代真实 sleep)
  void advance_time(std::chrono::milliseconds delta);
  // 设置新 workflow 的默认延迟
  void set_simulated_latency(std::chrono::milliseconds latency);

private:
  struct SignalEntry {
    std::string name;
    nlohmann::json payload;
  };

  struct WorkflowRecord {
    WorkflowState state{WorkflowState::CREATED};
    nlohmann::json args;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::milliseconds latency{0};
    std::vector<SignalEntry> signals;
    bool idempotent_replay{false};
  };

  // 内部: 检查并更新延迟到期的 workflow (CREATED/RUNNING -> COMPLETED/FAILED)
  void maybe_transition(std::chrono::steady_clock::time_point now);

  // 内部: 生成状态 JSON (含 workflow_id + state + 元数据)
  nlohmann::json make_status_json(const std::string& workflow_id,
                                  const WorkflowRecord& rec) const;

  // 内部: 创建 workflow record (供 start_workflow_async 复用)
  nlohmann::json create_workflow(const std::string& workflow_id,
                                  const nlohmann::json& args);

  std::mutex mu_;
  std::unordered_map<std::string, WorkflowRecord> workflows_;
  std::chrono::milliseconds default_latency_{100};  // 默认 100ms 延迟
  std::chrono::steady_clock::time_point current_time_;
};

}  // namespace pdk
}  // namespace agenticdsl
