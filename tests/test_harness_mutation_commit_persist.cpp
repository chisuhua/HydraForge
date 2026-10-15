// tests/test_harness_mutation_commit_persist.cpp
// Harness mutation commit + IGenomeRegistry persist 确定性集成测试
// (per AGENTS.md Pattern #10 hygiene + Oracle SHIP-with-fixes 2026-10-15 评审)
//
// 来源: openspec/changes/2026-10-15-harness-mutation-commit-real-llm/ (M1 路径 A 修正)
//
// 历史:
//   - 2026-10-15 P2 commit 7af59b7: 初版 4 cases redesign (must_realllm 误标)
//   - 2026-10-15 P2.5 Oracle SHIP-with-fixes: 4 Major + 6 Minor 评审
//   - 2026-10-15 P2.5 修正: M1 路径 A 实施 (移除 must_realllm, 重命名为集成测试)
//
// 4 cases 全部 redesign (per dual-agent 2026-10-14 调研共识):
//   Case 1: commit 成功路径 — state consistency assertion (per D1, 非固定路径)
//   Case 2: commit 事件流 — payload 3 字段断言 (per M3 + ADR-0068 Appendix A v2.3)
//   Case 3: gate denied 确定性回滚 — 3 iter 100% consistency (per D2, 非 LLM 输出依赖)
//   Case 4: drop_ratio descope (per D3, 物理不可行)
//
// 实施修正 (per Oracle M1 路径 A + M2-M4 + Minor 1-6):
//   - 移除 must_realllm label (M1): 本测试不调 LLM, 标 must_realllm 是反模式 (per AGENTS.md 2026-09-29)
//   - 移除 must_require_real_llm_env() 门 + 死分支 (M1): 无 LLM 调用则不需要 env 门
//   - 删除 RecordingLLMProvider fixture (M2 + Minor 3): 死代码, 0 实例化
//   - 删除 generate_prompt_delta_via_real_llm 死代码 (Minor 3): 0 调用
//   - 删除 generate_uuid_v4 重实现 (Minor 3): 不依赖 hermetic 内 uuid 路径
//   - 删 CapturingBus::wait_for_drain 假 drain 守卫 (Minor 1): stub emit 同步, 无 race
//   - Case 2 补 payload 3 字段断言 (M3): ADR-0068 Appendix A v2.3 契约
//   - Case 3 接 CapturingBus 验证 genome.committed 不触发 (M4): 校正 spec 事件契约
//
// 4 cases 全部 deterministic (不依赖 LLM), Recording Provider 守卫模式不适用本集成测试
// (M2 descope per Case 4 descope 先例, 需真 LLM 守卫的 test 是 H2/H3/H4 等独立 must_realllm 范围,
//  属 G5/rsi-anti-cheat-real-llm 等独立 change 范围)
//
// tag: [harness_rsi][genome][persist][integration]

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "catch_amalgamated.hpp"

#include "agenticdsl/evolution/harness_rsi.h"
#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/genome/genome.h"
#include "agenticdsl/types/attribution_record.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"
#include "common/llm/llm_types.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/llm_config.h"
#include "modules/budget/budget_controller.h"

namespace {

// ============================================================================
// HermeticGenomeHome fixture (per 2026-09-21 C1 genome.key fresh-deploy 教训)
// 隔离 ~/.hydraforge/genome.key 宿主状态
// ============================================================================

class HermeticGenomeHome {
 public:
  HermeticGenomeHome() {
    if (auto* h = std::getenv("HOME")) saved_home_ = h;
    if (auto* h = std::getenv("HYDRAFORGE_HOME")) saved_hydraforge_home_ = h;
    auto tmpdir = std::filesystem::temp_directory_path() /
                  ("hermetic_" +
                   std::to_string(std::random_device()()) + "_" +
                   std::to_string(std::chrono::system_clock::now()
                                      .time_since_epoch()
                                      .count()));
    std::filesystem::create_directories(tmpdir);
    setenv("HOME", tmpdir.c_str(), 1);
    setenv("HYDRAFORGE_HOME", tmpdir.c_str(), 1);
    tmpdir_ = tmpdir;
  }

  ~HermeticGenomeHome() {
    if (!saved_home_.empty()) {
      setenv("HOME", saved_home_.c_str(), 1);
    } else {
      unsetenv("HOME");
    }
    if (!saved_hydraforge_home_.empty()) {
      setenv("HYDRAFORGE_HOME", saved_hydraforge_home_.c_str(), 1);
    } else {
      unsetenv("HYDRAFORGE_HOME");
    }
    std::error_code ec;
    std::filesystem::remove_all(tmpdir_, ec);
  }

