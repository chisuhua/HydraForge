// include/agenticdsl/cognitive/behavioral_equivalence_evaluator.h
// 功能描述：V2 行为等价评估器 (BehavioralEquivalenceEvaluator, evaluator-v2-composite T0)
//          基于 T14 BehavioralRegressionGate (Hotelling T² + fingerprint) 判定两条执行
//          轨迹的行为等价性: compare(a,b) → fingerprint 相似 (Pass/Inconclusive) 返回 0,
//          差异 (Fail) 时按 reward scalar 比较返回 +1/-1。
//          evaluate(trace) V1 占位: 单 trace 无法评估等价性, 返回 Acceptable(0.5)。
//
// 设计依据：ADR-0083 §决策 5 (V2 评估器) + ADR-0061-02 (行为回归, T14)
//          + openspec/changes/evaluator-v2-composite/design.md
// 作者：HydraForge
// 最后修改日期：2026-08-27

#ifndef AGENTICDSL_COGNITIVE_BEHAVIORAL_EQUIVALENCE_EVALUATOR_H
#define AGENTICDSL_COGNITIVE_BEHAVIORAL_EQUIVALENCE_EVALUATOR_H

#include "agenticdsl/contract/ievaluator.h"

namespace agenticdsl {

/**
 * @brief V2 行为等价评估器: 基于行为指纹 (BehavioralRegressionGate) 比较两条轨迹。
 *
 * implements: IEvaluator (evaluate + compare)
 * 与 V1 TaskSuccessEvaluator 的区别: 不评估"单次执行质量", 而是判定"两次执行
 * 行为是否等价" — 用于变异循环 (GEPA/AFlow/fine-tune/行为克隆) 的回归门禁。
 */
class BehavioralEquivalenceEvaluator : public IEvaluator {
public:
  BehavioralEquivalenceEvaluator();

  // V1: 单 trace 无法评估等价性 → 占位 Acceptable(0.5)
  RewardSignal evaluate(const ExecutionTrace& trace) const override;

  // Hotelling T² fingerprint 比较:
  //   Pass/Inconclusive → 0 (行为等价)
  //   Fail → 按 reward scalar (a.final_result.ok) 比较 → +1/-1/0
  int compare(const ExecutionTrace& a, const ExecutionTrace& b) const override;
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_COGNITIVE_BEHAVIORAL_EQUIVALENCE_EVALUATOR_H