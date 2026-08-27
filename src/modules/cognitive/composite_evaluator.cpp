// src/modules/cognitive/composite_evaluator.cpp
// 功能描述：V2 复合评估器实现 (evaluator-v2-composite T1)
//          实现输入校验、权重归一化、RewardSignal 聚合与比较结果聚合。
//
// 设计依据：ADR-0083 §决策 5 + openspec/changes/evaluator-v2-composite/design.md
// 作者：HydraForge
// 最后修改日期：2026-08-27

#include "agenticdsl/cognitive/composite_evaluator.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace agenticdsl {

CompositeEvaluator::CompositeEvaluator(
    std::vector<std::shared_ptr<IEvaluator>> evaluators,
    std::vector<double> weights)
    : evaluators_(std::move(evaluators)), weights_(std::move(weights)) {
  if (evaluators_.empty() || evaluators_.size() != weights_.size()) {
    throw std::invalid_argument("evaluators and weights must be non-empty and equal-sized");
  }

  const double sum = std::accumulate(weights_.begin(), weights_.end(), 0.0);
  if (!std::isfinite(sum) || sum <= 0.0) {
    throw std::invalid_argument("weights must have a finite positive sum");
  }
  for (double& weight : weights_) {
    weight /= sum;
  }
}

RewardSignal CompositeEvaluator::evaluate(const ExecutionTrace& trace) const {
  double scalar = 0.0;
  double confidence = 1.0;
  int quality_counts[3] = {0, 0, 0};

  for (std::size_t i = 0; i < evaluators_.size(); ++i) {
    const RewardSignal signal = evaluators_[i]->evaluate(trace);
    scalar += signal.scalar * weights_[i];
    confidence = std::min(confidence, signal.confidence);
    const auto quality = static_cast<int>(signal.quality);
    if (quality >= 0 && quality < 3) {
      ++quality_counts[quality];
    }
  }

  const auto quality_it = std::max_element(std::begin(quality_counts),
                                           std::end(quality_counts));
  const auto quality = static_cast<RewardSignal::Quality>(
      std::distance(std::begin(quality_counts), quality_it));
  // 聚合后 scalar 仍须满足 RewardSignal 的 [-1, 1] 约束。
  scalar = std::clamp(scalar, -1.0, 1.0);
  return {quality, scalar, confidence};
}

int CompositeEvaluator::compare(const ExecutionTrace& a,
                                const ExecutionTrace& b) const {
  double score = 0.0;
  for (std::size_t i = 0; i < evaluators_.size(); ++i) {
    score += static_cast<double>(evaluators_[i]->compare(a, b)) * weights_[i];
  }
  if (score > 0.1) {
    return 1;
  }
  if (score < -0.1) {
    return -1;
  }
  return 0;
}

}  // namespace agenticdsl