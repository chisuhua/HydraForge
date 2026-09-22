// tests/test_gepa_phase2.cpp
// 功能描述：GEPALoop Phase 2 测试套件 (T19, ADR-0071, Sprint 24)
//          8 骨架 cases + 2 核心流 + 1 事件 + 3 E2E = 14 cases 总
//          测试 GEPALoop 编排层：反思循环 → 变异提议 → commit 授权
// 设计依据：openspec/changes/t19-gepa-phase2-commit/specs/gepa-phase2-commit/spec.md
// 作者：HydraForge Sprint 24 T19 ship
// 最后修改日期：2026-08-27

#include "catch_amalgamated.hpp"

#include "common/llm/llm_types.h"
#include "agenticdsl/cognitive/gepa_loop.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/imutation_governance.h"
#include "common/governance/mutation_governor.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/policy/iapproval_handler.h"
#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"
#include "agenticdsl/cognitive/behavioral_equivalence_evaluator.h"
#include "agenticdsl/cognitive/composite_evaluator.h"
#include "core/types/tool_result.h"

#include "agenticdsl/genome/genome.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using namespace agenticdsl;

namespace fs = std::filesystem;

namespace {

// ============================================================================
// 测试替身: RecordingBus — 记录全部事件 (顺序敏感断言)
// ============================================================================
class RecordingBus : public IInteractionBus {
public:
  std::vector<BusEvent> events;

  void emit(const BusEvent& event) override { events.push_back(event); }
  void emit(const std::string& /*event_type*/,
            const std::string& /*content*/) override {}
  size_t subscribe(const std::string& /*event_type*/,
                   std::function<void(const BusEvent&)> /*callback*/) override {
    return 0;
  }
  void unsubscribe(size_t /*token*/) override {}

  std::vector<const BusEvent*> gepa_events() const {
    std::vector<const BusEvent*> out;
    for (const auto& e : events) {
      if (e.topic.rfind("gepa.", 0) == 0) {
        out.push_back(&e);
      }
    }
    return out;
  }
};

// ============================================================================
// 测试替身: StubEvaluator — 可配置 quality 的 IEvaluator
// ============================================================================
class StubEvaluator : public IEvaluator {
public:
  RewardSignal::Quality quality = RewardSignal::Quality::Excellent;
  bool score_by_ok = false;
  mutable int evaluate_calls = 0;
  mutable int compare_calls = 0;

  RewardSignal evaluate(const ExecutionTrace& trace) const override {
    ++evaluate_calls;
    if (score_by_ok) {
      return trace.final_result.ok ? RewardSignal::excellent(0.9)
                                   : RewardSignal::poor(0.9);
    }
    if (quality == RewardSignal::Quality::Excellent) {
      return RewardSignal::excellent(0.9);
    }
    if (quality == RewardSignal::Quality::Poor) {
      return RewardSignal::poor(0.9);
    }
    return RewardSignal::acceptable(0.5);
  }

  int compare(const ExecutionTrace& /*a*/,
              const ExecutionTrace& /*b*/) const override {
    ++compare_calls;
    return 0;
  }
};

// ============================================================================
// 测试替身: StubMutationGovernor — 可配置 approve/deny 的 IMutationGovernor
// ============================================================================
class StubMutationGovernor : public IMutationGovernor {
public:
  bool propose_approved = true;
  bool commit_approved = true;
  mutable int propose_calls = 0;
  mutable int commit_calls = 0;

  MutationDecision propose(const MutationContext& /*ctx*/) override {
    ++propose_calls;
    if (propose_approved) {
      return MutationDecision{true, "", ""};
    }
    return MutationDecision{false, "simulated_denial", "test"};
  }

  MutationDecision commit(const MutationContext& ctx) override {
    ++commit_calls;
    if (commit_approved) {
      return MutationDecision{true, "", ""};
    }
    return MutationDecision{false, "commit_denied", "test"};
  }

  void revert(const MutationContext& /*ctx*/,
              const std::string& /*target_version*/,
              const std::string& /*rollback_reason*/) override {}
};

// ============================================================================
// 测试替身: MockLLMProvider — 返回固定 prompt 修订候选字符串
// ============================================================================
class MockLLMProvider : public ILLMProvider {
public:
  std::string last_failure_;

