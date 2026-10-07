// examples/pdk_chat_demo_evolution/tests/test_l2_real_llm_e2e.cpp
// L2 real LLM end-to-end tests (l2-evolution-real-llm-provider change).
//
// Per spec `real-llm-provider-construction` + `l2-real-llm-drop-ratio-r8-red-line`
// + `hermetic-home-compatibility-with-real-llm`:
//   Case 1: --real-llm deepseek 6-phase demo runs with real LLM responses
//           in trace JSONL (response != "Mock evolution response",
//           tokens > 0, cost_usd > 0).
//   Case 2: --real-llm deepseek --release-metrics writes real drop_ratio
//           to metrics.json (NOT the -1.0 mock sentinel).
//   Case 3: --real-llm <unknown> exits non-zero + stderr "ERROR: unknown provider".
//   Case 4: missing DEEPSEEK_API_KEY (skip unset) exits non-zero + stderr
//           "ERROR: real LLM requires DEEPSEEK_API_KEY".
//   Case 5: hermetic HOME + real LLM dual-track (setup_hermetic_home() +
//           --real-llm deepseek no collision; provider api_key read from env).
//
// Real LLM cases are tagged [must_realllm] + [l2-evolution]; the CTest binary
// is labeled "must_realllm;l2-evolution" so `ctest -L must_realllm` includes it
// (per a21c92e precedent).  Case 3/4 are deterministic config-error checks that
// do NOT hit the network; they run under the same labels without requiring a key.

#include "catch_amalgamated.hpp"
#include "context_request.h"
#include "evolution_session.h"
#include "hermetic_home.h"
#include "test_helpers/real_llm_env.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

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

// Run the demo binary with args, capture combined stderr+stdout, return exit code.
int run_demo(const std::string& args, std::string* out) {
#ifndef L2_DEMO_BINARY
#error "L2_DEMO_BINARY must be defined by CMake (path to pdk_chat_demo_evolution)"
#endif
    std::string cmd = std::string(L2_DEMO_BINARY) + " " + args + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return -1;
    char buf[4096];
    std::string captured;
    while (std::fgets(buf, sizeof(buf), pipe)) captured += buf;
    if (out) *out = captured;
    int status = pclose(pipe);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

}  // namespace

// --- Case 1: real-LLM deepseek 6-phase demo + trace JSONL (real response) ---
TEST_CASE("l2 real llm: deepseek runs 6-phase demo with real LLM trace",
          "[must_realllm][l2-evolution][real_llm][trace]") {
    agenticdsl::test::require_real_llm_env();
    if (agenticdsl::test::real_llm_env_skipped()) {
        SUCCEED("skipped: HYDRAFORGE_SKIP_REAL_LLM=1");
        return;
    }

    // Only deepseek has a non-placeholder URL in the helper; minimax is blocked
    // by the placeholder URL fail-fast (spec Scenario 2).  Guard provider match.
    auto cfg = agenticdsl::test::real_llm_config();
    if (cfg.provider != "deepseek") {
        SUCCEED("skipped: helper resolved provider=" + cfg.provider +
                " (deepseek required for real trace)");
        return;
    }

    std::string out;
    int rc = run_demo("--real-llm deepseek --trace-events "
                      "--context-file " + fix("valid_single_code.jsonl"), &out);
    INFO("rc=" << rc);
    INFO(out);
    CHECK(rc == 0);

    // Parse trace JSONL: baseline phase must carry a real LLM response
    bool found_baseline = false;
    std::istringstream iss(out);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            if (!j.is_object() || j.value("phase", "") != "baseline") continue;
            found_baseline = true;
            std::string response = j.value("response", "");
            CHECK_FALSE(response.empty());
            CHECK_FALSE(response == "Mock evolution response");
            CHECK(j.value("tokens", 0) > 0);
            CHECK(j.value("cost_usd", 0.0) > 0.0);
        } catch (const nlohmann::json::parse_error&) {
            // non-JSON line (e.g. stderr noise), skip
        }
    }
    CHECK(found_baseline);
}

