// test_cost_tracking_decorator.cpp
// Phase 5 ILLMProvider v2 — CostTrackingDecorator integration tests
// REQ-IPD-002, REQ-IPD-001 Scenario 2 (chain depth), Task 1.4.2 (streaming precision)

#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "common/llm/cost_tracking_decorator.h"
#include "common/llm/mock_provider.h"
#include "modules/budget/budget_controller.h"
#include "core/types/node.h"

#include <catch_amalgamated.hpp>
#include <memory>
#include <atomic>
#include <string>

using namespace agenticdsl;

namespace {

/// Test double: 一个最小化的 IBudgetController,记录所有 record_llm_call 调用以便断言
class MockBudget : public IBudgetController {
 public:
  std::atomic<int> call_count{0};
  std::atomic<int> last_tokens{0};
  std::string last_model;

  bool try_consume_node() override { return true; }
  bool try_consume_llm_call() override { return true; }
  bool try_consume_subgraph_depth() override { return true; }
  bool exceeded() const override { return false; }

  void set_termination_target(const NodePath&) override {}
  std::optional<NodePath> get_termination_target() const override { return std::nullopt; }
  const std::optional<ExecutionBudget>& get_budget() const override {
    static std::optional<ExecutionBudget> empty;
    return empty;
  }
  void set_budget(std::optional<ExecutionBudget>) override {}

  void record_llm_call(int tokens, const std::string& model) override {
    call_count++;
    last_tokens = tokens;
    last_model = model;
  }
  double get_total_cost_usd() const override { return 0.0; }
  void reset() override {}
};

}  // namespace

// === REQ-IPD-002 Scenario "同步 generate 计费" ===
TEST_CASE("CostTrackingDecorator charges on success", "[decorator][cost][sync]") {
  auto mock = std::make_unique<MockLLMProvider>();
  mock->set_fixed_response(GenerationResult{"hello", 10, 5, "stop"});
  auto budget = std::make_shared<MockBudget>();

  CostTrackingDecorator d(std::move(mock), budget);
  GenerationRequest req;
  req.prompt = "hi";
  req.params.model = "mock-llm-v1";
  auto r = d.generate(req, {});
  REQUIRE(r.has_value());
  REQUIRE(budget->call_count == 1);
  REQUIRE(budget->last_tokens == 15);  // 10 + 5
  REQUIRE(budget->last_model == "mock-llm-v1");
}

// === REQ-IPD-002 Scenario "错误结果不计费" ===
TEST_CASE("CostTrackingDecorator does not charge on failure", "[decorator][cost][failure]") {
  auto mock = std::make_unique<MockLLMProvider>();
  mock->set_simulate_error(LLMError::Code::NetworkError, "down");
  auto budget = std::make_shared<MockBudget>();

  CostTrackingDecorator d(std::move(mock), budget);
  GenerationRequest req;
  req.prompt = "x";
  req.params.model = "m";
  auto r = d.generate(req, {});
  REQUIRE_FALSE(r.has_value());
  REQUIRE(budget->call_count == 0);  // 失败不计费
}

// === REQ-IPD-002 Scenario "流式 generate_stream 计费" ===
TEST_CASE("CostTrackingDecorator charges on stream end", "[decorator][cost][stream]") {
  auto mock = std::make_unique<MockLLMProvider>();
  mock->set_stream_tokens({"hi", "world"});  // 2 tokens per chunk approximation
  auto budget = std::make_shared<MockBudget>();

  CostTrackingDecorator d(std::move(mock), budget);
  GenerationRequest req;
  req.prompt = "x";
  req.params.model = "m";
  req.params.max_tokens = 50;
  auto stream = d.generate_stream(req, {});
  while (stream->next({}).has_value()) {}
  REQUIRE(budget->call_count == 1);
  REQUIRE(budget->last_tokens > 0);
  REQUIRE(budget->last_model == "m");
}

// === REQ-IPD-001 Scenario "装饰器链深度限制" ===
TEST_CASE("Decorator chain depth > 3 throws DecoratorChainTooDeep",
          "[decorator][chain][depth]") {
  auto innermost = std::make_unique<MockLLMProvider>();
  std::vector<std::function<std::unique_ptr<ILLMProvider>(std::unique_ptr<ILLMProvider>)>>
      factories;
  // 4 层 = innermost + 3 decorators = max_depth=4 OK. 5 层 = throw.
  auto budget = std::make_shared<MockBudget>();
  for (int i = 0; i < 4; ++i) {  // 4 个装饰器 = 超出 3 上限
    factories.emplace_back([budget](std::unique_ptr<ILLMProvider> inner) {
      return std::make_unique<CostTrackingDecorator>(std::move(inner), budget);
    });
  }
  REQUIRE_THROWS_AS(
      ILLMProviderDecorator::wrap_chain(std::move(innermost), std::move(factories)),
      ILLMProviderDecorator::DecoratorChainTooDeep);
}

// === REQ-IPD-005 default order: CostTracking wraps MockLLMProvider ===
TEST_CASE("CostTrackingDecorator passes through results unchanged",
          "[decorator][passthrough]") {
  auto mock = std::make_unique<MockLLMProvider>();
  mock->set_fixed_response(GenerationResult{"exact-text", 1, 2, "stop"});
  auto budget = std::make_shared<MockBudget>();
  CostTrackingDecorator d(std::move(mock), budget);

  GenerationRequest req;
  req.prompt = "p";
  req.params.model = "m";
  auto r = d.generate(req, {});
  REQUIRE(r.has_value());
  // 业务返回值不变 (REQ-IPD-005 Scenario 4)
  REQUIRE(r.value().text == "exact-text");
  REQUIRE(r.value().prompt_tokens == 1);
  REQUIRE(r.value().completion_tokens == 2);
}
