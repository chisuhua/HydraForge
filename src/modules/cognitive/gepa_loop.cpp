// src/modules/cognitive/gepa_loop.cpp
// 功能描述：GEPALoop V1 编排层实现 (T19, ADR-0071)
//          Phase 0 存根：仅完成构造与 reflect_and_commit 链路占位
//          （完整反思循环实现见 Phase 1 提交）。
// 设计依据：openspec/changes/t19-gepa-phase2-commit/tasks.md Phase 0
// 作者：HydraForge Sprint 24 T19 ship
// 最后修改日期：2026-08-27

#include "agenticdsl/cognitive/gepa_loop.h"

#include "common/llm/llm_types.h"

#include <utility>

namespace agenticdsl {

GEPALoop::GEPALoop(std::shared_ptr<IEvaluator> evaluator,
                   std::shared_ptr<IMutationGovernor> governor,
                   std::shared_ptr<ILLMProvider> llm)
    : GEPALoop(std::move(evaluator),
               std::move(governor),
               std::move(llm),
               Config{}) {}

GEPALoop::GEPALoop(std::shared_ptr<IEvaluator> evaluator,
                   std::shared_ptr<IMutationGovernor> governor,
                   std::shared_ptr<ILLMProvider> llm,
                   Config config)
    : evaluator_(std::move(evaluator)),
      governor_(std::move(governor)),
      llm_(std::move(llm)),
      config_(std::move(config)) {}

GEPALoop::ReflectionResult GEPALoop::reflect_and_commit(
    const ExecutionTrace& /*failed_trace*/) {
  // Phase 0 存根：返回默认（失败）结果，占满契约签名。
  // Phase 1 起实现反思 + 修订 + 回归 + 评估 + commit 全流程。
  return ReflectionResult{};
}

}  // namespace agenticdsl