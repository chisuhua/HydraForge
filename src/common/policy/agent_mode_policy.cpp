// src/common/policy/agent_mode_policy.cpp
// 功能描述：Agent 模式策略实现 (默认模式)
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#include "common/policy/agent_mode_policy.h"

namespace agenticdsl {

bool AgentModePolicy::requires_approval(const ToolMetadata& meta,
                                        const ToolCallContext& /*ctx*/) const {
  // Agent 模式遵循工具声明: force_approval_always 永远 true, 否则 requires_approval_in_agent
  return meta.approval.force_approval_always ||
         meta.approval.requires_approval_in_agent;
}

bool AgentModePolicy::can_skip(const ToolMetadata& meta,
                               const ToolCallContext& /*ctx*/) const {
  // ReadOnly 工具可跳过审批 (Agent 模式自动化)
  return meta.category == ToolCategory::ReadOnly &&
         !meta.approval.force_approval_always;
}

bool AgentModePolicy::request_approval(const ToolMetadata& meta,
                                       const ToolCallContext& ctx,
                                       const ToolPreview& preview,
                                       ApprovalCallback callback) const {
  // 仅在需要审批时调用 callback
  if (!requires_approval(meta, ctx)) {
    return true;  // 不需要审批, 直接放行
  }
  if (!callback) {
    return false;  // 防御: 无 callback 拒绝
  }
  ApprovalRequest req{
    meta.name,
    meta,
    ctx,
    preview,
    /*request_id=*/"agent-" + std::to_string(ctx.call_count_this_session)
  };
  return callback(req, /*timeout_ms=*/300000);  // 5 分钟超时
}

}  // namespace agenticdsl
