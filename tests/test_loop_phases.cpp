// tests/test_loop_phases.cpp
// 文件头注释
// 功能描述：loop_phases.h 共享 helper 单元测试 (TDD 5步 Phase 0 RED, per ADR-0089 v1.3 D1.inv.model)
//          覆盖 8 cases: plan(3) + execute(2) + verify(3)
//          关键: plan_phase/verify_phase 显式 req.params.model.clear() 必须可由 Recording Provider 验证 (D1.inv.model)
// 设计依据：openspec/changes/consolidate-loop-phases-to-shared-helpers/{proposal,design,tasks,specs/*}.md §Phase 0
// 作者：AgenticDSL Sprint 37+ (consolidate-loop-phases)
// 最后修改日期：2026-10-09

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/agent_loops/loop_phases.h"
#include "common/llm/llm_types.h"
#include "core/engine.h"
#include "agenticdsl/types/layered_context.h"

#include <nlohmann/json.hpp>

#include <memory>
#include <stop_token>
#include <string>
#include <vector>

using agenticdsl::GenerationRequest;
using agenticdsl::GenerationResult;
using agenticdsl::ILLMProvider;
using agenticdsl::LLMError;
using hydraforge::pdk::loop_phases::run_execute_phase;
using hydraforge::pdk::loop_phases::run_plan_phase;
using hydraforge::pdk::loop_phases::run_verify_phase;

// RecordingLLMProvider — 录制 req.params.model 用于 model.clear() invariant 验证 (per AGENTS.md §REAL-LLM TEST PATTERNS #5)
// 设计依据: fix-generation-request-model-default 修复链 → plan_execute_loop.h:217-224 NOT redundant 注释
//           AGENTS.md §REAL-LLM TEST PATTERNS #5: 不需要 API key, CI 永远 PASS, 唯一真正守护生产契约的层
namespace {

class RecordingLLMProvider : public ILLMProvider {
 public:
  std::string last_model;       // 录制最近一次 generate 的 model 字段 (验证 model.clear() invariant)
  int generate_calls = 0;        // 调用计数
  GenerationResult preset_result_; // 预置返回结果 (success/error/特定文本)

  // 预置 success path
  explicit RecordingLLMProvider(GenerationResult result = make_ok("yes"))
      : preset_result_(std::move(result)) {}

  static GenerationResult make_ok(std::string text) {
    GenerationResult r;
    r.text = std::move(text);
    return r;
  }

  std::expected<GenerationResult, LLMError> generate(
      const GenerationRequest& req, std::stop_token /*token*/) override {
    last_model = req.params.model;
    ++generate_calls;
    return preset_result_;
  }

  std::unique_ptr<agenticdsl::IGenerationStream> generate_stream(
      const GenerationRequest& /*req*/, std::stop_token /*token*/) override {
    return nullptr;  // helper 不涉及 stream path
  }

  std::vector<agenticdsl::ILLMProvider::ModelInfo> available_models() const override { return {}; }
};

// Minimal DSL for execute_phase success (start -> end stub subgraph)
const std::string kMinimalDsl = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/main/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

}  // namespace

TEST_CASE("loop_phases run_plan_phase: LLM non-empty text returns optional with value",
          "[loop_phases][plan_phase][unit]") {
  RecordingLLMProvider mock(RecordingLLMProvider::make_ok("plan DSL markdown"));

  agenticdsl::LayeredContext ctx = agenticdsl::LayeredContext::load(nlohmann::json{
      {"system", nlohmann::json::object()},
      {"recent", nlohmann::json::object()},
      {"working", {{"data", nlohmann::json::object()}}},
      {"archive", nlohmann::json::object()},
      {"meta", nlohmann::json::object()}});

  auto result = run_plan_phase(mock, "compute 2+3", ctx, std::stop_token{});

  REQUIRE(result.has_value());
  REQUIRE(*result == "plan DSL markdown");
  REQUIRE(mock.generate_calls == 1);
}

TEST_CASE("loop_phases run_plan_phase: LLM empty response returns nullopt",
          "[loop_phases][plan_phase][unit]") {
  RecordingLLMProvider mock(RecordingLLMProvider::make_ok(""));

  agenticdsl::LayeredContext ctx = agenticdsl::LayeredContext::load(nlohmann::json{
      {"system", nlohmann::json::object()},
      {"recent", nlohmann::json::object()},
      {"working", {{"data", nlohmann::json::object()}}},
      {"archive", nlohmann::json::object()},
      {"meta", nlohmann::json::object()}});

  auto result = run_plan_phase(mock, "compute 2+3", ctx, std::stop_token{});

  REQUIRE_FALSE(result.has_value());
  REQUIRE(mock.generate_calls == 1);
}

