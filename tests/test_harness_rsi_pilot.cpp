// C4 harness-rsi-pilot RED tests (Phase 6c MetaRSI-v1)
// 4 mock test cases per tasks.md Phase 3 + Oracle bg_770d1308 SHIP-with-fixes 修正
// Catch2 v3.7.4 amalgamated

#include "catch_amalgamated.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "agenticdsl/genome/genome.h"

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/evolution/harness_rsi.h"
#include "agenticdsl/evolution/transition_guard.h"
#include "agenticdsl/types/attribution_record.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"
#include "common/llm/llm_types.h"  // Result<T, E>
#include "modules/budget/budget_controller.h"  // BudgetController concrete class

namespace agenticdsl::evolution {
namespace testing {
namespace {

// Minimal stub: IBudgetController 14 纯虚方法 full impl (per test_transition_guard.cpp pattern)
// derive from agenticdsl::BudgetController concrete class (already implements IBudgetController)
class StubBudgetOk : public agenticdsl::BudgetController {
 public:
  StubBudgetOk() : agenticdsl::BudgetController(std::nullopt), mock_exceeded(false) {}
  bool exceeded() const override { return mock_exceeded; }
  bool try_consume_node() override { return !mock_exceeded; }
  bool try_consume_llm_call() override { return !mock_exceeded; }
  bool try_consume_subgraph_depth() override { return !mock_exceeded; }
  bool try_consume_evolution_llm_call() override { return !mock_exceeded; }
  void set_termination_target(const agenticdsl::NodePath&) override {}
  std::optional<agenticdsl::NodePath> get_termination_target() const override {
    return std::nullopt;
  }
  void record_llm_call(int, const std::string&) override {}
  double get_total_cost_usd() const override { return 0.0; }
  void reset() override {}
  bool evolution_budget_exceeded() const override { return mock_exceeded; }
  void begin_evolution_cycle(const std::string& /*cycle_id*/) override {}
  void end_evolution_cycle(const std::string& /*cycle_id*/, bool /*success*/) override {}
  void reset_evolution_cycle_counter() override {}
 private:
  bool mock_exceeded;
};

// Same as above but exceeded() returns true (per Oracle M3 构造失败输入)
class StubBudgetExceeded : public agenticdsl::BudgetController {
 public:
  StubBudgetExceeded() : agenticdsl::BudgetController(std::nullopt), mock_exceeded(true) {}
  bool exceeded() const override { return mock_exceeded; }
  bool try_consume_node() override { return !mock_exceeded; }
  bool try_consume_llm_call() override { return !mock_exceeded; }
  bool try_consume_subgraph_depth() override { return !mock_exceeded; }
  bool try_consume_evolution_llm_call() override { return !mock_exceeded; }
  void set_termination_target(const agenticdsl::NodePath&) override {}
  std::optional<agenticdsl::NodePath> get_termination_target() const override {
    return std::nullopt;
  }
  void record_llm_call(int, const std::string&) override {}
  double get_total_cost_usd() const override { return 0.0; }
  void reset() override {}
  bool evolution_budget_exceeded() const override { return mock_exceeded; }
  void begin_evolution_cycle(const std::string& /*cycle_id*/) override {}
  void end_evolution_cycle(const std::string& /*cycle_id*/, bool /*success*/) override {}
  void reset_evolution_cycle_counter() override {}
 private:
  bool mock_exceeded;
};

// Minimal stub: IEvaluator returns Excellent quality (per Oracle bg_770d1308 fix)
class StubEvaluatorExcellent : public IEvaluator {
 public:
  RewardSignal evaluate(const ExecutionTrace& /*trace*/) const override {
    return RewardSignal::excellent(1.0);
  }
  int compare(const ExecutionTrace& /*a*/, const ExecutionTrace& /*b*/) const override {
    return 0;
  }
};

// Minimal stub: IEvaluator returns Poor quality (per Oracle M3 构造失败输入)
class StubEvaluatorPoor : public IEvaluator {
 public:
  RewardSignal evaluate(const ExecutionTrace& /*trace*/) const override {
    return RewardSignal::poor(1.0);
  }
  int compare(const ExecutionTrace& /*a*/, const ExecutionTrace& /*b*/) const override {
    return 0;
  }
};

// Minimal stub: IInteractionBus 捕获 emitted events (per Oracle M3 验证 4-field payload)
class CapturingBus : public IInteractionBus {
 public:
  struct CapturedEvent {
    std::string topic;
    nlohmann::json data;
    nlohmann::json meta;
  };
  std::vector<CapturedEvent> captured;

