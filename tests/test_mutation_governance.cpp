// tests/test_mutation_governance.cpp
// 功能描述：Mutation Governance V1 门禁与审计契约测试 (ADR-0084)
//          门禁链顺序 (design.md D6, 实现与测试严格一致):
//            白名单 fail-closed → L4 emit-then-throw → 模式×等级矩阵
//            → agent+L3 IApprovalHandler 人类复核 → IEvaluator 评估门
//            → 行为回归门 → commit (evaluation_refs 非空校验)
//          全部断言面向可客观验证的 topic / payload 字段 / 发射顺序 /
//          返回结果 / 异常类型 (REQUIRE_THROWS_AS)。
//          RecordingBus 为同步 IInteractionBus 测试替身 — emit 即入队,
//          因此 L4 emit-then-throw 的顺序可由 "catch 异常时事件已记录" 客观验证。
// 设计依据：openspec/changes/2026-08-26-adr-0084-mutation-governance-contract/
//          design.md (D1-D8) + specs/mutation-governance-contract/spec.md
// 作者：HydraForge Phase 6 / ADR-0084 V1 ship
// 最后修改日期：2026-08-26

#include "catch_amalgamated.hpp"

#include "agenticdsl/contract/imutation_governance.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/policy/iapproval_handler.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/mutation_record.h"
#include "agenticdsl/types/reward_signal.h"
#include "common/governance/mutation_governor.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

using namespace agenticdsl;

namespace {

// ============================================================================
// 测试替身: RecordingBus — 同步记录全部事件 (顺序敏感断言基础)
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

  std::vector<const BusEvent*> mutation_events() const {
    std::vector<const BusEvent*> out;
    for (const auto& e : events) {
      if (e.topic.rfind("mutation.", 0) == 0) {
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

  RewardSignal evaluate(const ExecutionTrace& /*trace*/) const override {
    ++evaluate_calls;
    switch (quality) {
      case RewardSignal::Quality::Excellent:
        return RewardSignal::excellent();
      case RewardSignal::Quality::Acceptable:
        return RewardSignal::acceptable();
      case RewardSignal::Quality::Poor:
        return RewardSignal::poor();
    }
    return RewardSignal::poor();
  }

  int compare(const ExecutionTrace& /*a*/,
              const ExecutionTrace& /*b*/) const override {
    return 0;
  }
};

// ============================================================================
// 测试替身: StubApprovalHandler — 计数 + 可配置结果的 IApprovalHandler
// ============================================================================
class StubApprovalHandler : public IApprovalHandler {
 public:
  bool result = true;
  int calls = 0;

  bool process_request(const ToolMetadata& /*meta*/,
                       const ToolCallContext& /*ctx*/,
                       const ToolPreview& /*preview*/) override {
    ++calls;
    return result;
  }
};

// ============================================================================
// 公共 fixture helper
// ============================================================================
MutationContext make_ctx(const std::string& kind, MutationMode mode) {
  MutationContext ctx;
  ctx.mutation_id = "mut-001";
  ctx.source_id = "R_T19_GEPA";
  ctx.mutation_kind = kind;
  ctx.subject_ref = "prompt_v1";
  ctx.proposed_change = "change-summary";
  ctx.parent_ref = "prompt_v0";
  ctx.version_id = "prompt_v2";
  ctx.mode = mode;
  ctx.evaluation_refs = {"eval-001"};
  return ctx;
}

struct Fixture {
  RecordingBus bus;
  std::shared_ptr<StubEvaluator> evaluator = std::make_shared<StubEvaluator>();
  StubApprovalHandler handler;
  std::unordered_set<std::string> whitelist{"R_T19_GEPA", "R_T20_AFLOW"};
};

}  // namespace

// ============================================================================
// T2.1: L1 prompt 变异 happy path (yolo 模式)
// ============================================================================
TEST_CASE("l1_prompt_mutation_happy_path", "[mutation][v1]") {
  Fixture f;
  MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);
  auto ctx = make_ctx("L1_prompt", MutationMode::Yolo);

  auto propose_decision = governor.propose(ctx);
  REQUIRE(propose_decision.approved);
  REQUIRE(propose_decision.denial_reason.empty());

  auto commit_decision = governor.commit(ctx);
  REQUIRE(commit_decision.approved);

  // 顺序: mutation.proposed → mutation.committed
  const auto evts = f.bus.mutation_events();
  REQUIRE(evts.size() == 2);
  REQUIRE(evts[0]->topic == "mutation.proposed");
  REQUIRE(evts[1]->topic == "mutation.committed");

  // proposed payload
  const auto& proposed = evts[0]->payload.data;
  REQUIRE(proposed["mutation_id"] == "mut-001");
  REQUIRE(proposed["source_id"] == "R_T19_GEPA");
  REQUIRE(proposed["subject_ref"] == "prompt_v1");
  REQUIRE(proposed["mutation_kind"] == "L1_prompt");
  REQUIRE(proposed["evaluation_refs"] == std::vector<std::string>{"eval-001"});

  // committed payload 原样透传 evaluation_refs (不透明)
  const auto& committed = evts[1]->payload.data;
  REQUIRE(committed["mutation_id"] == "mut-001");
  REQUIRE(committed["mutation_kind"] == "L1_prompt");
  REQUIRE(committed["version_id"] == "prompt_v2");
  REQUIRE(committed["evaluation_refs"] == std::vector<std::string>{"eval-001"});

  // yolo + L1 不触发人类复核
  REQUIRE(f.handler.calls == 0);
  // 评估门在 propose 阶段恰好执行一次
  REQUIRE(f.evaluator->evaluate_calls == 1);
}

// ============================================================================
// T2.2: L2 DSL 变异在 yolo 模式被拒绝 (plan_required)
// ============================================================================
TEST_CASE("l2_dsl_mutation_plan_only", "[mutation][v1]") {
  Fixture f;
  MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);
  auto ctx = make_ctx("L2_dsl", MutationMode::Yolo);

