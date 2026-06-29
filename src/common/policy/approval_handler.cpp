// src/common/policy/approval_handler.cpp
// 功能描述：ApprovalHandler 实现
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#include "common/policy/approval_handler.h"

#include <atomic>
#include <chrono>
#include <string>

namespace agenticdsl {

namespace {

// request_id 生成器 (简单自增, 测试可 mock)
std::atomic<std::uint64_t> request_counter{0};

std::string generate_request_id(const std::string& prefix) {
  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return prefix + "-" + std::to_string(now) + "-" +
         std::to_string(request_counter++);
}

}  // namespace

ApprovalHandler::ApprovalHandler(
    std::shared_ptr<IExecutionPolicy> policy,
    ApprovalCallback callback,
    int default_timeout_ms)
    : policy_(std::move(policy)),
      callback_(std::move(callback)),
      default_timeout_ms_(default_timeout_ms) {}

bool ApprovalHandler::process_request(const ToolMetadata& meta,
                                     const ToolCallContext& ctx,
                                     const ToolPreview& preview) {
  if (!policy_) {
    return false;  // 防御: 无 policy 拒绝
  }

  // Step 1: 检查 should_execute (Plan 模式可能 false)
  if (!policy_->should_execute(meta, ctx)) {
    return false;  // Plan 模式不自动执行
  }

  // Step 2: 检查 can_skip (ReadOnly 工具可跳过审批)
  if (policy_->can_skip(meta, ctx)) {
    return true;  // 跳过审批直接放行
  }

  // Step 3: 检查 requires_approval
  if (!policy_->requires_approval(meta, ctx)) {
    return true;  // 不需要审批直接放行
  }

  // Step 4: 需要审批, 构造 ApprovalRequest + 调用 callback
  if (!callback_) {
    return false;  // 防御: 无 callback 拒绝 (defense-in-depth)
  }

  // 构造 ApprovalRequest (request_id 来自 ctx 或生成新)
  std::string request_id = ctx.session_id.empty()
                              ? generate_request_id("req")
                              : ctx.session_id + "-" + std::to_string(ctx.call_count_this_session);

  ApprovalRequest req{
    meta.name,
    meta,
    ctx,
    preview,
    request_id,
    std::chrono::steady_clock::now()
  };

  return callback_(req, default_timeout_ms_);
}

}  // namespace agenticdsl
