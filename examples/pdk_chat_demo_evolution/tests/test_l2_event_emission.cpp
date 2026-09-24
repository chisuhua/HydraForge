// examples/pdk_chat_demo_evolution/tests/test_l2_event_emission.cpp
// L2 T6.4a closure verification (per Oracle Stage 2 M2, post-Batch-2 SHIP-with-fixes):
//
// Zero-runtime-verification fix: 4 emit sites in context_request.cpp were dead
// code (all callers used bus=nullptr fallback).  This test subscribes to an
// InMemoryBus, invokes load_context_file(path, errors, bus.get()), and asserts
// all 4 ADR-0068 v2.4 events fire with correct topic + payload schema.

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

std::shared_ptr<agenticdsl::InMemoryBus> make_bus() {
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    g_captured.clear();
    bus->subscribe("hint_containment_rejected", on_event);
    bus->subscribe("turn_input_network_keyword_rejected", on_event);
    bus->subscribe("mutation_metric_rejected", on_event);
    bus->subscribe("hidden_context_accepted_info", on_event);
    return bus;
}

const Captured* find_capture(const std::string& context_id) {
    for (const auto& c : g_captured) {
        if (c.context_id == context_id) return &c;
    }
    return nullptr;
}

}  // namespace

// --- Case R9.1: hint detection → hint_containment_rejected ---
TEST_CASE("l2_event_emission: R9.1 hint detection emits hint_containment_rejected",
          "[l2-evolution]") {
    auto bus = make_bus();

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_hint_input.jsonl"), errors, bus.get());
    bus->wait_for_drain();

    auto* cap = find_capture("anti-cheat-r9-1-hint-001");
    REQUIRE(cap != nullptr);
    CHECK(cap->topic == "hint_containment_rejected");
    CHECK(cap->payload.contains("turn_input_preview"));
    CHECK(cap->payload["turn_input_preview"].is_string());
    CHECK(cap->payload["matched_pattern"] == "the_answer_is_word");
}

// --- Case R9.2: prefix-rejection → mutation_metric_rejected ---
TEST_CASE("l2_event_emission: R9.2 prefix-rejection emits mutation_metric_rejected",
          "[l2-evolution]") {
    auto bus = make_bus();

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_metric_tampering.jsonl"), errors, bus.get());
    bus->wait_for_drain();

    auto* cap = find_capture("anti-cheat-r9-2-metric-tampering-001");
    REQUIRE(cap != nullptr);
    CHECK(cap->topic == "mutation_metric_rejected");
    CHECK(cap->payload.contains("task_class_preview"));
    CHECK(cap->payload["task_class_preview"] == "mutation_metric_evaluation");
    CHECK(cap->payload.contains("reason"));
    CHECK(cap->payload["reason"].get<std::string>().find("mutation_metric_") !=
          std::string::npos);
}

// --- Case R9.3: network keyword → turn_input_network_keyword_rejected ---
TEST_CASE("l2_event_emission: R9.3 network keyword emits turn_input_network_keyword_rejected",
          "[l2-evolution]") {
    auto bus = make_bus();

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_network_keyword.jsonl"), errors, bus.get());
    bus->wait_for_drain();

    auto* cap = find_capture("anti-cheat-r9-3-network-keyword-001");
    REQUIRE(cap != nullptr);
    CHECK(cap->topic == "turn_input_network_keyword_rejected");
    CHECK(cap->payload.contains("turn_input_preview"));
    CHECK(cap->payload.contains("keyword"));
    CHECK(cap->payload["keyword"] == "fetch_http");
}

// --- Case R13.4: is_hidden=true accepted → hidden_context_accepted_info ---
TEST_CASE("l2_event_emission: R13.4 is_hidden=true emits hidden_context_accepted_info",
          "[l2-evolution]") {
    auto bus = make_bus();

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("is_hidden_true.jsonl"), errors, bus.get());
    REQUIRE(reqs.size() == 1);
    bus->wait_for_drain();

    // The is_hidden fixture context_id (read from fixture file)
    const Captured* cap = nullptr;
    for (const auto& c : g_captured) {
        if (c.topic == "hidden_context_accepted_info") { cap = &c; break; }
    }
    REQUIRE(cap != nullptr);
    CHECK(cap->payload["is_hidden"] == true);
    CHECK(cap->payload["bucket"] == "hidden");
    CHECK(cap->payload.contains("task_class"));
}

// --- Case count: valid 3-class fixture emits 0 rejection events ---
TEST_CASE("l2_event_emission: valid 3-class emits 0 rejection events",
          "[l2-evolution]") {
    auto bus = make_bus();

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    pdk_chat_demo_evolution::load_context_file(
        fix("valid_3class_combined.jsonl"), errors, bus.get());
    bus->wait_for_drain();

    // 3 contexts, all valid → no rejection events from R9.1/R9.2/R9.3
    // (R13.4 only emits when is_hidden=true, which is false for these)
    std::lock_guard<std::mutex> lock(g_cap_mutex);
    CHECK(g_captured.empty());
}