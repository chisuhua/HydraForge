// src/common/policy/agent_mode_policy.h
// 功能描述：Agent 模式执行策略 (C3 5-method 重写版, **默认模式**)
// 设计依据：ADR-0031 §决策 1 (Oracle 5-method 接口) + Oracle 决议 Agent 默认
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#pragma once

#include "agenticdsl/policy/iexecution_policy.h"
#include "agenticdsl/policy/mode_config.h"

namespace agenticdsl {

/**
 * @brief Agent 模式策略 — **默认模式** (Oracle 决议)
 *
 * C3 5-method 重写 (Oracle session ses_0ee867023ffeaSqWQXET5ESbAo):
 * - requires_approval: meta.approval.requires_approval_in_agent || force_approval_always
 * - should_execute=true (自动执行)
 * - can_skip: ReadOnly 工具可跳过审批
 * - get_layer: ReadOnly→Workflow, 其他→Workflow (Agent 模式无 Cognitive 调用)
 * - request_approval: 仅在 requires_approval 时调用 callback, 否则直接返回 true
 *
 * ModeConfig (per-mode 常量): AgentModeConfig{show_plan=false, auto_decide_retry=true, fleet_max_concurrency=16}
 */
class AgentModePolicy : public IExecutionPolicy {
 public:
  // ===== 4 per-call 决策 =====

  /// @brief 遵循工具 ApprovalPolicy (force_approval_always 永远 true)
  bool requires_approval(const ToolMetadata& meta,
                         const ToolCallContext& /*ctx*/) const override;

  /// @brief Agent 模式自动执行
  bool should_execute(const ToolMetadata& /*meta*/,
                      const ToolCallContext& /*ctx*/) const override {
    return true;
  }

  /// @brief ReadOnly 工具可跳过审批
  bool can_skip(const ToolMetadata& meta,
                const ToolCallContext& /*ctx*/) const override;

  /// @brief Agent 模式全 Workflow 层 (ReadOnly 工具也 Workflow)
  LayerProfile get_layer(const ToolMetadata& /*meta*/) const override {
    return LayerProfile::Workflow;
  }

  // ===== 1 approval 同步回调 =====

  /// @brief 仅在 requires_approval 时调用 callback, 否则直接 true
  bool request_approval(const ToolMetadata& meta,
                        const ToolCallContext& ctx,
                        const ToolPreview& preview,
                        ApprovalCallback callback) const override;

  // ===== 静态配置访问 =====

  /// @brief 暴露 ModeConfig (Oracle 决议: 默认 Agent)
  static constexpr ModeConfig config() { return AgentModeConfig; }
};

}  // namespace agenticdsl