  void emit(const BusEvent& event) override {
    CapturedEvent e;
    e.topic = event.topic;
    e.data = event.payload.data;
    e.meta = event.payload.meta;
    captured.push_back(e);
  }
  void emit(const std::string& /*event_type*/, const std::string& /*content*/) override {}
  size_t subscribe(const std::string& /*event_type*/,
                  std::function<void(const BusEvent&)> /*callback*/) override { return 0; }
  void unsubscribe(size_t /*token*/) override {}
};

// Minimal stub: IToolRegistry with has_tool / register_tool_function / unregister_tool_function
class StubToolRegistry : public IToolRegistry {
 public:
  bool has_tool(const std::string& name) const override {
    return std::find(registered.begin(), registered.end(), name) != registered.end();
  }
  nlohmann::json call_tool(const std::string& /*name*/,
                           const std::unordered_map<std::string, std::string>& /*args*/) override {
    return nlohmann::json{};
  }
  std::vector<std::string> list_tools() const override { return registered; }
  void register_tool_function(std::string name, ToolMetadata /*meta*/,
                              ToolFunc /*fn*/) override {
    registered.push_back(std::move(name));
  }
  void unregister_tool_function(const std::string& name) override {
    auto it = std::find(registered.begin(), registered.end(), name);
    if (it != registered.end()) registered.erase(it);
  }
  void register_llm_tool(std::string /*name*/, std::unique_ptr<ILLMTool> /*tool*/,
                         const LLMParams& /*default_params*/) override {}
  bool is_llm_tool(const std::string& /*name*/) const override { return false; }
  const LLMParams& get_llm_params(const std::string& /*name*/) const override {
    static LLMParams p;
    return p;
  }
  nlohmann::json call_llm_tool(const std::string& /*name*/, const std::string& /*prompt*/,
                              const LLMParams& /*params*/) override { return nlohmann::json{}; }
  void set_cost_callback(CostCallback /*cb*/) override {}

 private:
  std::vector<std::string> registered = {"trusted_tool"};
};

}  // namespace
}  // namespace testing
}  // namespace agenticdsl::evolution

namespace fs = std::filesystem;
using namespace agenticdsl::evolution;

// 文件级 hermetic fixture: 注入 HMAC key (per C1 教训: 防 G4 测试依赖宿主机既存 key)
struct G4GenomeEnv {
    G4GenomeEnv() {
        setenv("HYDRAFORGE_GENOME_KEY",
               "test_key_g4_00000000000000000000000000000000", 1);
    }
};
const G4GenomeEnv g_g4_genome_env;

// ============================================================================
// Case 1: prompt_delta apply (success path, 确定性字符串断言)
// ============================================================================
TEST_CASE("C4 case-1: apply_harness_mutation prompt_delta apply succeeds",
          "[c4][harness-rsi][case-1]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{}
  };

  std::string system_prompt = "You are helpful.";
  std::vector<std::string> tools = {"trusted_tool"};
  GenomeMutations m;
  m.prompt_delta = "Be concise.";

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  REQUIRE(result.has_value());
  REQUIRE(system_prompt.find("Be concise.") != std::string::npos);
  REQUIRE(tools.size() == 1);
}

