// examples/pdk_chat_demo_evolution/tests/test_anti_cheat_sandbox_escape.cpp
// L2 R9.3 anti-cheat: keyword-rejection for sandbox escape (per P0-6)
//
// Per R9.3 + P0-6: keyword-rejection for outbound network access patterns.
// Per Wave 4 deferral note: real sandbox network_mode=none is deferred;
// keyword-rejection is the Batch 3 mitigation.

#include "catch_amalgamated.hpp"
#include "context_request.h"

#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/inmemory_bus.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

fs::path fixture_dir() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        fs::path anchor = p / "AGENTS.md";
        if (fs::exists(anchor)) {
            fs::path candidate = p / "examples" / "pdk_chat_demo_evolution"
                                  / "tests" / "fixtures" / "context_request";
            if (fs::exists(candidate)) return candidate;
        }
        if (p.has_parent_path()) p = p.parent_path();
        else break;
    }
    throw std::runtime_error("L2 fixtures not found");
}

std::string fix(const std::string& name) {
    return (fixture_dir() / name).string();
}

struct Captured {
    std::string topic;
    std::string context_id;
    nlohmann::json payload;
};

std::mutex g_cap_mutex;
std::vector<Captured> g_captured;

void on_event(const agenticdsl::BusEvent& e) {
    std::lock_guard<std::mutex> lock(g_cap_mutex);
    Captured c;
    c.topic = e.topic;
    c.payload = e.payload.data;
    if (c.payload.contains("context_id") && c.payload["context_id"].is_string()) {
        c.context_id = c.payload["context_id"];
    }
    g_captured.push_back(std::move(c));
}

}  // namespace

// --- R9.3: turn_input_network_keyword_rejected event emitted on keyword match ---
TEST_CASE("R9.3 anti-cheat sandbox_escape: network keyword emits turn_input_network_keyword_rejected",
          "[l2-evolution]") {
    g_captured.clear();
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    auto token = bus->subscribe("turn_input_network_keyword_rejected", on_event);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto out = pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_network_keyword.jsonl"), errors, bus.get());
    bus->wait_for_drain();
    bus->unsubscribe(token);

    CHECK(out.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::NetworkKeywordContained);

    REQUIRE(g_captured.size() == 1);
    CHECK(g_captured[0].topic == "turn_input_network_keyword_rejected");
    CHECK(g_captured[0].context_id == "anti-cheat-r9-3-network-keyword-001");
    CHECK(g_captured[0].payload.contains("turn_input_preview"));
    CHECK(g_captured[0].payload.contains("keyword"));
    CHECK(g_captured[0].payload["keyword"] == "fetch_http");
}

// --- R9.3: detection order — hint regex checked before network keyword ---
TEST_CASE("R9.3 anti-cheat sandbox_escape: detection order respects P0'-1 invariant",
          "[l2-evolution]") {
    // If both hint + network keyword present, hint wins (per P0'-1 prefix/keyword
    // runs at step 4-5 BEFORE task_class check at step 6-8)
    // The fixture only has hint regex match here (no network keyword),
    // so we verify hint fires instead of network keyword.
    g_captured.clear();
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    auto token_h = bus->subscribe("hint_containment_rejected", on_event);
    auto token_n = bus->subscribe("turn_input_network_keyword_rejected", on_event);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_hint_input.jsonl"), errors, bus.get());
    bus->wait_for_drain();
    bus->unsubscribe(token_h);
    bus->unsubscribe(token_n);

    // Hint event fires, network keyword event does NOT (per P0'-1 invariant)
    REQUIRE(g_captured.size() == 1);
    CHECK(g_captured[0].topic == "hint_containment_rejected");
}

// --- R9.3: valid 3-class fixture emits no network-keyword event ---
TEST_CASE("R9.3 anti-cheat sandbox_escape: valid fixture emits no rejection",
          "[l2-evolution]") {
    g_captured.clear();
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    auto token = bus->subscribe("turn_input_network_keyword_rejected", on_event);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto out = pdk_chat_demo_evolution::load_context_file(
        fix("valid_3class_combined.jsonl"), errors, bus.get());
    bus->wait_for_drain();
    bus->unsubscribe(token);

    CHECK(out.size() == 3);
    CHECK(errors.empty());
    CHECK(g_captured.empty());
}