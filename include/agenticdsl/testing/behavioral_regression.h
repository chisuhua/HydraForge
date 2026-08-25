// include/agenticdsl/testing/behavioral_regression.h
// 功能描述：行为回归套件 L1 契约 (T14, ADR-0061-02 AgentAssay-style)
//          三值 Verdict + Hotelling T² 行为指纹 + Adaptive Budget
//
// 设计依据：ADR-0061-02 §决策 1-5
//          + openspec/changes/2026-08-24-adr-0061-02-behavioral-regression
//          + Oracle 评审 ses_fcba5e477ffeG9wEBHVhU64J0o §八 关键发现:
//            "T14 行为回归 = 所有变异循环(GEPA/AFlow/fine-tune/行为克隆)安全前提"
//
// 作者：HydraForge Sprint 23 T14 ship
// 最后修改日期：2026-08-24

#pragma once

#include "core/types/tool_result.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace agenticdsl {

// ============================================================================
// §决策 1: 三值 Verdict
// ============================================================================

enum class Verdict {
  Pass,           // Hotelling T² < confidence_threshold
  Fail,           // Hotelling T² ≥ 2 × confidence_threshold
  Inconclusive    // 中间带（证据不足）
};

// Verdict → 字符串（用于日志/审计）
inline const char* verdict_to_string(Verdict v) {
  switch (v) {
    case Verdict::Pass: return "Pass";
    case Verdict::Fail: return "Fail";
    case Verdict::Inconclusive: return "Inconclusive";
  }
  return "Unknown";
}

// ============================================================================
// §决策 2: 行为指纹（V1 固定 4 维）
// ============================================================================

struct BehaviorFingerprint {
  std::array<double, 4> features{};  // V1 固定 4 维
  // features[0] = success_rate    (0.0 ~ 1.0)
  // features[1] = avg_latency_ms  (平均耗时, ms)
  // features[2] = avg_tokens      (平均 token 数)
  // features[3] = error_rate      (0.0 ~ 1.0)
  std::string label;                // 标识 (e.g. "v1.0" / "candidate")
};

// ============================================================================
// §决策 4: Adaptive Budget
// ============================================================================

struct RegressionBudget {
  uint32_t max_tokens{10000};
  uint32_t adaptive_test_count{50};
  double confidence_threshold{0.95};
  uint32_t max_wallclock_ms{60000};
};

// ============================================================================
// API（核心）
// ============================================================================

// §决策 2 + 3: Hotelling T² 检验（三值 Verdict）
Verdict hotelling_t2_test(const BehaviorFingerprint& fp_a,
                          const BehaviorFingerprint& fp_b,
                          const RegressionBudget& budget);

// 纯函数: Hotelling T² 统计量计算（V1: 单位协方差近似）
//   T² = (μ_a - μ_b)ᵀ S⁻¹ (μ_a - μ_b)
// V1 简化: S = I（单位矩阵），即 T² = Σᵢ (μ_a[i] - μ_b[i])²
double hotelling_t2_statistic(
    const std::array<double, 4>& mean_a,
    const std::array<double, 4>& mean_b,
    const std::array<std::array<double, 4>, 4>& pooled_cov);

// §决策 5: 指纹提取（V1 简化: 单次聚合，无样本协方差）
BehaviorFingerprint compute_fingerprint(
    const std::vector<ToolResult>& results);

}  // namespace agenticdsl