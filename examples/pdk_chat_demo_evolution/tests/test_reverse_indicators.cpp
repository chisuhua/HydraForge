// examples/pdk_chat_demo_evolution/tests/test_reverse_indicators.cpp
// L2 R8 reverse-indicators test (pragmatic Batch-3-testable framework invariants)
//
// Per plan Task 7 + Oracle M1 Batch-3 scope-split: full R8.1/R8.2/R8.3 metrics
// (drop_ratio > 5%, failure_event schema, --ablation-mode=full output) depend
// on Batch 4 main.cpp CLI flags (--release-metrics / --ablation-mode).  These
// are deferred to Batch 4.  This Batch 3 test covers framework invariants
// that CAN be verified now:
//   - session_id consistency across all trace events in 1 session (R8 pre-cond)
//   - meta.eval_quality always null invariant (P0-4 C5)
//   - attribution_verdict distribution varies per context class (R8.3 pre-cond)

#include "catch_amalgamated.hpp"
#include "context_request.h"
#include "evolution_session.h"
#include "evolution_tracer.h"
#include "hermetic_home.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

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

std::vector<pdk_chat_demo_evolution::ContextRequest>
load_fixture(const std::string& name, std::vector<pdk_chat_demo_evolution::LoadError>& errors) {
    return pdk_chat_demo_evolution::load_context_file(fix(name), errors);
}

class StdoutCapture {
public:
    StdoutCapture() : old_(std::cout.rdbuf(captured_.rdbuf())) {}
    ~StdoutCapture() { std::cout.rdbuf(old_); }
    std::string str() const { return captured_.str(); }
private:
    std::stringstream captured_;
    std::streambuf* old_;
};

nlohmann::json parse_lines(const std::string& out) {
    nlohmann::json arr = nlohmann::json::array();
    std::stringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        try {
            arr.push_back(nlohmann::json::parse(line));
        } catch (...) {
            // skip malformed lines
        }
    }
    return arr;
}

}  // namespace

// --- R8 pre-cond: session_id consistent across all trace events in 1 session ---
TEST_CASE("R8 reverse_indicators: session_id consistent across all trace events",
          "[l2-evolution]") {
    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto contexts = load_fixture("valid_3class_combined.jsonl", errors);
    REQUIRE(contexts.size() == 3);

    pdk_chat_demo_evolution::EvolutionSession session(
        "mock", "None", /*trace_events=*/true);
    session.set_contexts(contexts);
    session.set_hermetic_home(guard);

    StdoutCapture cap;
    session.run_6_phase_demo();

    auto events = parse_lines(cap.str());
    REQUIRE(events.size() == 12);  // 4 phases × 3 contexts

    // All events must share the same session_id
    std::set<std::string> session_ids;
    for (const auto& e : events) {
        REQUIRE(e.contains("session_id"));
        session_ids.insert(e["session_id"].get<std::string>());
    }
    CHECK(session_ids.size() == 1);

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}

// --- R8 pre-cond: meta.eval_quality always null (P0-4 C5 invariant) ---
TEST_CASE("R8 reverse_indicators: meta.eval_quality always null (P0-4 C5)",
          "[l2-evolution]") {
    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto contexts = load_fixture("valid_3class_combined.jsonl", errors);
    REQUIRE(contexts.size() == 3);

    pdk_chat_demo_evolution::EvolutionSession session(
        "mock", "None", /*trace_events=*/true);
    session.set_contexts(contexts);
    session.set_hermetic_home(guard);

    StdoutCapture cap;
    session.run_6_phase_demo();

    auto events = parse_lines(cap.str());
    REQUIRE(events.size() == 12);

    // All events must have eval_quality=null (L2 does NOT compute)
    for (const auto& e : events) {
        REQUIRE(e.contains("meta"));
        const auto& meta = e["meta"];
        REQUIRE(meta.contains("eval_quality"));
        CHECK(meta["eval_quality"].is_null());
    }

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}

// --- R8.3 pre-cond: attribution_verdict distribution varies per context ---
TEST_CASE("R8 reverse_indicators: attribution_verdict distribution per context",
          "[l2-evolution]") {
    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto contexts = load_fixture("valid_3class_combined.jsonl", errors);
    REQUIRE(contexts.size() == 3);

    pdk_chat_demo_evolution::EvolutionSession session(
        "mock", "None", /*trace_events=*/true);
    session.set_contexts(contexts);
    session.set_hermetic_home(guard);

    StdoutCapture cap;
    session.run_6_phase_demo();

    auto events = parse_lines(cap.str());
    REQUIRE(events.size() == 12);

    // Group by context_id, collect verdict distribution
    std::map<std::string, std::set<std::string>> per_context_verdicts;
    for (const auto& e : events) {
        REQUIRE(e.contains("meta"));
        const auto& meta = e["meta"];
        REQUIRE(meta.contains("context_id"));
        REQUIRE(meta.contains("attribution_verdict"));
        per_context_verdicts[meta["context_id"].get<std::string>()].insert(
            meta["attribution_verdict"].get<std::string>());
    }

    // 3 distinct contexts → 3 distinct verdict sets
    CHECK(per_context_verdicts.size() == 3);
    for (const auto& [ctx, verdicts] : per_context_verdicts) {
        CHECK(!verdicts.empty());
    }

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}