// ============================================================================
// Case 2: readiness denied + 4/4 event payload assertion (per Oracle M3)
// ============================================================================
TEST_CASE("C4 case-2: apply_harness_mutation readiness denied + 4-field event payload",
          "[c4][harness-rsi][case-2]") {
  using namespace agenticdsl::evolution::testing;

  // 构造失败输入 (per Oracle M3 措辞修正): stub IEvaluator returns Poor + IBudgetController.exceeded()=true
  AttributionRecord attr;
  attr.verdict = AttributionVerdict::NotAttempted;
  StubEvaluatorPoor evaluator;
  StubBudgetExceeded budget;
  CapturingBus bus;
  StubToolRegistry registry;

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{}
  };

  const std::string initial_prompt = "You are helpful.";
  std::string system_prompt = initial_prompt;
  std::vector<std::string> tools = {"trusted_tool"};
  GenomeMutations m;
  m.prompt_delta = "Be concise.";

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  // Zero state change (per C3 fail-closed contract)
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::NotReady);
  REQUIRE(system_prompt == initial_prompt);
  REQUIRE(tools.size() == 1);
  REQUIRE(tools[0] == "trusted_tool");

  // 4/4 event payload assertion (per Oracle M3 + ADR-0068 v2.2 line 253)
  REQUIRE(bus.captured.size() == 1);
  REQUIRE(bus.captured[0].topic == "evolution.readiness.denied");
  const auto& payload = bus.captured[0].data;
  REQUIRE(payload.contains("failed_conditions"));
  REQUIRE(payload["failed_conditions"].is_array());
  REQUIRE(payload["failed_conditions"].size() >= 1);
  REQUIRE(payload.contains("attribution_verdict"));
  REQUIRE(payload["attribution_verdict"].is_string());
  REQUIRE(payload.contains("eval_quality"));
  REQUIRE(payload["eval_quality"].is_string());
  REQUIRE(payload.contains("budget_state"));
  REQUIRE(payload["budget_state"].is_string());
}

// ============================================================================
// Case 3a: dangerous tool veto (per Oracle C3 重写 — MutationGovernancePolicy denied_tools)
// ============================================================================
TEST_CASE("C4 case-3a: apply_harness_mutation dangerous tool veto (denied_tools)",
          "[c4][harness-rsi][case-3a]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  // dangerous_tool NOT in registry (would-be-registered) — policy denies it
  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{denied_tools: {"dangerous_tool"}}
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  GenomeMutations m;
  m.tools_add = {"dangerous_tool"};

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::GovernanceDenied);
  REQUIRE(tools.empty());
  REQUIRE_FALSE(registry.has_tool("dangerous_tool"));
}

// ============================================================================
// Case 3b: trusted tool add positive case (per Metis 2.4 充分性)
// ============================================================================
TEST_CASE("C4 case-3b: apply_harness_mutation trusted tool add positive",
          "[c4][harness-rsi][case-3b]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{}  // empty policy: no denied_tools
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  GenomeMutations m;
  m.tools_add = {"trusted_tool"};

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  // trusted_tool IS in StubToolRegistry registered = {"trusted_tool"}, so registry.has_tool pass
  REQUIRE(result.has_value());
  REQUIRE(registry.has_tool("trusted_tool"));
  REQUIRE(std::find(tools.begin(), tools.end(), "trusted_tool") != tools.end());
}

// ============================================================================
// Case 3c: tools_remove positive path
// ============================================================================
TEST_CASE("C4 case-3c: apply_harness_mutation tools_remove (DB1 end-to-end)",
          "[c4][harness-rsi][case-3c]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{}
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools = {"trusted_tool"};
  GenomeMutations m;
  m.tools_remove = {"trusted_tool"};

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  REQUIRE(result.has_value());
  REQUIRE_FALSE(registry.has_tool("trusted_tool"));
  REQUIRE(std::find(tools.begin(), tools.end(), "trusted_tool") == tools.end());
}

// ============================================================================
// Case 0: InvalidMutation fail-fast
// ============================================================================
TEST_CASE("C4 case-0: apply_harness_mutation empty mutation → InvalidMutation",
          "[c4][harness-rsi][case-0]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{}
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  GenomeMutations m;

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::InvalidMutation);
  REQUIRE(system_prompt == "initial");
  REQUIRE(tools.empty());
}

