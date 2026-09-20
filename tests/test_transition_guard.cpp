// tests/test_transition_guard.cpp
// H→D→M Transition Guard 状态机测试 (ADR-0088 + OpenSpec change 2026-09-16-h-d-m-transition-guard)

#include <catch_amalgamated.hpp>
#include "agenticdsl/evolution/transition_guard.h"
#include "agenticdsl/types/attribution_record.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"
#include "core/types/execution_result.h"
#include "modules/budget/budget_controller.h"

using namespace agenticdsl::evolution;

namespace {

// MockIEvaluator — AlwaysAcceptable
struct MockAcceptableIEvaluator : agenticdsl::IEvaluator {
    agenticdsl::RewardSignal evaluate(const agenticdsl::ExecutionTrace&) const override {
        agenticdsl::RewardSignal rs;
        rs.quality = agenticdsl::RewardSignal::Quality::Acceptable;
        rs.scalar = 0.5;
        rs.confidence = 0.8;
        return rs;
    }
    int compare(const agenticdsl::ExecutionTrace&, const agenticdsl::ExecutionTrace&) const override { return 0; }
};

// MockPoorIEvaluator — AlwaysPoor
struct MockPoorIEvaluator : agenticdsl::IEvaluator {
    agenticdsl::RewardSignal evaluate(const agenticdsl::ExecutionTrace&) const override {
        agenticdsl::RewardSignal rs;
        rs.quality = agenticdsl::RewardSignal::Quality::Poor;
        rs.scalar = -0.5;
        rs.confidence = 0.8;
        return rs;
    }
    int compare(const agenticdsl::ExecutionTrace&, const agenticdsl::ExecutionTrace&) const override { return 0; }
};

// MockBudgetController — sufficient or exhausted (full impl, all 16 virtuals)
struct MockBudgetController : agenticdsl::BudgetController {
    bool mock_exceeded = false;
    explicit MockBudgetController(bool exceeded) : agenticdsl::BudgetController(std::nullopt), mock_exceeded(exceeded) {}
    bool exceeded() const override { return mock_exceeded; }
    bool try_consume_node() override { return !mock_exceeded; }
    bool try_consume_llm_call() override { return !mock_exceeded; }
    bool try_consume_subgraph_depth() override { return !mock_exceeded; }
    bool try_consume_evolution_llm_call() override { return !mock_exceeded; }
    void set_termination_target(const agenticdsl::NodePath&) override {}
    std::optional<agenticdsl::NodePath> get_termination_target() const override { return std::nullopt; }
    void record_llm_call(int, const std::string&) override {}
    double get_total_cost_usd() const override { return 0.0; }
    void reset() override {}
    bool evolution_budget_exceeded() const override { return mock_exceeded; }
    void begin_evolution_cycle(const std::string& /*cycle_id*/) override {}
    void end_evolution_cycle(const std::string& /*cycle_id*/, bool /*success*/) override {}
    void reset_evolution_cycle_counter() override {}
};

struct ValidReadinessFixture {
    AttributionRecord attributed_record;
    ValidReadinessFixture() {
        attributed_record.method = AttributionMethod::DirectComparison;
        attributed_record.verdict = AttributionVerdict::Attributed;
    }
};

}  // namespace

// AC-1
TEST_CASE("EvolutionState enum has 5 states", "[adr-0088][ac-1]") {
    CHECK(static_cast<int>(EvolutionState::Idle) == 0);
    CHECK(static_cast<int>(EvolutionState::Harness) == 1);
    CHECK(static_cast<int>(EvolutionState::Data) == 2);
    CHECK(static_cast<int>(EvolutionState::Model) == 3);
    CHECK(static_cast<int>(EvolutionState::Done) == 4);
}

TEST_CASE("EvolutionVerdict struct fields exist", "[adr-0088][ac-1]") {
    EvolutionVerdict v;
    CHECK(v.can_proceed == false);
    CHECK(v.recommended_next == EvolutionState::Idle);
    CHECK(v.reason.empty());
    CHECK(v.failed_conditions.empty());
}

// AC-2
TEST_CASE("can_transition valid transitions return true", "[adr-0088][ac-2]") {
    CHECK(can_transition(EvolutionState::Idle, EvolutionState::Harness) == true);
    CHECK(can_transition(EvolutionState::Harness, EvolutionState::Data) == true);
    CHECK(can_transition(EvolutionState::Data, EvolutionState::Model) == true);
    CHECK(can_transition(EvolutionState::Model, EvolutionState::Done) == true);
    CHECK(can_transition(EvolutionState::Done, EvolutionState::Idle) == true);
}

TEST_CASE("can_transition illegal transitions return false", "[adr-0088][ac-2]") {
    CHECK(can_transition(EvolutionState::Harness, EvolutionState::Idle) == false);
    CHECK(can_transition(EvolutionState::Data, EvolutionState::Harness) == false);
    CHECK(can_transition(EvolutionState::Model, EvolutionState::Data) == false);
    CHECK(can_transition(EvolutionState::Done, EvolutionState::Harness) == false);
    CHECK(can_transition(EvolutionState::Idle, EvolutionState::Data) == false);
    CHECK(can_transition(EvolutionState::Idle, EvolutionState::Model) == false);
    CHECK(can_transition(EvolutionState::Idle, EvolutionState::Done) == false);
    CHECK(can_transition(EvolutionState::Harness, EvolutionState::Model) == false);
    CHECK(can_transition(EvolutionState::Harness, EvolutionState::Done) == false);
    CHECK(can_transition(EvolutionState::Data, EvolutionState::Done) == false);
}

