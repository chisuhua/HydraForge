// examples/pdk_chat_demo_evolution/tests/test_context_request_validation.cpp
// L2 ContextRequest parser test (per R13.1 schema + P0'-1/P0'-2/P0-6 detection)
//
// TDD Step 1 (RED): Create test BEFORE implementation. Expect FAIL with "context_request.h not found".
// Per R13 spec: 11 fields (6 top + 4 metadata + trace_id), 4 validation gates, 3 prefix-rejections.
// Per spec §S28-S31: prefix-checks-before-enum (P0'-1 invariant).

#include "catch_amalgamated.hpp"
#include "context_request.h"

#include <filesystem>
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
    throw std::runtime_error("L2 fixtures not found (repo root with AGENTS.md)");
}

std::string fix(const std::string& name) {
    return (fixture_dir() / name).string();
}

}  // namespace

// --- Case 1: missing context_id → MissingField (S29) ---
TEST_CASE("context_request: missing context_id → MissingField", "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("invalid_missing_context_id.jsonl"), errors);
    REQUIRE(reqs.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::MissingField);
    CHECK(errors[0].line_number == 1);
}

// --- Case 2: empty turn_input → EmptyTurnInput (S30) ---
TEST_CASE("context_request: empty turn_input → EmptyTurnInput", "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("invalid_empty_turn_input.jsonl"), errors);
    REQUIRE(reqs.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::EmptyTurnInput);
}

// --- Case 3: invalid task_class enum → InvalidTaskClass (S31) ---
TEST_CASE("context_request: invalid task_class enum → InvalidTaskClass", "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("invalid_task_class_enum.jsonl"), errors);
    REQUIRE(reqs.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::InvalidTaskClass);
}

// --- Case 4: invalid invocation_mode → InvalidInvocationMode ---
TEST_CASE("context_request: invalid invocation_mode → InvalidInvocationMode", "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("invalid_invocation_mode.jsonl"), errors);
    REQUIRE(reqs.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::InvalidInvocationMode);
}

// --- Case 5: valid 3-class combined → 3 accepted ---
TEST_CASE("context_request: valid 3-class combined → all 3 accepted", "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("valid_3class_combined.jsonl"), errors);
    REQUIRE(reqs.size() == 3);
    REQUIRE(errors.empty());
    CHECK(reqs[0].context_id == "valid-3class-001-code");
    CHECK(reqs[0].task_class == "code_gen");
    CHECK(reqs[1].task_class == "research");
    CHECK(reqs[2].task_class == "debug");
    CHECK(reqs[0].invocation_mode == "mock");
    CHECK(reqs[0].metadata.is_hidden == false);
    CHECK(reqs[0].metadata.sensitivity == "public");
}

// --- Case 6 (P0'-1): mutation_metric_* prefix → PrefixRejected (BEFORE enum check) ---
TEST_CASE("context_request: mutation_metric_* prefix → PrefixRejected (P0'-1 invariant)",
          "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_metric_tampering.jsonl"), errors);
    REQUIRE(reqs.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::PrefixRejected);
    CHECK(errors[0].context_id == "anti-cheat-r9-2-metric-tampering-001");
}

// --- Case 7 (P0'-2): hint pattern in turn_input → HintContained ---
TEST_CASE("context_request: hint pattern 'the answer is X' → HintContained", "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_hint_input.jsonl"), errors);
    REQUIRE(reqs.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::HintContained);
    CHECK(errors[0].context_id == "anti-cheat-r9-1-hint-001");
}

// --- Case 8 (P0-6): network keyword fetch http:// → NetworkKeywordContained ---
TEST_CASE("context_request: 'fetch http://' keyword → NetworkKeywordContained", "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("anti_cheat_network_keyword.jsonl"), errors);
    REQUIRE(reqs.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::NetworkKeywordContained);
}

// --- Case 9: single valid code → 1 accepted ---
TEST_CASE("context_request: single valid code → accepted", "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("valid_single_code.jsonl"), errors);
    REQUIRE(reqs.size() == 1);
    REQUIRE(errors.empty());
    CHECK(reqs[0].task_class == "code_gen");
}

// --- Case 10: is_hidden=true → accepted + bucket=hidden (R13.4 MUST, NOT rejected) ---
TEST_CASE("context_request: is_hidden=true → accepted (R13.4 MUST)", "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        fix("is_hidden_true.jsonl"), errors);
    REQUIRE(reqs.size() == 1);
    REQUIRE(errors.empty());
    CHECK(reqs[0].metadata.is_hidden == true);

    auto meta = pdk_chat_demo_evolution::to_trace_meta(reqs[0]);
    REQUIRE(meta.contains("is_hidden"));
    REQUIRE(meta.contains("hidden_bucket"));
    CHECK(meta["is_hidden"] == true);
    CHECK(meta["hidden_bucket"] == true);
}

// --- Case 11: missing file → MissingContextFile ---
TEST_CASE("context_request: missing file → MissingContextFile", "[l2-evolution]") {
    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto reqs = pdk_chat_demo_evolution::load_context_file(
        "/tmp/nonexistent_l2_fixture_xxx.jsonl", errors);
    REQUIRE(reqs.empty());
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].kind == pdk_chat_demo_evolution::LoadResult::MissingContextFile);
}

// --- Case 12: to_trace_meta 8-field contract (R3 authoritative) ---
TEST_CASE("context_request: to_trace_meta contains 8+ meta fields", "[l2-evolution]") {
    pdk_chat_demo_evolution::ContextRequest req;
    req.context_id = "test-001";
    req.turn_input = "hello world";
    req.task_class = "code_gen";
    req.invocation_mode = "mock";
    req.metadata.domain = "test";
    req.metadata.tags = {"tag1"};
    req.metadata.is_hidden = false;
    req.metadata.sensitivity = "public";

    auto meta = pdk_chat_demo_evolution::to_trace_meta(req);

    CHECK(meta.contains("context_id"));
    CHECK(meta.contains("task_class"));
    CHECK(meta.contains("is_hidden"));
    CHECK(meta.contains("hidden_bucket"));
    CHECK(meta.contains("sensitivity"));
    CHECK(meta.contains("expected_eval_quality"));
    CHECK(meta.contains("trace_id"));
    CHECK(meta.contains("capture_mode"));
    CHECK(meta.size() >= 8);
}