// --- Case 2: real drop_ratio in metrics.json (NOT the -1.0 mock sentinel) ---
TEST_CASE("l2 real llm: --release-metrics writes real drop_ratio",
          "[must_realllm][l2-evolution][real_llm][metrics]") {
    agenticdsl::test::require_real_llm_env();
    if (agenticdsl::test::real_llm_env_skipped()) {
        SUCCEED("skipped: HYDRAFORGE_SKIP_REAL_LLM=1");
        return;
    }
    auto cfg = agenticdsl::test::real_llm_config();
    if (cfg.provider != "deepseek") {
        SUCCEED("skipped: helper resolved provider=" + cfg.provider +
                " (deepseek required for real metrics)");
        return;
    }

    const std::string metrics_path = "/tmp/l2-metrics-e2e-" +
        std::to_string(static_cast<long>(::getpid())) + ".json";
    ::unlink(metrics_path.c_str());

    std::string out;
    int rc = run_demo("--real-llm deepseek --release-metrics --metrics-output "
                      + metrics_path + " --context-file "
                      + fix("valid_single_code.jsonl"), &out);
    INFO("rc=" << rc);
    INFO(out);

    std::ifstream ifs(metrics_path);
    REQUIRE(ifs.is_open());
    nlohmann::json m;
    ifs >> m;
    ::unlink(metrics_path.c_str());

    REQUIRE(m.contains("drop_ratio"));
    CHECK_FALSE(m["drop_ratio"] == -1.0);
    CHECK(m["drop_ratio"].is_number());
    CHECK(m.contains("mutated_passes"));
    CHECK(m.contains("mutated_failures"));
    CHECK(m.contains("context_ids"));
}

// --- Case 3: unknown provider → exit non-zero + stderr message (no LLM needed) ---
TEST_CASE("l2 real llm: unknown provider exits non-zero",
          "[l2-evolution][fail_fast][config]") {
    std::string out;
    int rc = run_demo("--real-llm gpt-4 --context-file "
                      + fix("valid_single_code.jsonl"), &out);
    INFO(out);
    CHECK(rc != 0);
    CHECK(out.find("ERROR: unknown provider") != std::string::npos);
}

// --- Case 4: missing DEEPSEEK_API_KEY + skip unset → exit non-zero (no LLM) ---
TEST_CASE("l2 real llm: missing DEEPSEEK_API_KEY exits non-zero",
          "[l2-evolution][fail_fast][config]") {
    // Ensure the skip flag is NOT set so the missing-key path is exercised.
    const char* skip = std::getenv("HYDRAFORGE_SKIP_REAL_LLM");
    std::string saved_skip = skip ? skip : "";
    ::setenv("HYDRAFORGE_SKIP_REAL_LLM", "0", 1);

    const char* ds = std::getenv("DEEPSEEK_API_KEY");
    std::string saved_ds = ds ? ds : "";
    ::unsetenv("DEEPSEEK_API_KEY");

    std::string out;
    int rc = run_demo("--real-llm deepseek --context-file "
                      + fix("valid_single_code.jsonl"), &out);
    INFO(out);
    CHECK(rc != 0);
    CHECK(out.find("ERROR: real LLM requires DEEPSEEK_API_KEY") != std::string::npos);

    // restore env
    if (!saved_ds.empty()) ::setenv("DEEPSEEK_API_KEY", saved_ds.c_str(), 1);
    if (saved_skip == "1") ::setenv("HYDRAFORGE_SKIP_REAL_LLM", "1", 1);
    else ::unsetenv("HYDRAFORGE_SKIP_REAL_LLM");
}

// --- Case 5: hermetic HOME + real LLM dual-track compatibility ---
TEST_CASE("l2 real llm: hermetic HOME + real LLM dual-track compatible",
          "[must_realllm][l2-evolution][real_llm][hermetic]") {
    agenticdsl::test::require_real_llm_env();
    if (agenticdsl::test::real_llm_env_skipped()) {
        SUCCEED("skipped: HYDRAFORGE_SKIP_REAL_LLM=1");
        return;
    }
    auto cfg = agenticdsl::test::real_llm_config();
    if (cfg.provider != "deepseek") {
        SUCCEED("skipped: helper resolved provider=" + cfg.provider +
                " (deepseek required for hermetic dual-track)");
        return;
    }

    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);

    std::vector<pdk_chat_demo_evolution::LoadError> errors;
    auto contexts = load_fixture("valid_single_code.jsonl", errors);
    REQUIRE(contexts.size() == 1);

    pdk_chat_demo_evolution::EvolutionSession session(
        "deepseek", "None", /*trace_events=*/false);
    session.set_contexts(contexts);
    session.set_hermetic_home(guard);

    // Provider must be constructed from env var directly (not hermetic file).
    REQUIRE(session.real_llm_provider() != nullptr);

    int rc = session.run_6_phase_demo();
    CHECK(rc == 0);

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);
}