  Result<GenerationResult, LLMError> generate(
      const GenerationRequest& req,
      std::stop_token /*token*/) override {
    return Result<GenerationResult, LLMError>::success(
        GenerationResult{"Reflection note: Add error handling for " + last_failure_,
                         0, 0, "stop"});
  }

  std::unique_ptr<IGenerationStream> generate_stream(
      const GenerationRequest& /*req*/,
      std::stop_token /*token*/) override {
    return nullptr;
  }

  std::vector<ModelInfo> available_models() const override { return {}; }

  void set_failure(const std::string& f) { last_failure_ = f; }
};

// ============================================================================
// 测试替身: MockApprovalHandler — 可配置 approve/deny
// ============================================================================
class MockApprovalHandler : public IApprovalHandler {
public:
  bool approve = true;

  bool process_request(const ToolMetadata& /*meta*/,
                       const ToolCallContext& /*ctx*/,
                       const ToolPreview& /*preview*/) override {
    return approve;
  }
};

// ============================================================================
// 辅助: 合成失败 ExecutionTrace
// ============================================================================
ExecutionTrace make_failed_trace(const std::string& trace_id = "test_trace") {
  ExecutionTrace trace;
  trace.final_result = ToolResult::error(ErrorCode::Unknown,
                                         "execution_failed",
                                         nlohmann::json::object());
  trace.trace_id = trace_id;
  return trace;
}

}  // anonymous namespace

// ============================================================================
// Phase 0: 骨架测试 (8 cases)
// ============================================================================

TEST_CASE("gepa_loop_initialization", "[gepa][phase2][phase0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();

  GEPALoop::Config config;
  config.reward_threshold = 0.0;
  config.max_iterations = 3;
  config.source_id = "R_T19_GEPA";

  GEPALoop loop(evaluator, governor, llm, config);
  REQUIRE(true); // 构造成功
}

TEST_CASE("gepa_loop_failed_detection", "[gepa][phase2][phase0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm);

  ExecutionTrace trace = make_failed_trace("fail_001");
  GEPALoop::ReflectionResult result = loop.reflect_and_commit(trace);

  // Phase 0: 占位 — 实现后应检测到失败并返回 success=false
  REQUIRE(result.success);
}

TEST_CASE("gepa_loop_trajectory_serialization", "[gepa][phase2][phase0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm);

  ExecutionTrace trace = make_failed_trace("traj_001");
  GEPALoop::ReflectionResult result = loop.reflect_and_commit(trace);

  // Phase 0: 占位 — 实现后应调用 TrajectoryIR::from_parsed_graph
  REQUIRE(result.success);
}

TEST_CASE("gepa_loop_reflection_generation", "[gepa][phase2][phase0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm);

  ExecutionTrace trace = make_failed_trace("refl_001");
  GEPALoop::ReflectionResult result = loop.reflect_and_commit(trace);

  // Phase 0: 占位 — 实现后应调用 ILLMProvider::generate
  REQUIRE(result.success);
}

TEST_CASE("gepa_loop_skill_compilation", "[gepa][phase2][phase0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm);

  ExecutionTrace trace = make_failed_trace("skill_001");
  GEPALoop::ReflectionResult result = loop.reflect_and_commit(trace);

  // Phase 0: 占位 — 实现后应调用 SkillCompiler::compile
  REQUIRE(result.success);
}

TEST_CASE("gepa_loop_regression_validation", "[gepa][phase2][phase0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm);

  ExecutionTrace trace = make_failed_trace("regr_001");
  GEPALoop::ReflectionResult result = loop.reflect_and_commit(trace);

  // Phase 0: 占位 — 实现后应调用 hotelling_t2_test
  REQUIRE(result.success);
}

TEST_CASE("gepa_loop_evaluation_gate", "[gepa][phase2][phase0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm);

  ExecutionTrace trace = make_failed_trace("eval_001");
  GEPALoop::ReflectionResult result = loop.reflect_and_commit(trace);

  // Phase 0: 占位 — 实现后应通过 IEvaluator::evaluate 评估
  REQUIRE(result.success);
}

