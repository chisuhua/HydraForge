// tests/test_behavioral_regression.cpp
// 功能描述：行为回归套件单元测试 (T14, ADR-0061-02)
//          ≥ 6 cases: 三值 Verdict 边界 + Hotelling T² 统计量 + Fingerprint 提取
// 设计依据：openspec/changes/2026-08-24-adr-0061-02-behavioral-regression
//          ADR-0061-02 AgentAssay-style 行为回归套件
// 作者：HydraForge Sprint 23 T14 ship (Phase A Oracle 评审输入)
// 最后修改日期：2026-08-24

#include "catch_amalgamated.hpp"

#include "agenticdsl/testing/behavioral_regression.h"

#include "core/types/tool_result.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using agenticdsl::Verdict;
using agenticdsl::BehaviorFingerprint;
using agenticdsl::RegressionBudget;
using agenticdsl::hotelling_t2_test;
using agenticdsl::hotelling_t2_statistic;
using agenticdsl::compute_fingerprint;
using agenticdsl::ToolResult;

// ============================================================================
// Test 1: 三值 Verdict — 完全相同 fingerprint
// ============================================================================
TEST_CASE("verdict_pass_identical_fingerprints",
          "[behavioral_regression][t14][verdict][pass]") {
  BehaviorFingerprint fp_a{};
  fp_a.features = {1.0, 100.0, 500.0, 0.0};
  fp_a.label = "v1.0";

  BehaviorFingerprint fp_b = fp_a;  // 完全相同
  fp_b.label = "v1.0-candidate";

  RegressionBudget budget{};
  REQUIRE(hotelling_t2_test(fp_a, fp_b, budget) == Verdict::Pass);
}

// ============================================================================
// Test 2: 三值 Verdict — 大距离 fingerprint
// ============================================================================
TEST_CASE("verdict_fail_large_distance",
          "[behavioral_regression][t14][verdict][fail]") {
  BehaviorFingerprint fp_a{};
  fp_a.features = {0.9, 100.0, 500.0, 0.1};  // 高成功率 + 低延迟

  BehaviorFingerprint fp_b{};
  fp_b.features = {0.1, 5000.0, 5000.0, 0.9};  // 低成功率 + 高延迟

  RegressionBudget budget{};
  REQUIRE(hotelling_t2_test(fp_a, fp_b, budget) == Verdict::Fail);
}

// ============================================================================
// Test 3: 三值 Verdict — 边界带（Inconclusive）
// ============================================================================
TEST_CASE("verdict_inconclusive_boundary",
          "[behavioral_regression][t14][verdict][inconclusive]") {
  BehaviorFingerprint fp_a{};
  fp_a.features = {0.0, 100.0, 500.0, 0.0};

  BehaviorFingerprint fp_b{};
  // 差异 T² = 1.0 (落在 [confidence_threshold=0.95, 2*threshold=1.9) 区间)
  // diff[0]² + diff[1]² = 1² + 0² = 1.0
  fp_b.features = {1.0, 100.0, 500.0, 0.0};

  RegressionBudget budget{};
  REQUIRE(hotelling_t2_test(fp_a, fp_b, budget) == Verdict::Inconclusive);
}

// ============================================================================
// Test 4: Hotelling T² 统计量 — 单位协方差退化为欧氏距离平方
// ============================================================================
TEST_CASE("hotelling_t2_statistic_unit_variance",
          "[behavioral_regression][t14][statistic]") {
  std::array<double, 4> mean_a = {1.0, 2.0, 3.0, 4.0};
  std::array<double, 4> mean_b = {0.0, 0.0, 0.0, 0.0};

  std::array<std::array<double, 4>, 4> identity{};
  for (int i = 0; i < 4; ++i) identity[i][i] = 1.0;

  double t2 = hotelling_t2_statistic(mean_a, mean_b, identity);
  // 期望: 1² + 2² + 3² + 4² = 1 + 4 + 9 + 16 = 30
  REQUIRE(t2 == Catch::Approx(30.0));
}

// ============================================================================
// Test 5: compute_fingerprint — 多结果聚合
// ============================================================================
TEST_CASE("compute_fingerprint_basic",
          "[behavioral_regression][t14][fingerprint]") {
  std::vector<ToolResult> results;
  // 3 成功 + 1 失败,latency [100, 200, 300, 400], tokens [500, 600, 700, 800]
  for (int i = 0; i < 3; ++i) {
    ToolResult r;
    r.ok = true;
    r.latency_ms = static_cast<std::uint64_t>((i + 1) * 100);
    r.meta["tokens_used"] = 500 + i * 100;
    results.push_back(r);
  }
  ToolResult fail;
  fail.ok = false;
  fail.latency_ms = static_cast<std::uint64_t>(400);
  fail.meta["tokens_used"] = 800;
  results.push_back(fail);

  auto fp = compute_fingerprint(results);
  // success_rate = 3/4 = 0.75
  REQUIRE(fp.features[0] == Catch::Approx(0.75));
  // avg_latency_ms = (100 + 200 + 300 + 400) / 4 = 250
  REQUIRE(fp.features[1] == Catch::Approx(250.0));
  // avg_tokens = (500 + 600 + 700 + 800) / 4 = 650
  REQUIRE(fp.features[2] == Catch::Approx(650.0));
  // error_rate = 1/4 = 0.25
  REQUIRE(fp.features[3] == Catch::Approx(0.25));
}

// ============================================================================
// Test 6: compute_fingerprint — 空结果容错
// ============================================================================
TEST_CASE("compute_fingerprint_empty",
          "[behavioral_regression][t14][fingerprint][empty]") {
  std::vector<ToolResult> empty_results;
  auto fp = compute_fingerprint(empty_results);
  // 空结果 → 全零 fingerprint,不抛异常
  REQUIRE(fp.features[0] == Catch::Approx(0.0));
  REQUIRE(fp.features[1] == Catch::Approx(0.0));
  REQUIRE(fp.features[2] == Catch::Approx(0.0));
  REQUIRE(fp.features[3] == Catch::Approx(0.0));
  REQUIRE(fp.label.empty());  // 默认空 label
}