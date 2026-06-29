// agenticdsl/policy/iexecution_policy.h
// 功能描述：执行策略抽象接口 (Oracle 5-method 版, C3 ship)
// 设计依据：ADR-0031 §决策 1 (Oracle 推荐, 2026-06-26)
// Oracle session: ses_0faa4dabeffeHGFoLdXE7AqwH7
// 取代 8-method stub (should_auto_execute / should_show_plan / 等移入 ModeConfig)
// 配套值类型 (ToolMetadata / ToolCallContext / ToolCategory / LayerProfile)
// 规范定义位于 src/common/policy/execution_policy.h, 本头文件通过 #include 引入。
// 作者：AgenticDSL Pre-Phase / Sprint 13
// 最后修改日期：2026-06-26
#pragma once

#include <string>

#include "approval_types.h"
#include "common/policy/execution_policy.h"  // value types: ToolMetadata, ToolCallContext, ToolCategory, LayerProfile

namespace agenticdsl {

/**
 * @brief 执行策略抽象接口 — Oracle 5-method 版 (C3 ship)
 *
 * 设计依据：ADR-0031 §决策 1 (Oracle 推荐, 2026-06-26)
 * 取代 8-method stub (should_auto_execute / should_show_plan / 等移入 ModeConfig)
 */
class IExecutionPolicy {
 public:
  virtual ~IExecutionPolicy() = default;

  // ===== 4 per-call 决策 (核心) =====

  /// @brief 此工具调用是否需要用户审批？
  virtual bool requires_approval(const ToolMetadata& meta,
                                 const ToolCallContext& ctx) const = 0;

  /// @brief 是否应执行 (Plan 模式返回 false 让用户确认)
  virtual bool should_execute(const ToolMetadata& meta,
                              const ToolCallContext& ctx) const = 0;

  /// @brief 是否可跳过 (ReadOnly 工具可跳过审批直接执行)
  virtual bool can_skip(const ToolMetadata& meta,
                        const ToolCallContext& ctx) const = 0;

  /// @brief 工具所属的 Layer Profile (用于 ToolCoordinator 集成, C4)
  virtual LayerProfile get_layer(const ToolMetadata& meta) const = 0;

  // ===== 1 approval 同步回调 (Oracle 5th method) =====

  /// @brief 同步请求用户审批 (callback 由 executor 层注入, transport 可插拔)
  /// @return true=approved, false=denied (含 timeout)
  virtual bool request_approval(const ToolMetadata& meta,
                                const ToolCallContext& ctx,
                                const ToolPreview& preview,
                                ApprovalCallback callback) const = 0;
};

}  // namespace agenticdsl