// pdk/temporal_agent/src/grpc_temporal_backend.cpp
// 功能描述：GrpcTemporalBackend 实现 - 条件编译 (gRPC vs InMemory fallback)
//          TEMPORAL_ENABLE_GRPC=ON: 真实 gRPC (需 protoc + gRPC dev, 当前环境不可用)
//          TEMPORAL_ENABLE_GRPC=OFF (默认): 委托 InMemoryTemporalBackend
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.6
//          .rddf/plans/pkgm-temporal-agent.md Task 5
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#include "grpc_temporal_backend.h"

#include <stdexcept>

namespace pdk_temporal_agent {

#ifdef TEMPORAL_ENABLE_GRPC

GrpcTemporalBackend::GrpcTemporalBackend(std::string target)
    : target_(std::move(target)) {
  throw std::runtime_error(
      "GrpcTemporalBackend: TEMPORAL_ENABLE_GRPC=ON but gRPC dev env not available");
}

#else

GrpcTemporalBackend::GrpcTemporalBackend(std::string target)
    : target_(std::move(target)),
      fallback_(std::make_unique<InMemoryTemporalBackend>()) {}

InMemoryTemporalBackend& GrpcTemporalBackend::fallback() {
  if (!fallback_) {
    throw std::runtime_error("GrpcTemporalBackend: fallback backend is null");
  }
  return *fallback_;
}

#endif

GrpcTemporalBackend::~GrpcTemporalBackend() = default;

void GrpcTemporalBackend::connect() {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: connect() not implemented (gRPC env pending)");
#else
  connected_ = true;
#endif
}

bool GrpcTemporalBackend::is_connected() const {
  return connected_;
}

WorkflowResult GrpcTemporalBackend::start_workflow_blocking(
    const std::string& workflow_type,
    const std::string& task_queue,
    const std::string& input_json,
    const std::string& workflow_id,
    long long timeout_ms) {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: start_workflow_blocking not implemented");
#else
  return fallback().start_workflow_blocking(
      workflow_type, task_queue, input_json, workflow_id, timeout_ms);
#endif
}

WorkflowResult GrpcTemporalBackend::start_workflow_async(
    const std::string& workflow_type,
    const std::string& task_queue,
    const std::string& input_json,
    const std::string& workflow_id) {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: start_workflow_async not implemented");
#else
  return fallback().start_workflow_async(
      workflow_type, task_queue, input_json, workflow_id);
#endif
}

WorkflowResult GrpcTemporalBackend::poll(const std::string& workflow_id,
                                          long long timeout_ms) {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: poll not implemented");
#else
  return fallback().poll(workflow_id, timeout_ms);
#endif
}

bool GrpcTemporalBackend::signal(const std::string& workflow_id,
                                   const std::string& signal_name,
                                   const std::string& input_json) {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: signal not implemented");
#else
  return fallback().signal(workflow_id, signal_name, input_json);
#endif
}

WorkflowResult GrpcTemporalBackend::query(const std::string& workflow_id) {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: query not implemented");
#else
  return fallback().query(workflow_id);
#endif
}

void GrpcTemporalBackend::emit_signal(const std::string& workflow_id,
                                        const std::string& signal_name,
                                        const nlohmann::json& payload) {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: emit_signal not implemented");
#else
  fallback().emit_signal(workflow_id, signal_name, payload);
#endif
}

std::vector<ITemporalBackend::SignalEntry>
GrpcTemporalBackend::consume_signals(const std::string& workflow_id) {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: consume_signals not implemented");
#else
  return fallback().consume_signals(workflow_id);
#endif
}

void GrpcTemporalBackend::stream_workflow_events(
    const std::string& workflow_id,
    StreamCallback callback,
    std::atomic<bool>& stop_flag) {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: stream_workflow_events not implemented");
#else
  fallback().stream_workflow_events(workflow_id, callback, stop_flag);
#endif
}

int GrpcTemporalBackend::get_poll_count(const std::string& workflow_id) const {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: get_poll_count not implemented");
#else
  if (!fallback_) {
    return 0;
  }
  return fallback_->get_poll_count(workflow_id);
#endif
}

void GrpcTemporalBackend::complete_workflow(const std::string& workflow_id,
                                              const nlohmann::json& result) {
#ifdef TEMPORAL_ENABLE_GRPC
  throw std::runtime_error("GrpcTemporalBackend: complete_workflow not implemented");
#else
  fallback().complete_workflow(workflow_id, result);
#endif
}

}  // namespace pdk_temporal_agent
