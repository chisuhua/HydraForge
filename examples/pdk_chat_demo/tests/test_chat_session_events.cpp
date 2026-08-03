// tests/test_chat_session_events.cpp
// ChatSession 事件发射测试 (ADR-0068 §4)
// 覆盖: session.persisted / user.input / session.persist_request / budget.checked
//       以及 loop.done 的 EventBuilder 迁移

#include "catch_amalgamated.hpp"

#include "chat_session.h"

#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/event_builder.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <agenticdsl/contract/inmemory_bus.h>
#include <core/engine.h>
#include <core/types/budget.h>
#include <modules/budget/budget_controller.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace pdk_chat_demo;

namespace {

class TempDir {
public:
    explicit TempDir(const std::string& prefix = "hf_session_events_") {
        auto t = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() / (prefix + std::to_string(t));
        std::filesystem::create_directories(path_);
        path_str_ = path_.string();
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    const std::string& str() const { return path_str_; }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    std::string path_str_;
};

}  // namespace

TEST_CASE("ChatSession emits session.persisted after successful save", "[chat_session][event]") {
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    agenticdsl::DSLEngine engine{std::vector<agenticdsl::ParsedGraph>{}};
    engine.set_interaction_bus(bus);

    TempDir tmp;
    AgentConfig agent_cfg;
    SessionConfig session_cfg;
    session_cfg.persist_dir = tmp.str();

    ChatSession session(&engine, bus, &engine.get_tool_registry(), agent_cfg, session_cfg);

    bool found_persisted = false;
    std::string persisted_path;
    bus->subscribe("session.persisted", [&](const agenticdsl::BusEvent& ev) {
        found_persisted = true;
        REQUIRE(ev.payload.ok == true);
        REQUIRE(ev.payload.data["session_id"] == session.session_id());
        REQUIRE(ev.payload.data.contains("path"));
        persisted_path = ev.payload.data["path"].get<std::string>();
    });

    auto result = session.chat("hello");
    bus->wait_for_drain();

    REQUIRE(found_persisted == true);
    REQUIRE(result.success == true);
    REQUIRE(std::filesystem::exists(persisted_path));
}

TEST_CASE("ChatSession does not emit session.persisted when save fails", "[chat_session][event]") {
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    agenticdsl::DSLEngine engine{std::vector<agenticdsl::ParsedGraph>{}};
    engine.set_interaction_bus(bus);

    AgentConfig agent_cfg;
    SessionConfig session_cfg;
    session_cfg.persist_dir = "";  // 空目录 -> save_to_disk 返回 false

    ChatSession session(&engine, bus, &engine.get_tool_registry(), agent_cfg, session_cfg);

    int persisted_count = 0;
    bus->subscribe("session.persisted", [&](const agenticdsl::BusEvent&) {
        ++persisted_count;
    });

    REQUIRE_FALSE(session.save_to_disk());
    bus->wait_for_drain();

    REQUIRE(persisted_count == 0);
}

TEST_CASE("ChatSession emits user.input via EventBuilder", "[chat_session][event]") {
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    agenticdsl::DSLEngine engine{std::vector<agenticdsl::ParsedGraph>{}};
    engine.set_interaction_bus(bus);

    AgentConfig agent_cfg;
    SessionConfig session_cfg;
    session_cfg.persist_dir = "";  // 不触发持久化

    ChatSession session(&engine, bus, &engine.get_tool_registry(), agent_cfg, session_cfg);

    const std::string user_input = "hello world";
    bool found_user_input = false;
    bus->subscribe("user.input", [&](const agenticdsl::BusEvent& ev) {
        found_user_input = true;
        REQUIRE(ev.payload.ok == true);
        REQUIRE(ev.payload.data["input"] == user_input);
        REQUIRE(ev.payload.meta["session_id"] == session.session_id());
    });

    auto result = session.chat(user_input);
    bus->wait_for_drain();

    REQUIRE(found_user_input == true);
    REQUIRE(result.success == true);
}

TEST_CASE("ChatSession emits loop.done via EventBuilder", "[chat_session][event]") {
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    agenticdsl::DSLEngine engine{std::vector<agenticdsl::ParsedGraph>{}};
    engine.set_interaction_bus(bus);

    AgentConfig agent_cfg;
    SessionConfig session_cfg;
    session_cfg.persist_dir = "";

    ChatSession session(&engine, bus, &engine.get_tool_registry(), agent_cfg, session_cfg);

    bool found_loop_done = false;
    bus->subscribe("loop.done", [&](const agenticdsl::BusEvent& ev) {
        found_loop_done = true;
        REQUIRE(ev.payload.ok == true);
        REQUIRE(ev.payload.data.contains("response"));
        REQUIRE(ev.payload.data.contains("total_steps"));
        REQUIRE(ev.payload.data.contains("total_tokens"));
        REQUIRE(ev.payload.meta["session_id"] == session.session_id());
    });

    auto result = session.chat("hi");
    bus->wait_for_drain();

    REQUIRE(found_loop_done == true);
    REQUIRE(result.success == true);
}

TEST_CASE("ChatSession emits session.persist_request via EventBuilder", "[chat_session][event]") {
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    agenticdsl::DSLEngine engine{std::vector<agenticdsl::ParsedGraph>{}};
    engine.set_interaction_bus(bus);

    TempDir tmp;
    AgentConfig agent_cfg;
    SessionConfig session_cfg;
    session_cfg.persist_dir = tmp.str();

    ChatSession session(&engine, bus, &engine.get_tool_registry(), agent_cfg, session_cfg);

    bool found_request = false;
    bus->subscribe("session.persist_request", [&](const agenticdsl::BusEvent& ev) {
        found_request = true;
        REQUIRE(ev.payload.ok == true);
        REQUIRE(ev.payload.data.contains("messages"));
        REQUIRE(ev.payload.meta["session_id"] == session.session_id());
    });

    auto result = session.chat("persist me");
    bus->wait_for_drain();

    REQUIRE(found_request == true);
    REQUIRE(result.success == true);
}

TEST_CASE("ChatSession emits budget.checked via EventBuilder when exceeded", "[chat_session][event]") {
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    agenticdsl::DSLEngine engine{std::vector<agenticdsl::ParsedGraph>{}};
    engine.set_interaction_bus(bus);

    // 设置预算并强制 exceeded (max=1, used=2)
    agenticdsl::ExecutionBudget budget;
    budget.max_llm_calls = 1;
    {
        auto& bc = engine.get_budget_controller();
        bc.set_budget(std::move(budget));
        bc.try_consume_llm_call();
        const_cast<std::optional<agenticdsl::ExecutionBudget>&>(bc.get_budget())->llm_calls_used.store(2);
    }
    REQUIRE(engine.get_budget_controller().exceeded());

    TempDir tmp;
    AgentConfig agent_cfg;
    SessionConfig session_cfg;
    session_cfg.persist_dir = tmp.str();

    ChatSession session(&engine, bus, &engine.get_tool_registry(), agent_cfg, session_cfg);

    bool found_budget_checked = false;
    bus->subscribe("budget.checked", [&](const agenticdsl::BusEvent& ev) {
        found_budget_checked = true;
        // ADR-0068 §4: EventBuilder's outer payload.ok = true (event emission OK)
        // 业务语义 (budget exceeded) 通过 data["ok"] = false 表达
        REQUIRE(ev.payload.data.contains("limit"));
        REQUIRE(ev.payload.data.contains("used"));
        REQUIRE(ev.payload.data["unit"] == "llm_calls");
        REQUIRE(ev.payload.data["reason"] == "cost_limit");
        REQUIRE(ev.payload.data["ok"] == false);
        REQUIRE(ev.payload.meta["session_id"] == session.session_id());
    });

    auto result = session.chat("budget test");
    bus->wait_for_drain();

    REQUIRE(found_budget_checked == true);
    // budget 超出后 result.success 被设为 false
    REQUIRE(result.success == false);
}
