// src/common/tools/tool_coordinator.h
// 功能描述：ToolCoordinator — standalone middleware (Oracle Option C)
// 设计依据：ADR-0031 §决策 5 (Oracle session ses_0ed4408faffeLv8VfrC0s5PzW7, 2026-06-29)
//          + §决策 4 (Layer check) + §决策 7 (Audit log)
// 作者：AgenticDSL Phase3 / Sprint 14 C4 ship
// 最后修改日期：2026-06-29
#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/policy/iexecution_policy.h"
#include "common/policy/approval_handler.h"
#include "common/tools/registry.h"  // IToolRegistry + ToolResult

namespace agenticdsl {

/**
 * @brief ToolCoordinator — standalone middleware (ADR-0031 §决策 5)
 *
 * 内部流程:
 * 1. check_layer_permission(meta, ctx) → denied return ToolResult::error
 * 2. ApprovalHandler::process_request(meta, ctx, preview) → denied return ToolResult::error
 * 3. emit "tool.audit.invoked"
 * 4. registry.call_tool()
 * 5. emit "tool.audit.completed"
 * 6. return ToolResult
 */
class ToolCoordinator {
 public:
  /**
   * @brief 构造函数
   * @param registry IToolRegistry 引用 (non-owning)
   * @param policy IExecutionPolicy shared_ptr (委托给内部 ApprovalHandler)
   * @param callback ApprovalCallback (传入内部 ApprovalHandler)
   * @param bus IInteractionBus shared_ptr (可选, null=skip audit emit)
   * @param default_timeout_ms ApprovalHandler 默认超时 (ms, 默认 300000 = 5 分钟)
   */
  ToolCoordinator(IToolRegistry& registry,
                  std::shared_ptr<IExecutionPolicy> policy,
                  ApprovalCallback callback,
                  std::shared_ptr<IInteractionBus> bus = nullptr,
                  int default_timeout_ms = 300000);

  /**
   * @brief 主入口 — 执行一次工具调用 (含 layer + approval + audit)
   *
   * @param meta 工具元数据 (ToolMetadata V2)
   * @param ctx 工具调用上下文 (ToolCallContext)
   * @param args 工具参数
   * @return ToolResult (执行结果或错误)
   */
  ToolResult execute(const ToolMetadata& meta,
                     const ToolCallContext& ctx,
                     const std::unordered_map<std::string, std::string>& args);

 private:
  IToolRegistry& registry_;
  std::shared_ptr<IExecutionPolicy> policy_;
  std::unique_ptr<ApprovalHandler> approval_handler_;
  std::shared_ptr<IInteractionBus> bus_;
};

}  // namespace agenticdsl