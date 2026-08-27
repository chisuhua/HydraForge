// include/agenticdsl/cognitive/gepa_loop.h
// 功能描述：GEPALoop V1 编排层契约 (T19, ADR-0071)
//          连接失败轨迹、LLM 反思、SkillCompiler、行为回归、IEvaluator
//          与 MutationGovernor，提供同步反思与提交入口。
// 设计依据：openspec/changes/t19-gepa-phase2-commit/tasks.md Phase 0
// 作者：HydraForge Sprint 24 T19 ship
// 最后修改日期：2026-08-27
#pragma once

#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/imutation_governance.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/types/execution_trace.h"

#include <memory>
#include <string>
#include <vector>

namespace agenticdsl {

class ILLMProvider;

/** @brief GEPA 单 agent 同步反思循环编排器。 */
class GEPALoop {
 public:
  struct Config {
    double reward_threshold = 0.0;
    double regression_alpha = 0.05;
    int max_iterations = 3;
    std::string source_id = "R_T19_GEPA";
  };

  struct ReflectionResult {
    bool success = false;
    std::string failure_mode;
    std::vector<std::string> candidate_skills;
  };

  GEPALoop(std::shared_ptr<IEvaluator> evaluator,
           std::shared_ptr<IMutationGovernor> governor,
           std::shared_ptr<ILLMProvider> llm);
  GEPALoop(std::shared_ptr<IEvaluator> evaluator,
           std::shared_ptr<IMutationGovernor> governor,
           std::shared_ptr<ILLMProvider> llm,
           Config config,
           std::shared_ptr<IInteractionBus> bus = nullptr);

  ReflectionResult reflect_and_commit(const ExecutionTrace& failed_trace);

 private:
  std::shared_ptr<IEvaluator> evaluator_;
  std::shared_ptr<IMutationGovernor> governor_;
  std::shared_ptr<ILLMProvider> llm_;
  std::shared_ptr<IInteractionBus> bus_;
  Config config_;
};

}  // namespace agenticdsl
