// src/common/policy/yolo_mode_policy.h
// 功能描述：YOLO 模式执行策略 (C3 5-method 重写版)
// 设计依据：ADR-0031 §决策 1 (Oracle 5-method 接口) + §决策 6 (YOLO 切换需确认)
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#pragma once

#include "agenticdsl/policy/iexecution_policy.h"
#include "agenticdsl/policy/mode_config.h"

namespace agenticdsl {

/**
 * @brief YOLO 模式策略 — 高吞吐全自动模式
 *
 * C3 5-method 重写 (Oracle session ses_0ee867023ffeaSqWQXET5ESbAo):
 * - requires_approval: 仅 force_approval_always 工具 (defense-in-depth floor)
 * - should_execute=true (零确认)
 * - can_skip=true (所有工具可跳过审批)
 * - get_layer=Workflow
 * - request_approval: 仅 force_approval_always 时调用 callback (其他直接 true)
 *
 * ⚠️ 安全警告: YOLO 模式安全敏感度最低. 切换需要 ModeSwitchDialog 用户确认
 * (ADR-0031 §决策 6, ModeSwitchDialog::confirm_yolo_switch()).
 *
 * ModeConfig: YoloModeConfig{fleet_max_concurrency=32, auto_decide_retry=true}
 */
class YoloModePolicy : public IExecutionPolicy {
 public:
  // ===== 4 per-call 决策 =====

  /// @brief 仅 force_approval_always 工具需要审批 (defense-in-depth floor)
  bool requires_approval(const ToolMetadata& meta,
                         const ToolCallContext& /*ctx*/) const override;

  /// @brief YOLO 模式自动执行
  bool should_execute(const ToolMetadata& /*meta*/,
                      const ToolCallContext& /*ctx*/) const override {
    return true;
  }

  /// @brief YOLO 模式可跳过所有审批 (除 force_approval_always)
  bool can_skip(const ToolMetadata& meta,
                const ToolCallContext& /*ctx*/) const override;

  /// @brief YOLO 模式全 Workflow 层 (高吞吐)
  LayerProfile get_layer(const ToolMetadata& /*meta*/) const override {
    return LayerProfile::Workflow;
  }

  // ===== 1 approval 同步回调 =====

  /// @brief 仅 force_approval_always 时调用 callback
  bool request_approval(const ToolMetadata& meta,
                        const ToolCallContext& ctx,
                        const ToolPreview& preview,
                        ApprovalCallback callback) const override;

  // ===== 静态配置访问 =====

  /// @brief 暴露 ModeConfig (YOLO 高并发)
  static constexpr ModeConfig config() { return YoloModeConfig; }
};

}  // namespace agenticdsl
