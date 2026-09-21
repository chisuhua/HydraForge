// include/agenticdsl/evolution/harness_rsi.h
// C4 harness-rsi-pilot 轻量函数定义 (per ADR-0088 D4 显式取消 IHarnessRSI 三算子接口)
// Header-only 设计: 5 struct + 1 free function + 1 enum + 4 error variant
// 设计依据: openspec/changes/2026-09-16-harness-rsi-pilot/ (C4 DRAFT after 2nd Oracle review)
// 作者: HydraForge Sprint 34+ Phase 6c MetaRSI-v1 C4
// 最后修改日期: 2026-09-21

#pragma once

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/evolution/transition_guard.h"
#include "agenticdsl/types/attribution_record.h"
#include "agenticdsl/types/reward_signal.h"

#include <optional>
#include <string>
#include <vector>

namespace agenticdsl::evolution {

// C4 SCOPE: 3 mutation types only (prompt_delta / tools_add / tools_remove).
// No additions without ADR-0088 D4 amendment.
struct GenomeMutations {
  std::string prompt_delta;
  std::vector<std::string> tools_add;
  std::vector<std::string> tools_remove;
  std::optional<std::string> workflow_patch;  // 返回 UnsupportedVariant (Wave 3)
};

// MutationGovernancePolicy — C4 内部轻量 policy (per Oracle C3 不调 IMutationGovernance::propose)
// 与 ADR-0084 决策 1/6 (白名单 + 模式×等级) 对齐, 但只暴露 deny 列表 (allow 默认全放)
struct MutationGovernancePolicy {
  std::vector<std::string> denied_tools;
};

// MutationGateContext — 封装 readiness 评估所需的全部依赖
struct MutationGateContext {
  EvolutionState current;                                // evaluate_readiness 首参 (transition_guard.h:56)
  const AttributionRecord* attribution;                   // ADR-0086 v1.1
  IEvaluator* evaluator;
  IBudgetController* budget;                             // ADR-0019 §1.4
  IInteractionBus* bus;                                   // 发射 evolution.readiness.denied
  MutationGovernancePolicy policy;
};

// AppliedMutation — 记录实际应用的 mutation 列表 (无 Genome, 无 IGenomeRegistry 依赖, per Oracle M2)
struct AppliedMutation {
  std::vector<std::string> applied_prompts;
  std::vector<std::string> applied_tools_added;
  std::vector<std::string> applied_tools_removed;
};

// MutationError — 5 个变体覆盖所有失败路径
enum class MutationError {
  NotReady,             // 含 failed_conditions payload (per ADR-0088 D1-D4 readiness 4-condition fail)
  GovernanceDenied,     // 含 denial_reason payload (per C3 内部 policy check)
  UnsupportedVariant,   // workflow_patch 路径 (Wave 3 deferred)
  RegistryRejected,     // tools_add 注册时 ToolMetadata 校验失败 (与 GovernanceDenied 语义不同)
  InvalidMutation,      // 空 prompt_delta 或 tools_add/remove 同名 fail-fast
};

}  // namespace agenticdsl::evolution
