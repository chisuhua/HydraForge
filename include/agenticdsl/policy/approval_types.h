// agenticdsl/policy/approval_types.h
// 功能描述：审批机制相关类型定义 — ApprovalRequest / ToolPreview / ApprovalCallback
// 设计依据：ADR-0031 §决策 1 + §决策 5 (Oracle session ses_0ee867023ffeaSqWQXET5ESbAo,
//          sync callback 接口, transport 可插拔)
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "common/policy/execution_policy.h"

namespace agenticdsl {

/**
 * @brief 工具预览 — 用户审批时看到的变更摘要
 *
 * 由 executor 层在调用 IExecutionPolicy::request_approval 前构造.
 * 不包含敏感信息 (如密码), 仅用于用户决策 (defense-in-depth).
 */
struct ToolPreview {
  /// 完整命令 (如 `rm -rf /tmp/foo`)
  std::string command_line;
  /// 文件变更 diff (Edit/Write 工具)
  std::string diff_text;
  /// 受影响的文件路径列表
  std::vector<std::string> affected_paths;
  /// 预估执行时间 (秒, 0=未知)
  std::chrono::seconds estimated_duration{0};
  /// 风险摘要 (人工填写, 如 "删除 5 个文件")
  std::string risk_summary;
};

/**
 * @brief 审批请求 — 传给 ApprovalCallback 的唯一入参
 *
 * 包含工具元数据, 调用上下文, 预览信息和唯一 request_id.
 * request_id 由 executor 层生成 (UUID 或 "policy-call_count_this_session").
 */
struct ApprovalRequest {
  /// 工具名称 (与 ToolMetadata::name 一致)
  std::string tool_name;
  /// 工具元数据快照
  ToolMetadata meta;
  /// 工具调用上下文快照
  ToolCallContext ctx;
  /// 工具预览
  ToolPreview preview;
  /// 唯一 request_id (callback 用于关联响应)
  std::string request_id;
  /// 请求创建时间 (用于超时计算)
  std::chrono::steady_clock::time_point created_at{
    std::chrono::steady_clock::now()
  };
};

/**
 * @brief 审批回调 — Oracle 决议 sync callback 接口
 *
 * Executor 层在调用 IExecutionPolicy::request_approval 前注入此 callback.
 * callback 实现可选用不同 transport: stdin / IInteractionBus / 测试桩.
 *
 * **不实现 EventBus request_id 关联基础设施** (Oracle 决议):
 * 当前 IInteractionBus 只有 emit/subscribe, 无 request/response 关联原语.
 * 净造基础设施不优于 callback 接口. ADR-0030 协程落地后可包成协程 wrapper.
 *
 * @param req 审批请求 (含 tool_name, meta, ctx, preview, request_id)
 * @param timeout_ms 超时时间 (ms), callback 必须在超时前返回, 否则视为拒绝
 * @return true=approved, false=denied (含超时, defense-in-depth)
 */
using ApprovalCallback =
    std::function<bool(const ApprovalRequest& req, int timeout_ms)>;

}  // namespace agenticdsl