TEST_CASE("C4 case-0b: apply_harness_mutation add/remove same name → InvalidMutation",
          "[c4][harness-rsi][case-0b]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{}
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  GenomeMutations m;
  m.tools_add = {"trusted_tool"};
  m.tools_remove = {"trusted_tool"};

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::InvalidMutation);
}

// ============================================================================
// Case 4: workflow_patch → UnsupportedVariant
// ============================================================================
TEST_CASE("C4 case-4: apply_harness_mutation workflow_patch → UnsupportedVariant",
          "[c4][harness-rsi][case-4]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{}
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  GenomeMutations m;
  m.workflow_patch = std::string("some workflow");

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::UnsupportedVariant);
  REQUIRE(system_prompt == "initial");
  REQUIRE(tools.empty());
}

// ============================================================================
// Case 5a: remove denied tool → GovernanceDenied + 零状态变更 (AC-1)
// ============================================================================
TEST_CASE("C4 case-5a: apply_harness_mutation remove denied tool → GovernanceDenied",
          "[c4][harness-rsi][case-5a]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  // "trusted_tool" IS in StubToolRegistry, but policy denies it
  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{denied_tools: {"trusted_tool"}}
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools = {"trusted_tool"};
  GenomeMutations m;
  m.tools_remove = {"trusted_tool"};

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  // GovernanceDenied + 零状态变更
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::GovernanceDenied);
  // system_prompt 未变
  REQUIRE(system_prompt == "initial");
  // registry 中工具仍在（未被 unregister）
  REQUIRE(registry.has_tool("trusted_tool"));
  // tools 向量中工具仍在
  REQUIRE(tools.size() == 1);
  REQUIRE(tools[0] == "trusted_tool");
}

// ============================================================================
// Case 5b: remove trusted tool → success + registry unregister (AC-2)
// ============================================================================
TEST_CASE("C4 case-5b: apply_harness_mutation remove trusted tool → success",
          "[c4][harness-rsi][case-5b]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{}  // empty policy: no denied_tools
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools = {"trusted_tool"};
  GenomeMutations m;
  m.tools_remove = {"trusted_tool"};

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  REQUIRE(result.has_value());
  // registry 中工具已移除（unregister 成功）
  REQUIRE_FALSE(registry.has_tool("trusted_tool"));
  // tools 向量中工具已移除
  REQUIRE(tools.empty());
}

// ============================================================================
// Case 5c: add denied + remove denied 混合 → GovernanceDenied + 零状态变更 (AC-3)
// ============================================================================
TEST_CASE("C4 case-5c: apply_harness_mutation add+remove mixed denied → GovernanceDenied",
          "[c4][harness-rsi][case-5c]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  // policy denies both "dangerous_tool" (add) and "trusted_tool" (remove)
  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{denied_tools: {"dangerous_tool", "trusted_tool"}}
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools = {"trusted_tool"};
  GenomeMutations m;
  m.tools_add = {"dangerous_tool"};
  m.tools_remove = {"trusted_tool"};

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::GovernanceDenied);
  // 零状态变更: 两个路径都未应用
  REQUIRE(system_prompt == "initial");
  REQUIRE(tools.size() == 1);
  REQUIRE(tools[0] == "trusted_tool");
  REQUIRE(registry.has_tool("trusted_tool"));
  REQUIRE_FALSE(registry.has_tool("dangerous_tool"));
}

// ============================================================================
// Case 6: trace_id 透传断言 (AC-4)
// ============================================================================
TEST_CASE("C4 case-6: apply_harness_mutation trace_id from ctx → event meta",
          "[c4][harness-rsi][case-6]") {
  using namespace agenticdsl::evolution::testing;

  // 触发 readiness denied (poor evaluator)
  AttributionRecord attr;
  attr.verdict = AttributionVerdict::NotAttempted;
  StubEvaluatorPoor evaluator;
  StubBudgetExceeded budget;
  CapturingBus bus;
  StubToolRegistry registry;

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{},
      "trace-abc-123"  // explicit trace_id
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  GenomeMutations m;
  m.prompt_delta = "Be concise.";

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::NotReady);
  REQUIRE(bus.captured.size() == 1);
  REQUIRE(bus.captured[0].topic == "evolution.readiness.denied");
  // trace_id 从 ctx 透传到 event meta
  REQUIRE(bus.captured[0].meta.contains("trace_id"));
  REQUIRE(bus.captured[0].meta["trace_id"] == "trace-abc-123");
}

