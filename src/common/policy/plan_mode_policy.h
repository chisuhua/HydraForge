// src/common/policy/plan_mode_policy.h
// 功能描述：Plan模式执行策略。所有写入类工具调用均需要用户审批；执行前
//必须展示完整计划；执行后展示结果摘要；不自动重试；展示反思内容；
//舰队模式最大并发度为8。
// 设计依据：ADR-0031 §2 / ADR-0027 (Plan模式定义)
// 作者：AgenticDSL Phase3 / Track A
// 最后修改日期：2026-06-10
#pragma once

#include "agenticdsl/policy/iexecution_policy.h"

namespace agenticdsl {

/**
 * @brief Plan模式策略：保守的人工审批模式
 *
 *适用场景：用户首次接触一项任务，需要看清 Agent 将要做什么并人工放行每一步
 *关键行为：
 * - 所有非 ReadOnly工具调用都需要人工审批
 * - 不自动执行（每一步等待用户确认）
 * - 执行前必须展示完整计划
 * - 执行后展示结果摘要
 * - IPERReflect 后不自动重试，询问用户
 * -展示反思内容供用户审查
 * -舰队并发度上限为8（保守）
 */
class PlanModePolicy : public IExecutionPolicy {
 public:
 bool requires_approval(const ToolMetadata& meta,
 const ToolCallContext& ctx) const override;

 bool should_auto_execute() const override { return false; }
 bool should_show_plan() const override { return true; }
 bool should_show_result_summary() const override { return true; }

 std::string mode_name() const override { return "plan"; }

 bool should_auto_decide_retry() const override { return false; }
 bool should_show_reflection() const override { return true; }

 size_t fleet_max_concurrency() const override { return 8; }
};

} // namespace agenticdsl
