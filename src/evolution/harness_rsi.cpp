// src/evolution/harness_rsi.cpp
// C4 harness-rsi-pilot 轻量函数实现 (per ADR-0088 D4 + Oracle bg_3672cb57 修正)
// G4 extension: Gate 0 workflow_patch upmove + Gate 3 persist-before-apply + undo
// 设计依据: openspec/changes/genome-wiring-harness-rsi-gepa/ (G4, 2026-09-22)
// 作者: HydraForge Sprint 34+ Phase 6c MetaRSI-v1 C4+G4
// 最后修改日期: 2026-09-22

#include "agenticdsl/evolution/harness_rsi.h"

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/evolution/transition_guard.h"
#include "agenticdsl/genome/genome.h"

#include <algorithm>
#include <string>

namespace agenticdsl::evolution {

namespace {

bool is_tool_allowed(const std::string& tool_name,
                     const MutationGovernancePolicy& policy) {
  for (const auto& denied : policy.denied_tools) {
    if (denied == tool_name) return false;
  }
  return true;
}

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

const char* genome_error_name(genome::GenomeError e) {
  switch (e) {
    case genome::GenomeError::NotFound:           return "NotFound";
    case genome::GenomeError::SchemaViolation:    return "SchemaViolation";
    case genome::GenomeError::IntegrityViolation: return "IntegrityViolation";
    case genome::GenomeError::BrokenLineage:      return "BrokenLineage";
    case genome::GenomeError::CycleDetected:      return "CycleDetected";
    case genome::GenomeError::IOError:            return "IOError";
    case genome::GenomeError::NotImplemented:     return "NotImplemented";
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

// GATE ORDER (G4 expanded):
//   Gate 0 (校验, 含 workflow_patch + genome_name/parent_version)
//   → Gate 1 evaluate_readiness → Gate 2 is_tool_allowed
//   → Gate 2.5 tools_add registry 预检 → Gate 3 (persist-before-apply)
//   → Apply
//
// 失败路径零状态变更.
Result<AppliedMutation, MutationError> apply_harness_mutation(
    const GenomeMutations& mutations,
    std::string& system_prompt,
    std::vector<std::string>& tools,
    IToolRegistry& registry,
    const MutationGateContext& ctx) {

  // === Gate 0: InvalidMutation fail-fast + workflow_patch ===
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
  // G4: workflow_patch → UnsupportedVariant at Gate 0 (先于所有后续门禁)
  if (has_workflow) {
    return Result<AppliedMutation, MutationError>::failure(
        MutationError::UnsupportedVariant);
  }
  // G4: genome_name 空检查 (registry 非空时, Gate 0)
  if (ctx.genome_registry != nullptr && ctx.genome_name.empty()) {
    return Result<AppliedMutation, MutationError>::failure(
        MutationError::InvalidMutation);
  }
  // G4: parent_version==0 (registry 非空时, Gate 0)
  if (ctx.genome_registry != nullptr && ctx.parent_version == 0) {
    return Result<AppliedMutation, MutationError>::failure(
        MutationError::InvalidMutation);
  }

  // === Gate 1: evaluate_readiness ===
  if (!ctx.attribution || !ctx.evaluator || !ctx.budget) {
    return Result<AppliedMutation, MutationError>::failure(
        MutationError::InvalidMutation);
  }
  auto verdict = evaluate_readiness(ctx.current, *ctx.attribution,
                                    *ctx.evaluator, *ctx.budget);
  if (!verdict.can_proceed) {
    if (ctx.bus) {
      EventBuilder event("evolution.readiness.denied");
      event.args(nlohmann::json{
          {"failed_conditions", verdict.failed_conditions},
          {"attribution_verdict", attribution_verdict_name(ctx.attribution->verdict)},
          {"eval_quality", "Unknown"},
          {"budget_state", ctx.budget->exceeded() ? "exceeded" : "ok"}
      });
      event.meta(nlohmann::json{
          {"trace_id", ctx.trace_id},
          {"current_state", evolution_state_name(ctx.current)}
      });
      ctx.bus->emit(event.build());
    }
    return Result<AppliedMutation, MutationError>::failure(
        MutationError::NotReady);
  }

  // === Gate 2: is_tool_allowed policy check ===
  for (const auto& tool_name : mutations.tools_add) {
    if (!is_tool_allowed(tool_name, ctx.policy)) {
      return Result<AppliedMutation, MutationError>::failure(
          MutationError::GovernanceDenied);
    }
  }
  for (const auto& tool_name : mutations.tools_remove) {
    if (!is_tool_allowed(tool_name, ctx.policy)) {
      return Result<AppliedMutation, MutationError>::failure(
          MutationError::GovernanceDenied);
    }
  }

  // === Gate 2.5: tools_add 全量 registry 预检 ===
  for (const auto& tool_name : mutations.tools_add) {
    if (!registry.has_tool(tool_name)) {
      return Result<AppliedMutation, MutationError>::failure(
          MutationError::RegistryRejected);
    }
  }

  // === 记录 Apply 前快照 (G4) ===
  AppliedMutation applied;
  applied.prompt_snapshot = system_prompt;
  applied.tools_snapshot = tools;

  // === Gate 3: Genome persist-before-apply (G4) ===
  if (ctx.genome_registry != nullptr) {
    // 计算最终态 spec
    std::string final_harness = system_prompt;
    if (has_prompt) final_harness += mutations.prompt_delta;

    std::vector<std::string> final_tools = tools;
    for (const auto& add : mutations.tools_add) {
      final_tools.push_back(add);
    }
    for (const auto& rm : mutations.tools_remove) {
      auto it = std::find(final_tools.begin(), final_tools.end(), rm);
      if (it != final_tools.end()) final_tools.erase(it);
    }
    // 最终态 tools 为空 → InvalidMutation (V1 边界)
    if (final_tools.empty()) {
      return Result<AppliedMutation, MutationError>::failure(
          MutationError::InvalidMutation);
    }

    genome::GenomeSpec spec;
    spec.harness = final_harness;
    spec.tools = final_tools;
    auto fork_res = ctx.genome_registry->fork(
        ctx.genome_name, ctx.parent_version, spec);
    if (!fork_res.has_value()) {
      if (ctx.bus) {
        EventBuilder event("genome.persist_failed");
        event.args(nlohmann::json{
            {"genome_name", ctx.genome_name},
            {"error", genome_error_name(fork_res.error())}
        });
        event.meta(nlohmann::json{{"trace_id", ctx.trace_id}});
        ctx.bus->emit(event.build());
      }
      return Result<AppliedMutation, MutationError>::failure(
          MutationError::RegistryRejected);
    }
    applied.committed_genome_version = fork_res.value().version;

    if (ctx.bus) {
      EventBuilder event("genome.committed");
      event.args(nlohmann::json{
          {"genome_name", ctx.genome_name},
          {"version", fork_res.value().version},
          {"parent", ctx.parent_version},
          {"mutation_kind", "harness"}
      });
      event.meta(nlohmann::json{{"trace_id", ctx.trace_id}});
      ctx.bus->emit(event.build());
    }
  }

  // === Apply (顺序: prompt_delta → tools_add → tools_remove) ===
  if (has_prompt) {
    system_prompt += mutations.prompt_delta;
    applied.applied_prompts.push_back(mutations.prompt_delta);
  }
  for (const auto& tool_name : mutations.tools_add) {
    tools.push_back(tool_name);
    applied.applied_tools_added.push_back(tool_name);
  }
  for (const auto& tool_name : mutations.tools_remove) {
    registry.unregister_tool_function(tool_name);
    auto it = std::find(tools.begin(), tools.end(), tool_name);
    if (it != tools.end()) tools.erase(it);
    applied.applied_tools_removed.push_back(tool_name);
  }

  return Result<AppliedMutation, MutationError>::success(applied);
}

void undo_applied_mutation(const AppliedMutation& applied,
                           std::string& system_prompt,
                           std::vector<std::string>& tools,
                           IToolRegistry* /*registry*/) {
  system_prompt = applied.prompt_snapshot;
  tools = applied.tools_snapshot;
}

}  // namespace agenticdsl::evolution