TEST_CASE("can_transition only Done can reset to Idle", "[adr-0088][ac-2]") {
    CHECK(can_transition(EvolutionState::Harness, EvolutionState::Idle) == false);
    CHECK(can_transition(EvolutionState::Data, EvolutionState::Idle) == false);
    CHECK(can_transition(EvolutionState::Model, EvolutionState::Idle) == false);
    CHECK(can_transition(EvolutionState::Done, EvolutionState::Idle) == true);
}

// AC-3
TEST_CASE("evaluate_readiness all conditions pass → can_proceed=true",
          "[adr-0088][ac-3]") {
    ValidReadinessFixture f;
    MockAcceptableIEvaluator eval;
    MockBudgetController budget(false);
    auto verdict = evaluate_readiness(EvolutionState::Harness,
                                       f.attributed_record, eval, budget);
    CHECK(verdict.can_proceed == true);
    CHECK(verdict.failed_conditions.empty());
    CHECK(verdict.recommended_next == EvolutionState::Data);
}

TEST_CASE("evaluate_readiness Attributed fails → fail-closed deny",
          "[adr-0088][ac-3]") {
    ValidReadinessFixture f;
    f.attributed_record.verdict = AttributionVerdict::Insufficient;
    MockAcceptableIEvaluator eval;
    MockBudgetController budget(false);
    auto verdict = evaluate_readiness(EvolutionState::Harness,
                                       f.attributed_record, eval, budget);
    CHECK(verdict.can_proceed == false);
    CHECK(std::find(verdict.failed_conditions.begin(),
                    verdict.failed_conditions.end(),
                    "attribution_insufficient") != verdict.failed_conditions.end());
}

TEST_CASE("evaluate_readiness regression Poor → fail-closed deny",
          "[adr-0088][ac-3]") {
    ValidReadinessFixture f;
    MockPoorIEvaluator eval;
    MockBudgetController budget(false);
    auto verdict = evaluate_readiness(EvolutionState::Data,
                                       f.attributed_record, eval, budget);
    CHECK(verdict.can_proceed == false);
    CHECK(std::find(verdict.failed_conditions.begin(),
                    verdict.failed_conditions.end(),
                    "regression_failed") != verdict.failed_conditions.end());
}

TEST_CASE("evaluate_readiness budget exhausted → fail-closed deny",
          "[adr-0088][ac-3]") {
    ValidReadinessFixture f;
    MockAcceptableIEvaluator eval;
    MockBudgetController budget(true);
    auto verdict = evaluate_readiness(EvolutionState::Model,
                                       f.attributed_record, eval, budget);
    CHECK(verdict.can_proceed == false);
    CHECK(std::find(verdict.failed_conditions.begin(),
                    verdict.failed_conditions.end(),
                    "budget_exhausted") != verdict.failed_conditions.end());
}

TEST_CASE("evaluate_readiness accumulates all 3 conditions (no short-circuit)",
          "[adr-0088][ac-3]") {
    AttributionRecord attributed_fail;
    attributed_fail.verdict = AttributionVerdict::Insufficient;
    MockPoorIEvaluator eval;
    MockBudgetController budget(true);
    auto verdict = evaluate_readiness(EvolutionState::Data,
                                       attributed_fail, eval, budget);
    CHECK(verdict.can_proceed == false);
    CHECK(verdict.failed_conditions.size() == 3);
    CHECK(std::find(verdict.failed_conditions.begin(),
                    verdict.failed_conditions.end(),
                    "attribution_insufficient") != verdict.failed_conditions.end());
    CHECK(std::find(verdict.failed_conditions.begin(),
                    verdict.failed_conditions.end(),
                    "regression_failed") != verdict.failed_conditions.end());
    CHECK(std::find(verdict.failed_conditions.begin(),
                    verdict.failed_conditions.end(),
                    "budget_exhausted") != verdict.failed_conditions.end());
}

// AC-4
TEST_CASE("EvolutionState Done can reset to Idle via reset_to_idle",
          "[adr-0088][ac-4]") {
    EvolutionState current = EvolutionState::Done;
    EvolutionState reset = reset_to_idle(current);
    CHECK(reset == EvolutionState::Idle);
}

TEST_CASE("reset_to_idle from non-Done state is no-op", "[adr-0088][ac-4]") {
    CHECK(reset_to_idle(EvolutionState::Harness) == EvolutionState::Harness);
    CHECK(reset_to_idle(EvolutionState::Data) == EvolutionState::Data);
    CHECK(reset_to_idle(EvolutionState::Model) == EvolutionState::Model);
}

// AC-13
TEST_CASE("transition_guard functions are in agenticdsl::evolution namespace",
          "[adr-0088][ac-13]") {
    auto t = can_transition(EvolutionState::Idle, EvolutionState::Harness);
    CHECK(t == true);
}
