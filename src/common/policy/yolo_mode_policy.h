// src/common/policy/yolo_mode_policy.h
// 功能描述：YOLO模式执行策略。几乎全自动——仅 force_approval_always
//标记的工具（如 delete_file）才需要审批；自动执行；不展示计划
//与结果摘要；自动重试；不展示反思；舰队模式最大并发度为32。
// 设计依据：ADR-0031 §2 / ADR-0027 (YOLO模式定义)
// 作者：AgenticDSL Phase3 / Track A
// 最后修改日期：2026-06-10
#pragma once

#include "agenticdsl/policy/iexecution_policy.h"

namespace agenticdsl {

/**
 * @brief YOLO模式策略：全自动的高吞吐模式
 *
 *适用场景：CI/CD流水线、批量任务处理、用户对工具集完全信任
 *关键行为：
 * - 仅 force_approval_always=true 的工具需要审批（安全底线）
 * - 自动执行（零确认）
 * - 不展示计划与结果摘要（最大吞吐）
 * - IPERReflect 后自动重试
 * - 不展示反思内容（内部记录）
 * -舰队并发度上限为32（激进）
 *
 *警告：YOLO模式是安全敏感度最低的模式，应当仅在用户明确知情的
 *情况下启用（参见 ADR-0031 §6模式切换安全转换）。
 */
class YoloModePolicy : public IExecutionPolicy {
 public:
 bool requires_approval(const ToolMetadata& meta,
 const ToolCallContext& ctx) const override;

 bool should_auto_execute() const override { return true; }
 bool should_show_plan() const override { return false; }
 bool should_show_result_summary() const override { return false; }

 std::string mode_name() const override { return "yolo"; }

 bool should_auto_decide_retry() const override { return true; }
 bool should_show_reflection() const override { return false; }

 size_t fleet_max_concurrency() const override { return 32; }
};

} // namespace agenticdsl