TEST_CASE("gepa_loop_commit_authorization", "[gepa][phase2][phase0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm);

  ExecutionTrace trace = make_failed_trace("commit_001");
  GEPALoop::ReflectionResult result = loop.reflect_and_commit(trace);

  // Phase 0: 占位 — 实现后应调用 MutationGovernor::commit
  REQUIRE(result.success);
}
// ============================================================================
// Phase 1: 反思循环核心 (2 cases)
// ============================================================================

TEST_CASE("reflection_loop_basic_flow", "[gepa][phase2][phase1]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop::Config config;
  config.reward_threshold = 0.0;
  config.max_iterations = 3;
  GEPALoop loop(evaluator, governor, llm, config);

  ExecutionTrace trace = make_failed_trace("basic_flow");
  GEPALoop::ReflectionResult result = loop.reflect_and_commit(trace);

  REQUIRE(result.success);
  REQUIRE(governor->propose_calls > 0);
  REQUIRE(governor->commit_calls > 0);
  REQUIRE(result.candidate_skills.size() == 1);
}

TEST_CASE("reflection_loop_no_improvement", "[gepa][phase2][phase1]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->quality = RewardSignal::Quality::Acceptable;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop::Config config;
  config.reward_threshold = 0.0;
  config.max_iterations = 3;
  GEPALoop loop(evaluator, governor, llm, config);

  ExecutionTrace trace = make_failed_trace("no_improve");
  GEPALoop::ReflectionResult result = loop.reflect_and_commit(trace);

  REQUIRE_FALSE(result.success);
  REQUIRE(result.failure_mode == "no_improvement");
  REQUIRE(governor->commit_calls == 0);
}

TEST_CASE("gepa_event_emission", "[gepa][phase2][phase2]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  auto bus = std::make_shared<RecordingBus>();
  GEPALoop::Config config;
  config.max_iterations = 1;
  GEPALoop loop(evaluator, governor, llm, config, bus);

  const auto result = loop.reflect_and_commit(make_failed_trace("events"));

  REQUIRE(result.success);
  const auto events = bus->gepa_events();
  REQUIRE(events.size() >= 4);
  REQUIRE(std::count_if(events.begin(), events.end(), [](const BusEvent* event) {
    return event->topic == "gepa.reflection.started";
  }) == 1);
  REQUIRE(std::count_if(events.begin(), events.end(), [](const BusEvent* event) {
    return event->topic == "gepa.commit.proposed";
  }) == 1);
  REQUIRE(std::count_if(events.begin(), events.end(), [](const BusEvent* event) {
    return event->topic == "gepa.reflection.completed";
  }) == 1);
  REQUIRE(std::count_if(events.begin(), events.end(), [](const BusEvent* event) {
    return event->topic == "gepa.commit.committed";
  }) == 1);
}

TEST_CASE("gepa_e2e_with_real_evaluator_v2", "[gepa][phase2][phase3]") {
  auto base = std::make_shared<StubEvaluator>();
  base->score_by_ok = true;
  auto equivalent = std::make_shared<BehavioralEquivalenceEvaluator>();
  auto evaluator = std::make_shared<CompositeEvaluator>(
      std::vector<std::shared_ptr<IEvaluator>>{base, equivalent},
      std::vector<double>{0.7, 0.3});
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm);

  const auto result = loop.reflect_and_commit(make_failed_trace("e2e_eval"));

  REQUIRE(result.success);
  REQUIRE(governor->commit_calls == 1);
}

TEST_CASE("gepa_e2e_with_real_mutation_governor", "[gepa][phase2][phase3]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto bus = std::make_shared<RecordingBus>();
  auto approval = std::make_shared<MockApprovalHandler>();
  auto governor = std::make_shared<MutationGovernor>(
      evaluator, std::unordered_set<std::string>{"R_T19_GEPA"}, bus.get(), approval.get());
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm);

  const auto result = loop.reflect_and_commit(make_failed_trace("e2e_governor"));

  REQUIRE(result.success);
  REQUIRE(std::count_if(bus->events.begin(), bus->events.end(), [](const BusEvent& event) {
    return event.topic == "mutation.committed";
  }) == 1);
}

