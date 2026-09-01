// include/agenticdsl/cognitive/workflow_materializer.h
// T1 workflow-materializer-v1: WorkflowGraph → DSL 文本具体化桥
#pragma once

#include "agenticdsl/cognitive/mcts_workflow_search.h"
#include "agenticdsl/contract/iinteraction_bus.h"

#include <memory>
#include <optional>
#include <string>

namespace agenticdsl::cognitive {

/**
 * @brief WorkflowGraph → DSL 文本 Materializer V1
 *
 * 将 MCTSWorkflowSearch 搜索产出的 WorkflowGraph (图宇宙 A) 转换为
 * Markdown AgenticDSL 文本 (图宇宙 B 的中间表示), 供
 * `DSLEngine::continue_with_generated_dsl()` 解析注册为 ParsedGraph。
 *
 * 映射规则 (per OpenSpec change 2026-08-31-workflow-materializer-v1 design §决策 2):
 * - Axis1 Linear → start/assign/end 线性链
 * - Axis1 Branching → fork/join 节点对
 * - Axis4 Parallel → fork→join 并行
 * - Axis5 Error → on_failure + retry/fallback/abort
 * - Axis6 Reflect/Search/Compile → tool_call evolution::* (对齐 T5 cognitive-tools)
 * - Axis6 None → 不生成 cognitive 节点
 *
 * 输出格式 `### AgenticDSL /dynamic/mcts/<task_id>` (单一事实源原则,
 * DSL 文本 → MarkdownParser 是唯一跨宇宙桥梁)。
 */
class Materializer {
public:
    /**
     * @brief 将 WorkflowGraph 具体化为 DSL 文本
     * @param graph 搜索产出的工作流图 (图宇宙 A)
     * @return DSL 文本; 空图或无法映射的组合 → nullopt
     */
    std::optional<std::string> materialize_to_dsl(const WorkflowGraph& graph);

    void set_bus(std::shared_ptr<IInteractionBus> bus) { bus_ = bus; }

private:
    std::string render_node(const WorkflowNode& node) const;
    std::string axis6_tool_name(WorkflowNode::Axis6CognitiveDomain axis6) const;

    std::shared_ptr<IInteractionBus> bus_;
};

}  // namespace agenticdsl::cognitive
