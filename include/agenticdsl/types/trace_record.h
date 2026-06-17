// agenticdsl/types/trace_record.h
// 功能描述：TraceRecord data-only struct 定义 (从 src/modules/trace/trace_exporter.h 迁移)。
//           仅含字段, 不含方法; TraceExporter 行为保留在原 modules/trace/ 下。
//           迁移目的: ADR-0019 §1.4 "0 跨模块 include" 退出标准 — engine.h 不再
//           依赖 modules/trace/trace_exporter.h, 改依赖本 types 头文件。
// 设计依据：ADR-0019 §1.4 (IInteractionBus MVP, engine.h 解耦退出标准)
//           + ADR-0033 §2 (Session 层级体系 — TraceRecord 是 SubtaskSession 的关联对象)
//           + openspec/changes/2026-06-15-residual-engine-h-decoupling T3
// 作者：AgenticDSL Phase 1 P1.T3
// 最后修改日期：2026-06-18

#pragma once

#include "core/types/node.h"    // NodePath + NodeType (字段依赖)
#include "core/types/context.h" // Context (字段依赖, 由 nlohmann::json 替代)
#include "core/types/budget.h"  // ExecutionBudget (字段依赖)
#include <nlohmann/json.hpp>
#include <chrono>
#include <optional>
#include <string>

namespace agenticdsl {

/**
 * @brief 节点执行追踪记录 (data-only struct)
 *
 * 字段说明:
 *   - trace_id:      唯一 trace 标识
 *   - node_path:     节点路径
 *   - type:          节点类型 (NodeType 的字符串表示)
 *   - start_time:    节点开始执行时间
 *   - end_time:      节点结束执行时间
 *   - status:        "success" / "failed" / "skipped"
 *   - error_code:    错误码 (failed 时填充)
 *   - context_delta: 执行前后上下文变化 (简化表示)
 *   - ctx_snapshot_key: 关联的快照键 (v3.1)
 *   - budget_snapshot:  执行时的预算状态
 *   - metadata:      节点原始 metadata
 *   - llm_intent:    从注释解析的 LLM 意图
 *   - mode:          "dev" / "prod"
 *
 * 迁移说明: 原 src/modules/trace/trace_exporter.h:16-31 的 struct 定义; 字段
 * 完全保留, 注释从 11 行精简为文档化注释。TraceExporter 类行为不变 (保留在
 * src/modules/trace/trace_exporter.h 内)。
 */
struct TraceRecord {
  std::string trace_id;
  NodePath node_path;
  std::string type; // NodeType as string
  std::chrono::system_clock::time_point start_time;
  std::chrono::system_clock::time_point end_time;
  std::string status; // "success", "failed", "skipped"
  std::optional<std::string> error_code;
  nlohmann::json context_delta; // 执行前后上下文的变化 (simplified)
  std::optional<NodePath> ctx_snapshot_key; // 关联的快照键 (v3.1)
  nlohmann::json budget_snapshot; // 执行时的预算状态
  nlohmann::json metadata; // 节点原始 metadata
  std::optional<std::string> llm_intent; // 从注释解析
  std::string mode; // "dev" or "prod"
};

}  // namespace agenticdsl
