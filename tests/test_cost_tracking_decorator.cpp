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
  // T3 evolution-budget-cap additions
  bool try_consume_evolution_llm_call() override { return true; }
  bool evolution_budget_exceeded() const override { return false; }
  void begin_evolution_cycle(const std::string&) override {}
  void end_evolution_cycle(const std::string&, bool) override {}
  void reset_evolution_cycle_counter() override {}
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
// MAX_CHAIN_DEPTH=5 (innermost + 4 decorators); Phase 6a ADR-0068 bumped from 4 to 5
TEST_CASE("Decorator chain depth > 4 throws DecoratorChainTooDeep",
          "[decorator][chain][depth]") {
  auto innermost = std::make_unique<MockLLMProvider>();
  std::vector<std::function<std::unique_ptr<ILLMProvider>(std::unique_ptr<ILLMProvider>)>>
      factories;
  auto budget = std::make_shared<MockBudget>();
  for (int i = 0; i < 5; ++i) {
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

// === REQ-IPD-002 Scenario: 流式跟踪兜底计费 (流被提前销毁) ===
// Bug repro (修复前): TrackingStream 仅在 next() 返回 nullopt 时记录费用。
// 若调用方中途销毁流 (异常 / 取消 / 提前返回), recorded_ 保持 false,
// 费用永久不入账 — 又一次 budget hole。
TEST_CASE("CostTrackingDecorator charges on early stream destruction",
          "[decorator][cost][stream][destructor]") {
  auto mock = std::make_unique<MockLLMProvider>();
  mock->set_stream_tokens({"a", "b", "c", "d", "e"});  // 5 个 chunk
  auto budget = std::make_shared<MockBudget>();

  CostTrackingDecorator d(std::move(mock), budget);
  GenerationRequest req;
  req.prompt = "p";
  req.params.model = "m";
  req.params.max_tokens = 50;

  // 主动制造 partial consumption + 提前销毁场景:
  // 读 1 个 chunk 后不读了 — 既不到结尾 (recorded_ 应为 false),
  // 也直接丢掉 stream 对象。
  {
    auto stream = d.generate_stream(req, {});
    auto first = stream->next({});
    REQUIRE(first.has_value());
    REQUIRE(budget->call_count == 0);  // 中途不收费
    // stream 在这里离开作用域, ~TrackingStream 必须兜底收费
  }
  REQUIRE(budget->call_count == 1);
  REQUIRE(budget->last_tokens == 50);
  REQUIRE(budget->last_model == "m");
}

// === 配套: 完全不消费任何 chunk, 直接丢弃 stream, 仍需计费 ===
TEST_CASE("CostTrackingDecorator charges when stream never consumed",
          "[decorator][cost][stream][destructor]") {
  auto mock = std::make_unique<MockLLMProvider>();
  mock->set_stream_tokens({"a", "b", "c"});
  auto budget = std::make_shared<MockBudget>();

  CostTrackingDecorator d(std::move(mock), budget);
  GenerationRequest req;
  req.prompt = "p";
  req.params.model = "m";
  req.params.max_tokens = 25;

  {
    auto stream = d.generate_stream(req, {});
    // 故意连一次 next() 都不调用 — 只丢弃
  }
  REQUIRE(budget->call_count == 1);
  REQUIRE(budget->last_tokens == 25);
}