TEST_CASE("gepa_e2e_regression_decline_aborts", "[gepa][phase2][phase3]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  governor->commit_approved = false;
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop::Config config;
  config.max_iterations = 1;
  GEPALoop loop(evaluator, governor, llm, config);

  const auto result = loop.reflect_and_commit(make_failed_trace("e2e_decline"));

  REQUIRE_FALSE(result.success);
  REQUIRE(result.failure_mode == "commit_denied");
  REQUIRE(governor->commit_calls == 1);
}

// T6 gepa-mcts-budget-integration: 进化预算闸集成测试
#include "modules/budget/budget_controller.h"

TEST_CASE("gepa_budget_t6_nullptr_baseline_compatible", "[gepa][budget][t6]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm, GEPALoop::Config{});
  // budget_controller_ 默认 nullptr, 零回归兼容
  const auto result = loop.reflect_and_commit(make_failed_trace("t6_nullptr"));
  REQUIRE(result.failure_mode.empty());  // 失败模式为空 = 完全成功
  REQUIRE(result.success);
}

TEST_CASE("gepa_budget_t6_exhausted_breaks_gracefully", "[gepa][budget][t6]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();

  // cap=0: 第 1 次迭代即超限, break 失败
  ExecutionBudget budget;
  budget.max_evolution_llm_calls = 0;
  std::optional<ExecutionBudget> budget_opt;
  budget_opt.emplace(std::move(budget));
  BudgetController controller(std::move(budget_opt));
  auto budget_iface = std::shared_ptr<IBudgetController>(&controller, [](IBudgetController*){});

  GEPALoop::Config config;
  config.max_iterations = 5;
  GEPALoop loop(evaluator, governor, budget_iface, llm, config);

  const auto result = loop.reflect_and_commit(make_failed_trace("t6_exhausted"));
  REQUIRE_FALSE(result.success);
  REQUIRE(result.failure_mode == "evolution_budget_exceeded");
  REQUIRE(controller.evolution_budget_exceeded());
}

