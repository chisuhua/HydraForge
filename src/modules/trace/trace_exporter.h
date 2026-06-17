// modules/trace/include/trace/trace_exporter.h
// 功能描述：节点执行追踪导出器。仅保留 TraceExporter 类行为; TraceRecord
//           data-only struct 已迁移到 include/agenticdsl/types/trace_record.h
//           (P1.T3 — ADR-0019 §1.4 engine.h 解耦退出标准, openspec change
//            2026-06-15-residual-engine-h-decoupling)。
// 设计依据：ADR-0019 §1.4 + ADR-0033 §2 + openspec/changes/2026-06-15-...
// 作者：AgenticDSL Phase 1 P1.T3
// 最后修改日期：2026-06-18
#ifndef AGENTICDSL_MODULES_TRACE_TRACE_EXPORTER_H
#define AGENTICDSL_MODULES_TRACE_TRACE_EXPORTER_H

#include "agenticdsl/types/trace_record.h"
#include "core/types/node.h"    // 引入 NodePath + NodeType
#include "core/types/context.h" // 引入 Context
#include "core/types/budget.h"  // 引入 ExecutionBudget
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <optional>
#include <chrono>

namespace agenticdsl {

class TraceExporter {
public:
    void on_node_start(
        const NodePath& path,
        NodeType type,
        const nlohmann::json& initial_context,
        const std::optional<ExecutionBudget>& budget
    );

    void on_node_end(
        const NodePath& path,
        const std::string& status,
        const std::optional<std::string>& error_code,
        const nlohmann::json& initial_context, // For calculating delta
        const nlohmann::json& final_context,
        const std::optional<NodePath>& snapshot_key, // v3.1
        const std::optional<ExecutionBudget>& budget
    );

    std::vector<TraceRecord> get_traces() const;
    void clear_traces();

private:
    std::vector<TraceRecord> traces_;
    std::string current_trace_id_ = "t-default"; // Should be generated uniquely per execution

    // Helper to calculate context delta (simplified)
    nlohmann::json calculate_context_delta(const nlohmann::json& initial, const nlohmann::json& final);
    // Helper to serialize budget state to JSON
    nlohmann::json serialize_budget_state(const std::optional<ExecutionBudget>& budget) const;
};

} // namespace agenticdsl

#endif // AGENTICDSL_MODULES_TRACE_TRACE_EXPORTER_H
