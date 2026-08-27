// include/agenticdsl/cognitive/composite_evaluator.h
// 功能描述：V2 复合评估器 (CompositeEvaluator, evaluator-v2-composite T1)
//          聚合多个 IEvaluator 的 RewardSignal：scalar 按归一化权重加权平均，
//          quality 取众数（平局取较高质量），confidence 取最小值；compare() 按权重
//          聚合子评估器比较结果并使用 ±0.1 阈值判定胜负。
//
// 设计依据：ADR-0083 §决策 5 + openspec/changes/evaluator-v2-composite/design.md
// 作者：HydraForge
// 最后修改日期：2026-08-27

#ifndef AGENTICDSL_COGNITIVE_COMPOSITE_EVALUATOR_H
#define AGENTICDSL_COGNITIVE_COMPOSITE_EVALUATOR_H

#include "agenticdsl/contract/ievaluator.h"

#include <memory>
#include <vector>

namespace agenticdsl {

/** @brief 将多个 IEvaluator 聚合为一个可注入的 V2 评估器。 */
class CompositeEvaluator : public IEvaluator {
public:
  CompositeEvaluator(std::vector<std::shared_ptr<IEvaluator>> evaluators,
                     std::vector<double> weights);

  RewardSignal evaluate(const ExecutionTrace& trace) const override;
  int compare(const ExecutionTrace& a, const ExecutionTrace& b) const override;

private:
  std::vector<std::shared_ptr<IEvaluator>> evaluators_;
  std::vector<double> weights_;
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_COGNITIVE_COMPOSITE_EVALUATOR_H