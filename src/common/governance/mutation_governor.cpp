// src/common/governance/mutation_governor.cpp
// 功能描述：MutationGovernor V1 完整门禁链实现 (ADR-0084 gate-and-audit)
//          门禁链顺序 (design.md D6, 与 tests/test_mutation_governance.cpp 一致):
//            propose: 白名单 fail-closed → L4 emit-then-throw → 模式×等级矩阵
//                     → agent+L3 IApprovalHandler → IEvaluator 评估门
//                     → 行为回归门 → emit mutation.proposed
//            commit:  白名单 → evaluation_refs 非空校验 → emit mutation.committed
//            revert:  白名单 → emit mutation.reverted (audit-only)
//            任意失败: emit mutation.denied (denial_reason + failed_step), 终态
// 设计依据：openspec/changes/2026-08-26-adr-0084-mutation-governance-contract/design.md
//          D1 (V1 边界) / D3 (L3 语义统一) / D4 (evaluation_refs 不透明透传)
//          D5 (白名单 fail-closed) / D6 (审计事件全序) / D7 (IEvaluator 构造注入)
// 作者：HydraForge Phase 6 / ADR-0084 V1 ship
// 最后修改日期：2026-08-26

#include "common/governance/mutation_governor.h"

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/policy/iapproval_handler.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/types/mutation_record.h"
#include "agenticdsl/types/reward_signal.h"
#include "core/types/tool_result.h"

#include <stdexcept>
#include <utility>

