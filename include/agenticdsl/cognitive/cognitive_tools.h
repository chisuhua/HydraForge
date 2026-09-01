// include/agenticdsl/cognitive/cognitive_tools.h
// T5 cognitive-specialists-as-tools: GEPA/MCTS/SkillCompiler 注册为 tool
#pragma once

#include <memory>

namespace agenticdsl {

class ToolRegistry;
class GEPALoop;
class MCTSWorkflowSearch;
class SkillCompiler;

namespace cognitive {

/**
 * @brief 注册 3 个 cognitive specialist 为 tool (per OpenSpec change
 *        2026-08-31-cognitive-specialists-as-tools, Oracle T2 修正)
 *
 * 注册的工具:
 * - evolution::reflect  → GEPALoop::reflect_and_commit
 * - evolution::search   → MCTSWorkflowSearch::search
 * - evolution::compile  → SkillCompiler::compile
 *
 * 每个 tool 配 ToolMetadata V2 (category + approval_policy + allowed_layers
 * + cost_estimate + timeout_ms)。走 ADR-0004 ToolRegistry + ToolCoordinator
 * 4 步校验链。LayerProfile 限制: Workflow + Thinking 层可调用 (Cognitive L4 禁止)。
 *
 * @param registry       目标 ToolRegistry (可变, 不能为空)
 * @param gepa_loop      GEPALoop 实例 (可空, null 时跳过 reflect tool 注册)
 * @param mcts_searcher  MCTSWorkflowSearch 实例 (可空, null 时跳过 search tool 注册)
 * @param skill_compiler SkillCompiler 实例 (可空, null 时跳过 compile tool 注册)
 *
 * 注意: 默认命名空间前缀 "evolution::" (commit 06ddd13 已用), 与 docs/README.md
 * §adr/skill 表 ADR-0061-08 v1.0 保持一致。如架构组裁决需 rename 为 "cognitive::",
 * 单 commit 替换 3 处 string 字面量即可 (见 T5 plan §后续触发条件)。
 */
void register_cognitive_tools(
    ToolRegistry& registry,
    std::shared_ptr<GEPALoop> gepa_loop,
    std::shared_ptr<MCTSWorkflowSearch> mcts_searcher,
    std::shared_ptr<SkillCompiler> skill_compiler);

}  // namespace cognitive
}  // namespace agenticdsl
