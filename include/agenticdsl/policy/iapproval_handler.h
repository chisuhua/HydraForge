// agenticdsl/policy/iapproval_handler.h
// 功能描述：审批处理器抽象接口 — 解耦 NodeExecutor 与具体 ApprovalHandler 实现
// 设计依据：ADR-0019 §1.4 (依赖抽象) + ADR-0031 §决策 5 (approval handler 抽象层)
// 作者：AgenticDSL Pre-Phase / Sprint 19
// 最后修改日期：2026-07-31
#pragma once

#include "agenticdsl/policy/iexecution_policy.h"  // 复用值类型: ToolMetadata, ToolCallContext, ToolPreview

namespace agenticdsl {

/**
 * @brief 审批处理器抽象接口
 *
 * `NodeExecutor` 通过此接口持有审批处理器，不再直接依赖具体类
 * `ApprovalHandler`。新增实现只需继承此接口，不修改 executor 模块。
 *
 * **签名说明**：参数 `preview` 使用 `const ToolPreview&` (与既有
 * `ApprovalHandler::process_request` 一致). proposal.md 早期版本误写为
 * `std::string& preview`，以实际接口为准。
 *
 * **生命周期**：实现类由外部 (DSLEngine) 持有所有权，`NodeExecutor` 仅持
 * `IApprovalHandler*` 非拥有指针 (nullptr 跳过审批路径).
 */
class IApprovalHandler {
 public:
  virtual ~IApprovalHandler() = default;

  /**
   * @brief 处理一次工具调用的审批请求
   *
   * @param meta 工具元数据
   * @param ctx  工具调用上下文
   * @param preview 工具预览 (diff_text / command_line / 等)
   * @return true=approve, false=deny/timeout
   */
  virtual bool process_request(const ToolMetadata& meta,
                               const ToolCallContext& ctx,
                               const ToolPreview& preview) = 0;
};

}  // namespace agenticdsl