  HermeticGenomeHome(const HermeticGenomeHome&) = delete;
  HermeticGenomeHome& operator=(const HermeticGenomeHome&) = delete;

  const std::filesystem::path& path() const { return tmpdir_; }

 private:
  std::filesystem::path tmpdir_;
  std::string saved_home_;
  std::string saved_hydraforge_home_;
};

// ============================================================================
// CapturingBus stub (per test_harness_rsi_pilot.cpp pattern, 拦截 events)
// (Minor 1: 移除 wait_for_drain 假守卫 — stub emit 同步, 无 race, InMemoryBus
//  异步派发路径不在本 stub 覆盖范围, Pattern #9 contract 仅适用于真 InMemoryBus)
// ============================================================================

class CapturingBus : public agenticdsl::IInteractionBus {
 public:
  struct CapturedEvent {
    std::string topic;
    nlohmann::json data;
    nlohmann::json meta;
  };
  std::vector<CapturedEvent> captured;

  void emit(const agenticdsl::BusEvent& event) override {
    CapturedEvent e;
    e.topic = event.topic;
    e.data = event.payload.data;
    e.meta = event.payload.meta;
    captured.push_back(e);
  }
  void emit(const std::string&, const std::string&) override {}
  size_t subscribe(const std::string&,
                  std::function<void(const agenticdsl::BusEvent&)>) override { return 0; }
  void unsubscribe(size_t) override {}
};

// ============================================================================
// Stub 依赖 (per test_harness_mutation_proposal_real_llm + test_harness_rsi_pilot pattern)
// ============================================================================

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

// StubBudgetOk (per test_harness_rsi_pilot.cpp:74- pattern, derive from concrete BudgetController)
class StubBudgetOk : public agenticdsl::BudgetController {
 public:
  StubBudgetOk() : agenticdsl::BudgetController(std::nullopt), mock_exceeded(false) {}
  bool exceeded() const override { return mock_exceeded; }
  bool try_consume_node() override { return !mock_exceeded; }
  bool try_consume_llm_call() override { return !mock_exceeded; }
  bool try_consume_subgraph_depth() override { return !mock_exceeded; }
  bool try_consume_evolution_llm_call() override { return !mock_exceeded; }
  void set_termination_target(const agenticdsl::NodePath&) override {}

 private:
  bool mock_exceeded;
};

// 辅助: 构造 initial Genome (per spec.md Case 1 初始化)
agenticdsl::genome::Genome make_initial_genome(
    const std::string& name, uint64_t version,
    const std::string& prompt, const std::vector<std::string>& tools) {
  agenticdsl::genome::Genome g;
  g.metadata.name = name;
  g.metadata.version = version;
  g.metadata.capture_mode = "mock";  // 初始 fixture
  g.spec.harness = prompt;
  g.spec.tools = tools;
  return g;
}

}  // namespace

// ============================================================================
// Case 1: commit 成功路径 — state consistency assertion (per D1)
// ============================================================================