  auto decision = governor.propose(ctx);
  REQUIRE_FALSE(decision.approved);
  REQUIRE(decision.denial_reason == "plan_required");
  REQUIRE(decision.failed_step == "authorization");

  const auto evts = f.bus.mutation_events();
  REQUIRE(evts.size() == 1);
  REQUIRE(evts[0]->topic == "mutation.denied");
  REQUIRE(evts[0]->payload.data["denial_reason"] == "plan_required");
  REQUIRE(evts[0]->payload.data["failed_step"] == "authorization");
  REQUIRE(evts[0]->payload.data["subject_ref"] == "prompt_v1");

  // 不进入后续门禁
  REQUIRE(f.handler.calls == 0);
  REQUIRE(f.evaluator->evaluate_calls == 0);
}

// ============================================================================
// T2.3: L3 SKILL.md 变异在 plan 模式被拒绝 (plan_insufficient), 不调用 handler
// ============================================================================
TEST_CASE("l3_skill_mutation_plan_insufficient", "[mutation][v1]") {
  Fixture f;
  MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);
  auto ctx = make_ctx("L3_skill", MutationMode::Plan);

  auto decision = governor.propose(ctx);
  REQUIRE_FALSE(decision.approved);
  REQUIRE(decision.denial_reason == "plan_insufficient");
  REQUIRE(decision.failed_step == "authorization");

  const auto evts = f.bus.mutation_events();
  REQUIRE(evts.size() == 1);
  REQUIRE(evts[0]->topic == "mutation.denied");
  REQUIRE(evts[0]->payload.data["denial_reason"] == "plan_insufficient");

  // 不调用 IApprovalHandler, 不进入评估门
  REQUIRE(f.handler.calls == 0);
  REQUIRE(f.evaluator->evaluate_calls == 0);
}