TEST_CASE("loop_phases run_plan_phase: req.params.model cleared (D1.inv.model oracle M1)",
          "[loop_phases][plan_phase][invariant][must]") {
  // 此 case 是回归守卫 — 若 helper 抽取时遗漏 req.params.model.clear(),
  // Recording Provider 会看到非空 model 字段, 测试 FAIL.
  // 历史教训: fix-generation-request-model-default (2026-09-08 archive ship 5 站点)
  RecordingLLMProvider mock(RecordingLLMProvider::make_ok("plan"));

  agenticdsl::LayeredContext ctx = agenticdsl::LayeredContext::load(nlohmann::json{
      {"system", nlohmann::json::object()},
      {"recent", nlohmann::json::object()},
      {"working", {{"data", nlohmann::json::object()}}},
      {"archive", nlohmann::json::object()},
      {"meta", nlohmann::json::object()}});

  run_plan_phase(mock, "test goal", ctx, std::stop_token{});

  REQUIRE(mock.last_model.empty());  // helper 内部 MUST 显式清空 (D1.inv.model)
}

TEST_CASE("loop_phases run_execute_phase: valid DSL returns true",
          "[loop_phases][execute_phase][unit]") {
  RecordingLLMProvider mock;  // execute_phase 不调 LLM, mock 仅用于构造 engine

  auto engine = agenticdsl::DSLEngine::from_markdown(kMinimalDsl);
  REQUIRE(engine != nullptr);
  std::optional<std::string> err;

  bool ok = run_execute_phase(*engine, kMinimalDsl, err);

  REQUIRE(ok);
  REQUIRE_FALSE(err.has_value());
}

TEST_CASE("loop_phases run_execute_phase: parse failure returns false + execute_error_out filled",
          "[loop_phases][execute_phase][unit]") {
  RecordingLLMProvider mock;

  auto engine = agenticdsl::DSLEngine::from_markdown(kMinimalDsl);
  REQUIRE(engine != nullptr);
  std::optional<std::string> err;

  // 非法 node path (不满足 ^/[\w/\-]+$) → MarkdownParser::parse_from_string 抛异常
  // (markdown_parser.cpp:97-99 "Invalid node path format: <path>")
  // 注意: 纯文本字符串 (无 AgenticDSL fenced block) 会静默返回空图, 不抛异常 — 不能用于此 case
  const std::string kInvalidPathDsl =
      "# AgenticDSL `invalid path with spaces`\n"
      "```yaml\n"
      "# --- BEGIN AgenticDSL ---\n"
      "graph_type: subgraph\n"
      "# --- END AgenticDSL ---\n"
      "```\n";
  bool ok = run_execute_phase(*engine, kInvalidPathDsl, err);

  REQUIRE_FALSE(ok);
  REQUIRE(err.has_value());
  REQUIRE_FALSE(err->empty());
}

TEST_CASE("loop_phases run_verify_phase: LLM response containing 'yes' returns true (case insensitive)",
          "[loop_phases][verify_phase][unit]") {
  RecordingLLMProvider mock(RecordingLLMProvider::make_ok("YES, plan appended successfully"));

  bool ok = run_verify_phase(mock, "compute 2+3", nlohmann::json::object(), std::stop_token{});

  REQUIRE(ok);
  REQUIRE(mock.generate_calls == 1);
}

TEST_CASE("loop_phases run_verify_phase: LLM response 'no' or empty returns false",
          "[loop_phases][verify_phase][unit]") {
  SECTION("response 'no'") {
    RecordingLLMProvider mock(RecordingLLMProvider::make_ok("no, plan not found"));
    REQUIRE_FALSE(run_verify_phase(mock, "g", nlohmann::json::object(), std::stop_token{}));
  }
  SECTION("response empty") {
    RecordingLLMProvider mock(RecordingLLMProvider::make_ok(""));
    REQUIRE_FALSE(run_verify_phase(mock, "g", nlohmann::json::object(), std::stop_token{}));
  }
  SECTION("response ambiguous (no 'yes')") {
    RecordingLLMProvider mock(RecordingLLMProvider::make_ok("uncertain"));
    REQUIRE_FALSE(run_verify_phase(mock, "g", nlohmann::json::object(), std::stop_token{}));
  }
}

TEST_CASE("loop_phases run_verify_phase: req.params.model cleared (D1.inv.model oracle M1)",
          "[loop_phases][verify_phase][invariant][must]") {
  // 与 plan_phase model.clear() 同理由 — helper 抽取时同步移植
  RecordingLLMProvider mock(RecordingLLMProvider::make_ok("yes"));

  run_verify_phase(mock, "test goal", nlohmann::json::object(), std::stop_token{});

  REQUIRE(mock.last_model.empty());
}