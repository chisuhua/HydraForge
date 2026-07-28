// pdk/temporal_agent/src/temporal_client.cpp
// 功能描述：Temporal gRPC 客户端实现 - 单例连接管理 + 5 个 API 方法委托。
//          默认后端 InMemoryTemporalBackend 提供进程内模拟 (零 gRPC 依赖)。
//          Phase 2 可替换为 GrpcTemporalBackend (真实 gRPC stub)。
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §2.1-2.7
// 作者：pkgm-temporal-agent change
// 最后修改日期：2026-07-27

#include "temporal_client.h"

#include <chrono>
#include <random>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace pdk_temporal_agent {

// 发射事件的 helper (线程安全, 若 emitter 未设置则 no-op)
inline void emit_event(const EventEmitFunc& emitter,
                      const std::string& event_type,
                      const nlohmann::json& payload) {
  if (emitter) {
    emitter(event_type, payload);
  }
}

// ============================================================================
// TemporalClient 单例
// ============================================================================

TemporalClient& TemporalClient::instance() {
  static TemporalClient client;
  return client;
}

TemporalClient::TemporalClient() {
  // 默认使用 InMemory 后端
  backend_ = std::make_unique<InMemoryTemporalBackend>();
}

void TemporalClient::connect(const std::string& host) {
  std::lock_guard<std::mutex> lock(mutex_);
  host_ = host;
  // InMemory 后端仅标记 connected; Grpc 后端会 grpc::CreateChannel + NewStub
  connected_ = true;
}

void TemporalClient::shutdown() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Grpc 后端: 清理 stub + channel
  // InMemory 后端: 清理所有工作流状态
  connected_ = false;
  host_.clear();
  if (backend_) {
    // 后端析构由 unique_ptr 管理, 此处无需额外操作
  }
}

bool TemporalClient::is_connected() const {
  return connected_.load();
}

void TemporalClient::set_backend(std::unique_ptr<ITemporalBackend> backend) {
  std::lock_guard<std::mutex> lock(mutex_);
  backend_ = std::move(backend);
}

ITemporalBackend* TemporalClient::backend() const {
  return backend_.get();
}

void TemporalClient::set_event_emitter(EventEmitFunc emitter) {
  std::lock_guard<std::mutex> lock(mutex_);
  event_emitter_ = std::move(emitter);
}

const EventEmitFunc& TemporalClient::event_emitter() const {
  return event_emitter_;
}

WorkflowResult TemporalClient::start_workflow_blocking(
    const std::string& workflow_type,
    const std::string& task_queue,
    const std::string& input_json,
    const std::string& workflow_id,
    long long timeout_ms) {
  if (!backend_) {
    throw TemporalError(GrpcError::Unavailable, "TemporalClient not initialized");
  }
  auto result = backend_->start_workflow_blocking(
      workflow_type, task_queue, input_json, workflow_id, timeout_ms);
  nlohmann::json payload;
  payload["workflow_id"] = result.workflow_id;
  payload["run_id"] = result.run_id;
  payload["status"] = workflow_status_str(result.status);
  emit_event(event_emitter_, "temporal.workflow.start", payload);
  if (result.status == WorkflowStatus::Completed) {
    payload["result"] = result.result;
    emit_event(event_emitter_, "temporal.workflow.complete", payload);
  } else if (result.status == WorkflowStatus::Failed ||
             result.status == WorkflowStatus::Cancelled ||
             result.status == WorkflowStatus::TimedOut) {
    payload["failure_reason"] = result.failure_reason;
    emit_event(event_emitter_, "temporal.workflow.failed", payload);
  }
  return result;
}

WorkflowResult TemporalClient::start_workflow_async(
    const std::string& workflow_type,
    const std::string& task_queue,
    const std::string& input_json,
    const std::string& workflow_id) {
  if (!backend_) {
    throw TemporalError(GrpcError::Unavailable, "TemporalClient not initialized");
  }
  auto result = backend_->start_workflow_async(
      workflow_type, task_queue, input_json, workflow_id);
  nlohmann::json payload;
  payload["workflow_id"] = result.workflow_id;
  payload["run_id"] = result.run_id;
  payload["status"] = workflow_status_str(result.status);
  emit_event(event_emitter_, "temporal.workflow.start", payload);
  return result;
}

