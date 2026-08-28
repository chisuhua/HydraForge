// src/modules/cognitive/mcts_workflow_search.cpp
// 功能描述：AFlow 风格 MCTS 工作流搜索编排层实现 (T20, ADR-0061-08, Sprint 24)
//          Phase 0 骨架: 构造 + 占位 search() (契约编译/链接占位)
//          Phase 1 替换为完整 MCTS 算法 (UCB1 + 扩展/模拟/反向传播)
// 设计依据：docs/adr/skill/adr-0061-08-aflow-search.md
//          + openspec/changes/t20-aflow-mcts/tasks.md
// 作者：HydraForge Sprint 24 T20 ship
// 最后修改日期：2026-08-28

#include "agenticdsl/cognitive/mcts_workflow_search.h"

#include <utility>

namespace agenticdsl {

MCTSWorkflowSearch::MCTSWorkflowSearch(
    std::shared_ptr<IEvaluator> evaluator,
    std::shared_ptr<IMutationGovernor> governor,
    std::shared_ptr<BehavioralRegressionGate> regression_gate)
    : MCTSWorkflowSearch(std::move(evaluator), std::move(governor),
                         std::move(regression_gate), SearchConfig{}, nullptr) {}

MCTSWorkflowSearch::MCTSWorkflowSearch(
    std::shared_ptr<IEvaluator> evaluator,
    std::shared_ptr<IMutationGovernor> governor,
    std::shared_ptr<BehavioralRegressionGate> regression_gate,
    SearchConfig config,
    std::shared_ptr<IInteractionBus> bus)
    : evaluator_(std::move(evaluator)),
      governor_(std::move(governor)),
      regression_gate_(std::move(regression_gate)),
      bus_(std::move(bus)),
      config_(std::move(config)) {}

MCTSWorkflowSearch::SearchResult MCTSWorkflowSearch::search(
    const TaskSpec& /*spec*/) {
  // Phase 0 骨架: 契约占位 — Phase 1 替换为完整 MCTS 算法
  return SearchResult{};
}

}  // namespace agenticdsl
