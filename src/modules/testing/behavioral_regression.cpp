// src/modules/testing/behavioral_regression.cpp
// 功能描述：行为回归套件实现 (T14, ADR-0061-02)
//          V1 简化算法: Hotelling T² 在单位协方差下退化为欧氏距离平方
//          三值 Verdict: Pass/Fail/Inconclusive
//          Fingerprint 提取: 4 维特征 (success_rate, avg_latency, avg_tokens, error_rate)
//
// 设计依据：ADR-0061-02 §决策 1-5 + openspec/changes/2026-08-24-adr-0061-02
// 作者：HydraForge Sprint 23 T14 ship
// 最后修改日期：2026-08-24

#include "agenticdsl/testing/behavioral_regression.h"

#include <algorithm>
#include <cmath>

namespace agenticdsl {

// ============================================================================
// 纯函数: Hotelling T² 统计量
// ============================================================================

double hotelling_t2_statistic(
    const std::array<double, 4>& mean_a,
    const std::array<double, 4>& mean_b,
    const std::array<std::array<double, 4>, 4>& pooled_cov) {
  // V1 简化: 仅实现对角元素（即各特征独立等权）
  //   T² = (μ_a - μ_b)ᵀ S⁻¹ (μ_a - μ_b)
  // V1: S = I（单位矩阵）,T² = Σᵢ (μ_a[i] - μ_b[i])²
  // V2: 完整协方差矩阵求逆（需要 n_sample >> n_feature）
  //
  // 当前实现: 忽略 pooled_cov 参数,直接计算欧氏距离平方
  // (V1 行为与 ADR-0061-02 §决策 2 一致)
  double t2 = 0.0;
  for (size_t i = 0; i < 4; ++i) {
    const double diff = mean_a[i] - mean_b[i];
    t2 += diff * diff;
  }
  // 抑制 pooled_cov 未使用参数警告（V2 升级时使用）
  (void)pooled_cov;
  return t2;
}

// ============================================================================
// 三值 Verdict 决策
// ============================================================================

Verdict hotelling_t2_test(const BehaviorFingerprint& fp_a,
                          const BehaviorFingerprint& fp_b,
                          const RegressionBudget& budget) {
  // Hotelling T² 距离（V1: 单位协方差 → 欧氏距离平方）
  const double t2 = hotelling_t2_statistic(
      fp_a.features, fp_b.features,
      std::array<std::array<double, 4>, 4>{});  // V1: I 矩阵(由实现忽略)

  // 三值决策（ADR-0061-02 §决策 1）
  const double threshold = budget.confidence_threshold;
  if (t2 < threshold) {
    return Verdict::Pass;
  }
  if (t2 >= 2.0 * threshold) {
    return Verdict::Fail;
  }
  return Verdict::Inconclusive;
}

// ============================================================================
// 指纹提取
// ============================================================================

BehaviorFingerprint compute_fingerprint(
    const std::vector<ToolResult>& results) {
  BehaviorFingerprint fp{};
  if (results.empty()) {
    // 空结果 → 全零 fingerprint,空 label（不抛异常）
    return fp;
  }

  // V1 简化: 聚合 4 维特征
  // features[0] = success_rate (ok=true 的占比)
  // features[1] = avg_latency_ms (latency_ms 平均,缺失值视为 0)
  // features[2] = avg_tokens (meta["tokens_used"] 平均,缺失值视为 0)
  // features[3] = error_rate (ok=false 的占比)
  std::size_t success_count = 0;
  std::uint64_t total_latency = 0;
  std::uint64_t total_tokens = 0;
  std::size_t tokens_count = 0;

  for (const auto& r : results) {
    if (r.ok) ++success_count;
    if (r.latency_ms.has_value()) {
      total_latency += r.latency_ms.value();
    }
    if (r.meta.contains("tokens_used") && r.meta["tokens_used"].is_number()) {
      total_tokens += r.meta["tokens_used"].get<std::uint64_t>();
      ++tokens_count;
    }
  }

  const double n = static_cast<double>(results.size());
  fp.features[0] = static_cast<double>(success_count) / n;
  fp.features[1] = static_cast<double>(total_latency) / n;
  fp.features[2] = (tokens_count > 0)
      ? (static_cast<double>(total_tokens) / static_cast<double>(tokens_count))
      : 0.0;
  fp.features[3] = static_cast<double>(results.size() - success_count) / n;

  // 默认 label 为空（调用方负责设置 v1.0 / candidate 标识）
  return fp;
}

}  // namespace agenticdsl