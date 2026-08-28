// tests/test_prompt_evidence_gate.cpp
// 功能描述：T21 Prompt Evidence Gate 测试套件 (ADR-0074, Sprint 24/25)
//          质量门控层：Go/Conditional/No-Go 阈值 + parse-valid 计算 +
//          IEvaluator V2 集成 + 两阶段注入 + JSONL 导出 + llm.dsl.* 事件
// 设计依据：openspec/changes/t21-prompt-evidence-gate/spec.md + design.md
// 作者：HydraForge Sprint 25 T21 ship
// 最后修改日期：2026-08-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/prompt/evidence_gate.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/reward_signal.h"
#include "core/types/tool_result.h"

#include <memory>
#include <string>
#include <vector>

using namespace agenticdsl;

namespace {

// ============================================================================
// 测试替身: StubEvaluator — 可配置 quality 的 IEvaluator (V2 接口)
// ============================================================================
class StubEvaluator : public IEvaluator {
 public:
  RewardSignal::Quality quality = RewardSignal::Quality::Excellent;
  mutable int evaluate_calls = 0;

  RewardSignal evaluate(const ExecutionTrace& trace) const override {
    ++evaluate_calls;
    if (trace.final_result.ok) {
      return RewardSignal::excellent(0.9);
    }
    if (quality == RewardSignal::Quality::Poor) {
      return RewardSignal::poor(0.8);
    }
    return RewardSignal::acceptable(0.5);
  }

  int compare(const ExecutionTrace& /*a*/,
              const ExecutionTrace& /*b*/) const override {
    return 0;
  }
};

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

  std::vector<const BusEvent*> by_topic_prefix(const std::string& prefix) const {
    std::vector<const BusEvent*> out;
    for (const auto& e : events) {
      if (e.topic.rfind(prefix, 0) == 0) out.push_back(&e);
    }
    return out;
  }
};

// 构造一条规则 (前缀 P: 语法 / S: 语义 / 无前缀默认语义)
std::string rule(const std::string& body, bool parse = false) {
  return (parse ? "P:" : "S:") + body;
}

}  // namespace

// ============================================================================
// Phase 0 骨架 cases — 契约声明 + 编译验证 (T0)
// ============================================================================

TEST_CASE("golden task struct exposes 3 fields", "[prompt][t0]") {
  GoldenTask task;
  task.input = "计算 1+2";
  task.expected_output = "## math.add";
  task.validation_rules = {rule("## math.add"), rule("args: a=1, b=2")};

  REQUIRE(task.input == "计算 1+2");
  REQUIRE(task.expected_output == "## math.add");
  REQUIRE(task.validation_rules.size() == 2);
  REQUIRE(task.validation_rules[0] == "S:## math.add");
}

TEST_CASE("gate decision enum has 3 values", "[prompt][t0]") {
  GateDecision go = GateDecision::Go;
  GateDecision conditional = GateDecision::Conditional;
  GateDecision no_go = GateDecision::No_Go;

  REQUIRE(go != GateDecision::No_Go);
  REQUIRE(conditional != GateDecision::Go);
  REQUIRE(no_go != GateDecision::Conditional);
  REQUIRE(no_go == GateDecision::No_Go);
}

TEST_CASE("gate constructs with injected IEvaluator", "[prompt][t0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  PromptEvidenceGate gate(evaluator);
  SUCCEED("PromptEvidenceGate(shared_ptr<IEvaluator>) 可构造");
}

TEST_CASE("gate constructs without event bus (nullopt)", "[prompt][t0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  PromptEvidenceGate gate(evaluator, nullptr);
  SUCCEED("bus 可选, 默认 nullptr 可构造");
}

TEST_CASE("parse_valid_rate perfect response returns 1.0", "[prompt][t0]") {
  GoldenTask task;
  task.input = "task";
  task.expected_output = "## start";
  task.validation_rules = {rule("## start"), rule("## end"), rule("-> next")};

  const std::string response = "## start\n## end\n-> next\n";
  // 静态 helper: 全部规则满足 → 1.0
  REQUIRE(PromptEvidenceGate::parse_valid_rate(response, task) == Catch::Approx(1.0));
}

TEST_CASE("parse_valid_rate partial response returns 0.5", "[prompt][t0]") {
  GoldenTask task;
  task.input = "task";
  task.expected_output = "## start";
  task.validation_rules = {rule("## start"), rule("## missing")};

  const std::string response = "## start\n";
  REQUIRE(PromptEvidenceGate::parse_valid_rate(response, task) == Catch::Approx(0.5));
}

TEST_CASE("evaluate perfect response returns Go", "[prompt][t0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  PromptEvidenceGate gate(evaluator);

  GoldenTask task;
  task.input = "task";
  task.expected_output = "## start";
  task.validation_rules = {rule("## start"), rule("## end"), rule("-> next")};
  const std::string response = "## start\n## end\n-> next\n";

  GateDecision decision = gate.evaluate("prompt", response, task);
  REQUIRE(decision == GateDecision::Go);
}

TEST_CASE("evaluate updates last_parse_valid_rate", "[prompt][t0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  PromptEvidenceGate gate(evaluator);

  GoldenTask task;
  task.validation_rules = {rule("## start"), rule("## end"), rule("-> next")};
  const std::string response = "## start\n## end\n-> next\n";

  (void)gate.evaluate("prompt", response, task);
  REQUIRE(gate.last_parse_valid_rate() == Catch::Approx(1.0));
}

TEST_CASE("evaluate consults injected evaluator", "[prompt][t0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  PromptEvidenceGate gate(evaluator);
  GoldenTask task;
  task.validation_rules = {rule("## start")};

  (void)gate.evaluate("prompt", "## start", task);
  REQUIRE(evaluator->evaluate_calls == 1);
  REQUIRE(gate.last_reward().quality == RewardSignal::Quality::Excellent);
}

TEST_CASE("empty validation rules are not implicitly valid", "[prompt][t0]") {
  auto evaluator = std::make_shared<StubEvaluator>();
  PromptEvidenceGate gate(evaluator);
  GoldenTask task;

  REQUIRE(PromptEvidenceGate::parse_valid_rate("anything", task) == Catch::Approx(0.0));
  REQUIRE(gate.evaluate("prompt", "anything", task) == GateDecision::No_Go);
}