WorkflowResult TemporalClient::poll(const std::string& workflow_id,
                                     long long timeout_ms) {
  if (!backend_) {
    throw TemporalError(GrpcError::Unavailable, "TemporalClient not initialized");
  }
  auto result = backend_->poll(workflow_id, timeout_ms);
  nlohmann::json payload;
  payload["workflow_id"] = result.workflow_id;
  payload["run_id"] = result.run_id;
  payload["status"] = workflow_status_str(result.status);
  payload["poll_count"] = 1;
  emit_event(event_emitter_, "temporal.poll", payload);
  if (result.status == WorkflowStatus::Completed) {
    payload["result"] = result.result;
    emit_event(event_emitter_, "temporal.workflow.complete", payload);
  } else if (result.status == WorkflowStatus::Failed ||
             result.status == WorkflowStatus::Cancelled ||
             result.status == WorkflowStatus::TimedOut) {
    payload["failure_reason"] = result.failure_reason;
    emit_event(event_emitter_, "temporal.workflow.failed", payload);
  }
  return result;
}

bool TemporalClient::signal(const std::string& workflow_id,
                             const std::string& signal_name,
                             const std::string& input_json) {
  if (!backend_) {
    throw TemporalError(GrpcError::Unavailable, "TemporalClient not initialized");
  }
  return backend_->signal(workflow_id, signal_name, input_json);
}

WorkflowResult TemporalClient::query(const std::string& workflow_id) {
  if (!backend_) {
    throw TemporalError(GrpcError::Unavailable, "TemporalClient not initialized");
  }
  return backend_->query(workflow_id);
}

// ============================================================================
// InMemoryTemporalBackend (默认进程内模拟后端)
// ============================================================================

std::string InMemoryTemporalBackend::gen_run_id() {
  int n = run_counter_.fetch_add(1) + 1;
  std::ostringstream ss;
  ss << "run-" << std::hex << n << "-"
     << std::chrono::steady_clock::now().time_since_epoch().count();
  return ss.str();
}

InMemoryTemporalBackend::WorkflowState&
InMemoryTemporalBackend::find_or_throw(const std::string& workflow_id) {
  auto it = workflows_.find(workflow_id);
  if (it == workflows_.end()) {
    throw TemporalError(GrpcError::NotFound,
                        "workflow not found: " + workflow_id);
  }
  return it->second;
}

WorkflowResult InMemoryTemporalBackend::start_workflow_blocking(
    const std::string& workflow_type,
    const std::string& task_queue,
    const std::string& input_json,
    const std::string& workflow_id,
    long long timeout_ms) {
  std::lock_guard<std::mutex> lock(mutex_);

  // 生成或复用 workflow_id (幂等性)
  std::string wf_id = workflow_id;
  if (wf_id.empty()) {
    wf_id = "wf-" + gen_run_id();
  }

  // 幂等性: 已存在的工作流直接返回当前状态
  if (workflows_.count(wf_id) > 0) {
    const auto& existing = workflows_.at(wf_id);
    return WorkflowResult{
      .workflow_id = existing.workflow_id,
      .run_id = existing.run_id,
      .status = existing.status,
      .result = existing.result,
      .failure_reason = existing.failure_reason,
      .history_size_bytes = existing.history_size_bytes,
      .event_count = existing.event_count
    };
  }

  // 创建新工作流
  WorkflowState state;
  state.workflow_id = wf_id;
  state.run_id = gen_run_id();
  state.workflow_type = workflow_type;
  state.task_queue = task_queue;
  state.input_json = input_json;
  state.status = WorkflowStatus::Running;
  state.started_at = std::chrono::steady_clock::now();
  state.history_size_bytes = 256;  // 模拟初始历史大小
  state.event_count = 1;            // WorkflowExecutionStarted event

  workflows_[wf_id] = std::move(state);

  // 阻塞模式: 轮询直到终态或超时
  // InMemory 后端不会自动完成, 需测试主动推进。
  // 此处做简单轮询等待 (与真实 gRPC DescribeWorkflowExecution 轮询一致)
  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    auto& cur = workflows_[wf_id];
    if (cur.status != WorkflowStatus::Running) {
      return WorkflowResult{
        .workflow_id = cur.workflow_id,
        .run_id = cur.run_id,
        .status = cur.status,
        .result = cur.result,
        .failure_reason = cur.failure_reason,
        .history_size_bytes = cur.history_size_bytes,
        .event_count = cur.event_count
      };
    }
    mutex_.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mutex_.lock();
  }

  // 超时仍未完成
  auto& cur = workflows_[wf_id];
  return WorkflowResult{
    .workflow_id = cur.workflow_id,
    .run_id = cur.run_id,
    .status = cur.status,
    .result = cur.result,
    .failure_reason = cur.failure_reason,
    .history_size_bytes = cur.history_size_bytes,
    .event_count = cur.event_count
  };
}

