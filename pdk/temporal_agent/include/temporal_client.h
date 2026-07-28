// pdk/temporal_agent/include/temporal_client.h
// 功能描述：Temporal gRPC 客户端抽象 - 工作流编排 (start/poll/signal/query)
//          采用抽象后端模式: 默认 InMemoryTemporalBackend (进程内模拟, 零 gRPC 依赖),
//          Phase 2 可替换为 GrpcTemporalBackend (真实 gRPC + WorkflowService stub)。
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §2
//          gRPC Status -> ErrorCode 映射 (ADR-0023 ToolResult)
// 作者：pkgm-temporal-agent change
// 最后修改日期：2026-07-27

#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace pdk_temporal_agent {

// === 工作流状态枚举 (对应 Temporal DescribeWorkflowExecution.status) ===
enum class WorkflowStatus {
  Unknown = 0,
  Running,
  Completed,
  Failed,
  Cancelled,
  TimedOut,
  Terminated,
};

// 将状态转为字符串 (用于 JSON 序列化)
inline const char* workflow_status_str(WorkflowStatus s) {
  switch (s) {
    case WorkflowStatus::Running:    return "RUNNING";
    case WorkflowStatus::Completed:  return "COMPLETED";
    case WorkflowStatus::Failed:     return "FAILED";
    case WorkflowStatus::Cancelled:  return "CANCELLED";
    case WorkflowStatus::TimedOut:   return "TIMED_OUT";
    case WorkflowStatus::Terminated: return "TERMINATED";
    case WorkflowStatus::Unknown:
    default:                         return "UNKNOWN";
  }
}

// === 工作流执行结果 ===
struct WorkflowResult {
  std::string workflow_id;
  std::string run_id;
  WorkflowStatus status = WorkflowStatus::Unknown;
  nlohmann::json result;        // 完成时的输出 (status=Completed)
  std::string failure_reason;   // 失败时的原因 (status=Failed)
  long long history_size_bytes = 0;  // 历史 event 大小 (字节)
  int event_count = 0;          // 历史 event 数量
};

// === gRPC 错误码映射 (对应 grpc::StatusCode -> agenticdsl::ErrorCode) ===
// 注: 此处用 int 表达避免引入 ToolResult 头 (保持插件编译期零 core 依赖)
enum class GrpcError {
  Ok = 0,
  NotFound,          // WORKFLOW_NOT_FOUND (gRPC NOT_FOUND)
  AlreadyExists,     // 幂等性冲突 (gRPC ALREADY_EXISTS)
  DeadlineExceeded,  // 超时 (gRPC DEADLINE_EXCEEDED)
  Unavailable,       // Temporal 不可达 (gRPC UNAVAILABLE)
  PermissionDenied,  // 认证失败 (gRPC PERMISSION_DENIED)
  InvalidArgument,   // 参数非法 (gRPC INVALID_ARGUMENT)
  Internal,          // 未知错误 (gRPC INTERNAL)
};

// === 事件发射回调 (Phase 4: IInteractionBus 集成) ===
// host 端注入此回调, 客户端在关键操作后发射事件。
// event_type: "temporal.workflow.start" / "temporal.workflow.complete" /
//             "temporal.workflow.failed" / "temporal.poll"
// payload: JSON (workflow_id / status / run_id / poll_count 等)
using EventEmitFunc = std::function<void(const std::string& event_type,
                                         const nlohmann::json& payload)>;

// === 抽象后端接口 (供 InMemory / Grpc 双实现) ===
class ITemporalBackend {
 public:
  virtual ~ITemporalBackend() = default;

  // 启动工作流 (阻塞): StartWorkflowExecution + 轮询直到终态
  virtual WorkflowResult start_workflow_blocking(
      const std::string& workflow_type,
      const std::string& task_queue,
      const std::string& input_json,
      const std::string& workflow_id,
      long long timeout_ms) = 0;

  // 启动工作流 (异步): StartWorkflowExecution + 立即返回
  virtual WorkflowResult start_workflow_async(
      const std::string& workflow_type,
      const std::string& task_queue,
      const std::string& input_json,
      const std::string& workflow_id) = 0;

