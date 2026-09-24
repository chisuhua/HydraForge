// examples/pdk_chat_demo_evolution/tests/test_evolution_tracer_schema.cpp
// L2 evolution_tracer test (4-phase trace JSONL, 8 top + meta 12 fields per P2-1)
//
// TDD Step 1 (RED): Create test BEFORE implementation. Expect FAIL with "evolution_tracer.h not found".
// Per R3 authoritative (12 meta fields per P2-1):
//   context_id + task_class + is_hidden + hidden_bucket + sensitivity +
//   expected_eval_quality + trace_id + capture_mode + genome_version +
//   gate_passes + eval_quality (always null per P0-4 C5) + attribution_verdict

#include "catch_amalgamated.hpp"
#include "evolution_tracer.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace {

// Helper: capture stdout during fn execution
class StdoutCapture {
public:
    StdoutCapture() : old_(std::cout.rdbuf(captured_.rdbuf())) {}
    ~StdoutCapture() { std::cout.rdbuf(old_); }
    std::string str() const { return captured_.str(); }
private:
    std::stringstream captured_;
    std::streambuf* old_;
};

}  // namespace

// --- Case 4.1: trace 4 events → exactly 4 JSONL lines ---
TEST_CASE("evolution_tracer: 4 events produce 4 JSONL lines", "[l2-evolution]") {
    pdk_chat_demo_evolution::EvolutionTracer tracer(true);

    StdoutCapture cap;
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Baseline, {});
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Mutation, {});
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Reload, {});
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Compare, {});

    std::string out = cap.str();
    size_t line_count = std::count(out.begin(), out.end(), '\n');
    REQUIRE(line_count == 4);
}

// --- Case 4.2: meta contains 12 fields per P2-1 ---
TEST_CASE("evolution_tracer: meta contains 12 fields per P2-1", "[l2-evolution]") {
    pdk_chat_demo_evolution::EvolutionTracer tracer(true);

    nlohmann::json event = {{"meta", {
        {"context_id", "test-ctx-001"},
        {"task_class", "code_gen"},
        {"is_hidden", false},
        {"hidden_bucket", false},
        {"sensitivity", "public"},
        {"expected_eval_quality", "Acceptable"},
        {"trace_id", "test-trace-001"},
        {"capture_mode", "None"},
        {"genome_version", 1},
        {"gate_passes", 5},
        {"eval_quality", nullptr},
        {"attribution_verdict", "Attributed"}
    }}};

    StdoutCapture cap;
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Mutation, event);

    auto out = cap.str();
    REQUIRE(!out.empty());

    auto j = nlohmann::json::parse(out);
    REQUIRE(j.contains("meta"));
    auto& meta = j["meta"];

    // Per R3 authoritative: 12 fields per P2-1
    CHECK(meta.contains("context_id"));
    CHECK(meta.contains("task_class"));
    CHECK(meta.contains("is_hidden"));
    CHECK(meta.contains("hidden_bucket"));
    CHECK(meta.contains("sensitivity"));
    CHECK(meta.contains("expected_eval_quality"));
    CHECK(meta.contains("trace_id"));
    CHECK(meta.contains("capture_mode"));
    CHECK(meta.contains("genome_version"));
    CHECK(meta.contains("gate_passes"));
    CHECK(meta.contains("eval_quality"));
    CHECK(meta.contains("attribution_verdict"));

    // eval_quality always null per P0-4 C5 (L2 does NOT compute)
    CHECK(meta["eval_quality"].is_null());

    // Verify dual is_hidden + hidden_bucket (R13.4 P2-3 invariant)
    CHECK(meta["is_hidden"] == meta["hidden_bucket"]);
}

// --- Case 4.3: top-level 8 fields per R3 (phase / timestamp / session_id / turn_input / response / tokens / cost_usd / meta) ---
TEST_CASE("evolution_tracer: top-level has 8 fields per R3", "[l2-evolution]") {
    pdk_chat_demo_evolution::EvolutionTracer tracer(true);

    nlohmann::json event = {
        {"turn_input", "Write hello world"},
        {"response", "Here is hello world..."},
        {"tokens", 42},
        {"cost_usd", 0.0001}
    };

    StdoutCapture cap;
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Baseline, event);

    auto j = nlohmann::json::parse(cap.str());
    CHECK(j.contains("phase"));
    CHECK(j.contains("timestamp_iso8601"));
    CHECK(j.contains("session_id"));
    CHECK(j.contains("turn_input"));
    CHECK(j.contains("response"));
    CHECK(j.contains("tokens"));
    CHECK(j.contains("cost_usd"));
    CHECK(j.contains("meta"));

    CHECK(j["phase"] == "baseline");
    CHECK(j["turn_input"] == "Write hello world");
    CHECK(j["tokens"] == 42);
}

// --- Case 4.4: --trace-events flag controls emit (default off) ---
TEST_CASE("evolution_tracer: disabled by default emits nothing", "[l2-evolution]") {
    pdk_chat_demo_evolution::EvolutionTracer tracer(false);

    StdoutCapture cap;
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Baseline, {});
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Mutation, {});

    CHECK(cap.str().empty());
}

TEST_CASE("evolution_tracer: enable() switches emit on after construction", "[l2-evolution]") {
    pdk_chat_demo_evolution::EvolutionTracer tracer(false);

    StdoutCapture cap;
    tracer.enable(true);
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Baseline, {});

    REQUIRE(!cap.str().empty());
    auto j = nlohmann::json::parse(cap.str());
    CHECK(j["phase"] == "baseline");
}

// --- Case 4.5: 4 phases emit named strings (baseline/mutation/reload/compare) ---
TEST_CASE("evolution_tracer: phase names match expected strings", "[l2-evolution]") {
    pdk_chat_demo_evolution::EvolutionTracer tracer(true);

    StdoutCapture cap;
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Baseline, {});
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Mutation, {});
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Reload, {});
    tracer.record_phase(pdk_chat_demo_evolution::TracePhase::Compare, {});

    std::string out = cap.str();
    REQUIRE(out.find("\"phase\":\"baseline\"") != std::string::npos);
    REQUIRE(out.find("\"phase\":\"mutation\"") != std::string::npos);
    REQUIRE(out.find("\"phase\":\"reload\"") != std::string::npos);
    REQUIRE(out.find("\"phase\":\"compare\"") != std::string::npos);
}