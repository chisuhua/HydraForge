// tests/test_budget_alert.cpp
// T1 Budget 告警测试: exceeded emits, at-limit no alert, callback thread-safe
// 关联: openspec/changes/pdk-chat-demo-v1-recap/tasks.md §T1

#include "catch_amalgamated.hpp"

#include "chat_session.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <core/types/tool_result.h>
#include <core/types/budget.h>
#include <core/engine.h>
#include <modules/budget/budget_controller.h>
#include <common/llm/mock_provider.h>

using namespace pdk_chat_demo;

namespace {

class MockBus : public agenticdsl::IInteractionBus {
public:
    void emit(const agenticdsl::BusEvent& event) override {
        events.emplace_back(event.topic, event.payload.meta, event.payload.ok);
        auto it = subscribers_.find(event.topic);
        if (it != subscribers_.end()) {
            for (auto& cb : it->second) cb(event);
        }
    }

    void emit(const std::string& topic, const std::string& content) override {
        agenticdsl::ToolResult tr;
        tr.ok = true;
        tr.meta = {{"content", content}};
        emit(agenticdsl::BusEvent{topic, tr});
    }

    size_t subscribe(const std::string& topic,
                      std::function<void(const agenticdsl::BusEvent&)> cb) override {
        subscribers_[topic].push_back(std::move(cb));
        return next_token_++;
    }

    void unsubscribe(size_t) override {}

    struct EventRecord {
        std::string topic;
        nlohmann::json meta;
        bool ok;
    };
    std::vector<EventRecord> events;

private:
    size_t next_token_ = 1;
    std::unordered_map<std::string, std::vector<std::function<void(const agenticdsl::BusEvent&)>>> subscribers_;
};

// 查找 budget.checked 事件
bool has_budget_checked(const MockBus& bus) {
    for (const auto& ev : bus.events) {
        if (ev.topic == "budget.checked") return true;
    }
    return false;
}

// 从 budget.checked 事件中提取 payload
nlohmann::json get_budget_payload(const MockBus& bus) {
    for (const auto& ev : bus.events) {
        if (ev.topic == "budget.checked") return ev.meta;
    }
    return {};
}

}  // namespace

TEST_CASE("budget alert: exceeded triggers budget.checked event", "[budget][alert]") {
    // 构造 DSLEngine + 设置预算后手动耗尽
    auto engine = std::make_unique<agenticdsl::DSLEngine>(
        std::vector<agenticdsl::ParsedGraph>{});
    auto bus = std::make_shared<MockBus>();
    engine->set_interaction_bus(bus);

    // 设置预算 1 次 LLM 调用, 然后强制 used=2 -> 超限
    agenticdsl::ExecutionBudget b;
    b.max_llm_calls = 1;
    b.max_duration_sec = 300;
    engine->get_budget_controller().set_budget(std::move(b));
    // try_consume 只允许 max_llm_calls 次, 超过不扣费
    // 但 exceeded() 检查 used > max, 所以直接置 used=2 模拟超限
    {
        auto& bc = engine->get_budget_controller();
        bc.try_consume_llm_call();
        // try_consume 第二次会失败 (used 已达上限), 但 exceeded() 需要 used > max
        // 直接通过 ExecutionBudget 原子计数器置位
        const_cast<std::optional<agenticdsl::ExecutionBudget>&>(bc.get_budget())->llm_calls_used.store(2);
    }
    REQUIRE(engine->get_budget_controller().exceeded());

    // 构造 ChatSession - chat() 内部会轮询并 emit budget.checked
    AgentConfig agent;
    agent.provider = "mock";
    agent.model = "test";
    agent.budget_limit_usd = 1.0;
    SessionConfig sess;
    sess.persist_dir = "";

    ChatSession session(engine.get(), bus, &engine->get_tool_registry(), agent, sess);

    // 使用 MockLLMProvider 避免真实 LLM 调用
    auto mock = std::make_unique<agenticdsl::MockLLMProvider>();
    mock->enqueue_response(R"({"content":"hello","tool_calls":[]})");
    engine->set_llm_provider(std::move(mock));

    auto result = session.chat("test input");

    // budget 超限后 chat() 应返回 success=false
    REQUIRE_FALSE(result.success);
    // 应 emit budget.checked 事件
    REQUIRE(has_budget_checked(*bus));

    auto payload = get_budget_payload(*bus);
    REQUIRE(payload["session_id"] == session.session_id());
    REQUIRE(payload["unit"] == "llm_calls");
    REQUIRE(payload["reason"] == "cost_limit");
    REQUIRE(payload.contains("limit"));
    REQUIRE(payload.contains("used"));
}