WorkflowResult InMemoryTemporalBackend::start_workflow_async(
    const std::string& workflow_type,
    const std::string& task_queue,
    const std::string& input_json,
    const std::string& workflow_id) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::string wf_id = workflow_id;
  if (wf_id.empty()) {
    wf_id = "wf-" + gen_run_id();
  }

  // 幂等性: 已存在直接返回
  if (workflows_.count(wf_id) > 0) {
    const auto& existing = workflows_.at(wf_id);
    return WorkflowResult{
      .workflow_id = existing.workflow_id,
      .run_id = existing.run_id,
      .status = existing.status,
      .result = existing.result,
      .failure_reason = existing.failure_reason,
      .history_size_bytes = existing.history_size_bytes,
      .event_count = existing.event_count
    };
  }

  WorkflowState state;
  state.workflow_id = wf_id;
  state.run_id = gen_run_id();
  state.workflow_type = workflow_type;
  state.task_queue = task_queue;
  state.input_json = input_json;
  state.status = WorkflowStatus::Running;
  state.started_at = std::chrono::steady_clock::now();
  state.history_size_bytes = 256;
  state.event_count = 1;

  workflows_[wf_id] = std::move(state);

  auto& cur = workflows_[wf_id];
  return WorkflowResult{
    .workflow_id = cur.workflow_id,
    .run_id = cur.run_id,
    .status = cur.status,
    .result = cur.result,
    .failure_reason = cur.failure_reason,
    .history_size_bytes = cur.history_size_bytes,
    .event_count = cur.event_count
  };
}

WorkflowResult InMemoryTemporalBackend::poll(const std::string& workflow_id,
                                              long long timeout_ms) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& cur = find_or_throw(workflow_id);
  cur.poll_count += 1;

  // 简单轮询 (InMemory 后端: 状态由测试手动推进)
  auto deadline = std::chrono::steady_clock::now() +
                  std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (cur.status != WorkflowStatus::Running) {
      break;
    }
    mutex_.unlock();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    mutex_.lock();
    cur = find_or_throw(workflow_id);
  }

  return WorkflowResult{
    .workflow_id = cur.workflow_id,
    .run_id = cur.run_id,
    .status = cur.status,
    .result = cur.result,
    .failure_reason = cur.failure_reason,
    .history_size_bytes = cur.history_size_bytes,
    .event_count = cur.event_count
  };
}

bool InMemoryTemporalBackend::signal(const std::string& workflow_id,
                                     const std::string& signal_name,
                                     const std::string& input_json) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& cur = find_or_throw(workflow_id);

  // 模拟信号接收: 增加历史大小 + event 计数
  cur.event_count += 1;
  cur.history_size_bytes += 128 + signal_name.size();

  return true;
}

