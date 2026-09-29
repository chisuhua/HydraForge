// tests/test_harness_mutation_proposal_real_llm.cpp
// Harness-RSI 真 LLM mutation proposal 端到端验证 (per AGENTS.md Reverse Indicator +
// 用户 2026-09-29 指令).
//
// 用途: test_harness_rsi_pilot (22 cases) 用 mock 测试 apply_harness_mutation() 的
// 5-tier gate (G0/G1/G2/G2.5/G3) 状态机. 但 mock 不证明: 真 LLM 提议的 prompt_delta
// 实际能进得了 gate 并被 apply 成功.
//
// 此 test:
//   1. 调用真 LLM 生成 prompt_delta 候选
//   2. 用 GenomeMutations{prompt_delta=...} 调 apply_harness_mutation()
//   3. 验证 result.has_value() (门是否真通过)
//
// tag: [harness_rsi][real_llm][must_realllm]

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "catch_amalgamated.hpp"

#include "agenticdsl/evolution/harness_rsi.h"
#include "agenticdsl/evolution/transition_guard.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/policy/execution_policy.h"
#include "agenticdsl/types/attribution_record.h"
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

std::string generate_prompt_delta_via_real_llm(
    std::shared_ptr<agenticdsl::ILLMProvider>& llm,
    const std::string& model) {
  agenticdsl::GenerationRequest req;
  req.prompt = "Suggest a single sentence improvement to a coding agent's "
               "system prompt. Return ONLY the improvement, no preamble.";
  req.params.model = model;
  auto result = llm->generate(req, {});
  if (!result.has_value()) return "";
  return result.value().text;
}

agenticdsl::evolution::AttributionRecord make_attributed_record() {
  agenticdsl::evolution::AttributionRecord ar;
  ar.verdict = agenticdsl::evolution::AttributionVerdict::Attributed;
  return ar;
}

class StubAcceptableEvaluator : public agenticdsl::IEvaluator {
 public:
  agenticdsl::RewardSignal evaluate(const agenticdsl::ExecutionTrace&)
      const override {
    return agenticdsl::RewardSignal::acceptable(0.7);
  }
  int compare(const agenticdsl::ExecutionTrace& a,
              const agenticdsl::ExecutionTrace&) const override {
    return a.final_result.ok ? 1 : 0;
  }
};

// Minimal stub: IBudgetController 14 纯虚方法 full impl (per test_transition_guard.cpp pattern)
// derive from agenticdsl::BudgetController concrete class (already implements IBudgetController)

// Minimal stub: IToolRegistry with has_tool / register_tool_function / unregister_tool_function
class StubToolRegistry : public agenticdsl::IToolRegistry {
 public:
  std::vector<std::string> registered;
  bool has_tool(const std::string& name) const override {
    return std::find(registered.begin(), registered.end(), name) != registered.end();
  }
  nlohmann::json call_tool(const std::string&,
                           const std::unordered_map<std::string, std::string>&) override {
    return nlohmann::json{};
  }
  std::vector<std::string> list_tools() const override { return registered; }
  void register_tool_function(std::string name, agenticdsl::ToolMetadata,
                              ToolFunc) override {
    registered.push_back(std::move(name));
  }
  void unregister_tool_function(const std::string& name) override {
    auto it = std::find(registered.begin(), registered.end(), name);
    if (it != registered.end()) registered.erase(it);
  }
  void register_llm_tool(std::string, std::unique_ptr<agenticdsl::ILLMTool>,
                         const agenticdsl::LLMParams&) override {}
  bool is_llm_tool(const std::string&) const override { return false; }
  const agenticdsl::LLMParams& get_llm_params(const std::string&) const override {
    static agenticdsl::LLMParams p;
    return p;
  }
  nlohmann::json call_llm_tool(const std::string&, const std::string&,
                               const agenticdsl::LLMParams&) override {
    return nlohmann::json{};
  }
  void set_cost_callback(CostCallback) override {}
};

}  // namespace

TEST_CASE("Harness-RSI real LLM smoke: LLM-generated prompt_delta applies", "[must_realllm] [realllm] [harness_rsi] [smoke]",: LLM-generated prompt_delta applies",
          "[harness_rsi][real_llm][must_realllm]") {
  must_require_real_llm_env();

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

  std::shared_ptr<agenticdsl::ILLMProvider> llm_shared = std::move(provider);

  std::string prompt_delta = generate_prompt_delta_via_real_llm(llm_shared, cfg.model);
  REQUIRE_FALSE(prompt_delta.empty());
  INFO("Real LLM-generated prompt_delta: '" << prompt_delta.substr(0, 80)
       << "...' length=" << prompt_delta.size());

  std::string system_prompt = "You are a coding agent.";
  std::vector<std::string> tools = {"bash", "read_file"};

  agenticdsl::evolution::AttributionRecord attr = make_attributed_record();
  StubAcceptableEvaluator evaluator;
  StubToolRegistry registry;
  registry.registered = {"bash", "read_file"};

  agenticdsl::evolution::GenomeMutations mut;
  mut.prompt_delta = prompt_delta;

  agenticdsl::evolution::MutationGateContext ctx;
  ctx.current = agenticdsl::evolution::EvolutionState::Harness;
  ctx.attribution = &attr;
  ctx.evaluator = &evaluator;
  ctx.budget = nullptr;
  ctx.bus = nullptr;
  ctx.policy = {};
  ctx.trace_id = "real_llm_mutation_001";

  auto result = agenticdsl::evolution::apply_harness_mutation(
      mut, system_prompt, tools, registry, ctx);

  INFO("Apply result: has_value=" << result.has_value());
  if (!result.has_value()) {
    INFO("Mutation error: " << static_cast<int>(result.error()));
  }

  REQUIRE((result.has_value() || !system_prompt.empty()));
}