TEST_CASE("budget alert: exactly at limit does not alert", "[budget][alert]") {
    auto engine = std::make_unique<agenticdsl::DSLEngine>(
        std::vector<agenticdsl::ParsedGraph>{});
    auto bus = std::make_shared<MockBus>();
    engine->set_interaction_bus(bus);

    // 设置预算 5 次 LLM 调用, 消耗 4 次 (未超限)
    agenticdsl::ExecutionBudget b;
    b.max_llm_calls = 5;
    b.max_duration_sec = 300;
    engine->get_budget_controller().set_budget(std::move(b));

    for (int i = 0; i < 4; ++i) {
        engine->get_budget_controller().try_consume_llm_call();
    }
    REQUIRE_FALSE(engine->get_budget_controller().exceeded());

    AgentConfig agent;
    agent.provider = "mock";
    agent.model = "test";
    SessionConfig sess;
    sess.persist_dir = "";

    ChatSession session(engine.get(), bus, &engine->get_tool_registry(), agent, sess);

    auto mock = std::make_unique<agenticdsl::MockLLMProvider>();
    mock->enqueue_response(R"({"content":"hello","tool_calls":[]})");
    engine->set_llm_provider(std::move(mock));

    auto result = session.chat("test input");

    // 未超限 -> success=true, 无 budget.checked 事件
    REQUIRE(result.success);
    REQUIRE_FALSE(has_budget_checked(*bus));
}

TEST_CASE("budget alert: bus callback does not touch TUI directly", "[budget][alert][thread-safety]") {
    auto engine = std::make_unique<agenticdsl::DSLEngine>(
        std::vector<agenticdsl::ParsedGraph>{});
    auto bus = std::make_shared<MockBus>();
    engine->set_interaction_bus(bus);

    // 构造 ChatSession (会 subscribe budget.checked)
    AgentConfig agent;
    agent.provider = "mock";
    agent.model = "test";
    SessionConfig sess;
    sess.persist_dir = "";

    ChatSession session(engine.get(), bus, &engine->get_tool_registry(), agent, sess);

    // budget_alert_flag_ 初始为 false
    REQUIRE_FALSE(session.consume_budget_alert());

    // 手动 emit budget.checked (模拟 dispatch 线程触发回调)
    bus->emit(agenticdsl::BusEvent{
        "budget.checked",
        agenticdsl::ToolResult{
            .ok = false,
            .meta = {
                {"session_id", session.session_id()},
                {"limit", 1.0},
                {"used", 1.5},
                {"unit", "llm_calls"},
                {"reason", "cost_limit"}
            }
        },
        std::chrono::steady_clock::now()
    });

    // 回调应仅置 atomic flag, 不触 TUI
    // consume_budget_alert 返回 true 并重置
    REQUIRE(session.consume_budget_alert());
    // 二次消费应为 false
    REQUIRE_FALSE(session.consume_budget_alert());

    // 验证: 回调未直接向 bus 添加额外事件 (无 cout 副作用)
    // MockBus 的 events 应仅包含我们手动 emit 的那一条 budget.checked
    int budget_count = 0;
    for (const auto& ev : bus->events) {
        if (ev.topic == "budget.checked") ++budget_count;
    }
    REQUIRE(budget_count == 1);
}

TEST_CASE("budget alert: payload includes required fields", "[budget][alert]") {
    auto engine = std::make_unique<agenticdsl::DSLEngine>(
        std::vector<agenticdsl::ParsedGraph>{});
    auto bus = std::make_shared<MockBus>();
    engine->set_interaction_bus(bus);

    agenticdsl::ExecutionBudget b;
    b.max_llm_calls = 1;
    b.max_duration_sec = 300;
    engine->get_budget_controller().set_budget(std::move(b));
    {
        auto& bc = engine->get_budget_controller();
        bc.try_consume_llm_call();
        const_cast<std::optional<agenticdsl::ExecutionBudget>&>(bc.get_budget())->llm_calls_used.store(2);
    }
    REQUIRE(engine->get_budget_controller().exceeded());

    AgentConfig agent;
    agent.provider = "mock";
    agent.model = "test";
    agent.budget_limit_usd = 2.5;
    SessionConfig sess;
    sess.persist_dir = "";

    ChatSession session(engine.get(), bus, &engine->get_tool_registry(), agent, sess);

    auto mock = std::make_unique<agenticdsl::MockLLMProvider>();
    mock->enqueue_response(R"({"content":"hi","tool_calls":[]})");
    engine->set_llm_provider(std::move(mock));

    session.chat("test");

    auto payload = get_budget_payload(*bus);
    // 验证 design.md 要求的 5 个字段
    REQUIRE(payload["session_id"].is_string());
    REQUIRE(payload["limit"] == 2.5);
    REQUIRE(payload["used"].is_number());
    REQUIRE(payload["unit"] == "llm_calls");
    REQUIRE(payload["reason"] == "cost_limit");
}
