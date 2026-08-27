// src/modules/cognitive/behavioral_equivalence_evaluator.cpp
// 功能描述：V2 行为等价评估器实现 (evaluator-v2-composite T0)
//          compare(a, b): 通过 T14 BehavioralRegressionGate (Hotelling T²) 比较
//          两条轨迹的行为指纹 — Pass/Inconclusive → 0, Fail → 按 reward scalar 分胜负。
//          evaluate(trace): V1 占位返回 Acceptable(0.5)。
//
// 设计依据：ADR-0083 §决策 5 + ADR-0061-02 (行为回归 gate)
//          + openspec/changes/evaluator-v2-composite/design.md §3.2
// 作者：HydraForge
// 最后修改日期：2026-08-27

#include "agenticdsl/cognitive/behavioral_equivalence_evaluator.h"

#include "agenticdsl/testing/behavioral_regression.h"

#include <vector>

namespace agenticdsl {

BehavioralEquivalenceEvaluator::BehavioralEquivalenceEvaluator() = default;

RewardSignal BehavioralEquivalenceEvaluator::evaluate(
    const ExecutionTrace& trace) const {
  // V1 占位: 单 trace 无法评估行为等价性 (等价性是两两比较概念)
  (void)trace;
  return RewardSignal::acceptable(0.5);
}

int BehavioralEquivalenceEvaluator::compare(
    const ExecutionTrace& a, const ExecutionTrace& b) const {
  // 从单条 ExecutionTrace 提取行为指纹 (V1: 单结果样本)
  const auto fp_a = compute_fingerprint(std::vector<ToolResult>{a.final_result});
  const auto fp_b = compute_fingerprint(std::vector<ToolResult>{b.final_result});

  // Hotelling T² 检验 (默认 budget: confidence_threshold=0.95)
  const Verdict verdict = hotelling_t2_test(fp_a, fp_b, RegressionBudget{});

  // Pass/Inconclusive → 行为等价 → 0
  if (verdict != Verdict::Fail) {
    return 0;
  }

  // Fail → 行为不等价 → 按 reward scalar 比较 (ok=true → +1.0, 否则 -1.0)
  const double scalar_a = a.final_result.ok ? 1.0 : -1.0;
  const double scalar_b = b.final_result.ok ? 1.0 : -1.0;
  const double diff = scalar_a - scalar_b;
  if (diff > 0.0) {
    return 1;
  }
  if (diff < 0.0) {
    return -1;
  }
  return 0;
}

}  // namespace agenticdsl