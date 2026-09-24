// examples/pdk_chat_demo_evolution/tests/test_anti_cheat_metric_tampering.cpp
// L2 R9.2 anti-cheat: prefix-rejection + IEvaluator static contract guard
//
// Per R9.2 + P0'-1 invariant: prefix-rejection runs BEFORE closed-enum validation.
// Asserts (a) mutation_metric_rejected event emitted when task_class starts with
// "mutation_metric_", (b) IEvaluator has no set_*_metric mutator methods
// (static contract guard via grep on contract header).

#include "catch_amalgamated.hpp"
#include "context_request.h"

#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/inmemory_bus.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
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

fs::path ievaluator_header() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        fs::path anchor = p / "AGENTS.md";
        if (fs::exists(anchor)) {
            fs::path candidate = p / "include" / "agenticdsl" / "contract" / "ievaluator.h";
            if (fs::exists(candidate)) return candidate;
        }
        if (p.has_parent_path()) p = p.parent_path();
        else break;
    }
    throw std::runtime_error("ievaluator.h not found");
}

int count_set_metric_methods(const fs::path& header) {
    std::ifstream f(header);
    if (!f) return -1;
    std::stringstream buf;
    buf << f.rdbuf();
    std::string content = buf.str();
    int count = 0;
    size_t pos = 0;
    while ((pos = content.find("set_", pos)) != std::string::npos) {
        // Look ahead for "_metric" or "Metric" within 60 chars
        size_t next = content.find('\n', pos);
        std::string line = content.substr(pos, next - pos);
        if (line.find("_metric") != std::string::npos ||
            line.find("_Metric") != std::string::npos) {
            count++;
        }
        pos = next;
    }
    return count;
}

}  // namespace

// --- R9.2: mutation_metric_rejected event emitted on prefix-rejection ---
TEST_CASE("R9.2 anti-cheat metric_tampering: prefix-rejection emits mutation_metric_rejected",
          "[l2-evolution]") {
    g_captured.clear();
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    auto token = bus->subscribe("mutation_metric_rejected", on_event);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto out = pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_metric_tampering.jsonl"), errors, bus.get());
    bus->wait_for_drain();
    bus->unsubscribe(token);

    // P0'-1 invariant: prefix-rejection runs BEFORE enum check
    CHECK(out.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::PrefixRejected);

    REQUIRE(g_captured.size() == 1);
    CHECK(g_captured[0].topic == "mutation_metric_rejected");
    CHECK(g_captured[0].context_id == "anti-cheat-r9-2-metric-tampering-001");
    CHECK(g_captured[0].payload.contains("task_class_preview"));
    CHECK(g_captured[0].payload["task_class_preview"] == "mutation_metric_evaluation");
    CHECK(g_captured[0].payload.contains("reason"));
    CHECK(g_captured[0].payload["reason"].get<std::string>().find("mutation_metric_") !=
          std::string::npos);
}

// --- R9.2: IEvaluator static contract guard (no set_*_metric mutator methods) ---
TEST_CASE("R9.2 anti-cheat metric_tampering: IEvaluator has no set_*_metric mutator",
          "[l2-evolution]") {
    auto header = ievaluator_header();
    int count = count_set_metric_methods(header);
    INFO("IEvaluator set_*_metric method count: " << count
         << " (header: " << header.string() << ")");
    // Per R9.2 static contract: 复旦 (Fudan) 模式攻击 — IEvaluator 不应有
    // write interface (set_*_metric).  MutationGate 拦截 schema 改写.
    CHECK(count == 0);
}

// --- R9.2: valid 3-class fixture emits no metric-rejection event ---
TEST_CASE("R9.2 anti-cheat metric_tampering: valid fixture emits no rejection",
          "[l2-evolution]") {
    g_captured.clear();
    auto bus = std::make_shared<agenticdsl::InMemoryBus>();
    auto token = bus->subscribe("mutation_metric_rejected", on_event);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto out = pdk_chat_demo_evolution::load_context_file(
        fix("valid_3class_combined.jsonl"), errors, bus.get());
    bus->wait_for_drain();
    bus->unsubscribe(token);

    CHECK(out.size() == 3);
    CHECK(errors.empty());
    CHECK(g_captured.empty());
}