// src/common/policy/plan_mode_policy.cpp
// 功能描述：Plan 模式策略实现
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#include "common/policy/plan_mode_policy.h"

namespace agenticdsl {

bool PlanModePolicy::requires_approval(const ToolMetadata& meta,
                                       const ToolCallContext& /*ctx*/) const {
  // Plan 模式保守: 非 ReadOnly 工具 + force_approval_always 都需要审批
  return meta.approval.force_approval_always ||
         meta.category != ToolCategory::ReadOnly;
}

bool PlanModePolicy::request_approval(const ToolMetadata& meta,
                                      const ToolCallContext& ctx,
                                      const ToolPreview& preview,
                                      ApprovalCallback callback) const {
  // Plan 模式始终调用 callback (executor 层注入 transport)
  if (!callback) {
    return false;  // 防御: 无 callback 拒绝 (defense-in-depth)
  }
  ApprovalRequest req{
    meta.name,
    meta,
    ctx,
    preview,
    /*request_id=*/"plan-" + std::to_string(ctx.call_count_this_session)
  };
  return callback(req, /*timeout_ms=*/300000);  // 5 分钟超时
}

}  // namespace agenticdsl
