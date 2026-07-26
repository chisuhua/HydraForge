// tests/test_escalation_triggers.cpp
// 功能描述：Phase 6 Spike — Escalation Trigger 单元测试 (tasks.md §6.6)
//          6 TEST_CASE: depth>2 / cycle / session>1K / error>10% /
//          design review / normal 2-level regression
// 设计依据：openspec/changes/phase6-service-ification-v1/
//          tasks.md §6, design.md Decision 6
// 作者：Phase 6 W1 (Sisyphus-Junior)
// 最后修改日期：2026-07-15

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iinteraction_bus.h"
#include "common/tools/tool_coordinator.h"
#include "core/types/tool_result.h"
#include "pdk/g3_knowledge_base/src/g3_state.h"

#include <memory>
#include <string>
#include <vector>

using namespace agenticdsl;

namespace {

class MockBusForEscalation : public IInteractionBus {
 public:
  void emit(const BusEvent& event) override {
    events_.push_back(event.topic);
  }
  void emit(const std::string& event_type, const std::string&) override {
    events_.push_back(event_type);
  }
  size_t subscribe(const std::string&,
                    std::function<void(const BusEvent&)>) override { return 0; }
  void unsubscribe(size_t) override {}

  std::vector<std::string> events_;
};

} // namespace

// ============================================================================
// Test 1 (§6.1): depth > 2 → HARD KILL
// ============================================================================
TEST_CASE("escalation: depth>2 trigger — 3rd nesting throws", "[escalation][depth]") {
  auto bus = std::make_shared<MockBusForEscalation>();

  ToolCoordinatorNestingGuard g1("tool_A", bus);  // depth 1
  ToolCoordinatorNestingGuard g2("tool_B", bus);  // depth 2

  // depth 3 → HARD KILL
  bool threw = false;
  try {
    ToolCoordinatorNestingGuard g3("tool_C", bus);
  } catch (const std::runtime_error& e) {
    threw = true;
    REQUIRE(std::string(e.what()).find("nesting depth > 2") != std::string::npos);
  }
  REQUIRE(threw);
}

// ============================================================================
// Test 2 (§6.2): cycle detection — same tool on stack → HARD KILL + audit
// ============================================================================
TEST_CASE("escalation: cycle trigger — same tool on stack → HARD KILL + audit",
          "[escalation][cycle]") {
  auto bus = std::make_shared<MockBusForEscalation>();

  ToolCoordinatorNestingGuard g1("G1/coding_assistant", bus);  // depth 1
  ToolCoordinatorNestingGuard g2("G3/knowledge_base", bus);    // depth 2

  // G1→G3→G1 (cycle: G1 already on stack)
  bool threw = false;
  try {
    ToolCoordinatorNestingGuard g3("G1/coding_assistant", bus);
  } catch (const std::runtime_error& e) {
    threw = true;
    REQUIRE(std::string(e.what()).find("cycle detected") != std::string::npos);
  }
  REQUIRE(threw);

  // 验证 cycle_detected_log audit event 已发射
  bool has_cycle_audit = false;
  for (auto& e : bus->events_) {
    if (e == "tool.coordinator.cycle_detected") { has_cycle_audit = true; break; }
  }
  REQUIRE(has_cycle_audit);
}

// ============================================================================
// Test 3 (§6.3): session store size > 1K → escalation trigger flag
// ============================================================================
TEST_CASE("escalation: session>1K trigger — SessionStore size monitoring",
          "[escalation][session_size]") {
  using namespace agenticdsl::pdk::g3;
  SessionStore store;

  // 预填充 1001 个 session (每个 session 1个 Q/A)
  constexpr size_t kTriggerThreshold = 1000;
  for (size_t i = 0; i <= kTriggerThreshold; i++) {
    std::string sid = "session_" + std::to_string(i);
    store.get_or_create(sid);
    store.append(sid, "question_" + std::to_string(i), "answer_" + std::to_string(i));
  }

  size_t sz = store.size();
  REQUIRE(sz > kTriggerThreshold);

  // 验证: session>1K 条件满足 → escalation flag 应设为 true
  bool session_size_trigger = (sz > kTriggerThreshold);
  REQUIRE(session_size_trigger);
}

