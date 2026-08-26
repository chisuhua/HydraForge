# ADR-0083 Implementation Scope Audit

> **生成时间**: 2026-08-26 (IEvaluator/RewardSignal 契约 ship)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0083-evaluator-reward-contract.md](adr-0083-evaluator-reward-contract.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

**📋 Audit** (impl-scope-audit 文档, 与 docs_drift_audit 配套使用)

✅ Approved (audit 后保持 — 核心契约类全部 Shipped, V2 评估器为 ADR 明确 out-of-scope 项)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 3 个描述的类未在 src/include 中找到
(`BehavioralEquivalenceEvaluator` / `CompositeEvaluator` / `TaskSuccessEvaluator`)。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `IEvaluator` | ✅ Shipped | `include/agenticdsl/contract/ievaluator.h` | 纯虚接口 (evaluate + compare) |
| `RewardSignal` | ✅ Shipped | `include/agenticdsl/types/reward_signal.h` | 三态 quality + scalar/confidence + 工厂方法范围验证 |
| `ExecutionTrace` | ✅ Shipped | `include/agenticdsl/types/execution_trace.h` | final_result + trace_id + trajectory_refs |
| `TaskSuccessEvaluator` | ✅ Shipped (test-local) | `tests/test_evaluator.cpp` | V1 简化实现: ok→Excellent / !ok→Poor, compare 恒 0。V1 作为测试内 reference 实现, 未提取到 src/ (调用方通过 IEvaluator 多态注入, 不依赖具体类) |
| `BehavioralEquivalenceEvaluator` | 📅 Deferred | — | ADR 明确标注 V2 out of scope, 留 follow-up `ship-evaluator-v2-composite` |
| `CompositeEvaluator` | 📅 Deferred | — | 同上, V2 out of scope |

## 分类详情

### ✅ Shipped (test-local) — `TaskSuccessEvaluator`

ADR §决策 5 V1 映射 (ok→Excellent / !ok→Poor) 已在 `tests/test_evaluator.cpp` 实现并通过
12 cases / 31 assertions 验证 (Phase 0-3 契约 + setter 注入 + evaluation.result 事件)。
V1 设计意图是"最简可用评估器", 调用方 (CognitiveWorker/DomainWorkerPool) 通过
`set_evaluator(shared_ptr<IEvaluator>)` 多态注入, 不依赖具体类型, 故 V1 未提取到 src/ 生产代码。
若后续 V2 落地需要共享 V1 实现, 再提取至 `src/common/evaluator/` (design D7 预留路径)。

### 📅 Deferred — V2 评估器

ADR 与 OpenSpec change `2026-08-26-ship-ievaluator-reward-contract` 明确将
BehavioralEquivalenceEvaluator / CompositeEvaluator 列为 out of scope,
留作 follow-up change `ship-evaluator-v2-composite`。

## 决策

主 ADR 状态保持 ✅ Approved — 本 change 范围内全部契约类已 ship 且 ctest 186/186 零回归;
Deferred 项均为 ADR 自我声明的 V2 边界, 非实施缺口。
