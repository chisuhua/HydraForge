// tests/test_chat_session_loop_result_ok.cpp
// 5 个 TEST_CASE 验证 ChatSession::chat() 对 loop_result 的 success/error_message 传播
// (per openspec/changes/2026-09-16-fix-loop-run-return-contract)
#include "catch_amalgamated.hpp"

#include <agenticdsl/pdk/chat_session.h>

#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/event_builder.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <agenticdsl/contract/inmemory_bus.h>
#include <common/tools/registry.h>
#include <core/engine.h>

#include <memory>
#include <unordered_map>

using namespace agenticdsl;
using hydraforge::pdk::ChatSession;

// helper: 注册 mock loop/run 返回指定 JSON
static void register_mock_loop_run(ToolRegistry& reg, nlohmann::json mock_response) {
    reg.register_tool_function("loop/run",
        ToolMetadata{
            .name = "loop/run",
            .description = "mock loop/run for test",
            .domain = "loop",
            .category = ToolCategory::Execute,
            .min_layer = LayerProfile::Workflow,
            .approval = ApprovalPolicy{
                .requires_approval_in_plan = false,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = false
            },
            .allowed_layers = {LayerProfile::Workflow}
        },
        [mock_response](const std::unordered_map<std::string, std::string>&) -> nlohmann::json {
            return mock_response;
        });
}

// fixture: engine + registry + bus + session 都在同一作用域, engine 生命周期长于 session
// 防止 session 持有 dangling engine 指针
struct ChatSessionFixture {
    std::unique_ptr<DSLEngine> engine;
    std::shared_ptr<InMemoryBus> bus;
    std::unique_ptr<ToolRegistry> registry;
    std::unique_ptr<ChatSession> session;

    ChatSessionFixture(nlohmann::json mock_response) {
        registry = std::make_unique<ToolRegistry>();
        register_mock_loop_run(*registry, mock_response);
        engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});
        bus = std::make_shared<InMemoryBus>();
        engine->set_interaction_bus(bus);
        hydraforge::pdk::AgentConfig agent_cfg;
        hydraforge::pdk::SessionConfig session_cfg;
        session_cfg.persist_dir = "";
        session = std::make_unique<ChatSession>(
            engine.get(), bus, registry.get(), agent_cfg, session_cfg);
    }
};

// Case 4 专用 fixture: 空 registry
struct ChatSessionFixtureEmpty {
    std::unique_ptr<DSLEngine> engine;
    std::shared_ptr<InMemoryBus> bus;
    std::unique_ptr<ToolRegistry> registry;
    std::unique_ptr<ChatSession> session;

    ChatSessionFixtureEmpty() {
        registry = std::make_unique<ToolRegistry>();
        engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});
        bus = std::make_shared<InMemoryBus>();
        engine->set_interaction_bus(bus);
        hydraforge::pdk::AgentConfig agent_cfg;
        hydraforge::pdk::SessionConfig session_cfg;
        session_cfg.persist_dir = "";
        session = std::make_unique<ChatSession>(
            engine.get(), bus, registry.get(), agent_cfg, session_cfg);
    }
};

TEST_CASE("ChatSession: loop_result.ok=true 成功路径", "[chat_session][loop_result]") {
    ChatSessionFixture fx(nlohmann::json{
        {"ok", true}, {"success", true}, {"error_code", nullptr},
        {"response", "hello"}, {"steps", 1}, {"tokens_used", 10}, {"cost_usd", 0.001}
    });
    auto result = fx.session->chat("test input");
    REQUIRE(result.success == true);
    REQUIRE(result.response == "hello");
    REQUIRE(result.total_steps == 1);
    REQUIRE(result.error_message.empty());
}

TEST_CASE("ChatSession: loop_result.ok=false ToolNotRegistered 错误路径", "[chat_session][loop_result]") {
    ChatSessionFixture fx(nlohmann::json{
        {"ok", false}, {"success", false}, {"error_code", "ToolNotRegistered"},
        {"error", "Tool not found: loop/decide_react"}, {"response", ""},
        {"steps", 0}, {"tokens_used", 0}, {"cost_usd", 0.0}
    });
    auto result = fx.session->chat("test input");
    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Tool not found: loop/decide_react");
    REQUIRE(result.total_steps == 0);
}

TEST_CASE("ChatSession: 缺 ok 字段但含 success 字段 (向后兼容)", "[chat_session][loop_result]") {
    ChatSessionFixture fx(nlohmann::json{
        {"success", true}, {"response", "legacy"}, {"steps", 1}
    });
    auto result = fx.session->chat("test input");
    REQUIRE(result.success == true);
    REQUIRE(result.response == "legacy");
}

TEST_CASE("ChatSession: registry 错误信封 (仅含 error 字段, C3 关键)", "[chat_session][loop_result]") {
    ChatSessionFixtureEmpty fx;
    auto result = fx.session->chat("test input");
    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Tool not found: loop/run");
}

TEST_CASE("ChatSession: ok=false Cancelled 错误码", "[chat_session][loop_result]") {
    ChatSessionFixture fx(nlohmann::json{
        {"ok", false}, {"success", false}, {"error_code", "Cancelled"},
        {"error", "cancelled"}, {"response", ""},
        {"steps", 0}, {"tokens_used", 0}, {"cost_usd", 0.0}
    });
    auto result = fx.session->chat("test input");
    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "cancelled");
}

TEST_CASE("ChatSession: registry 'Tool not found' 错误信封 remap 到 ToolNotRegistered (Major #2 修正)", "[chat_session][loop_result]") {
    // Per Oracle review Major #2: registry-level 错误信封缺 error_code 字段,
    // 默认 "Unknown" 错失语义. ChatSession 应基于 error_message 前缀 remap.
    std::vector<BusEvent> captured;
    ChatSessionFixtureEmpty fx;
    fx.bus->subscribe("loop.error", [&](const BusEvent& e) { captured.push_back(e); });
    auto result = fx.session->chat("test input");
    REQUIRE(result.success == false);
    REQUIRE(result.error_message == "Tool not found: loop/run");
    REQUIRE(captured.size() == 1);
    REQUIRE(captured[0].payload.data["error"] == "Tool not found: loop/run");
    REQUIRE(captured[0].payload.data["error_code"] == "ToolNotRegistered");
}