TEST_CASE("Harness mutation commit 集成测试 - Case 1: state consistency",
          "[harness_rsi][genome][persist][integration][case1]") {
  HermeticGenomeHome hermetic;  // 隔离 ~/.hydraforge/genome.key (per D5)

  // === 确定性 prompt_delta (本集成测试不调 LLM, 必须_realllm 误标已移除 per M1 路径 A) ===
  std::string prompt_delta = "Be concise and explicit about your reasoning.";

  std::string system_prompt = "You are a coding agent.";
  std::vector<std::string> tools = {"bash", "read_file"};

  agenticdsl::evolution::AttributionRecord attr = make_attributed_record();
  StubAcceptableEvaluator evaluator;
  StubBudgetOk budget;
  StubToolRegistry registry;
  registry.registered = {"bash", "read_file"};

  // === ACT: apply_harness_mutation (含 Gate 0-3 真实持久化) ===
  agenticdsl::evolution::GenomeMutations mut;
  mut.prompt_delta = prompt_delta;

  auto genome_registry = agenticdsl::genome::IGenomeRegistry::create_filesystem(
      hermetic.path() / "genome_root");

  auto initial = make_initial_genome("test_genome", 0, system_prompt, tools);
  auto commit0 = genome_registry->commit(initial);
  REQUIRE(commit0.has_value());
  uint64_t parent_version = commit0.value().version;

  agenticdsl::evolution::MutationGateContext ctx;
  ctx.current = agenticdsl::evolution::EvolutionState::Harness;
  ctx.attribution = &attr;
  ctx.evaluator = &evaluator;
  ctx.budget = &budget;  // StubBudgetOk
  ctx.bus = nullptr;  // Case 1 不需要事件验证 (Case 2 验证)
  ctx.policy = {};
  ctx.trace_id = "case1_state_consistency";
  ctx.genome_registry = genome_registry.get();
  ctx.genome_name = "test_genome";
  ctx.parent_version = parent_version;

  auto result = agenticdsl::evolution::apply_harness_mutation(
      mut, system_prompt, tools, registry, ctx);

  // === ASSERT (per D1 state consistency, 非固定路径) ===
  INFO("Apply result: has_value=" << result.has_value());
  if (!result.has_value()) {
    INFO("Mutation error code: " << static_cast<int>(result.error()));
  }
  REQUIRE(result.has_value());
  REQUIRE(result.value().committed_genome_version > parent_version);

  auto loaded = genome_registry->load("test_genome",
                                      result.value().committed_genome_version);
  REQUIRE(loaded.has_value());
  CHECK(loaded.value().spec.harness == system_prompt);
  CHECK(loaded.value().spec.tools == tools);
}

// ============================================================================
// Case 2: commit 事件流 — payload 3 字段断言 (per M3 + ADR-0068 Appendix A v2.3)
// ============================================================================

TEST_CASE("Harness mutation commit 集成测试 - Case 2: 事件流 + payload 3 字段",
          "[harness_rsi][genome][persist][integration][case2]") {
  HermeticGenomeHome hermetic;

  std::string prompt_delta = "Be concise and explicit about your reasoning.";

  std::string system_prompt = "You are a coding agent.";
  std::vector<std::string> tools = {"bash", "read_file"};

  agenticdsl::evolution::AttributionRecord attr = make_attributed_record();
  StubAcceptableEvaluator evaluator;
  StubBudgetOk budget;
  StubToolRegistry registry;
  registry.registered = {"bash", "read_file"};

  auto genome_registry = agenticdsl::genome::IGenomeRegistry::create_filesystem(
      hermetic.path() / "genome_root");
  auto initial = make_initial_genome("test_genome", 0, system_prompt, tools);
  auto commit0 = genome_registry->commit(initial);
  REQUIRE(commit0.has_value());

  // CapturingBus 收集 genome.committed 事件 (per M4 校正 spec 事件契约)
  CapturingBus bus;

  agenticdsl::evolution::GenomeMutations mut;
  mut.prompt_delta = prompt_delta;

  agenticdsl::evolution::MutationGateContext ctx;
  ctx.current = agenticdsl::evolution::EvolutionState::Harness;
  ctx.attribution = &attr;
  ctx.evaluator = &evaluator;
  ctx.budget = &budget;
  ctx.bus = &bus;
  ctx.policy = {};
  ctx.trace_id = "case2_event_flow";
  ctx.genome_registry = genome_registry.get();
  ctx.genome_name = "test_genome";
  ctx.parent_version = commit0.value().version;

  auto result = agenticdsl::evolution::apply_harness_mutation(
      mut, system_prompt, tools, registry, ctx);
  REQUIRE(result.has_value());

  // === ASSERT: genome.committed 事件 MUST present + payload 字段断言 (per M3 + ADR-0068 Appendix A v2.3 实际 emit 字段) ===
  // 实际 emit (src/evolution/harness_rsi.cpp:221-228):
  //   args: {"genome_name", "version", "parent", "mutation_kind"}
  //   meta: {"trace_id"}
  bool found_committed = false;
  for (const auto& c : bus.captured) {
    if (c.topic == "genome.committed") {
      found_committed = true;
      INFO("genome.committed payload keys: " << c.data.dump().substr(0, 200));
      // M3 修正: payload 4 字段断言 (per src/evolution/harness_rsi.cpp:225-228 实际 emit)
      CHECK(c.data.contains("genome_name"));
      CHECK(c.data.contains("version"));
      CHECK(c.data.contains("parent"));
      CHECK(c.data.contains("mutation_kind"));
      // trace_id 在 meta
      CHECK(c.meta.contains("trace_id"));
    }
  }
  CHECK(found_committed);

  auto walk = genome_registry->walk_ancestors(
      "test_genome", result.value().committed_genome_version);
  REQUIRE(walk.has_value());
  CHECK(walk.value().intermediate_versions.size() >= 2);
}