// ============================================================================
// Case 3d: partial apply regression guard (Oracle bg_afa84d4d Critical-1)
// prompt_delta + unregistered tools_add → RegistryRejected + 零状态变更
// ============================================================================
TEST_CASE("C4 case-3d: partial apply prevention — prompt_delta + unregistered_add → Rejected",
          "[c4][harness-rsi][case-3d]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{}
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  GenomeMutations m;
  m.prompt_delta = "Be concise.";         // 合法 prompt delta
  m.tools_add = {"nonexistent_tool"};     // registry 中不存在

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::RegistryRejected);
  REQUIRE(system_prompt == "initial");
  REQUIRE(tools.empty());
}

// ============================================================================
// G4 Case 7a: Gate 3 persist success (AC-1) — 真实 FilesystemGenomeRegistry + fork
// ============================================================================
TEST_CASE("C4 case-7a: Gate 3 persist success with FilesystemGenomeRegistry",
          "[c4][harness-rsi][case-7a][g4]") {
  using namespace agenticdsl::evolution::testing;

  auto tmpdir = fs::temp_directory_path() / "hydraforge_g4_7a";
  fs::remove_all(tmpdir);
  auto genome_reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(tmpdir);
  REQUIRE(genome_reg != nullptr);

  // Seed root v1
  agenticdsl::genome::Genome root;
  root.metadata.name = "chat_harness";
  root.metadata.created_by = "test";
  root.metadata.created_at = "2026-09-22T00:00:00Z";
  root.metadata.capture_mode = "mock";
  root.spec.harness = "You are helpful.";
  root.spec.tools = {"trusted_tool"};
  auto root_res = genome_reg->commit(root);
  REQUIRE(root_res.has_value());
  REQUIRE(root_res.value().version == 1);

  // Mutation setup
  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry t_reg;

  MutationGateContext ctx{
    EvolutionState::Data,
    &attr, &evaluator, &budget, &bus,
    MutationGovernancePolicy{},
    "trace-g4-7a",
    genome_reg.get(),
    "chat_harness",
    1
  };

  std::string system_prompt = "You are helpful.";
  std::vector<std::string> tools = {"trusted_tool"};
  GenomeMutations m;
  m.prompt_delta = " Be concise.";

  auto result = apply_harness_mutation(m, system_prompt, tools, t_reg, ctx);

  REQUIRE(result.has_value());
  REQUIRE(result.value().committed_genome_version > 0);

  // Load back and verify
  uint64_t saved_version = result.value().committed_genome_version;
  auto loaded = genome_reg->load("chat_harness", saved_version);
  REQUIRE(loaded.has_value());

  // genome.committed event
  bool found_committed = false;
  for (const auto& c : bus.captured) {
    if (c.topic == "genome.committed") {
      found_committed = true;
    }
  }
  REQUIRE(found_committed);

  fs::remove_all(tmpdir);
}

