// src/core/types/session.h
// 功能描述：ADR-0033 Session Hierarchy — 三层会话模型类型定义
//           UserSession (顶层, 对应一次对话周期)
//           TaskSession (单次 DSLEngine::run() 执行)
//           SubtaskSession (fork/join 最小执行单元, POD-like)
// 设计依据：ADR-0033 + OpenSpec change 2026-06-26-adr-0033-session-hierarchy
//           Oracle 审查: ses_0e28703caffeBUB7tgDqsClZiw (2026-07-01)
//           Metis 审查: ses_0e26217edffeHrMpGUPLuhTecP (2026-07-01)
// 关键决策：
//   - deque 替代 vector 确保地址稳定性 (Metis F1/F2)
//   - TaskSession 持有 shared_ptr<IExecutionPolicy> (Oracle R3)
//   - failure_count 仅可重试错误递增 (Oracle R6)
//   - 不重命名 ExecutionSession (Oracle R1)
// 作者：AgenticDSL Sprint 15 (C5)
// 最后修改日期：2026-07-02

#ifndef AGENTICDSL_CORE_TYPES_SESSION_H
#define AGENTICDSL_CORE_TYPES_SESSION_H

#include "core/types/context.h"          // Context (nlohmann::json)
#include "core/types/budget.h"           // ExecutionResult
#include "core/types/tool_result.h"      // ToolResult, ErrorCode
#include "agenticdsl/types/trace_record.h" // TraceRecord
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace agenticdsl {

// 前向声明
class IExecutionPolicy;
class UserSession;

// ============================================================
// SubtaskSession: 最小执行单元，对应 fork 分支 (POD-like)
// ============================================================
struct SubtaskSession {
  std::string branch_path;                                  // 分叉路径标识
  Context initial_context;                                  // 入口 Context 快照
  Context final_context;                                    // 出口 Context（执行后填充）
  std::vector<TraceRecord> execution_trace;                 // 执行轨迹
  std::string status = "pending";                           // pending | running | completed | failed
  std::chrono::steady_clock::time_point started_at;
  std::optional<std::chrono::steady_clock::time_point> completed_at;

  /// @brief 便捷工厂构造
  static SubtaskSession make(const std::string& path, Context initial);
};

// ============================================================
// TaskSession: 对应一次完整的 DSLEngine::run() 执行
// ============================================================
class TaskSession {
public:
  /// @brief 失败重试策略
  enum class FailureMode { KeepSession, NewSession };

  /// @brief 构造 — 必须绑定 UserSession
  explicit TaskSession(UserSession& user_sess);

  // --- 反向引用 ---
  UserSession& user_session() { return user_session_; }
  const UserSession& user_session() const { return user_session_; }

  // --- SubtaskSession 管理 (fork/join 分支隔离) ---
  /// @brief 创建新 SubtaskSession，返回 deque 内元素引用（地址稳定）
  SubtaskSession& create_subtask(const std::string& branch_path, Context initial_context);
  /// @brief 归档已完成的分支结果（成功+失败均归档，const& 原地更新 deque 元素）
  void archive_subtask_result(const SubtaskSession& subtask);
  const std::deque<SubtaskSession>& subtask_sessions() const { return subtask_sessions_; }

  // --- 执行策略 (与 DSLEngine 共享所有权) ---
  void set_policy(std::shared_ptr<IExecutionPolicy> policy);
  std::shared_ptr<IExecutionPolicy> current_policy() const { return current_policy_; }

  // --- 失败计数与重试策略 ---
  /// @brief 仅在 success=false 且 error_code 为可重试类别时递增
  void record_failure(const ExecutionResult& result);
  /// @brief <3 次 → KeepSession, ≥3 次 → NewSession
  FailureMode determine_failure_mode() const;
  uint32_t failure_count() const { return failure_count_; }

  // --- 状态 ---
  std::string status() const { return status_; }
  void set_status(const std::string& s) { status_ = s; }

  // --- Context 封装 ---
  const Context& context() const { return context_; }
  Context& context() { return context_; }
  void set_context(Context ctx) { context_ = std::move(ctx); }

private:
  UserSession& user_session_;                               // 反向引用（绑定 UserSession 本身）
  std::deque<SubtaskSession> subtask_sessions_;             // deque 确保地址稳定
  std::shared_ptr<IExecutionPolicy> current_policy_;        // 与 DSLEngine 共享
  uint32_t failure_count_ = 0;
  std::string status_ = "active";
  Context context_;                                         // 当前执行上下文
};

// ============================================================
// UserSession: 顶层会话，对应一次用户对话周期
// ============================================================
class UserSession {
public:
  /// @brief 构造 — 分配 user_id 和时间戳
  explicit UserSession(std::string user_id);

  // --- messages 追加写保护 (ADR-0023 ToolResult 信封) ---
  void append_message(ToolResult msg);
  const std::vector<ToolResult>& messages() const { return messages_; }

  // --- TaskSession 管理 ---
  /// @brief 创建新 TaskSession，返回引用并自动设为 current
  TaskSession& create_task_session();
  TaskSession* current_task_session() const { return current_task_session_; }
  const std::deque<TaskSession>& task_sessions() const { return task_sessions_; }

  // --- 元数据 ---
  const std::string& user_id() const { return user_id_; }
  auto created_at() const { return created_at_; }

private:
  std::string user_id_;
  std::chrono::steady_clock::time_point created_at_;
  std::vector<ToolResult> messages_;                        // 追加写（ADR-0023）
  std::deque<TaskSession> task_sessions_;                   // deque 确保 current_task_session_ 地址稳定
  TaskSession* current_task_session_ = nullptr;             // 指向 deque 中元素
};

/// @brief 判断 ErrorCode 是否属于可重试类别
/// @note 设计决策 D10: 仅 Retry/Timeout/ResourceExhausted 递增 failure_count
inline bool is_retryable_error(ErrorCode code) {
  switch (code) {
    case ErrorCode::Retry:
    case ErrorCode::Timeout:
    case ErrorCode::ResourceExhausted:
      return true;
    default:
      return false;
  }
}

} // namespace agenticdsl

#endif // AGENTICDSL_CORE_TYPES_SESSION_H