WorkflowResult InMemoryTemporalBackend::query(const std::string& workflow_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto& cur = find_or_throw(workflow_id);

  // query 只读, 不修改状态
  return WorkflowResult{
    .workflow_id = cur.workflow_id,
    .run_id = cur.run_id,
    .status = cur.status,
    .result = cur.result,
    .failure_reason = cur.failure_reason,
    .history_size_bytes = cur.history_size_bytes,
    .event_count = cur.event_count
  };
}

void InMemoryTemporalBackend::complete_workflow(
    const std::string& workflow_id, const nlohmann::json& result) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& cur = find_or_throw(workflow_id);
  cur.status = WorkflowStatus::Completed;
  cur.result = result;
  cur.failure_reason.clear();
  cur.event_count += 1;
  cur.history_size_bytes += 64;
}

void InMemoryTemporalBackend::fail_workflow(
    const std::string& workflow_id, const std::string& reason) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& cur = find_or_throw(workflow_id);
  cur.status = WorkflowStatus::Failed;
  cur.failure_reason = reason;
  cur.event_count += 1;
  cur.history_size_bytes += 64;
}

void InMemoryTemporalBackend::advance_workflow(
    const std::string& workflow_id, WorkflowStatus new_status) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto& cur = find_or_throw(workflow_id);
  cur.status = new_status;
  cur.event_count += 1;
}

bool InMemoryTemporalBackend::exists(const std::string& workflow_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return workflows_.count(workflow_id) > 0;
}

void InMemoryTemporalBackend::emit_signal(const std::string& workflow_id,
                                          const std::string& signal_name,
                                          const nlohmann::json& payload) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = workflows_.find(workflow_id);
  if (it == workflows_.end()) {
    throw TemporalError(GrpcError::NotFound,
                        "emit_signal: workflow not found: " + workflow_id);
  }
  it->second.pending_signals.push_back({signal_name, payload});
}

std::vector<ITemporalBackend::SignalEntry>
InMemoryTemporalBackend::consume_signals(const std::string& workflow_id) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = workflows_.find(workflow_id);
  if (it == workflows_.end()) {
    return {};
  }
  auto result = std::move(it->second.pending_signals);
  it->second.pending_signals.clear();
  return result;
}

static WorkflowResult make_result_from_state(
    const std::string& wf_id, const std::string& run_id,
    WorkflowStatus status, const nlohmann::json& result,
    const std::string& failure_reason,
    long long history_size, int event_count) {
  return WorkflowResult{
    .workflow_id = wf_id,
    .run_id = run_id,
    .status = status,
    .result = result,
    .failure_reason = failure_reason,
    .history_size_bytes = history_size,
    .event_count = event_count
  };
}

void InMemoryTemporalBackend::stream_workflow_events(
    const std::string& workflow_id,
    StreamCallback callback,
    std::atomic<bool>& stop_flag) {
  while (!stop_flag.load(std::memory_order_relaxed)) {
    WorkflowResult snapshot;
    WorkflowStatus prev_status = WorkflowStatus::Unknown;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = workflows_.find(workflow_id);
      if (it == workflows_.end()) {
        return;
      }
      auto& s = it->second;
      prev_status = s.last_streamed_status;
      s.last_streamed_status = s.status;
      snapshot = make_result_from_state(
          s.workflow_id, s.run_id, s.status, s.result,
          s.failure_reason, s.history_size_bytes, s.event_count);
    }

    if (snapshot.status != prev_status) {
      callback(snapshot);
    }

    if (snapshot.status == WorkflowStatus::Completed ||
        snapshot.status == WorkflowStatus::Failed ||
        snapshot.status == WorkflowStatus::Cancelled ||
        snapshot.status == WorkflowStatus::Terminated ||
        snapshot.status == WorkflowStatus::TimedOut) {
      return;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

int InMemoryTemporalBackend::get_poll_count(const std::string& workflow_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = workflows_.find(workflow_id);
  if (it == workflows_.end()) {
    return 0;
  }
  return it->second.poll_count;
}

}  // namespace pdk_temporal_agent