// ============================================================================
// Case 3: gate denied 确定性回滚 (per D2 + M4 校正 spec 事件契约)
// 关键: 用确定性非法 input (缺必需字段), 非依赖 LLM 输出
// ============================================================================

TEST_CASE("Harness mutation commit 集成测试 - Case 3: 确定性 denied 回滚",
          "[harness_rsi][genome][persist][integration][case3]") {
  // === 关键: Case 3 跑 3 iter 验证 100% consistency (零 flaky, per R1 Scenario) ===
  for (int iter = 0; iter < 3; ++iter) {
    HermeticGenomeHome hermetic;
    std::string system_prompt = "You are a coding agent.";
    std::vector<std::string> tools = {"bash", "read_file"};

    agenticdsl::evolution::AttributionRecord attr = make_attributed_record();
    StubAcceptableEvaluator evaluator;
    StubBudgetOk budget;
    StubToolRegistry registry;
    registry.registered = {"bash", "read_file"};

    // M4 修正: Case 3 接 CapturingBus 验证 genome.committed 不触发
    CapturingBus bus;

    auto genome_registry =
        agenticdsl::genome::IGenomeRegistry::create_filesystem(
            hermetic.path() / "genome_root");
    auto initial = make_initial_genome("test_genome", 0, system_prompt, tools);
    auto commit0 = genome_registry->commit(initial);
    REQUIRE(commit0.has_value());
    uint64_t parent_version_before = commit0.value().version;

    // === ACT: 确定性非法 input (缺 tools_add 和 prompt_delta) ===
    agenticdsl::evolution::GenomeMutations bad_mut;  // 全部空

    agenticdsl::evolution::MutationGateContext ctx;
    ctx.current = agenticdsl::evolution::EvolutionState::Harness;
    ctx.attribution = &attr;
    ctx.evaluator = &evaluator;
    ctx.budget = &budget;
    ctx.bus = &bus;  // M4 修正: 接 bus 验证事件
    ctx.policy = {};
    ctx.trace_id = "case3_denied_iter" + std::to_string(iter);
    ctx.genome_registry = genome_registry.get();
    ctx.genome_name = "test_genome";
    ctx.parent_version = parent_version_before;

    auto result = agenticdsl::evolution::apply_harness_mutation(
        bad_mut, system_prompt, tools, registry, ctx);

    // === ASSERT (per R4 Scenario + M4 校正) ===
    CHECK_FALSE(result.has_value());
    if (!result.has_value()) {
      INFO("Iter " << iter << " Mutation error: "
           << static_cast<int>(result.error()));
    }

    // M4 校正: genome.committed MUST NOT 触发 (denied 路径零状态变更)
    bool found_committed = false;
    for (const auto& c : bus.captured) {
      if (c.topic == "genome.committed") {
        found_committed = true;
      }
    }
    CHECK_FALSE(found_committed);

    // IGenomeRegistry 零状态变更: list_versions 仍只含 parent_version
    auto versions = genome_registry->list_versions("test_genome");
    REQUIRE(versions.has_value());
    CHECK(versions.value().size() == 1);
    CHECK(versions.value()[0] == parent_version_before);
  }
}

// ============================================================================
// Case 4: drop_ratio descope (per D3 提案 4 spec.md R5)
// 验证 descope rationale 而非 drop_ratio 断言
// ============================================================================

TEST_CASE("Harness mutation commit 集成测试 - Case 4: drop_ratio descope ack",
          "[harness_rsi][genome][persist][integration][case4]") {
  // === Case 4 descope rationale ack (per 2026-10-06 l2-evolution-real-llm-provider C4) ===
  // L2 1-2 contexts 粒度物理不可行 (0/50/100% 二值, 5% 阈值无法稳定构造)
  // 需 ≥20 mutations 才能稳定触发
  // 红线验证属独立 OpenSpec change (per .rddf/audits/2026-10-14-rddf-improvements-audit.md)
  //
  // 本 test 仅 ack descope, NOT 实施 drop_ratio 断言
  SUCCEED("Case 4 descope ack: drop_ratio 红线 5% 在 L2 1-2 contexts 物理不可行, "
          "属独立 R8.1 红线 触发 验证 change. 详见提案 4 spec.md R5.");
}
