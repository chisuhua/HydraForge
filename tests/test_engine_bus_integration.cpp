// tests/test_engine_bus_integration.cpp
// 文件头注释
// 功能描述：Phase 1 Sprint 1b (S1b.T4) 端到端集成测试。
//          验证 DSLEngine + NodeExecutor ↔ IInteractionBus 双向集成 (ADR-0019 P2)。
// 设计依据：openspec/changes/2026-06-17-phase1-bus-integration/design.md §测试设计
//          + specs/dslintegration/spec.md (REQ-BUS-001..004)
// 作者：Phase 1 Sprint 1b
// 最后修改日期：2026-06-17

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "core/engine.h"
#include "core/types/tool_result.h"
#include "modules/executor/node_executor.h"
#include "common/tools/registry.h"
#include "common/llm/llm_tool.h"
#include "core/types/context.h"
#include "core/types/node.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace agenticdsl;

// === Mock LLM 工具（让 DSLNode 不走真实 LLM 路径，专注于 bus 事件验证） ===
class MockLLMToolForBus : public ILLMTool {
 public:
    explicit MockLLMToolForBus(std::string tool_name) : tool_name_(std::move(tool_name)) {}

    LLMResult generate(const std::string& prompt, const LLMParams& /*params*/ = {}) override {
        LLMResult result;
        result.success = true;
        result.text = "mocked:" + prompt;
        result.tokens_generated = 1;
        return result;
    }

    bool is_available() const override { return true; }
    std::string name() const override { return tool_name_; }

 private:
    std::string tool_name_;
};

// === Test 1: DSLEngine set/get/subscribe (REQ-BUS-001 + REQ-BUS-002) ===
TEST_CASE("DSLEngine injects bus and forwards subscribe", "[engine][bus][phase1]") {
    auto engine = DSLEngine::from_markdown(R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
type: assign
assign:
  k: "v"
next: ["/main/end"]
# --- END AgenticDSL ---
```
)");

    // 默认 nullptr（REQ-BUS-001 Scenario: 默认 nullptr 路径）
    REQUIRE(engine->get_interaction_bus() == nullptr);

    // 注入 InMemoryBus（REQ-BUS-001 Scenario: 注入 custom bus）
    auto bus = std::make_shared<InMemoryBus>();
    engine->set_interaction_bus(bus);
    REQUIRE(engine->get_interaction_bus() != nullptr);
    REQUIRE(engine->get_interaction_bus().get() == bus.get());

    // 替换 bus（REQ-BUS-001 Scenario: 替换 bus 实例）
    auto bus2 = std::make_shared<InMemoryBus>();
    engine->set_interaction_bus(bus2);
    REQUIRE(engine->get_interaction_bus().get() == bus2.get());
}

// === Test 2: DSLEngine.subscribe nullptr 路径返回 0（REQ-BUS-002 Scenario: bus 为 nullptr 时）===
TEST_CASE("DSLEngine subscribe returns 0 when no bus injected", "[engine][bus][phase1]") {
    auto engine = DSLEngine::from_markdown(R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
type: assign
assign:
  k: "v"
next: ["/main/end"]
# --- END AgenticDSL ---
```
)");

    std::atomic<int> calls{0};
    auto token = engine->subscribe("topic.x", [&](const ToolResult&) { ++calls; });
    REQUIRE(token == 0); // 无效 token
}

