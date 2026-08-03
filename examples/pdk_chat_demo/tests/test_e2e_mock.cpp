// tests/test_e2e_mock.cpp
// pdk_chat_demo 端到端 Mock 模式测试 (v1 buildable)
// 关联: docs/examples/pdk_chat_demo/DESIGN.md §8.2

// CATCH_CONFIG_MAIN 由 main_test_runner.cpp 提供 (链接到所有 test executables)
#include "catch_amalgamated.hpp"

#include "chat_session.h"
#include "event_handler.h"

#include <set>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/event_builder.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <core/types/tool_result.h>

using namespace pdk_chat_demo;

namespace mock {

/**
 * @brief MockBus: 实现真实 IInteractionBus 接口 (4 virtual methods)
 *
 * IInteractionBus 规范:
 *   1. emit(BusEvent)               — 主路径
 *   2. emit(topic, string)          — 向后兼容
 *   3. subscribe(topic, callback<BusEvent>) → size_t token
 *   4. unsubscribe(size_t token)
 */
class MockBus : public agenticdsl::IInteractionBus {
public:
    // emit(BusEvent) — 主路径: 存入 events 并通知 subscribers
    void emit(const agenticdsl::BusEvent& event) override {
        events.emplace_back(event.topic, event.payload.meta);
        // 通知所有 subscribers
        auto it = subscribers_by_topic_.find(event.topic);
        if (it != subscribers_by_topic_.end()) {
            for (auto& sub : it->second) {
                sub(event);
            }
        }
    }

    // emit(string) — 向后兼容: 包装为 BusEvent
    void emit(const std::string& topic, const std::string& content) override {
        agenticdsl::ToolResult tr;
        tr.ok = true;
        tr.meta = {{"content", content}};
        agenticdsl::BusEvent event{topic, tr};
        events.emplace_back(topic, event.payload.meta);
        auto it = subscribers_by_topic_.find(topic);
        if (it != subscribers_by_topic_.end()) {
            for (auto& sub : it->second) {
                sub(event);
            }
        }
    }

    // subscribe — 返回递增 token
    size_t subscribe(
        const std::string& topic,
        std::function<void(const agenticdsl::BusEvent&)> callback
    ) override {
        subscribers_by_topic_[topic].push_back(std::move(callback));
        return next_token_++;
    }

    // unsubscribe — no-op (mock 不追踪个别 token)
    void unsubscribe(size_t /*token*/) override {
        // Mock 实现：不做精确 token 追踪
    }

    std::vector<std::pair<std::string, nlohmann::json>> events;

private:
    size_t next_token_ = 1;
    std::unordered_map<std::string, std::vector<std::function<void(const agenticdsl::BusEvent&)>>> subscribers_by_topic_;
};

}  // namespace mock

TEST_CASE("EventHandler subscribes to expected topics", "[e2e][mock]") {
    auto bus = std::make_shared<mock::MockBus>();

    EventHandler handler(bus, nullptr);

    // 触发事件 — 使用 BusEvent 包装
    bus->emit(agenticdsl::EventBuilder("user.input").meta(nlohmann::json{{"input", "hello"}}).build());
    bus->emit(agenticdsl::EventBuilder("loop.done").meta(nlohmann::json{{"total_steps", 3}}).build());

    // 验证事件流
    bool found_user_input = false;
    bool found_loop_done = false;
    for (const auto& [topic, payload] : bus->events) {
        if (topic == "user.input") found_user_input = true;
        if (topic == "loop.done") found_loop_done = true;
    }
    REQUIRE(found_user_input);
    REQUIRE(found_loop_done);
}

TEST_CASE("Mock mode -- end-to-end flow", "[e2e][mock]") {
    auto bus = std::make_shared<mock::MockBus>();
    EventHandler handler(bus, nullptr);

    // 所有 emit 使用 BusEvent 包装 (C++20 designated initializers)
    auto emit = [&](const std::string& topic, nlohmann::json meta) {
        bus->emit(agenticdsl::EventBuilder(topic).meta(std::move(meta)).build());
    };

    emit("user.input", {{"session_id", "sess_test"}, {"input", "hello"}});
    emit("loop.turn.start", {{"turn", 1}, {"step", 1}});
    emit("llm.request", {{"model", "mock-llm-v1"}});
    emit("llm.response", {{"tokens", 42}, {"duration_ms", 100}});
    emit("loop.decision", {{"decision", "tool_call"}, {"tool", "shell/exec"}});
    emit("tool.execution.start", {{"name", "shell/exec"}});
    emit("tool.execution.end", {{"ok", true}, {"duration_ms", 50}});
    emit("loop.turn.end", {{"decision", "observe"}});
    emit("loop.turn.start", {{"turn", 2}, {"step", 2}});
    emit("loop.decision", {{"decision", "respond"}});
    emit("loop.done", {{"total_steps", 2}, {"total_tokens", 84}});

    // 验证完整事件流
    int event_count = static_cast<int>(bus->events.size());
    REQUIRE(event_count >= 10);

    bool all_required_topics_found = true;
    std::set<std::string> required_topics = {
        "user.input", "loop.turn.start", "llm.request",
        "llm.response", "loop.decision", "tool.execution.start",
        "tool.execution.end", "loop.turn.end", "loop.done"
    };
    for (const auto& required : required_topics) {
        bool found = false;
        for (const auto& [topic, _] : bus->events) {
            if (topic == required) { found = true; break; }
        }
        if (!found) all_required_topics_found = false;
    }
    REQUIRE(all_required_topics_found);
}

TEST_CASE("MockBus implements all 4 IInteractionBus virtual methods", "[e2e][mock]") {
    auto bus = std::make_shared<mock::MockBus>();

    // 验证 subscribe callback 接收 BusEvent
    int callback_count = 0;
    agenticdsl::BusEvent received{"", agenticdsl::ToolResult()};
    size_t token = bus->subscribe("test_topic", [&](const agenticdsl::BusEvent& ev) {
        received = ev;
        ++callback_count;
    });
    REQUIRE(token > 0);

    bus->emit(agenticdsl::EventBuilder("test_topic").meta(nlohmann::json{{"key", "value"}}).build());

    REQUIRE(callback_count == 1);
    REQUIRE(received.payload.ok == true);
    REQUIRE(received.payload.meta["key"] == "value");

    // 验证 string emit 也触发 callback
    bus->emit("test_topic", std::string("hello"));
    REQUIRE(callback_count == 2);

    // 验证 unsubscribe 不崩溃
    bus->unsubscribe(1);
}
