// include/agenticdsl/contract/imutation_governance.h
// 功能描述：Mutation Governance 契约接口 (ADR-0084, V1 = gate-and-audit only)
//          IMutationGovernor 抽象接口: propose / commit / revert 三方法,
//          每次调用恰好发射一个终态 mutation.* 审计事件 (ADR-0068 附录 A 登记)。
//          V1 边界: 不存储 subject 版本 / 不恢复 subject / 无保留窗口 /
//          revert() 为纯审计记录 API (实际恢复由调用方经 ADR-0079 session fork 负责)。
// 设计依据：docs/adr/adr-0084-mutation-governance-contract.md §决策 1-6
//          + openspec/changes/2026-08-26-adr-0084-mutation-governance-contract/design.md
// 作者：HydraForge Phase 6 / ADR-0084 V1 ship
// 最后修改日期：2026-08-26
#pragma once

#include <string>
#include <vector>

namespace agenticdsl {

// ============================================================================
// 授权模式 (复用 ADR-0004 ApprovalPolicy / ADR-0031 ExecutionPolicy 三模式概念;
// 契约层自定义枚举以避免依赖 src/common/policy/ 内部头文件)
// ============================================================================
enum class MutationMode {
  Yolo,   // 自动执行, 无人工介入
  Plan,   // 计划模式
  Agent   // Agent 模式 (L3 需 IApprovalHandler 人类复核)
};

// ============================================================================
// MutationContext — 一次变异 attempt 的完整上下文 (值类型, 调用方构造)
// 所有标识字段为不透明字符串: governor 透传不解释、不持久化 (design D1/D4)
// ============================================================================
struct MutationContext {
  std::string mutation_id;                  // attempt 唯一标识 (调用方提供)
  std::string source_id;                    // 变异来源 (须属构造注入的白名单)
  std::string mutation_kind;                // "L1_prompt" | "L2_dsl" | "L3_skill" | "L4_weight"
  std::string subject_ref;                  // 被变异对象引用 (不透明)
  std::string proposed_change;              // 提议变更摘要 (不透明)
  std::string parent_ref;                   // 父版本引用 (可空, 不透明)
  std::string version_id;                   // commit 目标版本 (不透明)
  MutationMode mode = MutationMode::Agent;  // 授权模式 (模式×等级矩阵输入)
  std::vector<std::string> evaluation_refs; // 不透明 evaluation_id 数组 (ADR-0083 评估层产出)
};

// ============================================================================
// MutationDecision — propose() / commit() 返回的判定结果
// approved=false 时 denial_reason / failed_step 非空 (与 mutation.denied payload 一致)
// ============================================================================
struct MutationDecision {
  bool approved = false;
  std::string denial_reason;  // 空 = approved
  std::string failed_step;    // 空 = approved
};

// ============================================================================
// mutation.* 主题常量 (ADR-0068 附录 A 文档注册; EventBuilder 无运行时注册 API)
// ============================================================================
namespace mutation_topics {
inline constexpr const char* kProposed = "mutation.proposed";
inline constexpr const char* kCommitted = "mutation.committed";
inline constexpr const char* kReverted = "mutation.reverted";
inline constexpr const char* kDenied = "mutation.denied";
}  // namespace mutation_topics

// ============================================================================
// IMutationGovernor — 变异治理门禁与审计抽象接口
//
// 门禁链顺序 (design.md D6, 实现与测试必须一致):
//   propose: 白名单 fail-closed → L4 emit-then-throw → 模式×等级矩阵
//            → agent+L3 IApprovalHandler 人类复核 → IEvaluator 评估门
//            → 行为回归门 → emit mutation.proposed
//   commit:  evaluation_refs 非空校验 → emit mutation.committed
//   revert:  emit mutation.reverted (audit-only, 不恢复任何状态)
//   任意失败: emit mutation.denied (含 denial_reason + failed_step), 不 emit 后续事件
//   L4: emit denied(l4_forbidden_v1) 严格先于 std::runtime_error 抛出
// ============================================================================
class IMutationGovernor {
 public:
  virtual ~IMutationGovernor() = default;

  /**
   * @brief 提议一次变异, 顺序执行全部门禁链
   * @param ctx 变异上下文 (值传递)
   * @return MutationDecision — approved=true 表示全部门禁通过并已 emit mutation.proposed
   * @throws std::runtime_error 当 mutation_kind="L4_weight" (emit denied 后抛出)
   */
  virtual MutationDecision propose(const MutationContext& ctx) = 0;

  /**
   * @brief 提交一次已提议的变异
   * @param ctx 变异上下文 (evaluation_refs 必须非空, 否则 fail-closed)
   * @return MutationDecision — approved=true 表示已 emit mutation.committed
   */
  virtual MutationDecision commit(const MutationContext& ctx) = 0;

  /**
   * @brief 记录一次回滚 (V1: 纯审计记录, 不读取/不修改/不恢复任何 subject 状态)
   * @param ctx 原变异上下文
   * @param target_version 回滚目标版本 (调用方提供, 不透明)
   * @param rollback_reason 回滚原因 (调用方提供, 不透明)
   */
  virtual void revert(const MutationContext& ctx,
                      const std::string& target_version,
                      const std::string& rollback_reason) = 0;
};

}  // namespace agenticdsl
