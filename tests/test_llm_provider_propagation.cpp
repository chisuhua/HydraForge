// tests/test_llm_provider_propagation.cpp
// loop-agent-dsl-execution: DSLEngine 双字段存储、from_markdown 新重载、装饰器链继承

#include "catch_amalgamated.hpp"
#include "core/engine.h"
#include "common/llm/mock_provider.h"
#include "agenticdsl/types/layered_context.h"

#include <thread>
#include <atomic>

using namespace agenticdsl;

static const char* kMinimalDSL = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
entry: start
nodes:
  - id: start
    type: assign
    assign:
      ok: "true"
    next: /main/end
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)";

TEST_CASE("from_markdown with parent provider uses it", "[engine][provider-propagation]") {
    MockLLMProvider parent;
    auto child = DSLEngine::from_markdown(kMinimalDSL, parent);
    REQUIRE(child->get_llm_provider() == &parent);

    LayeredContext ctx;
    auto result = child->run(ctx);
    REQUIRE(result.success);
    REQUIRE(result.final_context["ok"] == "true");
}

TEST_CASE("from_markdown single-param backward compat", "[engine][provider-propagation][backward-compat]") {
    auto engine = DSLEngine::from_markdown(kMinimalDSL);
    auto* provider = engine->get_llm_provider();
    REQUIRE(provider != nullptr);

    LayeredContext ctx;
    auto result = engine->run(ctx);
    REQUIRE(result.success);
}

TEST_CASE("set_borrowed_provider routes get_llm_provider", "[engine][provider-propagation][dual-field]") {
    MockLLMProvider borrowed;
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});
    auto* original = engine->get_llm_provider();
    REQUIRE(original != nullptr);

    engine->set_borrowed_provider(borrowed);
    REQUIRE(engine->get_llm_provider() == &borrowed);

    // set_llm_provider 清除 borrowed，owned 接管
    auto* new_mock_ptr = new MockLLMProvider();
    engine->set_llm_provider(std::unique_ptr<MockLLMProvider>(new_mock_ptr));
    // 经过 decorate_provider 后指针身份已变，但 borrowed 已被清除
    // 所以 get_llm_provider() 返回 owned (非空即可验证)
    REQUIRE(engine->get_llm_provider() != &borrowed);
    REQUIRE(engine->get_llm_provider() != nullptr);
}

TEST_CASE("from_markdown with provider bypasses decorate", "[engine][provider-propagation][cost]") {
    MockLLMProvider parent;
    auto child = DSLEngine::from_markdown(kMinimalDSL, parent);
    REQUIRE(child->get_llm_provider() == &parent);

    LayeredContext ctx;
    auto result = child->run(ctx);
    REQUIRE(result.success);
}

TEST_CASE("set_borrowed_provider overwrite", "[engine][provider-propagation][dual-field]") {
    MockLLMProvider a, b;
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});

    engine->set_borrowed_provider(a);
    REQUIRE(engine->get_llm_provider() == &a);

    engine->set_borrowed_provider(b);
    REQUIRE(engine->get_llm_provider() == &b);
}

TEST_CASE("from_markdown invalid DSL throws", "[engine][provider-propagation][error]") {
    MockLLMProvider parent;
    REQUIRE_THROWS_AS(
        DSLEngine::from_markdown("not valid DSL", parent),
        std::runtime_error
    );
}

TEST_CASE("from_markdown single-param invalid DSL throws", "[engine][provider-propagation][error]") {
    REQUIRE_THROWS_AS(
        DSLEngine::from_markdown("not valid DSL"),
        std::runtime_error
    );
}

TEST_CASE("react loop DSL can be parsed", "[engine][loop-agent][dsl]") {
    MockLLMProvider parent;
    auto child = DSLEngine::from_markdown(
        R"(### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: [/main/process]
  - id: process
    type: assign
    assign:
      response: "React: {{user_input}}"
    next: [/main/end]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)", parent);

    REQUIRE(child != nullptr);
    REQUIRE(child->get_llm_provider() == &parent);

    LayeredContext ctx;
    ctx.working["user_input"] = "hello react";
    auto result = child->run(ctx);
    REQUIRE(result.success);
}

TEST_CASE("plan_execute loop DSL can be parsed", "[engine][loop-agent][dsl]") {
    MockLLMProvider parent;
    auto child = DSLEngine::from_markdown(
        R"(### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: [/main/plan]
  - id: plan
    type: assign
    assign:
      response: "Plan: {{user_input}}"
    next: [/main/end]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)", parent);

    REQUIRE(child != nullptr);
    LayeredContext ctx;
    ctx.working["user_input"] = "hello plan_execute";
    auto result = child->run(ctx);
    REQUIRE(result.success);
}

TEST_CASE("fork_join loop DSL can be parsed", "[engine][loop-agent][dsl]") {
    MockLLMProvider parent;
    auto child = DSLEngine::from_markdown(
        R"(### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: [/main/process]
  - id: process
    type: assign
    assign:
      response: "ForkJoin: {{user_input}}"
    next: [/main/end]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)", parent);

    REQUIRE(child != nullptr);
    LayeredContext ctx;
    ctx.working["user_input"] = "hello fork_join";
    auto result = child->run(ctx);
    REQUIRE(result.success);
}

TEST_CASE("child provider chain preserves CostTrackingDecorator (no re-wrap)", "[engine][provider-propagation][cost]") {
    // 父引擎通过 set_llm_provider → decorate_provider 包裹 CostTrackingDecorator
    auto parent = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});
    auto mock = std::make_unique<MockLLMProvider>();
    mock->enqueue_response(R"({"content":"test","tool_calls":[]})");
    parent->set_llm_provider(std::move(mock));

    // 子引擎继承已装饰 provider
    auto child = DSLEngine::from_markdown(
        R"(### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: [/main/end]
  - id: end
    type: end
    termination_mode: hard
# --- END AgenticDSL ---
```
)", *parent->get_llm_provider());

    // 关键断言：child provider == parent provider（同一个对象，未重新装饰）
    auto* parent_provider = parent->get_llm_provider();
    auto* child_provider  = child->get_llm_provider();
    REQUIRE(child_provider == parent_provider);

    // verify generate() works through the chain
    GenerationRequest req;
    req.prompt = "test";
    auto result = child_provider->generate(req, std::stop_token{});
    REQUIRE(result.has_value());
}

TEST_CASE("dual-engine concurrency isolation", "[engine][provider-propagation][concurrency]") {
    // 验证不同线程的 engine 实例互不干扰
    MockLLMProvider pA, pB;

    std::atomic<bool> ready{false};
    std::atomic<int> phase{0};

    auto thread_func = [&](MockLLMProvider& provider, int expected_phase) {
        auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});
        engine->set_borrowed_provider(provider);

        // 等待两个线程都就绪
        phase++;
        while (phase < 2) { std::this_thread::yield(); }

        // 验证各自引擎的 provider 独立
        REQUIRE(engine->get_llm_provider() == &provider);
    };

    std::thread t1(thread_func, std::ref(pA), 0);
    std::thread t2(thread_func, std::ref(pB), 0);

    t1.join();
    t2.join();
}
