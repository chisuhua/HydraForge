// include/agenticdsl/types/mutation_record.h
// 功能描述：Mutation Governance 审计事件 payload 值类型 (ADR-0084 §决策 4)
//          4 个 mutation.* 主题 payload + MutationRecord 聚合值类型。
//          所有标识字段为不透明字符串 (调用方提供, governor 透传不解释)。
//          V1 = gate-and-audit only: 无 subject 内容/版本存储, 无保留窗口。
// 设计依据：openspec/changes/2026-08-26-adr-0084-mutation-governance-contract/design.md D1/D4
//          + docs/adr/adr-0068-event-emission-contract.md 附录 A (文档注册)
// 作者：HydraForge Phase 6 / ADR-0084 V1 ship
// 最后修改日期：2026-08-26
#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <utility>
#include <vector>

namespace agenticdsl {

// ============================================================================
// mutation.proposed payload (通过全部门禁后发射)
// ============================================================================
struct MutationProposedPayload {
  std::string mutation_id;                  // 本次变异 attempt 唯一标识 (不透明)
  std::string source_id;                    // 变异来源 (白名单内)
  std::string subject_ref;                  // 被变异对象引用 (不透明)
  std::string mutation_kind;                // "L1_prompt" | "L2_dsl" | "L3_skill"
  std::string proposed_change;              // 提议变更摘要 (不透明)
  std::string parent_ref;                   // 父版本引用 (可空, 不透明)
  std::vector<std::string> evaluation_refs; // 不透明 evaluation_id 数组 (ADR-0083)

  nlohmann::json to_json() const {
    return nlohmann::json{
        {"mutation_id", mutation_id},
        {"source_id", source_id},
        {"subject_ref", subject_ref},
        {"mutation_kind", mutation_kind},
        {"proposed_change", proposed_change},
        {"parent_ref", parent_ref},
        {"evaluation_refs", evaluation_refs},
    };
  }
};

// ============================================================================
// mutation.committed payload (commit 校验通过后发射)
// ============================================================================
struct MutationCommittedPayload {
  std::string mutation_id;                  // 与 proposed 同一 attempt 标识
  std::string version_id;                   // commit 目标版本 (调用方提供, 不透明)
  std::string mutation_kind;                // 变异等级
  std::vector<std::string> evaluation_refs; // 非空 (commit fail-closed 校验)

  nlohmann::json to_json() const {
    return nlohmann::json{
        {"mutation_id", mutation_id},
        {"version_id", version_id},
        {"mutation_kind", mutation_kind},
        {"evaluation_refs", evaluation_refs},
    };
  }
};

// ============================================================================
// mutation.reverted payload (revert() 纯审计记录, 不触发任何状态恢复)
// ============================================================================
struct MutationRevertedPayload {
  std::string mutation_id;      // 原 attempt 标识 (不透明)
  std::string target_version;   // 回滚目标版本 (调用方提供, 不透明)
  std::string rollback_reason;  // 回滚原因 (调用方提供, 不透明)

  nlohmann::json to_json() const {
    return nlohmann::json{
        {"mutation_id", mutation_id},
        {"target_version", target_version},
        {"rollback_reason", rollback_reason},
    };
  }
};

// ============================================================================
// mutation.denied payload (任一门禁步骤失败时发射, 终态事件)
// ============================================================================
struct MutationDeniedPayload {
  std::string mutation_id;    // attempt 标识 (不透明)
  std::string denial_reason;  // e.g. "non_whitelisted_source" / "l4_forbidden_v1" /
                              //      "plan_required" / "plan_insufficient" /
                              //      "approval_denied" / "approval_handler_unavailable" /
                              //      "evaluation_poor_quality" /
                              //      "behavioral_regression_failed" /
                              //      "missing_evaluation_refs"
  std::string failed_step;    // "source_whitelist" | "authorization" | "human_review" |
                              // "evaluation" | "behavioral_regression"
  std::string subject_ref;    // 被变异对象引用 (不透明)

  nlohmann::json to_json() const {
    return nlohmann::json{
        {"mutation_id", mutation_id},
        {"denial_reason", denial_reason},
        {"failed_step", failed_step},
        {"subject_ref", subject_ref},
    };
  }
};

// ============================================================================
// MutationRecord — 审计记录聚合值类型 (V1: 仅事件 payload 聚合, 不持久化)
// ============================================================================
struct MutationRecord {
  std::string mutation_id;
  std::string source_id;
  std::string mutation_kind;
  std::string subject_ref;
  std::string state;  // "proposed" | "committed" | "reverted" | "denied"

  nlohmann::json to_json() const {
    return nlohmann::json{
        {"mutation_id", mutation_id},
        {"source_id", source_id},
        {"mutation_kind", mutation_kind},
        {"subject_ref", subject_ref},
        {"state", state},
    };
  }
};

}  // namespace agenticdsl