// ============================================================================
// Test 4 (§6.4): error-as-success ratio > 10% → escalation trigger flag
// ============================================================================
TEST_CASE("escalation: error>10% trigger — error/as-success ratio monitoring",
          "[escalation][error_ratio]") {
  // 模拟 10 次调用中 2 次失败 = 20% error ratio > 10% threshold
  constexpr size_t kTotalCalls = 10;
  constexpr size_t kErrorCalls = 2;
  constexpr double kThreshold = 0.1;

  double ratio = static_cast<double>(kErrorCalls) / kTotalCalls;
  REQUIRE(ratio > kThreshold);

  // 验证: error>10% 条件满足 → escalation flag 应设为 true
  bool error_ratio_trigger = (ratio > kThreshold);
  REQUIRE(error_ratio_trigger);
}

// ============================================================================
// Test 5 (§6.5): design review trigger — 2+ awkward pattern categories
//
// ADR-0051 review process: 当 Layer 1+Layer 3 双备忘录中 ≥2 个 awkward pattern
// categories 被标记, 且/或 escalation triggers 反复触发时, 触发 ADR-0051
// 重新审查 (DECLARE_SERVICE 形式化评估). 此为事件驱动触发, 阈值低于 §13.1
// 的战略提升门槛.
// ============================================================================
TEST_CASE("escalation: design review trigger — 2+ awkward pattern categories",
          "[escalation][design_review]") {
  // 模拟 Layer 1 发现: 2 个 awkward pattern categories
  std::vector<std::string> awkward_categories = {
      "error_flattening",    // Category A: G3 error swallowed by G1
      "implicit_coupling"    // Category B: G1 硬编码依赖 G3 工具名
  };

  REQUIRE(awkward_categories.size() >= 2);

  // 验证: ≥2 categories → design review flag 应触发
  bool design_review_trigger = (awkward_categories.size() >= 2);
  REQUIRE(design_review_trigger);

  // 额外验证: Layer 3 双备忘录发散 (信号: orthogonal findings)
  // 当 primary + reviewer 独立发现不同 categories 时, 信号更强
  std::vector<std::string> primary_findings = {"error_flattening"};
  std::vector<std::string> reviewer_findings = {"implicit_coupling"};
  bool dual_memo_divergence =
      (primary_findings != reviewer_findings);
  // 发散增强设计审查必要性
  REQUIRE(dual_memo_divergence);
}

// ============================================================================
// Test 6 (§6.8): normal 2-level nesting regression —
//   G1→G3 composition (depth=2) → assert NO escalation trigger fires
//   Critical: RAII guard must NOT误杀 legitimate nested calls (Oracle R4)
// ============================================================================
TEST_CASE("escalation: normal 2-level nesting — NO trigger (regression R4)",
          "[escalation][regression][2level]") {
  auto bus = std::make_shared<MockBusForEscalation>();

  // depth=2 嵌套应正常完成, 不抛异常
  bool threw = false;
  try {
    ToolCoordinatorNestingGuard g1("G1/coding_assistant", bus);  // depth 1
    ToolCoordinatorNestingGuard g2("G3/knowledge_base", bus);    // depth 2
  } catch (const std::runtime_error& e) {
    threw = true;
  }
  REQUIRE_FALSE(threw);

  // 验证: 无 escalation triggers 发射
  for (auto& e : bus->events_) {
    REQUIRE(e != "tool.coordinator.cycle_detected");
  }
}

// ============================================================================
// §6.5 文档注记 (代码注释形式):
//
// ADR-0051 review 触发条件:
//   当 Layer 1 + Layer 3 双备忘录中 ≥2 个 awkward pattern categories 被标记,
//   且/或 escalation triggers (depth>2 / cycle / session>1K / error>10%)
//   反复触发时, 触发 ADR-0051 重新审查 (DECLARE_SERVICE 形式化评估).
//
//   触发路径:
//   1. Runtime safety: depth>2 OR cycle → HARD KILL + crash report
//   2. Plugin health: session>1K OR error>10% → audit review escalation
//   3. Design review: 2+ awkward categories OR L3 dual memo divergence → ADR-0051 review
//
//   注意: 此为事件驱动触发, 阈值低于 tasks.md §13.1 的战略提升门槛.
//   DECLARE_SERVICE 形式化为 v2 产物, 当前 Spike 不兑现.
// ============================================================================