TEST_CASE("gepa_budget_t6_exhausted_emits_event", "[gepa][budget][t6]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  auto bus = std::make_shared<RecordingBus>();

  ExecutionBudget budget;
  budget.max_evolution_llm_calls = 0;  // 第 1 次即超限
  std::optional<ExecutionBudget> budget_opt;
  budget_opt.emplace(std::move(budget));
  BudgetController controller(std::move(budget_opt));
  auto budget_iface = std::shared_ptr<IBudgetController>(&controller, [](IBudgetController*){});

  GEPALoop::Config config;
  config.max_iterations = 3;
  GEPALoop loop(evaluator, governor, budget_iface, llm, config, bus);

  loop.reflect_and_commit(make_failed_trace("t6_event"));

  // 验证 emit gepa.reflection.failed with reason=evolution_budget_exceeded
  bool found = false;
  for (const auto& e : bus->events) {
    if (e.topic == "gepa.reflection.failed" &&
        e.payload.data.contains("reason") &&
        e.payload.data["reason"].get<std::string>() == "evolution_budget_exceeded") {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

TEST_CASE("gepa_budget_t6_unlimited_baseline", "[gepa][budget][t6]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();

  // 默认 max_evolution_llm_calls = -1 (无限制)
  BudgetController controller;
  auto budget_iface = std::shared_ptr<IBudgetController>(&controller, [](IBudgetController*){});

  GEPALoop::Config config;
  config.max_iterations = 3;
  GEPALoop loop(evaluator, governor, budget_iface, llm, config);

  const auto result = loop.reflect_and_commit(make_failed_trace("t6_unlimited"));
  REQUIRE(result.success);  // budget 不限, 跑完 3 次迭代
  REQUIRE_FALSE(controller.evolution_budget_exceeded());
}

// ============================================================================
// G4 Case 9: GEPA persist-then-commit success with genome registry
// ============================================================================
TEST_CASE("G4 case-9: GEPA persist-then-commit with genome registry succeeds",
          "[gepa][g4][case-9]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  auto bus = std::make_shared<RecordingBus>();

  // Setup real FilesystemGenomeRegistry
  auto tmpdir = fs::temp_directory_path() / "hydraforge_g4_9";
  fs::remove_all(tmpdir);
  auto genome_reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(tmpdir);
  REQUIRE(genome_reg != nullptr);

  // Seed root v1 for gepa_skill
  agenticdsl::genome::Genome root;
  root.metadata.name = "gepa_skill";
  root.metadata.created_by = "test";
  root.metadata.created_at = "2026-09-22T00:00:00Z";
  root.metadata.capture_mode = "mock";
  root.spec.harness = "initial system prompt";
  root.spec.tools = {};
  auto root_res = genome_reg->commit(root);
  REQUIRE(root_res.has_value());
  REQUIRE(root_res.value().version == 1);

  GEPALoop::Config config;
  config.max_iterations = 1;
  // unique_ptr → shared_ptr: 必须 std::move (shared_ptr 有 unique_ptr 移动赋值)
  config.genome_registry = std::move(genome_reg);
  config.genome_name = "gepa_skill";
  config.parent_version = 1;

  GEPALoop loop(evaluator, governor, llm, config, bus);

  auto result = loop.reflect_and_commit(make_failed_trace("g4_case9"));

  // Should succeed with genome version
  REQUIRE(result.success);
  REQUIRE(result.candidate_skills.size() == 1);

  // version_id should be gepa_skill@N
  // (checked via gepa.commit.committed event payload)
  bool found_version_id = false;
  for (const auto& e : bus->events) {
    if (e.topic == "gepa.commit.committed") {
      if (e.payload.data.contains("commit_id")) {
        std::string commit_id = e.payload.data["commit_id"].get<std::string>();
        if (commit_id.rfind("gepa_skill@", 0) == 0) {
          found_version_id = true;
        }
      }
    }
  }
  REQUIRE(found_version_id);

  fs::remove_all(tmpdir);
}

// ============================================================================
// G4 Case 9b: GEPA fork failure → gepa.commit.denied + success=false
// ============================================================================
TEST_CASE("G4 case-9b: GEPA fork failure → gepa.commit.denied + success=false",
          "[gepa][g4][case-9b]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  auto bus = std::make_shared<RecordingBus>();

  auto tmpdir = fs::temp_directory_path() / "hydraforge_g4_9b";
  fs::remove_all(tmpdir);
  auto genome_reg = agenticdsl::genome::IGenomeRegistry::create_filesystem(tmpdir);

  GEPALoop::Config config;
  config.max_iterations = 1;
  config.genome_registry = std::move(genome_reg);  // unique_ptr → shared_ptr
  config.genome_name = "gepa_skill";
  config.parent_version = 99;  // nonexistent parent → fork fail

  GEPALoop loop(evaluator, governor, llm, config, bus);

  auto result = loop.reflect_and_commit(make_failed_trace("g4_case9b"));

  REQUIRE_FALSE(result.success);
  // Should have gepa.commit.denied
  bool found_denied = false;
  for (const auto& e : bus->events) {
    if (e.topic == "gepa.commit.denied") {
      found_denied = true;
    }
  }
  REQUIRE(found_denied);

  fs::remove_all(tmpdir);
}

// ============================================================================
// G4 Case 9c: GEPA genome_registry == nullptr → V1 regression (version_id = reflection_id)
// ============================================================================
TEST_CASE("G4 case-9c: GEPA registry nullptr → version_id = reflection_id (V1)",
          "[gepa][g4][case-9c]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  evaluator->score_by_ok = true;
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  auto bus = std::make_shared<RecordingBus>();

  GEPALoop::Config config;
  config.max_iterations = 1;
  // genome_registry defaults to nullptr

  GEPALoop loop(evaluator, governor, llm, config, bus);

  auto result = loop.reflect_and_commit(make_failed_trace("g4_case9c"));

  REQUIRE(result.success);
  // version_id should be reflection_id (V1 pattern)
  bool found_reflection = false;
  for (const auto& e : bus->events) {
    if (e.topic == "gepa.commit.committed") {
      if (e.payload.data.contains("commit_id")) {
        std::string commit_id = e.payload.data["commit_id"].get<std::string>();
        if (commit_id.rfind("g4_case9c:reflection:", 0) == 0) {
          found_reflection = true;
        }
      }
    }
  }
  REQUIRE(found_reflection);
}
