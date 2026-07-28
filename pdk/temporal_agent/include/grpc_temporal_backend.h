// pdk/temporal_agent/include/grpc_temporal_backend.h
// 功能描述：GrpcTemporalBackend - 真实 Temporal gRPC 后端 (条件编译)
//          TEMPORAL_ENABLE_GRPC=ON: 真实 gRPC stub (需 protoc + gRPC dev)
//          TEMPORAL_ENABLE_GRPC=OFF (默认): 委托 InMemoryTemporalBackend (零 gRPC 依赖)
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.6
//          .rddf/plans/pkgm-temporal-agent.md Task 5
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "temporal_client.h"

namespace pdk_temporal_agent {

class GrpcTemporalBackend : public ITemporalBackend {
 public:
  explicit GrpcTemporalBackend(std::string target);
  ~GrpcTemporalBackend() override;

  GrpcTemporalBackend(const GrpcTemporalBackend&) = delete;
  GrpcTemporalBackend& operator=(const GrpcTemporalBackend&) = delete;

  void connect();
  bool is_connected() const;

  // ITemporalBackend 接口
  WorkflowResult start_workflow_blocking(
      const std::string& workflow_type,
      const std::string& task_queue,
      const std::string& input_json,
      const std::string& workflow_id,
      long long timeout_ms) override;

  WorkflowResult start_workflow_async(
      const std::string& workflow_type,
      const std::string& task_queue,
      const std::string& input_json,
      const std::string& workflow_id) override;

  WorkflowResult poll(const std::string& workflow_id,
                      long long timeout_ms) override;

  bool signal(const std::string& workflow_id,
              const std::string& signal_name,
              const std::string& input_json) override;

  WorkflowResult query(const std::string& workflow_id) override;

  void emit_signal(const std::string& workflow_id,
                   const std::string& signal_name,
                   const nlohmann::json& payload) override;

  std::vector<SignalEntry> consume_signals(
      const std::string& workflow_id) override;

  void stream_workflow_events(const std::string& workflow_id,
                              StreamCallback callback,
                              std::atomic<bool>& stop_flag) override;

  int get_poll_count(const std::string& workflow_id) const override;

  void complete_workflow(const std::string& workflow_id,
                         const nlohmann::json& result);

 private:
  std::string target_;
  bool connected_ = false;

#ifdef TEMPORAL_ENABLE_GRPC
  std::shared_ptr<void> grpc_channel_;
#else
  std::unique_ptr<InMemoryTemporalBackend> fallback_;
  InMemoryTemporalBackend& fallback();
#endif
};

}  // namespace pdk_temporal_agent