namespace agenticdsl {

namespace {

// agent+L3 人类复核调用的合成参数 (design D3: 复用既有 IApprovalHandler 接口,
// 不创建新审批逻辑; ToolMetadata/ToolCallContext/ToolPreview 为合成值)
ToolMetadata make_review_meta(const MutationContext& ctx) {
  ToolMetadata meta{};
  meta.name = "mutation::" + ctx.mutation_kind;
  meta.description = "mutation governance approval";
  meta.domain = "mutation";
  meta.category = ToolCategory::StateModify;
  meta.min_layer = LayerProfile::Workflow;
  return meta;
}

ToolCallContext make_review_call_ctx(const MutationContext& ctx) {
  ToolCallContext call_ctx{};
  call_ctx.session_id = "";
  call_ctx.caller_layer = "mutation_governor";
  call_ctx.target_path = ctx.subject_ref;
  call_ctx.trace_id = ctx.mutation_id;
  return call_ctx;
}

ToolPreview make_review_preview(const MutationContext& ctx) {
  ToolPreview preview{};
  preview.command_line =
      "mutation::propose " + ctx.mutation_kind + " " + ctx.subject_ref;
  preview.risk_summary = "L3 skill mutation requires human review";
  return preview;
}

}  // namespace

MutationGovernor::MutationGovernor(std::shared_ptr<IEvaluator> evaluator,
                                   std::unordered_set<std::string> source_whitelist,
                                   IInteractionBus* bus,
                                   IApprovalHandler* approval_handler,
                                   RegressionGate regression_gate)
    : evaluator_(std::move(evaluator)),
      whitelist_(std::move(source_whitelist)),
      bus_(bus),
      approval_handler_(approval_handler),
      regression_gate_(std::move(regression_gate)) {
  // D7: 构造时强制注入非空 IEvaluator (fail-fast, 无可绕过路径)
  if (!evaluator_) {
    throw std::invalid_argument(
        "MutationGovernor requires non-null IEvaluator (ADR-0083 硬前置)");
  }
  // 审计为强制职责: 无总线即无法履行 ADR-0084 §决策 4
  if (bus_ == nullptr) {
    throw std::invalid_argument(
        "MutationGovernor requires non-null IInteractionBus (审计强制)");
  }
}

// ============================================================================
// 内部 helper
// ============================================================================

bool MutationGovernor::source_allowed(const std::string& source_id) const {
  // D5: 空串 / 不在集合 → fail-closed
  return !source_id.empty() && whitelist_.count(source_id) != 0;
}

MutationDecision MutationGovernor::deny(const MutationContext& ctx,
                                        const std::string& denial_reason,
                                        const std::string& failed_step) {
  const MutationDeniedPayload payload{ctx.mutation_id, denial_reason,
                                      failed_step, ctx.subject_ref};
  bus_->emit(EventBuilder(mutation_topics::kDenied)
                 .args(payload.to_json())
                 .ok(false)
                 .meta(nlohmann::json{{"trace_id", ctx.mutation_id}})
                 .build());
  return MutationDecision{false, denial_reason, failed_step};
}

void MutationGovernor::emit_proposed(const MutationContext& ctx) {
  const MutationProposedPayload payload{ctx.mutation_id,   ctx.source_id,
                                        ctx.subject_ref,   ctx.mutation_kind,
                                        ctx.proposed_change, ctx.parent_ref,
                                        ctx.evaluation_refs};
  bus_->emit(EventBuilder(mutation_topics::kProposed)
                 .args(payload.to_json())
                 .meta(nlohmann::json{{"trace_id", ctx.mutation_id}})
                 .build());
}

void MutationGovernor::emit_committed(const MutationContext& ctx) {
  const MutationCommittedPayload payload{ctx.mutation_id, ctx.version_id,
                                         ctx.mutation_kind,
                                         ctx.evaluation_refs};
  bus_->emit(EventBuilder(mutation_topics::kCommitted)
                 .args(payload.to_json())
                 .meta(nlohmann::json{{"trace_id", ctx.mutation_id}})
                 .build());
}

// ============================================================================
// propose: 白名单 → L4 → 模式×等级矩阵 → agent+L3 复核 → 评估门 → 回归门
// ============================================================================

MutationDecision MutationGovernor::propose(const MutationContext& ctx) {
  // Gate 1 (D5): 白名单 fail-closed — 不执行任何后续门禁
  if (!source_allowed(ctx.source_id)) {
    return deny(ctx, "non_whitelisted_source", "source_whitelist");
  }

  // Gate 2 (D6): L4 emit-then-throw — 事件严格先于异常
  if (ctx.mutation_kind == "L4_weight") {
    deny(ctx, "l4_forbidden_v1", "authorization");
    throw std::runtime_error("L4 weight mutation forbidden in V1 (ADR-0084)");
  }

  // Gate 3 (D3): 模式 × 等级矩阵
  if (ctx.mutation_kind == "L1_prompt") {
    // 全模式允许, 直接进入评估门
  } else if (ctx.mutation_kind == "L2_dsl") {
    if (ctx.mode == MutationMode::Yolo) {
      return deny(ctx, "plan_required", "authorization");
    }
  } else if (ctx.mutation_kind == "L3_skill") {
    if (ctx.mode == MutationMode::Yolo) {
      return deny(ctx, "plan_required", "authorization");
    }
    if (ctx.mode == MutationMode::Plan) {
      return deny(ctx, "plan_insufficient", "authorization");
    }
    // Gate 4 (D3): agent + L3 → IApprovalHandler 人类复核 (恰好一次)
    if (approval_handler_ == nullptr) {
      return deny(ctx, "approval_handler_unavailable", "human_review");
    }
    if (!approval_handler_->process_request(make_review_meta(ctx),
                                            make_review_call_ctx(ctx),
                                            make_review_preview(ctx))) {
      return deny(ctx, "approval_denied", "human_review");
    }
  } else {
    // 未知变异等级 → fail-closed
    return deny(ctx, "unknown_mutation_kind", "authorization");
  }

  // Gate 5 (D7): IEvaluator 评估门 — proposed_change 包装为合成 ExecutionTrace
  ExecutionTrace trace;
  trace.final_result = ToolResult::success(ctx.proposed_change);
  trace.trace_id = ctx.mutation_id;
  const RewardSignal signal = evaluator_->evaluate(trace);
  if (signal.quality == RewardSignal::Quality::Poor) {
    return deny(ctx, "evaluation_poor_quality", "evaluation");
  }

  // Gate 6: 行为回归门 (ADR-0061-02 hook; 空 = V1 默认 Pass)
  if (regression_gate_ && !regression_gate_(ctx)) {
    return deny(ctx, "behavioral_regression_failed", "behavioral_regression");
  }

  // 全部门禁通过 → emit proposed (终态)
  emit_proposed(ctx);
  return MutationDecision{true, "", ""};
}

// ============================================================================
// commit: 白名单 → evaluation_refs 非空校验 → emit committed
// ============================================================================

MutationDecision MutationGovernor::commit(const MutationContext& ctx) {
  // D5: commit 同样经过白名单 fail-closed
  if (!source_allowed(ctx.source_id)) {
    return deny(ctx, "non_whitelisted_source", "source_whitelist");
  }

  // D4: evaluation_refs 非空校验 (fail-closed)
  if (ctx.evaluation_refs.empty()) {
    return deny(ctx, "missing_evaluation_refs", "evaluation");
  }

  emit_committed(ctx);
  return MutationDecision{true, "", ""};
}

// ============================================================================
// revert: audit-only (D1) — emit reverted, 不读取/不修改/不恢复任何状态
// ============================================================================

void MutationGovernor::revert(const MutationContext& ctx,
                              const std::string& target_version,
                              const std::string& rollback_reason) {
  // D5: revert 同样经过白名单 fail-closed
  if (!source_allowed(ctx.source_id)) {
    deny(ctx, "non_whitelisted_source", "source_whitelist");
    return;
  }

  const MutationRevertedPayload payload{ctx.mutation_id, target_version,
                                        rollback_reason};
  bus_->emit(EventBuilder(mutation_topics::kReverted)
                 .args(payload.to_json())
                 .meta(nlohmann::json{{"trace_id", ctx.mutation_id}})
                 .build());
}

}  // namespace agenticdsl
