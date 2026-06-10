// src/common/policy/yolo_mode_policy.cpp
// 功能描述：YoloModePolicy实现文件。requires_approval方法实现：仅当
// force_approval_always=true 时才要求审批——这是 YOLO模式的安全底线，
// 用于 delete_file / rm -rf 等危险工具仍然需要用户确认。
// 设计依据：ADR-0031 §2
// 作者：AgenticDSL Phase3 / Track A
// 最后修改日期：2026-06-10
#include "common/policy/yolo_mode_policy.h"

namespace agenticdsl {

// YOLO模式：仅 force_approval_always标记的工具触发审批
// 这是 YOLO模式的安全底线，确保 delete_file / rm -rf 等危险操作
//不会被静默执行
// 此处 ctx 参数未使用——YOLO模式决策仅依赖 force_approval_always标志
bool YoloModePolicy::requires_approval(const ToolMetadata& meta,
 const ToolCallContext& /*ctx*/) const {
 return meta.approval.force_approval_always;
}

} // namespace agenticdsl
