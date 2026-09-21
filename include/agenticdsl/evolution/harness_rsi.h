// include/agenticdsl/evolution/harness_rsi.h
// C4 harness-rsi-pilot 轻量函数定义 (per ADR-0088 D4 取消 IHarnessRSI 三算子接口)
// Header-only 设计: 5 struct + 1 enum + 1 free function
// 设计依据: openspec/changes/2026-09-16-harness-rsi-pilot/
// 作者: HydraForge Sprint 34+ Phase 6c MetaRSI-v1 C4
// 最后修改日期: 2026-09-21

#pragma once

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/evolution/transition_guard.h"
#include "agenticdsl/types/attribution_record.h"
#include "agenticdsl/types/reward_signal.h"
#include "modules/budget/budget_controller.h"
#include "common/llm/llm_types.h"

#include <optional>
#include <string>
#include <vector>

namespace agenticdsl::evolution {

struct GenomeMutations {
  std::string prompt_delta;
  std::vector<std::string> tools_add;
  std::vector<std::string> tools_remove;
  std::optional<std::string> workflow_patch;
};

struct MutationGovernancePolicy {
  std::vector<std::string> denied_tools;
};

struct MutationGateContext {
  EvolutionState current;
  const AttributionRecord* attribution;
  IEvaluator* evaluator;
  IBudgetController* budget;
  IInteractionBus* bus;
  MutationGovernancePolicy policy;
};

struct AppliedMutation {
  std::vector<std::string> applied_prompts;
  std::vector<std::string> applied_tools_added;
  std::vector<std::string> applied_tools_removed;
};

enum class MutationError {
  NotReady,
  GovernanceDenied,
  UnsupportedVariant,
  RegistryRejected,
  InvalidMutation,
};

// DUAL-GATE ORDER: evaluate_readiness → is_tool_allowed → apply.
// 失败路径零状态变更. workflow_patch → UnsupportedVariant.
agenticdsl::Result<AppliedMutation, MutationError> apply_harness_mutation(
    const GenomeMutations& mutations,
    std::string& system_prompt,
    std::vector<std::string>& tools,
    IToolRegistry& registry,
    const MutationGateContext& ctx);

}  // namespace agenticdsl::evolution
