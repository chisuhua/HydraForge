// src/common/policy/approval_handler.h
// 功能描述：ApprovalHandler — 整合 IExecutionPolicy + ApprovalCallback
// 设计依据：ADR-0031 §决策 5 (Oracle session ses_0ee867023ffeaSqWQXET5ESbAo)
// Sprint 19 (OpenSpec change pimpl-node-executor-h):
//   继承 IApprovalHandler 抽象接口 — NodeExecutor 通过抽象持有, 解耦 executor 与具体类
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship + Sprint 19
// 最后修改日期：2026-07-31
#pragma once

#include <memory>
#include <string>

#include "agenticdsl/policy/iapproval_handler.h"

namespace agenticdsl {

/**
 * @brief 审批处理器 — 整合 policy 决策 + callback 调用
 *
 * 流程 (C3 tasks §6.3):
 * 1. 调用 policy.requires_approval(meta, ctx) 判断是否需要审批
 * 2. 若不需要审批, 直接返回 true (auto-approve)
 * 3. 若需要审批, 构造 ApprovalRequest 并调用注入的 callback
 * 4. callback 超时视为拒绝 (defense-in-depth)
 *
 * 集成方式: 由 NodeExecutor 在 tool 调用前调用 process_request()
 *
 * Sprint 19: 继承 IApprovalHandler — NodeExecutor 仅持有抽象指针
 */
class ApprovalHandler : public IApprovalHandler {
 public:
  /**
   * @brief 构造函数
   * @param policy IExecutionPolicy 实例 (PolicyFactory::create() 返回)
   * @param callback ApprovalCallback (make_tui_stdin_callback 或 make_event_bus_callback)
   * @param default_timeout_ms callback 默认超时 (ms), 默认 300000 = 5 分钟
   */
  explicit ApprovalHandler(
      std::shared_ptr<IExecutionPolicy> policy,
      ApprovalCallback callback,
      int default_timeout_ms = 300000);

  /**
   * @brief 处理一次工具调用的审批请求 (Sprint 19: override IApprovalHandler)
   *
   * 流程:
   * 1. policy.requires_approval(meta, ctx) -> 是否需要审批
   * 2. policy.can_skip(meta, ctx) -> 是否可跳过审批
   * 3. policy.should_execute(meta, ctx) -> 是否应执行 (Plan 模式可能 false)
   * 4. 构造 ApprovalRequest + 调用 callback(req, timeout)
   *
   * @param meta 工具元数据
   * @param ctx 工具调用上下文
   * @param preview 工具预览 (diff_text / command_line / 等)
   * @return true=approved/auto-approved, false=denied/timeout
   */
  bool process_request(const ToolMetadata& meta,
                       const ToolCallContext& ctx,
                       const ToolPreview& preview) override;

 private:
  std::shared_ptr<IExecutionPolicy> policy_;
  ApprovalCallback callback_;
  int default_timeout_ms_;
};

}  // namespace agenticdsl
