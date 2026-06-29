// src/common/policy/yolo_mode_policy.cpp
// 功能描述：YOLO 模式策略实现 (高吞吐模式)
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#include "common/policy/yolo_mode_policy.h"

namespace agenticdsl {

bool YoloModePolicy::requires_approval(const ToolMetadata& meta,
                                       const ToolCallContext& /*ctx*/) const {
  // YOLO 模式仅 force_approval_always 工具需要审批 (defense-in-depth floor)
  // 注意: 任何非 force_approval_always 工具均不需要审批 (包括 WriteFile/Execute/Network)
  return meta.approval.force_approval_always;
}

bool YoloModePolicy::can_skip(const ToolMetadata& meta,
                              const ToolCallContext& /*ctx*/) const {
  // YOLO 模式可跳过所有审批 (除 force_approval_always)
  return !meta.approval.force_approval_always;
}

bool YoloModePolicy::request_approval(const ToolMetadata& meta,
                                      const ToolCallContext& ctx,
                                      const ToolPreview& preview,
                                      ApprovalCallback callback) const {
  // YOLO 模式仅在 force_approval_always 时调用 callback
  if (!requires_approval(meta, ctx)) {
    return true;  // 不需要审批, 直接放行
  }
  if (!callback) {
    return false;  // 防御: 无 callback 拒绝 (即使是 YOLO 也尊重 floor)
  }
  ApprovalRequest req{
    meta.name,
    meta,
    ctx,
    preview,
    /*request_id=*/"yolo-" + std::to_string(ctx.call_count_this_session)
  };
  return callback(req, /*timeout_ms=*/300000);  // 5 分钟超时
}

}  // namespace agenticdsl
