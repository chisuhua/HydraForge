// src/common/policy/agent_mode_policy.h
// 功能描述：Agent模式执行策略。遵循工具自身的 ApprovalPolicy审批策略；
//自动执行（不需每步确认）；不展示完整计划但展示结果摘要；自动重试；
//不展示反思；舰队模式最大并发度为16。
// 设计依据：ADR-0031 §2 / ADR-0027 (Agent模式定义)
// 作者：AgenticDSL Phase3 / Track A
// 最后修改日期：2026-06-10
#pragma once

#include "agenticdsl/policy/iexecution_policy.h"

namespace agenticdsl {

/**
 * @brief Agent模式策略：平衡的半自动模式
 *
 *适用场景：用户信任 Agent 后，日常任务以中等速度执行
 *关键行为：
 * -遵循工具元数据中声明的 approval.requires_approval_in_agent字段
 * - 自动执行（无需每步确认）
 * - 不展示完整计划（用户已熟悉任务）
 * - 执行后展示结果摘要
 * - IPERReflect 后自动重试
 * - 不展示反思内容（内部记录）
 * -舰队并发度上限为16（中等）
 */
class AgentModePolicy : public IExecutionPolicy {
 public:
 bool requires_approval(const ToolMetadata& meta,
 const ToolCallContext& ctx) const override;

 bool should_auto_execute() const override { return true; }
 bool should_show_plan() const override { return false; }
 bool should_show_result_summary() const override { return true; }

 std::string mode_name() const override { return "agent"; }

 bool should_auto_decide_retry() const override { return true; }
 bool should_show_reflection() const override { return false; }

 size_t fleet_max_concurrency() const override { return 16; }
};

} // namespace agenticdsl
