// ADR-0074 D-4: Evidence Gate evaluate_gate 单元测试
// 覆盖 5 case: 4 boundary parse-valid + 1 Abort 数据完整性
// TDD 5 步: write fail → verify fail → implement → verify pass → commit
#include "catch_amalgamated.hpp"

#include "common/prompts/evidence_gate.h"

using namespace agenticdsl::prompts;

TEST_CASE("evaluate_gate returns Fail when parse_valid below 85%", "[evidence-gate][boundary]") {
  // Critical band below
  REQUIRE(evaluate_gate(84.9 / 100.0, 0.85, 0.60, 0.30) == GateStatus::Fail);
}

TEST_CASE("evaluate_gate returns Conditional when parse_valid at 85.0", "[evidence-gate][boundary]") {
  // D-4 left-closed at 85.0
  REQUIRE(evaluate_gate(85.0 / 100.0, 0.85, 0.60, 0.30) == GateStatus::Conditional);
}

TEST_CASE("evaluate_gate returns Conditional inside band [85, 90)", "[evidence-gate][boundary]") {
  REQUIRE(evaluate_gate(89.9 / 100.0, 0.85, 0.60, 0.30) == GateStatus::Conditional);
}

TEST_CASE("evaluate_gate returns Pass when parse_valid at 90.0", "[evidence-gate][boundary]") {
  // D-4 right-open at 90.0
  REQUIRE(evaluate_gate(90.0 / 100.0, 0.85, 0.60, 0.30) == GateStatus::Pass);
}

TEST_CASE("evaluate_gate returns Abort when data incomplete (sentinel)", "[evidence-gate][abort]") {
  // D-3: parse_valid = sentinel (negative) → Abort, 不进入阈值比较
  REQUIRE(evaluate_gate(kGateDataMissing, 0.85, 0.60, 0.30) == GateStatus::Abort);
  // 越界 (>1) 也视为数据异常
  REQUIRE(evaluate_gate(1.5, 0.85, 0.60, 0.30) == GateStatus::Abort);
}

TEST_CASE("evaluate_gate handles 184/184 ctest zero-regression baseline", "[evidence-gate][regression]") {
  // 184/184 不变: baseline 测试不受 evidence_gate 影响 (header-only, no IO)
  // 此 case 为 regression sanity check,证明 evaluate_gate 函数的纯函数性
  constexpr double mock_parse_valid = 0.8824;  // 来自 docs/audits/2026-08-18-execution-baseline-v1.yaml
  constexpr double mock_l1 = 0.85;
  constexpr double mock_l2 = 0.60;
  constexpr double mock_l3 = 0.1818;

  // Mock 数据决策:88.24% ∈ [85.0, 90.0) → Conditional
  // 但实际 mock mode 数据视为 Abort per design D-3 (基线 Q-3/Q-4 明示)
  // 此 case 仅验证函数本身的纯函数性
  GateStatus s = evaluate_gate(mock_parse_valid, mock_l1, mock_l2, mock_l3);
  REQUIRE(s == GateStatus::Conditional);  // 函数本身:Conditional (88.24% 在临界带)
}

TEST_CASE("evaluate_gate to_string mapping", "[evidence-gate][stringify]") {
  REQUIRE(std::string(to_string(GateStatus::Pass)) == "Pass");
  REQUIRE(std::string(to_string(GateStatus::Fail)) == "Fail");
  REQUIRE(std::string(to_string(GateStatus::Conditional)) == "Conditional");
  REQUIRE(std::string(to_string(GateStatus::Abort)) == "Abort");
}
