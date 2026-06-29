// src/common/policy/plan_mode_policy.h
// 功能描述：Plan 模式执行策略 (C3 5-method 重写版)
// 设计依据：ADR-0031 §决策 1 (Oracle 推荐, 5-method 接口) + ModeConfig
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#pragma once

#include <string>

#include "agenticdsl/policy/iexecution_policy.h"
#include "agenticdsl/policy/mode_config.h"

namespace agenticdsl {

/**
 * @brief Plan 模式策略 — 保守的人工审批模式
 *
 * C3 5-method 重写 (Oracle 决议 ses_0ee867023ffeaSqWQXET5ESbAo):
 * - 所有非 ReadOnly 工具需审批
 * - Plan 模式 should_execute=false (等用户确认每步)
 * - can_skip=false (ReadOnly 也需要审批以显示计划)
 * - get_layer=Workflow (无 fleet 模式)
 * - request_approval: meta.approval.force_approval_always || meta.category != ReadOnly → 强制审批
 *
 * ModeConfig (per-mode 常量): PlanModeConfig{show_plan=true, ...} (从虚接口移出)
 */
class PlanModePolicy : public IExecutionPolicy {
 public:
  // ===== 4 per-call 决策 =====

  /// @brief 非 ReadOnly 需审批 (Plan 模式保守)
  bool requires_approval(const ToolMetadata& meta,
                         const ToolCallContext& /*ctx*/) const override;

  /// @brief Plan 模式不自动执行 (等用户确认)
  bool should_execute(const ToolMetadata& /*meta*/,
                      const ToolCallContext& /*ctx*/) const override {
    return false;
  }

  /// @brief Plan 模式不可跳过 (即使 ReadOnly 也需确认)
  bool can_skip(const ToolMetadata& /*meta*/,
                const ToolCallContext& /*ctx*/) const override {
    return false;
  }

  /// @brief Plan 模式全 Workflow 层
  LayerProfile get_layer(const ToolMetadata& /*meta*/) const override {
    return LayerProfile::Workflow;
  }

  // ===== 1 approval 同步回调 =====

  /// @brief Plan 模式始终请求审批 (调用 callback)
  bool request_approval(const ToolMetadata& meta,
                        const ToolCallContext& ctx,
                        const ToolPreview& preview,
                        ApprovalCallback callback) const override;

  // ===== 静态配置访问 =====

  /// @brief 暴露 ModeConfig (从虚接口移出的常量)
  static constexpr ModeConfig config() { return PlanModeConfig; }
};

}  // namespace agenticdsl