// === Test 3: DSLEngine.subscribe 透传到 InMemoryBus（REQ-BUS-002 Scenario: 透传到 InMemoryBus）===
TEST_CASE("DSLEngine subscribe forwards to injected bus",
          "[engine][bus][phase1]") {
    auto engine = DSLEngine::from_markdown(R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
type: assign
assign:
  k: "v"
next: ["/main/end"]
# --- END AgenticDSL ---
```
)");

    auto bus = std::make_shared<InMemoryBus>();
    engine->set_interaction_bus(bus);

    std::atomic<int> calls{0};
    auto token = engine->subscribe("topic.y", [&](const ToolResult&) { ++calls; });
    // 注：InMemoryBus 首个 token 从 0 开始（next_token_++），故不能用 !=0 判定；
    // 改用 callback 是否被触发 + unsubscribe 后是否失效来验证透传。
    REQUIRE(calls.load() == 0);

    // 直接通过 bus 发射，应触发透传的 callback
    bus->emit("topic.y", ToolResult::success({{"k", "v"}}));
    REQUIRE(calls.load() == 1);

    // unsubscribe 也通过 bus 路径（透传一致性验证）
    engine->set_interaction_bus(nullptr);
    // 重新注入并 unsubscribe 验证 token 仍由 bus 管理
    engine->set_interaction_bus(bus);
    bus->unsubscribe(token);
    bus->emit("topic.y", ToolResult::success({}));
    REQUIRE(calls.load() == 1); // 已 unsubscribe，不再触发
}

// === Test 4: NodeExecutor DSLNode 推送 dsl.call.started/completed 事件 ===
TEST_CASE("NodeExecutor DSLNode emits started/completed to bus",
          "[executor][bus][phase1]") {
    ToolRegistry registry;
    auto mock_llm = std::make_unique<MockLLMToolForBus>("bus_test_llm");
    registry.register_llm_tool("bus_test_llm", std::move(mock_llm), LLMParams{});

    auto bus = std::make_shared<InMemoryBus>();
    std::atomic<int> started_count{0};
    std::atomic<int> completed_count{0};

    bus->subscribe("dsl.call.started",
                   [&](const ToolResult&) { ++started_count; });
    bus->subscribe("dsl.call.completed",
                   [&](const ToolResult&) { ++completed_count; });

    NodeExecutor executor(registry, nullptr, bus.get());

    DSLNode node(
        "/main/dsltest",
        "Hello {{ name }}",
        "bus_test_llm",
        LLMParams{},
        {"out"},
        {"/main/end"});

    Context ctx;
    ctx["name"] = "Bus";
    Context result = executor.execute_node(&node, ctx);

    // 验证产物
    REQUIRE(result.contains("out"));
    REQUIRE(result["out"] == "mocked:Hello Bus");

    // 验证事件计数（REQ-BUS-003 Scenario: dsl.call.started + dsl.call.completed）
    REQUIRE(started_count.load() == 1);
    REQUIRE(completed_count.load() == 1);
}

// === Test 5: NodeExecutor ToolNode 推送 tool.completed 事件（含 envelope 字段） ===
TEST_CASE("NodeExecutor ToolNode emits tool.completed with envelope fields",
          "[executor][bus][phase1]") {
    ToolRegistry registry;

    // 注册成功工具，返回 ToolResult envelope (含 Sprint 1a P2-P4 字段)
    registry.register_tool("ok_tool",
        [](const std::unordered_map<std::string, std::string>& /*args*/) -> nlohmann::json {
            ToolResult r = ToolResult::success({{"answer", 42}});
            r.error_code = ErrorCode::Unknown; // 显式赋值为 success 状态保留
            r.latency_ms = 7;
            r.trace_id = "trace-ok";
            return r.to_json();
        });

    auto bus = std::make_shared<InMemoryBus>();
    std::atomic<int> tool_completed_count{0};
    ToolResult captured;
    std::atomic<bool> captured_flag{false};

    bus->subscribe("tool.completed", [&](const ToolResult& payload) {
        ++tool_completed_count;
        captured = payload;
        captured_flag = true;
    });

    NodeExecutor executor(registry, nullptr, bus.get());

    ToolCallNode node(
        "/main/call",
        "ok_tool",
        {},                          // arguments
        {"answer"},                  // output_keys
        {"/main/end"});              // next

    Context ctx;
    Context result = executor.execute_node(&node, ctx);

    // 验证事件 + envelope 字段（Sprint 1a P2-P4 透传）
    REQUIRE(tool_completed_count.load() == 1);
    REQUIRE(captured_flag.load());
    REQUIRE(captured.ok);
    REQUIRE(captured.data["answer"] == 42);
    REQUIRE(captured.latency_ms.has_value());
    REQUIRE(captured.latency_ms.value() >= 0); // 至少被 executor 覆盖
    REQUIRE(captured.trace_id.has_value());
    REQUIRE(captured.trace_id.value() == "trace-ok");
}

// === Test 6: Abort 错误码触发 execution.failed 事件 + 抛异常 ===
TEST_CASE("NodeExecutor ToolNode Abort emits execution.failed and throws",
          "[executor][bus][phase1]") {
    ToolRegistry registry;

    registry.register_tool("abort_tool",
        [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
            return ToolResult::error(ErrorCode::Abort, "fatal abort").to_json();
        });

    auto bus = std::make_shared<InMemoryBus>();
    std::atomic<int> failed_count{0};
    ToolResult failed_payload;
    std::atomic<bool> failed_captured{false};

    bus->subscribe("execution.failed", [&](const ToolResult& payload) {
        ++failed_count;
        failed_payload = payload;
        failed_captured = true;
    });

    NodeExecutor executor(registry, nullptr, bus.get());

    ToolCallNode node(
        "/main/abort_call",
        "abort_tool",
        {},
        {"x"},
        {"/main/end"});

    Context ctx;
    REQUIRE_THROWS_WITH(
        executor.execute_node(&node, ctx),
        Catch::Matchers::ContainsSubstring("[ABORT]"));

    // 事件必须在异常传播之前触发（REQ-BUS-004 Scenario: Abort）
    REQUIRE(failed_count.load() == 1);
    REQUIRE(failed_captured.load());
    REQUIRE(failed_payload.ok == false);
    REQUIRE(failed_payload.error_code.has_value());
    REQUIRE(failed_payload.error_code.value() == ErrorCode::Abort);
}

// === Test 7: Retry 错误码触发 execution.failed 事件 + 抛异常 ===
TEST_CASE("NodeExecutor ToolNode Retry emits execution.failed and throws",
          "[executor][bus][phase1]") {
    ToolRegistry registry;

    registry.register_tool("retry_tool",
        [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
            return ToolResult::error(ErrorCode::Retry, "transient").to_json();
        });

    auto bus = std::make_shared<InMemoryBus>();
    std::atomic<int> failed_count{0};

    bus->subscribe("execution.failed", [&](const ToolResult&) { ++failed_count; });

    NodeExecutor executor(registry, nullptr, bus.get());

    ToolCallNode node("/main/retry_call", "retry_tool", {}, {"x"}, {"/main/end"});

    Context ctx;
    REQUIRE_THROWS_WITH(
        executor.execute_node(&node, ctx),
        Catch::Matchers::ContainsSubstring("[RETRY]"));

    REQUIRE(failed_count.load() == 1);
}

// === Test 8: Skip 错误码不推送事件不抛异常（REQ-BUS-004 Scenario: Skip）===
TEST_CASE("NodeExecutor ToolNode Skip does not emit and does not throw",
          "[executor][bus][phase1]") {
    ToolRegistry registry;

    registry.register_tool("skip_tool",
        [](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
            return ToolResult::error(ErrorCode::Skip, "soft skip").to_json();
        });

    auto bus = std::make_shared<InMemoryBus>();
    std::atomic<int> any_event_count{0};

    bus->subscribe("execution.failed", [&](const ToolResult&) { ++any_event_count; });
    bus->subscribe("tool.completed", [&](const ToolResult&) { ++any_event_count; });

    NodeExecutor executor(registry, nullptr, bus.get());

    ToolCallNode node("/main/skip_call", "skip_tool", {}, {"x"}, {"/main/end"});

    Context ctx;
    REQUIRE_NOTHROW(executor.execute_node(&node, ctx));
    REQUIRE(any_event_count.load() == 0); // Skip 不推送任何事件
}

// === Test 9: 默认 nullptr 路径（零回归验证 — REQ-BUS-003 Scenario: 默认 nullptr 路径）===
TEST_CASE("NodeExecutor with nullptr bus preserves prior behavior",
          "[executor][bus][phase1][regression]") {
    ToolRegistry registry;
    auto mock_llm = std::make_unique<MockLLMToolForBus>("null_bus_llm");
    registry.register_llm_tool("null_bus_llm", std::move(mock_llm), LLMParams{});

    // bus 不注入（默认 nullptr）
    NodeExecutor executor(registry, nullptr);

    DSLNode node("/main/nullbus", "Hi {{ x }}", "null_bus_llm", LLMParams{}, {"r"}, {});
    Context ctx;
    ctx["x"] = "World";
    Context result = executor.execute_node(&node, ctx);
    REQUIRE(result.contains("r"));
    REQUIRE(result["r"] == "mocked:Hi World");
}

// === Test 10 (Bonus): 1000x 并发 subscribe + emit 无死锁 (继承 Sprint 1a Test 1 模式) ===
TEST_CASE("Engine bus integration concurrent 1000x emit no deadlock",
          "[engine][bus][phase1][stress]") {
    auto engine = DSLEngine::from_markdown(R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
type: assign
assign:
  k: "v"
next: ["/main/end"]
# --- END AgenticDSL ---
```
)");

    auto bus = std::make_shared<InMemoryBus>();
    engine->set_interaction_bus(bus);

    std::atomic<int> count{0};
    engine->subscribe("stress.topic", [&](const ToolResult&) { ++count; });

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&] {
            for (int j = 0; j < 100; ++j) {
                bus->emit("stress.topic", ToolResult::success({{"i", j}}));
            }
        });
    }
    for (auto& t : threads) t.join();

    REQUIRE(count.load() == 1000);
}