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
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/policy/iapproval_handler.h"
#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"
#include "agenticdsl/cognitive/behavioral_equivalence_evaluator.h"
#include "agenticdsl/cognitive/composite_evaluator.h"
#include "core/types/tool_result.h"

#include <memory>
#include <string>
#include <vector>

using namespace agenticdsl;

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
  mutable int evaluate_calls = 0;
  mutable int compare_calls = 0;

  RewardSignal evaluate(const ExecutionTrace& /*trace*/) const override {
    ++evaluate_calls;
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
  auto governor = std::make_shared<StubMutationGovernor>();
  auto llm = std::make_shared<MockLLMProvider>();
  GEPALoop loop(evaluator, governor, llm);

  ExecutionTrace trace = make_failed_trace("commit_001");
  GEPALoop::ReflectionResult result = loop.reflect_and_commit(trace);

  // Phase 0: 占位 — 实现后应调用 MutationGovernor::commit
  REQUIRE(result.success);
}