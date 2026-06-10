// src/common/policy/agent_mode_policy.cpp
// 功能描述：AgentModePolicy实现文件。requires_approval方法实现：完全
//遵循工具元数据中 approval.requires_approval_in_agent字段的设置。
// 设计依据：ADR-0031 §2
// 作者：AgenticDSL Phase3 / Track A
// 最后修改日期：2026-06-10
#include "common/policy/agent_mode_policy.h"

namespace agenticdsl {

// Agent模式：直接读取工具自带的 Agent模式审批策略
// 此处 ctx 参数未使用——Agent模式决策完全由工具元数据声明
bool AgentModePolicy::requires_approval(const ToolMetadata& meta,
 const ToolCallContext& /*ctx*/) const {
 return meta.approval.requires_approval_in_agent;
}

} // namespace agenticdsl
