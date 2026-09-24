// examples/pdk_chat_demo_evolution/tests/test_anti_cheat_search_solution.cpp
// L2 R9.1 anti-cheat: parser-side hint detection (per P0'-2)
//
// Asserts `hint_containment_rejected` event emitted when loading
// anti_cheat_hint_input.jsonl (parser-side regex `the answer is \w+`,
// deterministic, LLM not invoked).

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

// --- R9.1: hint_containment_rejected event emitted on hint regex match ---
TEST_CASE("R9.1 anti-cheat search_solution: hint regex emits hint_containment_rejected",
          "[l2-evolution]") {
    g_captured.clear();
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    auto token = bus->subscribe("hint_containment_rejected", on_event);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto out = pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_hint_input.jsonl"), errors, bus.get());
    bus->wait_for_drain();
    bus->unsubscribe(token);

    // R9.1 invariant: hint entry rejected (not in output)
    CHECK(out.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::HintContained);

    // R9.1 invariant: event emitted with correct schema
    REQUIRE(g_captured.size() == 1);
    CHECK(g_captured[0].topic == "hint_containment_rejected");
    CHECK(g_captured[0].context_id == "anti-cheat-r9-1-hint-001");
    CHECK(g_captured[0].payload.contains("turn_input_preview"));
    CHECK(g_captured[0].payload.contains("matched_pattern"));
    CHECK(g_captured[0].payload["matched_pattern"] == "the_answer_is_word");
}

TEST_CASE("R9.1 anti-cheat search_solution: hint detection is parser-side (no LLM)",
          "[l2-evolution]") {
    // Per P0'-2: parser-side regex, deterministic, LLM NOT invoked.
    // Verify by ensuring no chat_session / LLM provider calls happen
    // (test runs without MockLLMProvider setup → would fail if LLM invoked).
    g_captured.clear();
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    auto token = bus->subscribe("hint_containment_rejected", on_event);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto out = pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_hint_input.jsonl"), errors, bus.get());
    bus->wait_for_drain();

    // Determinism: re-run, same outcome (token still subscribed — captures both)
    std::vector<pdk_chat_demo_evolution::LoadError> errors2;
    auto out2 = pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_hint_input.jsonl"), errors2, bus.get());
    bus->wait_for_drain();
    bus->unsubscribe(token);

    CHECK(out.size() == out2.size());
    CHECK(errors.size() == errors2.size());
    CHECK(errors[0].kind == errors2[0].kind);
    CHECK(g_captured.size() == 2);
}

TEST_CASE("R9.1 anti-cheat search_solution: valid 3-class fixture emits no hint event",
          "[l2-evolution]") {
    // Valid contexts should NOT trigger hint detection
    g_captured.clear();
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    auto token = bus->subscribe("hint_containment_rejected", on_event);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto out = pdk_chat_demo_evolution::load_context_file(
        fix("valid_3class_combined.jsonl"), errors, bus.get());
    bus->wait_for_drain();
    bus->unsubscribe(token);

    CHECK(out.size() == 3);
    CHECK(errors.empty());
    CHECK(g_captured.empty());
}