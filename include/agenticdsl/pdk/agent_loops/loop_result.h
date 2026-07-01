// include/agenticdsl/pdk/agent_loops/loop_result.h
// 文件头注释
// 功能描述：LoopResult — PDK Agent 循环 (React / PlanExecute / ForkJoin) 的统一返回值。
//          包含成功标志 + 消息 + 最终 LayeredContext + 重试计数 + 总步数 + 失败阶段。
//          所有 Loop 类型的 run() 方法返回此类型 (Sprint 4 React 路径已 ship,
//          Sprint 20 扩展 PlanExecute / ForkJoin, ADR-0021 §3.2)。
// 设计依据：ADR-0021 §3.2 + ADR-0008 LayeredContext
//          + openspec/changes/pdk-plan-execute-fork-join
// 作者：AgenticDSL Phase 1 Sprint 20
// 最后修改日期：2026-08-01

#pragma once

#include "agenticdsl/types/layered_context.h"

#include <optional>
#include <string>

namespace hydraforge::pdk {

/**
 * @brief Agent 循环统一返回值 (Sprint 4 React 已 ship, Sprint 20 扩展 PlanExecute/ForkJoin)
 *
 * 字段语义:
 *  - success:        循环是否成功结束 (Done 状态为 true, Retry/整体失败为 false)
 *  - message:        成功/失败描述 (失败时包含失败原因)
 *  - final_context:  最终 LayeredContext (5 层结构化, ADR-0008)
 *                    PlanExecute: 累计 working 层数据
 *                    ForkJoin:    merge 后的 working 层 (并集, branch_id 排序, 后覆盖前)
 *                    React:       保持单轮 ToolResult 序列化到 working.data
 *  - retries_used:   PlanExecute 重试次数 (0 表示首次即成功)
 *  - total_steps:    循环总步数 (PlanExecute = 1+retries_used, ForkJoin = branches.size)
 *  - failed_phase:   失败时记录哪一阶段 (Plan / Execute / Verify / Fork / Join),
 *                    成功时 std::nullopt
 *
 * PDK 静态链接到插件 (P3), 此结构体保持 POD-ish 兼容 (无虚函数, 仅 nlohmann::json
 * 与 LayeredContext 字段), 避免引入额外依赖。
 */
struct LoopResult {
  bool success = false;
  std::string message;
  agenticdsl::LayeredContext final_context;
  int retries_used = 0;
  int total_steps = 0;
  std::optional<std::string> failed_phase;
};

} // namespace hydraforge::pdk