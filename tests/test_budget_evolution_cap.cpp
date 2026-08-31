// tests/test_budget_evolution_cap.cpp
// T3 evolution-budget-cap: N1 修复 - 进化周期预算上限
#include "catch_amalgamated.hpp"
#include "modules/budget/budget_controller.h"
#include "core/types/budget.h"

using namespace agenticdsl;

TEST_CASE("evolution budget: default -1 unlimited accepts any number", "[budget][evolution]") {
    ExecutionBudget b;
    b.max_evolution_llm_calls = -1;
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE_FALSE(b.evolution_budget_exceeded());
}

TEST_CASE("evolution budget: cap=3, 4th call returns false", "[budget][evolution]") {
    ExecutionBudget b;
    b.max_evolution_llm_calls = 3;
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE_FALSE(b.try_consume_evolution_llm_call());
    REQUIRE(b.evolution_budget_exceeded());
}

TEST_CASE("evolution budget independent from total llm_calls", "[budget][evolution]") {
    ExecutionBudget b;
    b.max_llm_calls = 10;
    b.max_evolution_llm_calls = 2;
    b.llm_calls_used = 10;
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE(b.try_consume_evolution_llm_call());
    REQUIRE_FALSE(b.try_consume_evolution_llm_call());
}

TEST_CASE("evolution budget reset via reset_evolution_cycle_counter", "[budget][evolution]") {
    ExecutionBudget b;
    b.max_evolution_llm_calls = 2;
    b.try_consume_evolution_llm_call();
    b.try_consume_evolution_llm_call();
    REQUIRE(b.evolution_budget_exceeded());
    b.reset_evolution_cycle_counter();
    REQUIRE_FALSE(b.evolution_budget_exceeded());
    REQUIRE(b.try_consume_evolution_llm_call());
}

TEST_CASE("ExecutionBudget move-construct resets evolution counter", "[budget][evolution]") {
    ExecutionBudget a;
    a.max_evolution_llm_calls = 2;
    a.try_consume_evolution_llm_call();
    a.try_consume_evolution_llm_call();
    REQUIRE(a.evolution_budget_exceeded());  // a used 2 of 2
    ExecutionBudget b = std::move(a);
    REQUIRE(b.max_evolution_llm_calls == 2);
    REQUIRE_FALSE(b.evolution_budget_exceeded());  // b counter reset to 0
    REQUIRE(b.try_consume_evolution_llm_call());  // b counter -> 1
    REQUIRE_FALSE(b.evolution_budget_exceeded());  // 1 < 2, OK
    REQUIRE(b.try_consume_evolution_llm_call());  // b counter -> 2
    REQUIRE(b.evolution_budget_exceeded());  // 2 >= 2
    REQUIRE_FALSE(b.try_consume_evolution_llm_call());  // 3rd call should fail
}

TEST_CASE("BudgetController evolution methods delegate to ExecutionBudget", "[budget][evolution][controller]") {
    ExecutionBudget budget;
    budget.max_evolution_llm_calls = 5;
    std::optional<ExecutionBudget> budget_opt;
    budget_opt.emplace(std::move(budget));
    BudgetController controller(std::move(budget_opt));
    REQUIRE(controller.try_consume_evolution_llm_call());
    REQUIRE(controller.try_consume_evolution_llm_call());
    REQUIRE_FALSE(controller.evolution_budget_exceeded());
    controller.begin_evolution_cycle("test_cycle_001");
    controller.end_evolution_cycle("test_cycle_001", true);
    REQUIRE(controller.try_consume_evolution_llm_call());
    controller.reset_evolution_cycle_counter();
    REQUIRE_FALSE(controller.evolution_budget_exceeded());
}