  // 轮询工作流状态
  virtual WorkflowResult poll(const std::string& workflow_id,
                              long long timeout_ms) = 0;

  // 发送信号
  virtual bool signal(const std::string& workflow_id,
                      const std::string& signal_name,
                      const std::string& input_json) = 0;

  // 查询 (只读元数据)
  virtual WorkflowResult query(const std::string& workflow_id) = 0;
};

// === 错误异常 (后端抛出, 客户端捕获并映射) ===
class TemporalError : public std::runtime_error {
 public:
  GrpcError code;
  TemporalError(GrpcError c, const std::string& msg)
      : std::runtime_error(msg), code(c) {}
};

// === 单例客户端 (线程安全, 管理后端) ===
class TemporalClient {
 public:
  // 获取单例
  static TemporalClient& instance();

  // 连接到 Temporal gRPC host (e.g. "localhost:7233")
  // 若后端为 InMemory 则仅标记 connected=true
  void connect(const std::string& host);

  // 关闭连接 (清理 stub + channel)
  void shutdown();

  // 是否已连接
  bool is_connected() const;

  // 注入自定义后端 (主要用于测试)
  void set_backend(std::unique_ptr<ITemporalBackend> backend);

  // 获取当前后端 (用于测试断言)
  ITemporalBackend* backend() const;

  // 注入事件发射回调 (Phase 4: IInteractionBus 集成)
  void set_event_emitter(EventEmitFunc emitter);

  // 获取事件发射回调 (内部使用)
  const EventEmitFunc& event_emitter() const;

  // === 5 个 API 方法 (委托给后端) ===
  WorkflowResult start_workflow_blocking(
      const std::string& workflow_type,
      const std::string& task_queue,
      const std::string& input_json,
      const std::string& workflow_id = "",
      long long timeout_ms = 30000);

  WorkflowResult start_workflow_async(
      const std::string& workflow_type,
      const std::string& task_queue,
      const std::string& input_json,
      const std::string& workflow_id = "");

  WorkflowResult poll(const std::string& workflow_id,
                      long long timeout_ms = 5000);

  bool signal(const std::string& workflow_id,
              const std::string& signal_name,
              const std::string& input_json = "");

  WorkflowResult query(const std::string& workflow_id);

 private:
  TemporalClient();

  mutable std::mutex mutex_;
  std::unique_ptr<ITemporalBackend> backend_;
  std::string host_;
  std::atomic<bool> connected_{false};
  EventEmitFunc event_emitter_;
};

// === 进程内模拟后端 (默认实现, 零 gRPC 依赖) ===
// 模拟 Temporal 行为: 工作流注册后保持 Running, 可手动推进到终态。
// 适用于测试与开发。幂等性: 相同 workflow_id 不创建新 run。
class InMemoryTemporalBackend : public ITemporalBackend {
 public:
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

  // === 测试辅助: 手动推进工作流状态 ===
  void complete_workflow(const std::string& workflow_id,
                         const nlohmann::json& result);
  void fail_workflow(const std::string& workflow_id,
                     const std::string& reason);
  void advance_workflow(const std::string& workflow_id,
                        WorkflowStatus new_status);

  // 检查是否存在
  bool exists(const std::string& workflow_id) const;

 private:
  struct WorkflowState {
    std::string workflow_id;
    std::string run_id;
    std::string workflow_type;
    std::string task_queue;
    std::string input_json;
    WorkflowStatus status = WorkflowStatus::Running;
    nlohmann::json result;
    std::string failure_reason;
    long long history_size_bytes = 0;
    int event_count = 0;
    std::chrono::steady_clock::time_point started_at;
  };

  mutable std::mutex mutex_;
  std::map<std::string, WorkflowState> workflows_;
  std::atomic<int> run_counter_{0};

  // 生成唯一 run_id
  std::string gen_run_id();

  // 查找或抛 NotFound
  WorkflowState& find_or_throw(const std::string& workflow_id);
};

}  // namespace pdk_temporal_agent
