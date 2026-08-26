// src/common/governance/mutation_governor.h
// 功能描述：MutationGovernor — IMutationGovernor 的 V1 gate-and-audit 实现
//          构造注入: 非空 IEvaluator (fail-fast) + 不可变白名单 + IInteractionBus
//          + 可空 IApprovalHandler* (agent+L3 人类复核) + 可注入行为回归门
//          门禁链顺序 (design.md D6):
//            白名单 fail-closed → L4 emit-then-throw → 模式×等级矩阵
//            → agent+L3 IApprovalHandler → IEvaluator 评估门 → 行为回归门
//            → emit proposed / commit 校验 → emit committed
// 设计依据：docs/adr/adr-0084-mutation-governance-contract.md §决策 1-6
//          + openspec/changes/2026-08-26-adr-0084-mutation-governance-contract/design.md
// 作者：HydraForge Phase 6 / ADR-0084 V1 ship
// 最后修改日期：2026-08-26
#pragma once

#include "agenticdsl/contract/imutation_governance.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>

namespace agenticdsl {

// 前向声明 (契约层接口, 头文件解耦)
class IInteractionBus;
class IEvaluator;
class IApprovalHandler;

/**
 * @brief MutationGovernor — V1 gate-and-audit 实现
 *
 * V1 边界 (design D1): 不存储 subject 版本 / 不恢复 subject / 无保留窗口 /
 * revert() 为纯审计记录 API。governor 内部无任何 subject 可读回状态。
 *
 * 线程安全: V1 实现为无状态 (仅读构造注入的不可变字段), emit 经 IInteractionBus
 * (实现方负责线程安全, InMemoryBus 已保证)。
 */
class MutationGovernor : public IMutationGovernor {
 public:
  /**
   * @brief 行为回归门 (ADR-0061-02 集成点)
   * @return true = Pass (继续), false = Fail (denied behavioral_regression_failed)
   *
   * V1 默认 = 始终 Pass (文档化简化): 完整 BehavioralRegressionGate 接线
   * (hotelling_t2_test + BehaviorFingerprint) 由调用方经此 hook 注入;
   * governor 提供 fail-closed 语义 (hook 返回 false → denied)。
   */
  using RegressionGate = std::function<bool(const MutationContext&)>;

  /**
   * @param evaluator 非空 IEvaluator (ADR-0083); nullptr → std::invalid_argument
   * @param source_whitelist 不可变来源白名单; 默认空 = 全部拒绝 (fail-closed)
   * @param bus 审计事件总线; nullptr → std::invalid_argument (审计为强制职责)
   * @param approval_handler 可空; agent+L3 时 nullptr → fail-closed
   *        denial_reason="approval_handler_unavailable"
   * @param regression_gate 可空; 空 = V1 默认 Pass
   */
  MutationGovernor(std::shared_ptr<IEvaluator> evaluator,
                   std::unordered_set<std::string> source_whitelist,
                   IInteractionBus* bus,
                   IApprovalHandler* approval_handler = nullptr,
                   RegressionGate regression_gate = {});

  MutationDecision propose(const MutationContext& ctx) override;
  MutationDecision commit(const MutationContext& ctx) override;
  void revert(const MutationContext& ctx,
              const std::string& target_version,
              const std::string& rollback_reason) override;

 private:
  bool source_allowed(const std::string& source_id) const;
  MutationDecision deny(const MutationContext& ctx,
                        const std::string& denial_reason,
                        const std::string& failed_step);
  void emit_proposed(const MutationContext& ctx);
  void emit_committed(const MutationContext& ctx);

  std::shared_ptr<IEvaluator> evaluator_;              // 非空 (构造 fail-fast)
  const std::unordered_set<std::string> whitelist_;    // 构造后不可变
  IInteractionBus* bus_;                               // 非空, 非拥有
  IApprovalHandler* approval_handler_;                 // 可空, 非拥有
  RegressionGate regression_gate_;                     // 可空 (空 = V1 Pass)
};

}  // namespace agenticdsl
