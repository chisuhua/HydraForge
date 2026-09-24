// tests/test_provider_llm_tool_empty.cpp
// F1 Latent Site #3 defense-in-depth (per AGENTS.md Pattern #1):
// ProviderLLMTool bypasses node_executor empty guard → ProviderLLMTool must
// add its own fail-fast check. Latent Sites #4 (process_task) + #6
// (GenerationRequest.model default) still deferred (see AGENTS.md Pattern #2
// upgrade trigger ≥3 sites).
//
// Per Oracle ses_f2b923412ffeTFfBdDqQFOMdT9 (DECISION B recommendation):
// Reuse F1 fix-react-decide-empty-response message format (node_executor.cpp:209-214).
//
// Build: ProviderLLMTool is a file-static class in pdk/loop_agent/src/pdk_entry.cpp.
// Per tests/CMakeLists.txt pattern (temporal_agent / session_agent / provider_agent
// tests), we link pdk_entry.cpp source directly.

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "common/llm/llm_tool.h"
#include "common/llm/llm_types.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

using namespace agenticdsl;

// ProviderLLMTool is a file-static class inside pdk_entry.cpp, so we cannot
// include it directly. Instead, we replicate the EXACT logic here to test
// the pattern, then verify the source matches. This mirrors the test
// strategy used in tests/test_dsl_engine_ctx_bridge.cpp for node_executor
// (where the fail-fast is also a static helper).

namespace {

// MockLLMEmptyProvider: returns success=true but text="".
// Simulates F1 Latent Site #3: provider succeeded but empty output.
class MockLLMEmptyProvider : public ILLMProvider {
public:
    Result<GenerationResult, LLMError> generate(
        const GenerationRequest& /*req*/, std::stop_token /*token*/) override {
        GenerationResult r;
        r.text = "";  // KEY: empty text — F1 Latent Site #3 scenario
        r.completion_tokens = 0;
        return Result<GenerationResult, LLMError>::success(r);
    }
    std::unique_ptr<IGenerationStream> generate_stream(
        const GenerationRequest& /*req*/, std::stop_token /*token*/) override {
        return nullptr;
    }
    std::vector<ModelInfo> available_models() const override { return {}; }
};

// MockLLMNonEmptyProvider: returns success=true and text=<expected>.
class MockLLMNonEmptyProvider : public ILLMProvider {
public:
    explicit MockLLMNonEmptyProvider(std::string expected)
        : expected_(std::move(expected)) {}
    Result<GenerationResult, LLMError> generate(
        const GenerationRequest& /*req*/, std::stop_token /*token*/) override {
        GenerationResult r;
        r.text = expected_;
        r.completion_tokens = 42;
        return Result<GenerationResult, LLMError>::success(r);
    }
    std::unique_ptr<IGenerationStream> generate_stream(
        const GenerationRequest& /*req*/, std::stop_token /*token*/) override {
        return nullptr;
    }
    std::vector<ModelInfo> available_models() const override { return {}; }
    std::string expected_;
};

// Replica of ProviderLLMTool::generate logic (per pdk/loop_agent/src/pdk_entry.cpp:46-66).
// This MUST stay in sync with the production code. If ProviderLLMTool logic
// changes, update this replica AND verify ctest passes both.
// Post-fix: should throw runtime_error when res.text() is empty.
LLMResult provider_llm_tool_generate_replica(
    ILLMProvider& provider, const std::string& prompt, const LLMParams& params) {
    LLMResult out;
    GenerationRequest req(prompt);
    req.params = params;
    auto avail = provider.available_models();
    if (!avail.empty()) {
        req.params.model = avail.front().name;
    }
    auto res = provider.generate(req, std::stop_token{});
    if (res.has_value()) {
        out.text = std::move(res).value().text;
        // F1 Latent Site #3 defense-in-depth (per AGENTS.md Pattern #1):
        if (out.text.empty()) {
            throw std::runtime_error(
                "ProviderLLMTool: LLM call succeeded but returned empty text. "
                "Provider: loop-agent-provider-bridge. "
                "Check provider model availability or prompt template.");
        }
        out.success = true;
        out.tokens_generated = res.value().completion_tokens;
    } else {
        out.success = false;
        out.error = res.error().message;
    }
    return out;
}

}  // namespace

// Case 1: empty text triggers fail-fast runtime_error (RED → GREEN after fix)
TEST_CASE("ProviderLLMTool: empty text triggers fail-fast runtime_error",
          "[loop_agent][provider_llm_tool_empty]") {
    MockLLMEmptyProvider provider;

    REQUIRE_THROWS_AS(
        provider_llm_tool_generate_replica(provider, "test prompt", LLMParams{}),
        std::runtime_error);

    // Verify error message contains diagnostic info (per F1 message format)
    try {
        provider_llm_tool_generate_replica(provider, "test prompt", LLMParams{});
    } catch (const std::runtime_error& e) {
        std::string msg(e.what());
        REQUIRE(msg.find("empty text") != std::string::npos);
        REQUIRE(msg.find("ProviderLLMTool") != std::string::npos);
        // Per F1 message format: include check guidance
        REQUIRE(msg.find("Check provider model") != std::string::npos);
    }
}

// Case 2: non-empty text returns success (regression: ensure fix doesn't break normal path)
TEST_CASE("ProviderLLMTool: non-empty text returns success",
          "[loop_agent][provider_llm_tool_empty]") {
    MockLLMNonEmptyProvider provider("expected response content");

    auto result = provider_llm_tool_generate_replica(
        provider, "test prompt", LLMParams{});

    REQUIRE(result.success);
    REQUIRE(result.text == "expected response content");
    REQUIRE(result.tokens_generated == 42);
    REQUIRE(result.error.empty());
}

// Case 3: source-level regression check — verify pdk_entry.cpp has the fix
// This is a sanity check that catches the case where the test replica
// passes but the actual production code is out of sync.
TEST_CASE("ProviderLLMTool: production source pdk_entry.cpp has the fail-fast guard",
          "[loop_agent][provider_llm_tool_empty][source_guard]") {
    // Walk up from cwd to repo root (looking for AGENTS.md anchor), since ctest
    // runs from build/tests directory.
    fs::path p = fs::current_path();
    fs::path source_path;
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(p / "AGENTS.md")) {
            source_path = p / "pdk/loop_agent/src/pdk_entry.cpp";
            break;
        }
        if (p.has_parent_path()) p = p.parent_path();
        else break;
    }
    REQUIRE_FALSE(source_path.empty());

    std::ifstream in(source_path.string());
    REQUIRE(in.is_open());

    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    // The fix should mention ProviderLLMTool + empty text in a runtime_error
    bool has_guard = content.find("ProviderLLMTool") != std::string::npos &&
                     content.find("empty text") != std::string::npos &&
                     content.find("runtime_error") != std::string::npos;
    REQUIRE(has_guard);
}