// src/common/policy/plan_mode_policy.cpp
// 功能描述：PlanModePolicy实现文件。requires_approval方法实现：除
// ReadOnly 外所有工具调用均需要审批。
// 设计依据：ADR-0031 §2
// 作者：AgenticDSL Phase3 / Track A
// 最后修改日期：2026-06-10
#include "common/policy/plan_mode_policy.h"

namespace agenticdsl {

// Plan模式：所有非只读工具调用都需要人工审批
// 此处 ctx 参数未使用——Plan模式决策仅依赖工具自身分类，与上下文无关
bool PlanModePolicy::requires_approval(const ToolMetadata& meta,
 const ToolCallContext& /*ctx*/) const {
 return meta.category != ToolCategory::ReadOnly;
}

} // namespace agenticdsl
