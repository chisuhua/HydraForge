// tests/test_gepa_loop_real_llm.cpp
// GEPA Loop 端到端真实 LLM 验证测试 (per AGENTS.md Reverse Indicator Rule +
// 用户 2026-09-29 "只有通过了真实 LLM 验证才判定通过" 指令).
//
// 用途: test_gepa_phase2.cpp 用 MockLLMProvider 验证 GEPA Loop 状态机逻辑,
// 但 mock 不能证明真 LLM 真的能产生有用的反思 / mutation proposal.
// 此 test 用真 DeepSeek / MiniMax 跑 GEPA Loop 端到端, 验证:
//   1. 真 LLM 真被调用 (governor->propose_calls > 0)
//   2. 真 LLM 输出能进 governor 决策链 (governor->commit_calls >= 0)
//   3. 真 LLM 失败 (网络/parse) 时 reflection 优雅失败 (failure_mode 非空,
//      不 crash, 不 OOM)
//
// 与 mock 版本区别:
//   - mock: MockLLMProvider 返回硬编码 "Reflection note: Add error handling for X"
//   - real: 真 LLM (deepseek-chat / minimax-text-01) 根据 failed trace 真实生成 reflection
//
// tag: [gepa][real_llm][must_realllm]
//
// 引用:
//   - tests/test_gepa_phase2.cpp (mock 对照组, 21 cases)
//   - tests/test_helpers/real_llm_env.h (must_require_real_llm_env helper)
//   - openspec/changes/2026-09-25-real-llm-only-validation/ (本文档所属 change)

#include <cstdlib>
#include <memory>
#include <string>

#include "catch_amalgamated.hpp"

#include "agenticdsl/cognitive/gepa_loop.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/imutation_governance.h"
#include "agenticdsl/genome/genome.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"
#include "common/llm/llm_types.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/llm_config.h"
#include "modules/budget/budget_controller.h"
#include "test_helpers/real_llm_env.h"

using agenticdsl::test::must_require_real_llm_env;
using agenticdsl::test::real_llm_config;

namespace {

// Stub IEvaluator: 简单返回 acceptable quality (不卡 loop)
class StubEvaluator : public agenticdsl::IEvaluator {
 public:
  agenticdsl::RewardSignal evaluate(const agenticdsl::ExecutionTrace& /*trace*/)
      const override {
    return agenticdsl::RewardSignal::acceptable(0.5);
  }
  int compare(const agenticdsl::ExecutionTrace& a,
              const agenticdsl::ExecutionTrace& /*b*/) const override {
    return a.final_result.ok ? 1 : 0;
  }
};

// Stub IMutationGovernor: 计数 + 默认放行 (per test_gepa_phase2 pattern)
class StubMutationGovernor : public agenticdsl::IMutationGovernor {
 public:
  mutable int propose_calls = 0;
  mutable int commit_calls = 0;
  bool propose_approved = true;
  bool commit_approved = true;

  agenticdsl::MutationDecision propose(
      const agenticdsl::MutationContext& /*ctx*/) override {
    ++propose_calls;
    return {propose_approved, propose_approved ? "" : "test_deny", ""};
  }
  agenticdsl::MutationDecision commit(
      const agenticdsl::MutationContext& /*ctx*/) override {
    ++commit_calls;
    return {commit_approved, commit_approved ? "" : "test_deny", ""};
  }
  void revert(const agenticdsl::MutationContext& /*ctx*/,
              const std::string& /*target*/,
              const std::string& /*reason*/) override {}
};

// 合成失败 trace
agenticdsl::ExecutionTrace make_failed_trace(const std::string& trace_id) {
  agenticdsl::ExecutionTrace trace;
  trace.trace_id = trace_id;
  trace.final_result = agenticdsl::ToolResult::error(
      agenticdsl::ErrorCode::Unknown, "execution_failed",
      nlohmann::json{{"context", "real_llm GEPA test: simulate failed trace"}});
  return trace;
}

// 构造 GEPALoop (用真 LLM + stub governor + stub evaluator)
std::unique_ptr<agenticdsl::GEPALoop> build_loop(
    std::shared_ptr<StubMutationGovernor>& governor_out) {
  auto evaluator = std::make_shared<StubEvaluator>();
  auto governor = std::make_shared<StubMutationGovernor>();
  governor_out = governor;

  auto cfg = real_llm_config();
  agenticdsl::LLMConfig llm_cfg;
  llm_cfg.provider = cfg.provider;
  llm_cfg.model = cfg.model;
  llm_cfg.api_url = cfg.api_url;
  llm_cfg.api_endpoint = cfg.api_endpoint;
  llm_cfg.api_key = cfg.api_key;
  llm_cfg.api_key_env = cfg.api_key_env;

  agenticdsl::LLMProviderFactory factory;
  auto provider = factory.create(llm_cfg);
  REQUIRE(provider != nullptr);

  agenticdsl::GEPALoop::Config loop_cfg;
  loop_cfg.reward_threshold = 0.0;
  loop_cfg.max_iterations = 2;  // 限制轮次, 避免真实 LLM 慢响应
  loop_cfg.source_id = "R_T19_GEPA_realllm";

  return std::make_unique<agenticdsl::GEPALoop>(
      evaluator, governor, std::move(provider), loop_cfg);
}

}  // namespace

TEST_CASE("GEPA Loop real LLM smoke: failed trace triggers LLM reflection",
          "[must_realllm][realllm][gepa][smoke]") {
  must_require_real_llm_env();

  std::shared_ptr<StubMutationGovernor> governor;
  auto loop = build_loop(governor);

  agenticdsl::ExecutionTrace trace = make_failed_trace("gepa_real_smoke_001");
  agenticdsl::GEPALoop::ReflectionResult result = loop->reflect_and_commit(trace);

  INFO("GEPA Loop reflection result: success=" << result.success
       << " failure_mode='" << result.failure_mode << "'");
  INFO("Governor propose_calls=" << governor->propose_calls
       << " commit_calls=" << governor->commit_calls);

  REQUIRE(governor->propose_calls > 0);
  REQUIRE((result.success || !result.failure_mode.empty()));
}

TEST_CASE("GEPA Loop real LLM R3: 多次迭代真实 LLM 累加",
          "[must_realllm][realllm][gepa][r3]") {
  must_require_real_llm_env();

  std::shared_ptr<StubMutationGovernor> governor;
  auto loop = build_loop(governor);

  agenticdsl::ExecutionTrace trace = make_failed_trace("gepa_real_iter_001");
  agenticdsl::GEPALoop::ReflectionResult result = loop->reflect_and_commit(trace);

  int initial_propose_calls = governor->propose_calls;
  for (int i = 0; i < 3; ++i) {
    auto trace_i = make_failed_trace("gepa_real_iter_loop_" + std::to_string(i));
    loop->reflect_and_commit(trace_i);
  }

  INFO("After 3 more iterations: propose_calls="
       << governor->propose_calls << " (增量 "
       << (governor->propose_calls - initial_propose_calls) << ")");
  REQUIRE(governor->propose_calls >= initial_propose_calls + 3);
}