// ============================================================================
// T2.4: L3 SKILL.md 变异在 agent 模式必须经 IApprovalHandler 人类复核
// ============================================================================
TEST_CASE("l3_skill_mutation_agent_requires_approval", "[mutation][v1]") {
  Fixture f;
  auto ctx = make_ctx("L3_skill", MutationMode::Agent);

  SECTION("handler 返回 true → 继续门禁并 emit proposed") {
    f.handler.result = true;
    MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);

    auto decision = governor.propose(ctx);
    REQUIRE(decision.approved);
    REQUIRE(f.handler.calls == 1);  // 恰好调用一次

    const auto evts = f.bus.mutation_events();
    REQUIRE(evts.size() == 1);
    REQUIRE(evts[0]->topic == "mutation.proposed");
  }

  SECTION("handler 返回 false → denied approval_denied / human_review") {
    f.handler.result = false;
    MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);

    auto decision = governor.propose(ctx);
    REQUIRE_FALSE(decision.approved);
    REQUIRE(decision.denial_reason == "approval_denied");
    REQUIRE(decision.failed_step == "human_review");
    REQUIRE(f.handler.calls == 1);

    const auto evts = f.bus.mutation_events();
    REQUIRE(evts.size() == 1);
    REQUIRE(evts[0]->topic == "mutation.denied");
    REQUIRE(evts[0]->payload.data["denial_reason"] == "approval_denied");
    REQUIRE(evts[0]->payload.data["failed_step"] == "human_review");

    // 复核拒绝后不进入评估门
    REQUIRE(f.evaluator->evaluate_calls == 0);
  }
}

// ============================================================================
// T2.5: agent+L3 但 IApprovalHandler 未注入 → fail-closed
// ============================================================================
TEST_CASE("l3_skill_mutation_agent_handler_missing_fail_closed",
          "[mutation][v1]") {
  Fixture f;
  MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, nullptr);
  auto ctx = make_ctx("L3_skill", MutationMode::Agent);

  auto decision = governor.propose(ctx);
  REQUIRE_FALSE(decision.approved);
  REQUIRE(decision.denial_reason == "approval_handler_unavailable");
  REQUIRE(decision.failed_step == "human_review");

  const auto evts = f.bus.mutation_events();
  REQUIRE(evts.size() == 1);
  REQUIRE(evts[0]->topic == "mutation.denied");
  REQUIRE(evts[0]->payload.data["denial_reason"] ==
          "approval_handler_unavailable");
  REQUIRE(evts[0]->payload.data["failed_step"] == "human_review");

  REQUIRE(f.evaluator->evaluate_calls == 0);
}

// ============================================================================
// T2.6: L4 权重变异 V1 显式拒绝 (emit-then-throw)
// ============================================================================
TEST_CASE("l4_weight_mutation_emit_then_throw", "[mutation][v1]") {
  Fixture f;
  MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);

  for (auto mode :
       {MutationMode::Yolo, MutationMode::Plan, MutationMode::Agent}) {
    f.bus.events.clear();
    auto ctx = make_ctx("L4_weight", mode);

    REQUIRE_THROWS_AS(governor.propose(ctx), std::runtime_error);

    // emit-then-throw: RecordingBus 同步记录, catch 异常时 denied 事件已在册
    // → 事件发射严格先于异常抛出 (程序序保证)
    const auto evts = f.bus.mutation_events();
    REQUIRE(evts.size() == 1);
    REQUIRE(evts[0]->topic == "mutation.denied");
    REQUIRE(evts[0]->payload.data["denial_reason"] == "l4_forbidden_v1");
    REQUIRE(evts[0]->payload.data["failed_step"] == "authorization");

    // L4 拒绝后不进入后续门禁
    REQUIRE(f.handler.calls == 0);
    REQUIRE(f.evaluator->evaluate_calls == 0);
  }
}

// ============================================================================
// T2.7: 审计事件完整性 — 全序 + evaluation_refs 透传 + 无第 3 个事件
// ============================================================================
TEST_CASE("audit_event_ordering_and_evaluation_refs", "[mutation][v1]") {
  Fixture f;
  MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);
  auto ctx = make_ctx("L1_prompt", MutationMode::Agent);
  ctx.evaluation_refs = {"eval-001", "eval-002"};

  REQUIRE(governor.propose(ctx).approved);
  REQUIRE(governor.commit(ctx).approved);

  const auto evts = f.bus.mutation_events();
  // 同一 attempt 恰好 2 个终态事件, 全序 proposed → committed
  REQUIRE(evts.size() == 2);
  REQUIRE(evts[0]->topic == "mutation.proposed");
  REQUIRE(evts[1]->topic == "mutation.committed");

  // committed 含非空 evaluation_refs 且原样透传
  const auto& refs = evts[1]->payload.data["evaluation_refs"];
  REQUIRE(refs.is_array());
  REQUIRE(refs.size() == 2);
  REQUIRE(refs == std::vector<std::string>{"eval-001", "eval-002"});
}

