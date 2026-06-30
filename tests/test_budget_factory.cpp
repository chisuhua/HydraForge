// tests/test_budget_factory.cpp
// Sprint 16 Coverage Backfill: 测试 src/modules/budget/factory.cpp
#include "catch_amalgamated.hpp"
#include "budget/factory.h"
#include "modules/budget/budget_controller.h"

using namespace agenticdsl;

TEST_CASE("budget::create_controller returns IBudgetController", "[budget][factory]") {
  auto controller = budget::create_controller();
  REQUIRE(controller != nullptr);

  // 验证返回的是 IBudgetController 抽象类型, 可正常调用虚函数
  REQUIRE(controller->try_consume_node());
  REQUIRE(controller->try_consume_llm_call());
  REQUIRE(controller->try_consume_subgraph_depth());
  REQUIRE_FALSE(controller->exceeded());
  REQUIRE(controller->get_budget().has_value() == false);
  REQUIRE(controller->get_total_cost_usd() == 0.0);
}

TEST_CASE("budget::create_controller returns independent instances", "[budget][factory]") {
  auto c1 = budget::create_controller();
  auto c2 = budget::create_controller();

  REQUIRE(c1 != nullptr);
  REQUIRE(c2 != nullptr);
  REQUIRE(c1.get() != c2.get());

  // 各自消耗后, 互不影响
  REQUIRE(c1->try_consume_node());
  REQUIRE(c2->try_consume_node());

  // 析构第一个不应影响第二个
  c1.reset();
  REQUIRE(c2->try_consume_node());
}

TEST_CASE("budget::create_controller BudgetController concrete behavior", "[budget][factory]") {
  auto controller = budget::create_controller();

  // 通过 IBudgetController 抽象接口调用 record_llm_call
  controller->record_llm_call(/*tokens=*/100, /*model=*/"test-model");
  REQUIRE(controller->get_total_cost_usd() > 0.0);

  // reset 后 cost 应清零
  controller->reset();
  REQUIRE(controller->get_total_cost_usd() == 0.0);
}