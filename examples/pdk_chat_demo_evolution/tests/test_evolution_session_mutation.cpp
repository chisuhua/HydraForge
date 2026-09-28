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
#include <fstream>
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

std::string read_file(const fs::path& p) {
    std::ifstream ifs(p);
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return oss.str();
}

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

// --- Helper: parse JSONL output into array ---
nlohmann::json parse_lines(const std::string& in) {
    nlohmann::json arr = nlohmann::json::array();
    std::stringstream ss(in);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        try {
            arr.push_back(nlohmann::json::parse(line));
        } catch (...) {}
    }
    return arr;
}

// --- Helper (T5 / Oracle Mi4 refactor): setup distillation writer output dir ---
void setup_distillation_writer(pdk_chat_demo_evolution::EvolutionSession& session,
                               const fs::path& output_dir) {
    fs::remove_all(output_dir);
    session.set_distillation_output_dir(output_dir);
}

// --- Helper (T5): list *.distill.v1.jsonl files under a dir (glob via directory_iterator) ---
std::vector<fs::path> list_distillation_files(const fs::path& dir) {
    std::vector<fs::path> files;
    if (!fs::exists(dir)) return files;
    for (auto& e : fs::directory_iterator(dir)) {
        if (e.path().string().ends_with(".distill.v1.jsonl")) files.push_back(e.path());
    }
    return files;
}

// --- Case 2 (T2): mutation genome_version > 1 after real wiring ---
TEST_CASE("evolution_session: mutation genome_version > 1 (real mutation)",
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
    CHECK(rc == 0);

    auto events = parse_lines(cap.str());
    REQUIRE(events.size() >= 4);

    // Find mutation phase and verify genome_version > 1
    auto mutation = std::find_if(events.begin(), events.end(),
        [](const auto& e) { return e.value("phase", "") == "mutation"; });
    REQUIRE(mutation != events.end());
    REQUIRE(mutation->contains("meta"));
    const auto& meta = (*mutation)["meta"];
    REQUIRE(meta.contains("genome_version"));
    // After seed commit (version 1) + mutation fork (version 2) → genome_version > 1
    CHECK(meta["genome_version"].get<int>() > 1);

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}

// --- Case 3 (T3): reload genome_version chain-links to phase3 mutation version ---
TEST_CASE("evolution_session: reload genome_version == mutation genome_version (chain-link)",
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
    CHECK(rc == 0);

    auto events = parse_lines(cap.str());
    REQUIRE(events.size() >= 4);

    auto mutation = std::find_if(events.begin(), events.end(),
        [](const auto& e) { return e.value("phase", "") == "mutation"; });
    REQUIRE(mutation != events.end());
    REQUIRE(mutation->contains("meta"));
    const auto& mut_meta = (*mutation)["meta"];
    REQUIRE(mut_meta.contains("genome_version"));
    int mut_version = mut_meta["genome_version"].get<int>();

    auto reload = std::find_if(events.begin(), events.end(),
        [](const auto& e) { return e.value("phase", "") == "reload"; });
    REQUIRE(reload != events.end());
    REQUIRE(reload->contains("meta"));
    const auto& rel_meta = (*reload)["meta"];
    REQUIRE(rel_meta.contains("genome_version"));

    // Chain-link: reload reloads mutation.new_version (per design T3.1 Oracle M6)
    CHECK(rel_meta["genome_version"].get<int>() == mut_version);

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}

// --- Case 4 (T3): reload verdict is a legal closed-enum value (R4) ---
TEST_CASE("evolution_session: reload verdict is R4 closed-enum legal",
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
    CHECK(rc == 0);

    auto events = parse_lines(cap.str());
    REQUIRE(events.size() >= 4);

    auto reload = std::find_if(events.begin(), events.end(),
        [](const auto& e) { return e.value("phase", "") == "reload"; });
    REQUIRE(reload != events.end());
    REQUIRE(reload->contains("meta"));
    const auto& meta = (*reload)["meta"];
    REQUIRE(meta.contains("attribution_verdict"));
    const auto& verdict = meta["attribution_verdict"].get<std::string>();
    CHECK((verdict == "NotAttempted" || verdict == "Attributed" ||
           verdict == "Insufficient" || verdict == "Confounded"));

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}

// --- T5.1 (RED): capture-mode=Training emits IDistillationWriter JSONL ---
TEST_CASE("capture-mode=Training: emits IDistillationWriter JSONL", "[l2-evolution]") {
    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto contexts = load_fixture("valid_3class_combined.jsonl", errors);
    REQUIRE(contexts.size() == 3);

    pdk_chat_demo_evolution::EvolutionSession session(
        "mock", "Training", /*trace_events=*/true);
    session.set_contexts(contexts);
    session.set_hermetic_home(guard);

    auto writer_root = fs::temp_directory_path() / "l2-distillation-test";
    setup_distillation_writer(session, writer_root);

    int rc = session.run_6_phase_demo();
    CHECK(rc == 0);

    auto files = list_distillation_files(writer_root);
    CHECK(files.size() >= 3);  // 1 per context

    if (!files.empty()) {
        auto rec = nlohmann::json::parse(read_file(files[0]));
        CHECK(rec.contains("agent_id"));
        CHECK(rec.contains("input"));
        CHECK(rec.contains("output"));
        CHECK(rec.contains("capture_mode"));
        CHECK(rec.contains("convergence"));
        CHECK(rec["agent_id"].get<std::string>().starts_with("l2-distillation-"));
        CHECK(rec["capture_mode"].get<std::string>() == "Training");
    }

    fs::remove_all(writer_root);
    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}

// --- T5.2 (RED): capture-mode=None does NOT emit distillation file ---
TEST_CASE("capture-mode=None: does NOT emit distillation file", "[l2-evolution]") {
    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto contexts = load_fixture("valid_single_code.jsonl", errors);
    REQUIRE(contexts.size() == 1);

    pdk_chat_demo_evolution::EvolutionSession session(
        "mock", "None", /*trace_events=*/true);
    session.set_contexts(contexts);
    session.set_hermetic_home(guard);

    auto writer_root = fs::temp_directory_path() / "l2-distillation-test-none";
    setup_distillation_writer(session, writer_root);

    int rc = session.run_6_phase_demo();
    CHECK(rc == 0);

    auto files = list_distillation_files(writer_root);
    CHECK(files.empty());

    fs::remove_all(writer_root);
    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}