// ============================================================================
// G4 Case 7b: Gate 0 workflow_patch 上移拒绝 (AC-2 顺序守卫)
// ============================================================================
TEST_CASE("C4 case-7b: workflow_patch → UnsupportedVariant at Gate 0 (before Gate 1)",
          "[c4][harness-rsi][case-7b][g4]") {
  using namespace agenticdsl::evolution::testing;

  auto tmpdir = fs::temp_directory_path() / "hydraforge_g4_7b";
  fs::remove_all(tmpdir);
  auto genome_reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(tmpdir);

  // Seed root to make fork available
  agenticdsl::genome::Genome root;
  root.metadata.name = "chat_harness";
  root.metadata.created_by = "test";
  root.metadata.created_at = "2026-09-22T00:00:00Z";
  root.metadata.capture_mode = "mock";
  root.spec.harness = "initial";
  root.spec.tools = {};
  auto root_res = genome_reg->commit(root);
  REQUIRE(root_res.has_value());
  REQUIRE(root_res.value().version == 1);

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry t_reg;

  MutationGateContext ctx{
    EvolutionState::Data,
    &attr, &evaluator, &budget, &bus,
    MutationGovernancePolicy{},
    "trace-g4-7b",
    genome_reg.get(),
    "chat_harness",
    1
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  GenomeMutations m;
  m.workflow_patch = std::string("some workflow");
  m.prompt_delta = " should not apply";

  auto result = apply_harness_mutation(m, system_prompt, tools, t_reg, ctx);

  // UnsupportedVariant at Gate 0 — before Gate 1, before Gate 3 fork
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::UnsupportedVariant);
  // Zero state change
  REQUIRE(system_prompt == "initial");
  REQUIRE(tools.empty());
  // No genome.* event (fork never called)
  bool found_genome_event = false;
  for (const auto& c : bus.captured) {
    if (c.topic.rfind("genome.", 0) == 0) found_genome_event = true;
  }
  REQUIRE_FALSE(found_genome_event);

  fs::remove_all(tmpdir);
}

// ============================================================================
// G4 Case 7c: Gate 3 fork failure → RegistryRejected (AC-3)
// ============================================================================
TEST_CASE("C4 case-7c: Gate 3 fork failure → RegistryRejected + zero state change",
          "[c4][harness-rsi][case-7c][g4]") {
  using namespace agenticdsl::evolution::testing;

  auto tmpdir = fs::temp_directory_path() / "hydraforge_g4_7c";
  fs::remove_all(tmpdir);
  auto genome_reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(tmpdir);

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry t_reg;

  // parent_version 指向不存在的版本 → fork returns NotFound
  MutationGateContext ctx{
    EvolutionState::Data,
    &attr, &evaluator, &budget, &bus,
    MutationGovernancePolicy{},
    "trace-g4-7c",
    genome_reg.get(),
    "chat_harness",
    99  // nonexistent parent version
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools = {"trusted_tool"};
  GenomeMutations m;
  m.prompt_delta = " should not apply";

  auto result = apply_harness_mutation(m, system_prompt, tools, t_reg, ctx);

  // RegistryRejected + zero state change
  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::RegistryRejected);
  REQUIRE(system_prompt == "initial");
  REQUIRE(tools.size() == 1);
  REQUIRE(tools[0] == "trusted_tool");  // 零状态变更

  // genome.persist_failed event
  bool found_failed = false;
  for (const auto& c : bus.captured) {
    if (c.topic == "genome.persist_failed") {
      found_failed = true;
    }
  }
  REQUIRE(found_failed);

  fs::remove_all(tmpdir);
}

// ============================================================================
// G4 Case 7d: parent_version=0 → InvalidMutation (AC-4 root guard)
// ============================================================================
TEST_CASE("C4 case-7d: parent_version=0 → InvalidMutation + zero events",
          "[c4][harness-rsi][case-7d][g4]") {
  using namespace agenticdsl::evolution::testing;

  auto tmpdir = fs::temp_directory_path() / "hydraforge_g4_7d";
  fs::remove_all(tmpdir);
  auto genome_reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(tmpdir);

  MutationGateContext ctx{
    EvolutionState::Data,
    nullptr, nullptr, nullptr, nullptr,
    MutationGovernancePolicy{}
  };
  // 不设 genome_registry — test 纯 parent_version=0 拦截

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  GenomeMutations m;
  m.prompt_delta = " should not apply";

  StubToolRegistry t_reg;
  auto result = apply_harness_mutation(m, system_prompt, tools, t_reg, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::InvalidMutation);

  fs::remove_all(tmpdir);
}