// ============================================================================
// T2.8: 白名单 fail-closed — 空 source_id / 未注册 / 默认空白名单
// ============================================================================
TEST_CASE("fail_closed_on_unknown_source", "[mutation][v1]") {
  Fixture f;

  SECTION("source_id 为空字符串 → denied, 不执行后续任何门禁") {
    MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);
    auto ctx = make_ctx("L1_prompt", MutationMode::Yolo);
    ctx.source_id = "";

    auto decision = governor.propose(ctx);
    REQUIRE_FALSE(decision.approved);
    REQUIRE(decision.denial_reason == "non_whitelisted_source");
    REQUIRE(decision.failed_step == "source_whitelist");

    const auto evts = f.bus.mutation_events();
    REQUIRE(evts.size() == 1);
    REQUIRE(evts[0]->topic == "mutation.denied");
    REQUIRE(evts[0]->payload.data["denial_reason"] ==
            "non_whitelisted_source");
    REQUIRE(evts[0]->payload.data["failed_step"] == "source_whitelist");

    REQUIRE(f.handler.calls == 0);
    REQUIRE(f.evaluator->evaluate_calls == 0);
  }

  SECTION("source_id 未注册 → denied") {
    MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);
    auto ctx = make_ctx("L1_prompt", MutationMode::Yolo);
    ctx.source_id = "external_user_input";

    auto decision = governor.propose(ctx);
    REQUIRE_FALSE(decision.approved);
    REQUIRE(decision.denial_reason == "non_whitelisted_source");
    REQUIRE(f.evaluator->evaluate_calls == 0);
  }

  SECTION("默认空白名单 = 全部拒绝 (含白名单常用 source)") {
    MutationGovernor governor(f.evaluator, {}, &f.bus, &f.handler);
    auto ctx = make_ctx("L1_prompt", MutationMode::Yolo);

    auto decision = governor.propose(ctx);
    REQUIRE_FALSE(decision.approved);
    REQUIRE(decision.denial_reason == "non_whitelisted_source");

    const auto evts = f.bus.mutation_events();
    REQUIRE(evts.size() == 1);
    REQUIRE(evts[0]->topic == "mutation.denied");
  }
}

// ============================================================================
// T2.9: commit 缺少 evaluation_refs → fail-closed
// ============================================================================
TEST_CASE("commit_missing_evaluation_refs_fail_closed", "[mutation][v1]") {
  Fixture f;
  MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);
  auto ctx = make_ctx("L1_prompt", MutationMode::Yolo);
  ctx.evaluation_refs = {};  // propose 通过, 但 refs 为空

  REQUIRE(governor.propose(ctx).approved);

  auto decision = governor.commit(ctx);
  REQUIRE_FALSE(decision.approved);
  REQUIRE(decision.denial_reason == "missing_evaluation_refs");
  REQUIRE(decision.failed_step == "evaluation");

  const auto evts = f.bus.mutation_events();
  REQUIRE(evts.size() == 2);
  REQUIRE(evts[0]->topic == "mutation.proposed");
  REQUIRE(evts[1]->topic == "mutation.denied");
  REQUIRE(evts[1]->payload.data["denial_reason"] == "missing_evaluation_refs");
  REQUIRE(evts[1]->payload.data["failed_step"] == "evaluation");
}

// ============================================================================
// T2.10: 构造时 IEvaluator 为空 → fail-fast
// ============================================================================
TEST_CASE("constructor_null_evaluator_throws", "[mutation][v1]") {
  Fixture f;
  REQUIRE_THROWS_AS(
      MutationGovernor(std::shared_ptr<IEvaluator>(nullptr), f.whitelist,
                       &f.bus, &f.handler),
      std::invalid_argument);
}

