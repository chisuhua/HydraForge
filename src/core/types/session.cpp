// src/core/types/session.cpp
// 功能描述：ADR-0033 Session Hierarchy — 三层会话模型方法实现
// 设计依据：ADR-0033 + OpenSpec change 2026-06-26-adr-0033-session-hierarchy
//           Oracle 审查: ses_0e28703caffeBUB7tgDqsClZiw (2026-07-01)
//           Metis 审查: ses_0e26217edffeHrMpGUPLuhTecP (2026-07-01)
// 作者：AgenticDSL Sprint 15 (C5)
// 最后修改日期：2026-07-02

#include "core/types/session.h"
#include <stdexcept>

namespace agenticdsl {

// ============================================================
// SubtaskSession
// ============================================================

SubtaskSession SubtaskSession::make(const std::string& path, Context initial) {
  SubtaskSession s;
  s.branch_path = path;
  s.initial_context = std::move(initial);
  s.status = "pending";
  s.started_at = std::chrono::steady_clock::now();
  return s;
}

// ============================================================
// TaskSession
// ============================================================

TaskSession::TaskSession(UserSession& user_sess)
    : user_session_(user_sess), status_("active") {}

SubtaskSession& TaskSession::create_subtask(const std::string& branch_path,
                                             Context initial_context) {
  auto& sub = subtask_sessions_.emplace_back(SubtaskSession::make(branch_path, std::move(initial_context)));
  sub.status = "running";
  return sub; // deque::emplace_back 不使已有引用失效
}

void TaskSession::archive_subtask_result(const SubtaskSession& subtask) {
  // 查找并更新同一 branch_path 的 running SubtaskSession (地址稳定, 原地覆盖)
  for (auto& existing : subtask_sessions_) {
    if (existing.branch_path == subtask.branch_path &&
        existing.status == "running") {
      existing = subtask;
      existing.completed_at = std::chrono::steady_clock::now();
      return;
    }
  }
  // Fallback: 未找到匹配的 running 条目，追加
  SubtaskSession copy = subtask;
  copy.completed_at = std::chrono::steady_clock::now();
  subtask_sessions_.push_back(std::move(copy));
}

void TaskSession::set_policy(std::shared_ptr<IExecutionPolicy> policy) {
  current_policy_ = std::move(policy);
}

void TaskSession::record_failure(const ExecutionResult& result) {
  // 仅在可重试错误时递增 (P9 error-taxonomy-execution-boundary)
  if (result.is_retryable()) {
    ++failure_count_;
  }
}

TaskSession::~TaskSession() {
  // 显式析构：清理 subtask_sessions_ 与 context_
  // deque<SubtaskSession> 自动析构
  // Context (nlohmann::json) 自动析构
}

TaskSession::FailureMode TaskSession::determine_failure_mode() const {
  if (failure_count_ < 3) {
    return FailureMode::KeepSession;
  }
  return FailureMode::NewSession;
}

// ============================================================
// UserSession
// ============================================================

UserSession::UserSession(std::string user_id)
    : user_id_(std::move(user_id)),
      created_at_(std::chrono::steady_clock::now()) {}

void UserSession::append_message(ToolResult msg) {
  messages_.push_back(std::move(msg));
}

TaskSession& UserSession::create_task_session() {
  // 自动裁剪：超过 100 条历史时清理最旧的一项
  // deque::pop_front 不使其他元素引用失效
  if (task_sessions_.size() >= 100) {
    task_sessions_.pop_front();
  }

  auto& ts = task_sessions_.emplace_back(*this);
  current_task_session_ = &ts;
  return ts; // deque::emplace_back 不使已有引用失效
}

UserSession::~UserSession() {
  // 显式析构：清理 current_task_session_ 裸指针引用
  // task_sessions_ (deque) 自动析构
  current_task_session_ = nullptr;
}

} // namespace agenticdsl