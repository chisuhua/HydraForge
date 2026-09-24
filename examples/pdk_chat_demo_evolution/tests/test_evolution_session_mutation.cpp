// examples/pdk_chat_demo_evolution/tests/test_evolution_session_mutation.cpp
// L2 EvolutionSession::Case 1 mock mutation (per harness-rsi-pilot scenario)
//
// TDD Step 1 (RED): Create test BEFORE implementation. Expect FAIL with
// "evolution_session.h not found". Per plan Task 5 + tasks.md T2.1.
//
// Per R13 spec: EvolutionSession orchestrates Phase 0-6 with bus injection
// (T6.8a closure gate). Mock mutation path: 4 trace events emitted
// (baseline / mutation / reload / compare) + 0 return code on success.

#include "catch_amalgamated.hpp"
#include "context_request.h"
#include "evolution_session.h"
#include "evolution_tracer.h"
#include "hermetic_home.h"

#include <filesystem>
#include <iostream>
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

std::vector<pdk_chat_demo_evolution::ContextRequest>
load_fixture(const std::string& name, std::vector<pdk_chat_demo_evolution::LoadError>& errors) {
    return pdk_chat_demo_evolution::load_context_file(
        (fixture_dir() / name).string(), errors);
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

}  // namespace

// --- Case 1.1: mock mutation → run_6_phase_demo returns 0 ---
TEST_CASE("evolution_session: mock mutation baseline returns 0", "[l2-evolution]") {
    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto contexts = load_fixture("valid_single_code.jsonl", errors);
    REQUIRE(contexts.size() == 1);

    pdk_chat_demo_evolution::EvolutionSession session(
        "mock", "None", /*trace_events=*/false);
    session.set_contexts(contexts);
    session.set_hermetic_home(guard);

    int rc = session.run_6_phase_demo();
    CHECK(rc == 0);

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}

// --- Case 1.2: empty contexts → run returns non-zero ---
TEST_CASE("evolution_session: empty contexts returns non-zero", "[l2-evolution]") {
    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);

    pdk_chat_demo_evolution::EvolutionSession session(
        "mock", "None", /*trace_events=*/false);
    session.set_contexts({});
    session.set_hermetic_home(guard);

    int rc = session.run_6_phase_demo();
    CHECK(rc != 0);

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}

// --- Case 1.3: trace_events=true emits 4 JSONL lines (one per phase) ---
TEST_CASE("evolution_session: trace_events emits 4 JSONL lines for 1 context",
          "[l2-evolution]") {
    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto contexts = load_fixture("valid_single_code.jsonl", errors);
    REQUIRE(contexts.size() == 1);

    pdk_chat_demo_evolution::EvolutionSession session(
        "mock", "None", /*trace_events=*/true);
    session.set_contexts(contexts);
    session.set_hermetic_home(guard);

    StdoutCapture cap;
    int rc = session.run_6_phase_demo();

    std::string out = cap.str();
    // 4 phases × 1 context = 4 JSONL lines (baseline + mutation + reload + compare)
    size_t line_count = std::count(out.begin(), out.end(), '\n');
    CHECK(line_count >= 4);

    // Verify phase names appear
    CHECK(out.find("\"phase\":\"baseline\"") != std::string::npos);
    CHECK(out.find("\"phase\":\"mutation\"") != std::string::npos);
    CHECK(out.find("\"phase\":\"reload\"") != std::string::npos);
    CHECK(out.find("\"phase\":\"compare\"") != std::string::npos);

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}

// --- Case 1.4: 3 contexts → 12 JSONL lines (4 phases × 3) ---
TEST_CASE("evolution_session: 3 contexts emit 12 JSONL lines", "[l2-evolution]") {
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
    int rc = session.run_6_phase_demo();

    std::string out = cap.str();
    // 4 phases × 3 contexts = 12 JSONL lines
    size_t line_count = std::count(out.begin(), out.end(), '\n');
    CHECK(line_count >= 12);

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}