// ============================================================================
// T2.11: revert 为纯审计记录 (audit-only), governor 无可读回状态
// ============================================================================
TEST_CASE("revert_is_audit_only", "[mutation][v1]") {
  Fixture f;
  MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);
  auto ctx = make_ctx("L1_prompt", MutationMode::Yolo);

  REQUIRE(governor.propose(ctx).approved);
  REQUIRE(governor.commit(ctx).approved);
  governor.revert(ctx, "prompt_v1", "regression");

  const auto evts = f.bus.mutation_events();
  REQUIRE(evts.size() == 3);
  REQUIRE(evts[2]->topic == "mutation.reverted");
  REQUIRE(evts[2]->payload.data["target_version"] == "prompt_v1");
  REQUIRE(evts[2]->payload.data["rollback_reason"] == "regression");
  REQUIRE(evts[2]->payload.data["mutation_id"] == "mut-001");

  // 无可读回状态: 接口层无任何状态查询 API (static 性质), 且 revert 后
  // 对同一 ctx 再次 propose 行为与首次完全一致 (governor 无内部状态)
  f.bus.events.clear();
  REQUIRE(governor.propose(ctx).approved);
  const auto evts2 = f.bus.mutation_events();
  REQUIRE(evts2.size() == 1);
  REQUIRE(evts2[0]->topic == "mutation.proposed");
}

// ============================================================================
// T2.12 (扩展): 行为回归门失败 → denied behavioral_regression_failed
// ============================================================================
TEST_CASE("behavioral_regression_gate_fail", "[mutation][v1]") {
  Fixture f;
  auto ctx = make_ctx("L1_prompt", MutationMode::Yolo);

  SECTION("回归门返回 false → denied, 不 emit proposed") {
    MutationGovernor governor(
        f.evaluator, f.whitelist, &f.bus, &f.handler,
        [](const MutationContext&) { return false; });

    auto decision = governor.propose(ctx);
    REQUIRE_FALSE(decision.approved);
    REQUIRE(decision.denial_reason == "behavioral_regression_failed");
    REQUIRE(decision.failed_step == "behavioral_regression");

    const auto evts = f.bus.mutation_events();
    REQUIRE(evts.size() == 1);
    REQUIRE(evts[0]->topic == "mutation.denied");
    REQUIRE(evts[0]->payload.data["denial_reason"] ==
            "behavioral_regression_failed");
    REQUIRE(evts[0]->payload.data["failed_step"] == "behavioral_regression");

    // 评估门已执行 (在回归门之前), 回归失败后才拒绝
    REQUIRE(f.evaluator->evaluate_calls == 1);
  }

  SECTION("回归门返回 true → 全链通过") {
    MutationGovernor governor(
        f.evaluator, f.whitelist, &f.bus, &f.handler,
        [](const MutationContext&) { return true; });

    REQUIRE(governor.propose(ctx).approved);
    const auto evts = f.bus.mutation_events();
    REQUIRE(evts.size() == 1);
    REQUIRE(evts[0]->topic == "mutation.proposed");
  }
}

// ============================================================================
// 扩展 (spec Requirement #2 Scenario #2): 评估门失败 → denied / evaluation
// ============================================================================
TEST_CASE("evaluation_gate_poor_quality_denied", "[mutation][v1]") {
  Fixture f;
  f.evaluator->quality = RewardSignal::Quality::Poor;
  MutationGovernor governor(f.evaluator, f.whitelist, &f.bus, &f.handler);
  auto ctx = make_ctx("L1_prompt", MutationMode::Yolo);

  auto decision = governor.propose(ctx);
  REQUIRE_FALSE(decision.approved);
  REQUIRE(decision.failed_step == "evaluation");
  REQUIRE_FALSE(decision.denial_reason.empty());

  const auto evts = f.bus.mutation_events();
  REQUIRE(evts.size() == 1);
  REQUIRE(evts[0]->topic == "mutation.denied");
  REQUIRE(evts[0]->payload.data["failed_step"] == "evaluation");
  REQUIRE(evts[0]->payload.data["denial_reason"] ==
          "evaluation_poor_quality");
}
