// src/evolution/harness_rsi.cpp
// C4 harness-rsi-pilot 轻量函数实现 (per ADR-0088 D4 + Oracle bg_3672cb57 修正)
//   - 双门禁: evaluate_readiness → 失败发 evolution.readiness.denied + 零状态变更
//   - 内部 is_tool_allowed policy check (per Oracle C3 — 不调 IMutationGovernor::propose)
//   - 3 mutation 路径 (prompt_delta + tools_add + tools_remove)
//   - workflow_patch → UnsupportedVariant (Wave 3 deferred)
// 设计依据: openspec/changes/2026-09-16-harness-rsi-pilot/ (C4 DRAFT after 2nd Oracle review)
// 作者: HydraForge Sprint 34+ Phase 6c MetaRSI-v1 C4
// 最后修改日期: 2026-09-21

#include "agenticdsl/evolution/harness_rsi.h"

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/evolution/transition_guard.h"

#include <string>

namespace agenticdsl::evolution {

namespace {

// 内部 policy check (per Oracle C3 — 不调 IMutationGovernance::propose)
// 返回 true = allowed, false = denied
bool is_tool_allowed(const std::string& tool_name,
                     const MutationGovernancePolicy& policy) {
  for (const auto& denied : policy.denied_tools) {
    if (denied == tool_name) return false;
  }
  return true;
}

// MutationError 转字符串 (for Decision Record 记录, per Metis 2.6 摩擦清单)
const char* mutation_error_name(MutationError e) {
  switch (e) {
    case MutationError::NotReady:            return "NotReady";
    case MutationError::GovernanceDenied:    return "GovernanceDenied";
    case MutationError::UnsupportedVariant:  return "UnsupportedVariant";
    case MutationError::RegistryRejected:    return "RegistryRejected";
    case MutationError::InvalidMutation:    return "InvalidMutation";
  }
  return "Unknown";
}

const char* attribution_verdict_name(AttributionVerdict v) {
  switch (v) {
    case AttributionVerdict::NotAttempted: return "NotAttempted";
    case AttributionVerdict::Attributed:   return "Attributed";
    case AttributionVerdict::Confounded:   return "Confounded";
    case AttributionVerdict::Insufficient: return "Insufficient";
  }
  return "Unknown";
}

const char* evolution_state_name(EvolutionState s) {
  switch (s) {
    case EvolutionState::Idle:    return "Idle";
    case EvolutionState::Harness: return "Harness";
    case EvolutionState::Data:    return "Data";
    case EvolutionState::Model:   return "Model";
    case EvolutionState::Done:    return "Done";
  }
  return "Unknown";
}

}  // namespace

// apply_harness_mutation — 5 参轻量函数 (per Oracle bg_770d1308 修正)
//
// DUAL-GATE ORDER (per Metis 3.5 + Oracle 1.4 验证):
//   1. evaluate_readiness → 失败发 evolution.readiness.denied (D8 ship) + 零状态变更
//   2. is_tool_allowed (per tools in mutations) → 拒绝直接返回 error
//
// 失败路径零状态变更 (system_prompt + tools + registry 全部不动)
// workflow_patch → UnsupportedVariant
// invalid mutation (空 prompt + empty tools_add/remove) → InvalidMutation
Result<AppliedMutation, MutationError> apply_harness_mutation(
    const GenomeMutations& mutations,
    std::string& system_prompt,
    std::vector<std::string>& tools,
    IToolRegistry& registry,
    const MutationGateContext& ctx) {

  // === Gate 0: InvalidMutation fail-fast ===
  const bool has_prompt = !mutations.prompt_delta.empty();
  const bool has_tools_add = !mutations.tools_add.empty();
  const bool has_tools_remove = !mutations.tools_remove.empty();
  const bool has_workflow = mutations.workflow_patch.has_value();
  if (!has_prompt && !has_tools_add && !has_tools_remove && !has_workflow) {
    return Result<AppliedMutation, MutationError>::failure(
        MutationError::InvalidMutation);
  }
  // tools_add 与 tools_remove 同名 (互斥)
  for (const auto& a : mutations.tools_add) {
    for (const auto& r : mutations.tools_remove) {
      if (a == r) {
        return Result<AppliedMutation, MutationError>::failure(
            MutationError::InvalidMutation);
      }
    }
  }

  // === Gate 1: evaluate_readiness (C3 ship) ===
  if (!ctx.attribution || !ctx.evaluator || !ctx.budget) {
    // Missing required context (caller's bug) — fail-fast InvalidMutation
    return Result<AppliedMutation, MutationError>::failure(
        MutationError::InvalidMutation);
  }
  auto verdict = evaluate_readiness(ctx.current, *ctx.attribution,
                                    *ctx.evaluator, *ctx.budget);
  if (!verdict.can_proceed) {
    // 零状态变更 + 发 evolution.readiness.denied (per ADR-0068 v2.2 line 253, 4-field payload)
    if (ctx.bus) {
      EventBuilder event("evolution.readiness.denied");
      event.args(nlohmann::json{
          {"failed_conditions", verdict.failed_conditions},
          {"attribution_verdict", attribution_verdict_name(ctx.attribution->verdict)},
          {"eval_quality", "Unknown"},
          {"budget_state", ctx.budget->exceeded() ? "exceeded" : "ok"}
      });
      event.meta(nlohmann::json{
          {"trace_id", ""},
          {"current_state", evolution_state_name(ctx.current)}
      });
      ctx.bus->emit(event.build());
    }
    return Result<AppliedMutation, MutationError>::failure(
        MutationError::NotReady);
  }

  // === Gate 2: is_tool_allowed policy check (per Oracle C3) ===
  for (const auto& tool_name : mutations.tools_add) {
    if (!is_tool_allowed(tool_name, ctx.policy)) {
      return Result<AppliedMutation, MutationError>::failure(
          MutationError::GovernanceDenied);
    }
  }

  // === Apply (顺序: workflow_patch 检查 → prompt_delta → tools_add → tools_remove) ===
  AppliedMutation applied;

  // workflow_patch → UnsupportedVariant (Wave 3 deferred)
  if (has_workflow) {
    return Result<AppliedMutation, MutationError>::failure(
        MutationError::UnsupportedVariant);
  }

  // mutation 路径 1: prompt_delta
  if (has_prompt) {
    system_prompt += mutations.prompt_delta;
    applied.applied_prompts.push_back(mutations.prompt_delta);
  }

  // mutation 路径 2: tools_add (调用 register_tool_function 需 ToolMetadata, 此处仅记录应用列表)
  for (const auto& tool_name : mutations.tools_add) {
    if (!registry.has_tool(tool_name)) {
      // 工具未注册 — fail-fast (per AGENTS.md "no as any" — 不假设 metadata)
      return Result<AppliedMutation, MutationError>::failure(
          MutationError::RegistryRejected);
    }
    tools.push_back(tool_name);
    applied.applied_tools_added.push_back(tool_name);
  }

  // mutation 路径 3: tools_remove (per DB1 IToolRegistry::unregister_tool_function)
  for (const auto& tool_name : mutations.tools_remove) {
    registry.unregister_tool_function(tool_name);
    auto it = std::find(tools.begin(), tools.end(), tool_name);
    if (it != tools.end()) tools.erase(it);
    applied.applied_tools_removed.push_back(tool_name);
  }

  return Result<AppliedMutation, MutationError>::success(applied);
}

}  // namespace agenticdsl::evolution
