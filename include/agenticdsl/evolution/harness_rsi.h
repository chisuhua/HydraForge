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
#include "agenticdsl/genome/genome.h"
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
  // 末尾追加, 默认空, 非 BREAKING. Wave 3 调用方必填.
  std::string trace_id;
  // G4: Genome 持久化上下文 (可选). registry=nullptr 时 Gate 3 跳过 → V1 逐字节一致.
  genome::IGenomeRegistry* genome_registry = nullptr;
  std::string genome_name;
  uint64_t parent_version = 0;
};

struct AppliedMutation {
  std::vector<std::string> applied_prompts;
  std::vector<std::string> applied_tools_added;
  std::vector<std::string> applied_tools_removed;
  // G4: 自包含快照 (apply 成功时填充) + 持久化版本号
  uint64_t committed_genome_version = 0;
  std::string prompt_snapshot;
  std::vector<std::string> tools_snapshot;
};

enum class MutationError {
  NotReady,
  GovernanceDenied,
  UnsupportedVariant,
  RegistryRejected,
  InvalidMutation,
};

// DUAL-GATE ORDER: Gate 0 (含 workflow_patch) → evaluate_readiness → is_tool_allowed → apply.
// G4 扩展: Gate 3 (persist-before-apply) 在 is_tool_allowed 之后、apply 之前.
// 失败路径零状态变更. workflow_patch → UnsupportedVariant (Gate 0).
agenticdsl::Result<AppliedMutation, MutationError> apply_harness_mutation(
    const GenomeMutations& mutations,
    std::string& system_prompt,
    std::vector<std::string>& tools,
    IToolRegistry& registry,
    const MutationGateContext& ctx);

// G4: 自包含快照 undo — 从 AppliedMutation 读取 prompt_snapshot/tools_snapshot 恢复.
// V1 限制: tools_remove 已 unregister 的 ToolFunc 无法经 IToolRegistry 恢复（接口无 getter）.
void undo_applied_mutation(const AppliedMutation& applied,
                           std::string& system_prompt,
                           std::vector<std::string>& tools,
                           IToolRegistry* registry);

}  // namespace agenticdsl::evolution