// ============================================================================
// G4 Case 7e: tools 最终态为空 → InvalidMutation (V1 边界)
// ============================================================================
TEST_CASE("C4 case-7e: final tools empty → InvalidMutation + zero state change",
          "[c4][harness-rsi][case-7e][g4]") {
  using namespace agenticdsl::evolution::testing;

  auto tmpdir = fs::temp_directory_path() / "hydraforge_g4_7e";
  fs::remove_all(tmpdir);
  auto genome_reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(tmpdir);

  agenticdsl::genome::Genome root;
  root.metadata.name = "chat_harness";
  root.metadata.created_by = "test";
  root.metadata.created_at = "2026-09-22T00:00:00Z";
  root.metadata.capture_mode = "mock";
  root.spec.harness = "initial";
  root.spec.tools = {"tool_a"};
  auto root_res = genome_reg->commit(root);
  REQUIRE(root_res.has_value());
  REQUIRE(root_res.value().version == 1);

  // 有效 stubs — 让流程通过 Gate 0/1/2，真正触达 Gate 3 的 final-tools-empty 检查
  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  MutationGateContext ctx{
    EvolutionState::Data,
    &attr, &evaluator, &budget, &bus,
    MutationGovernancePolicy{},
    "trace-g4-7e",
    genome_reg.get(),
    "chat_harness",
    1
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools = {"tool_a"};
  StubToolRegistry t_reg;
  GenomeMutations m;
  m.tools_remove = {"tool_a"};

  auto result = apply_harness_mutation(m, system_prompt, tools, t_reg, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::InvalidMutation);
  // 零状态变更
  REQUIRE(system_prompt == "initial");
  REQUIRE(tools.size() == 1);
  REQUIRE(tools[0] == "tool_a");

  fs::remove_all(tmpdir);
}

// ============================================================================
// G4 Case 7f: parent_version=0 with registry → InvalidMutation
// ============================================================================
TEST_CASE("C4 case-7f: parent_version=0 with registry → InvalidMutation",
          "[c4][harness-rsi][case-7f][g4]") {
  using namespace agenticdsl::evolution::testing;

  auto tmpdir = fs::temp_directory_path() / "hydraforge_g4_7f";
  fs::remove_all(tmpdir);
  auto genome_reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(tmpdir);

  MutationGateContext ctx{
    EvolutionState::Data,
    nullptr, nullptr, nullptr, nullptr,
    MutationGovernancePolicy{},
    "",
    genome_reg.get(),
    "chat_harness",
    0  // parent_version = 0
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  StubToolRegistry t_reg;
  GenomeMutations m;
  m.prompt_delta = " should not apply";

  auto result = apply_harness_mutation(m, system_prompt, tools, t_reg, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::InvalidMutation);
  REQUIRE(system_prompt == "initial");

  fs::remove_all(tmpdir);
}

// ============================================================================
// G4 Case 7g: workflow_patch + registry → UnsupportedVariant (Gate 0 order guard)
// ============================================================================
TEST_CASE("C4 case-7g: workflow_patch + registry → UnsupportedVariant at Gate 0 (no disk version)",
          "[c4][harness-rsi][case-7g][g4]") {
  using namespace agenticdsl::evolution::testing;

  auto tmpdir = fs::temp_directory_path() / "hydraforge_g4_7g";
  fs::remove_all(tmpdir);
  auto genome_reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(tmpdir);

  agenticdsl::genome::Genome root;
  root.metadata.name = "chat_harness";
  root.metadata.created_by = "test";
  root.metadata.created_at = "2026-09-22T00:00:00Z";
  root.metadata.capture_mode = "mock";
  root.spec.harness = "initial";
  auto root_res = genome_reg->commit(root);
  REQUIRE(root_res.has_value());

  MutationGateContext ctx{
    EvolutionState::Data,
    nullptr, nullptr, nullptr, nullptr,
    MutationGovernancePolicy{},
    "",
    genome_reg.get(),
    "chat_harness",
    1
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  StubToolRegistry t_reg;
  GenomeMutations m;
  m.workflow_patch = std::string("some workflow");

  auto result = apply_harness_mutation(m, system_prompt, tools, t_reg, ctx);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == MutationError::UnsupportedVariant);
  REQUIRE(system_prompt == "initial");

  // Verify no disk version beyond root v1 was created
  auto versions = genome_reg->list_versions("chat_harness");
  REQUIRE(versions.has_value());
  REQUIRE(versions.value().size() == 1);
  REQUIRE(versions.value()[0] == 1);

  fs::remove_all(tmpdir);
}

// ============================================================================
// G4 Case 7h: undo_applied_mutation 恢复 tools_snapshot (AC-5)
// ============================================================================
TEST_CASE("C4 case-7h: undo_applied_mutation restores prompt+tools from AppliedMutation snapshot",
          "[c4][harness-rsi][case-7h][g4]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry t_reg;

  MutationGateContext ctx{
    EvolutionState::Data,
    &attr, &evaluator, &budget, &bus,
    MutationGovernancePolicy{}
  };

  std::string system_prompt = "You are helpful.";
  std::vector<std::string> tools = {"trusted_tool"};
  GenomeMutations m;
  m.prompt_delta = " Be concise.";
  m.tools_add = {"another_tool"};
  // Gate 2.5 要求 tools_add 的 tool 已在 registry 注册
  t_reg.register_tool_function("another_tool", agenticdsl::ToolMetadata{},
                               agenticdsl::IToolRegistry::ToolFunc{});

  auto result = apply_harness_mutation(m, system_prompt, tools, t_reg, ctx);
  REQUIRE(result.has_value());

  // Verify mutation applied
  std::string applied_prompt = system_prompt;
  std::vector<std::string> applied_tools = tools;

  // Undo — restore from snapshot
  std::string restored_prompt = system_prompt;
  std::vector<std::string> restored_tools = tools;
  undo_applied_mutation(result.value(), restored_prompt, restored_tools, &t_reg);

  // Undo should restore to pre-mutation state
  REQUIRE(restored_prompt == "You are helpful.");
  REQUIRE(restored_tools.size() == 1);
  REQUIRE(restored_tools[0] == "trusted_tool");

  // V1 limitation: registry has_tool for removed stays false
  if (!m.tools_remove.empty()) {
    REQUIRE_FALSE(t_reg.has_tool(m.tools_remove[0]));
  }
}

// ============================================================================
// Case 5d (Oracle bg_8237a316 SHIP-with-fixes Major #1): Spec R1 scenario 3
// "remove 不存在的工具" → 幂等 no-op + applied_tools_removed 记录 ("请求移除")
// ============================================================================
TEST_CASE("C4 case-5d: apply_harness_mutation remove 不存在工具 → success no-op + applied record",
          "[c4][harness-rsi][case-5d][oracle-ship-with-fixes]") {
  using namespace agenticdsl::evolution::testing;

  AttributionRecord attr;
  attr.verdict = AttributionVerdict::Attributed;
  StubEvaluatorExcellent evaluator;
  StubBudgetOk budget;
  CapturingBus bus;
  StubToolRegistry registry;  // 只含 {"trusted_tool"}

  MutationGateContext ctx{
      EvolutionState::Data,
      &attr, &evaluator, &budget, &bus,
      MutationGovernancePolicy{}  // 空 policy, 不阻止 remove
  };

  std::string system_prompt = "initial";
  std::vector<std::string> tools;
  GenomeMutations m;
  m.tools_remove = {"nonexistent_tool"};  // 不在 registry → 幂等 no-op

  auto result = apply_harness_mutation(m, system_prompt, tools, registry, ctx);

  // 断言 1: 返回 success (unregister 对不存在工具为幂等)
  REQUIRE(result.has_value());
  // 断言 2: applied_tools_removed 记录 "nonexistent_tool"
  //   (spec R1 scenario 3 "请求移除" 而非 "实际移除" 语义)
  REQUIRE(result.value().applied_tools_removed.size() == 1);
  REQUIRE(result.value().applied_tools_removed[0] == "nonexistent_tool");
  // 断言 3: 零状态变更 (system_prompt 不变)
  REQUIRE(system_prompt == "initial");
  // 断言 4: 原有 "trusted_tool" 不受影响 (assertion by side-effect)
  REQUIRE(registry.has_tool("trusted_tool"));
  // 断言 5: nonexistent_tool 仍不存在 (写入仍然幂等)
  REQUIRE_FALSE(registry.has_tool("nonexistent_tool"));
}
